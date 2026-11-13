//! The execution DAG of one query, which is the formula after rewrites, with shared subterms merged.
//!
//! The rewrites keep the relation.
//! - complements are pushed down to leaves and non-Boolean nodes (De Morgan, and `¬¬X = X` for an `X`
//!   without padding, as a complement has no padded words);
//! - `op(id(A), id(B)) = id(op(A, B))` for union and intersection;
//! - projections fold, `project(id(L), [i]) = L`, `project(id(L), [0, 1] or [1, 0]) = id(L)`, and a projection
//!   to all tapes in order is dropped;
//! - `project(id(U) ∩ X, [i])` becomes one diagonal node, or `U ∩ X'` when `X` is a sync product of a tape of
//!   each operand, where `X'` also synchronizes those two tapes, so it builds only the moves that the diagonal keeps;
//! - `op(X, X) = X`, and structurally equal nodes are built once;
//! - the cheaper operand of a union or intersection goes left.
//!
//! Explicit nodes stop the rewrites from looking into them.

use std::cmp::Ordering;
use std::collections::hash_map::Entry;
use std::collections::{BTreeMap, btree_map};
use std::ops::Deref;
use std::sync::Arc;

use rustc_hash::{FxHashMap, FxHashSet};
use typed_index_collections::TiVec;

use crate::formula::{Formula, Node, ResultTape, SyncSpec, TapePair};
use crate::ids::{ExecId, Id, LeafId, NodeId};
use crate::leaf::Leaf;
use crate::{ExplicitOptions, Tape};

/// A leaf of the formula, or an explicit node that the query built.
#[derive(Clone, Debug)]
pub(crate) enum LeafRef<'f> {
	Borrowed(&'f Leaf),
	Shared(Arc<Leaf>),
}

impl Deref for LeafRef<'_> {
	type Target = Leaf;

	fn deref(&self) -> &Leaf {
		match self {
			LeafRef::Borrowed(leaf) => leaf,
			LeafRef::Shared(leaf) => leaf,
		}
	}
}

#[derive(Clone, Debug)]
pub(crate) enum Op<'f> {
	Leaf(LeafRef<'f>),
	/// Nested unions flattened into one node, cheaper branches first.
	Union(Arc<[ExecId]>),
	Intersection(ExecId, ExecId),
	Complement(ExecId),
	Identity(ExecId),
	Project(ExecId, Arc<[usize]>),
	Sync(ExecId, ExecId, Arc<SyncSpec>),
	/// `{w | w ∈ lhs, (w, w) ∈ rhs}`, with pair macrostates.
	Diagonal(ExecId, ExecId),
	/// Replaced by a leaf, or by [`Transparent`](Op::Transparent), before the search.
	Explicit {
		child: ExecId,
		options: ExplicitOptions,
		origin: NodeId,
	},
	/// An explicit node that stayed lazy, whose macrostates are its child's.
	Transparent(ExecId),
}

impl Op<'_> {
	pub(crate) fn children(&self) -> impl Iterator<Item = ExecId> {
		let (operands, branches): ([Option<ExecId>; 2], &[ExecId]) = match self {
			Op::Leaf(_) => ([None, None], &[]),
			Op::Union(branches) => ([None, None], branches),
			Op::Intersection(a, b) | Op::Sync(a, b, _) | Op::Diagonal(a, b) => {
				([Some(*a), Some(*b)], &[])
			}
			Op::Complement(a)
			| Op::Identity(a)
			| Op::Project(a, _)
			| Op::Explicit { child: a, .. }
			| Op::Transparent(a) => ([Some(*a), None], &[]),
		};

		operands
			.into_iter()
			.flatten()
			.chain(branches.iter().copied())
	}

	/// The child whose macrostates this node shares, if any.
	pub(crate) fn transparent_child(&self) -> Option<ExecId> {
		match *self {
			Op::Identity(child) | Op::Project(child, _) | Op::Transparent(child) => Some(child),
			_ => None,
		}
	}
}

#[derive(Debug)]
pub(crate) struct ExecNode<'f> {
	pub(crate) op: Op<'f>,
	pub(crate) arity: usize,
}

#[derive(Debug)]
pub(crate) struct Exec<'f> {
	/// Children come before their parents.
	pub(crate) nodes: TiVec<ExecId, ExecNode<'f>>,
	pub(crate) root: ExecId,
	pub(crate) reachable: TiVec<ExecId, bool>,
}

impl Exec<'_> {
	/// `node` and the children whose macrostates it shares, down to their owner.
	pub(crate) fn sharing(&self, node: ExecId) -> impl Iterator<Item = ExecId> {
		std::iter::successors(Some(node), |&node| self.nodes[node].op.transparent_child())
	}

	/// The node that owns the macrostates of `node`.
	pub(crate) fn owner(&self, node: ExecId) -> ExecId {
		self.sharing(node).last().unwrap_or(node)
	}
}

/// A source node in one of three forms; `Inner` is a node marked explicit by the query, without the mark.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, PartialOrd, Ord)]
enum Form {
	Inner,
	Plain,
	Negated,
}

type Src = (NodeId, Form);

fn plain(node: NodeId) -> Src {
	(node, Form::Plain)
}

#[derive(Clone, Copy, Debug)]
enum Boolean {
	Union,
	Intersection,
}

impl Boolean {
	/// The operation that De Morgan's laws swap with this one.
	fn dual(self) -> Boolean {
		match self {
			Boolean::Union => Boolean::Intersection,
			Boolean::Intersection => Boolean::Union,
		}
	}
}

/// What a source node in one form becomes in the DAG.
#[derive(Debug)]
enum Decision {
	Alias(Src),
	Leaf(LeafId),
	Complement(Src),
	Boolean(Boolean, Src, Src),
	IdentityOf(Boolean, Src, Src),
	Identity(Src),
	Project(Src, Arc<[usize]>),
	Diagonal(Src, Src),
	/// `U ∩ X'` for the diagonal of `U` and a sync product, with the product's two tapes synchronized.
	DiagonalSync(Src, Src, Src, Arc<SyncSpec>),
	Sync(Src, Src, Arc<SyncSpec>),
	Explicit(Src, ExplicitOptions, NodeId),
}

impl Decision {
	fn deps(&self) -> Vec<Src> {
		match self {
			Decision::Leaf(_) => Vec::new(),
			Decision::Alias(s)
			| Decision::Complement(s)
			| Decision::Identity(s)
			| Decision::Project(s, _)
			| Decision::Explicit(s, ..) => vec![*s],
			Decision::Boolean(_, a, b)
			| Decision::IdentityOf(_, a, b)
			| Decision::Diagonal(a, b)
			| Decision::Sync(a, b, _) => vec![*a, *b],
			Decision::DiagonalSync(u, a, b, _) => vec![*u, *a, *b],
		}
	}
}

struct Rewriter<'a> {
	formula: &'a Formula,
	marked: &'a FxHashSet<NodeId>,
}

impl Rewriter<'_> {
	fn is_marked(&self, node: NodeId) -> bool {
		self.marked.contains(&node) && !matches!(self.formula.nodes[node], Node::Explicit(..))
	}

	fn identity_child(&self, node: NodeId) -> Option<NodeId> {
		match self.formula.nodes[node] {
			Node::Identity(child) if !self.is_marked(node) => Some(child),
			_ => None,
		}
	}

	/// The decision of every source node that `root` depends on, in an order where dependencies come first (a
	/// smaller node, or the same node in an earlier form).
	fn decide_all(&self, root: NodeId) -> BTreeMap<Src, Decision> {
		let mut decisions = BTreeMap::new();
		let mut stack = vec![plain(root)];
		while let Some(src) = stack.pop() {
			if let btree_map::Entry::Vacant(entry) = decisions.entry(src) {
				let decision = self.decide(src);
				stack.extend(decision.deps());
				entry.insert(decision);
			}
		}

		decisions
	}

	fn decide(&self, (node, form): Src) -> Decision {
		if self.is_marked(node) {
			match form {
				Form::Plain => {
					return Decision::Explicit((node, Form::Inner), ExplicitOptions::new(), node);
				}
				Form::Negated => return Decision::Complement(plain(node)),
				Form::Inner => {}
			}
		}

		let negated = form == Form::Negated;

		match &self.formula.nodes[node] {
			Node::Complement(child) if negated && self.formula.pads[*child] => {
				Decision::Complement(plain(node))
			}
			Node::Complement(child) => {
				Decision::Alias((*child, if negated { Form::Plain } else { Form::Negated }))
			}
			Node::Union(a, b) => self.decide_boolean(Boolean::Union, *a, *b, negated),
			Node::Intersection(a, b) => self.decide_boolean(Boolean::Intersection, *a, *b, negated),
			_ if negated => Decision::Complement(plain(node)),
			Node::Leaf(leaf) => Decision::Leaf(*leaf),
			Node::Identity(child) => Decision::Identity(plain(*child)),
			Node::Project(child, tapes) => self.decide_project(*child, tapes),
			Node::Sync(a, b, spec) => Decision::Sync(plain(*a), plain(*b), Arc::clone(spec)),
			Node::Explicit(child, options) => Decision::Explicit(plain(*child), *options, node),
		}
	}

	fn decide_boolean(&self, op: Boolean, a: NodeId, b: NodeId, negated: bool) -> Decision {
		if negated {
			return Decision::Boolean(op.dual(), (a, Form::Negated), (b, Form::Negated));
		}

		match (self.identity_child(a), self.identity_child(b)) {
			(Some(lhs), Some(rhs)) => Decision::IdentityOf(op, plain(lhs), plain(rhs)),
			_ => Decision::Boolean(op, plain(a), plain(b)),
		}
	}

	fn decide_project(&self, child: NodeId, tapes: &Arc<[usize]>) -> Decision {
		let mut base = child;
		let mut kept = Arc::clone(tapes);
		while !self.is_marked(base)
			&& let Node::Project(inner, inner_tapes) = &self.formula.nodes[base]
		{
			kept = kept.iter().map(|&tape| inner_tapes[tape]).collect();
			base = *inner;
		}

		if let Some(language) = self.identity_child(base) {
			match kept.len() {
				1 => return Decision::Alias(plain(language)),
				2 => return Decision::Alias(plain(base)),
				_ => {}
			}
		}

		if kept.iter().copied().eq(0..self.formula.arities[base]) {
			return Decision::Alias(plain(base));
		}

		if kept.len() == 1
			&& !self.is_marked(base)
			&& let Node::Intersection(p, q) = self.formula.nodes[base]
			&& self.formula.arities[base] == 2
		{
			if let Some(u) = self.identity_child(p) {
				return self.decide_diagonal(u, q);
			}

			if let Some(u) = self.identity_child(q) {
				return self.decide_diagonal(u, p);
			}
		}

		Decision::Project(plain(base), kept)
	}

	/// `{w | w ∈ u, (w, w) ∈ x}`.
	fn decide_diagonal(&self, u: NodeId, x: NodeId) -> Decision {
		if !self.is_marked(x)
			&& let Node::Sync(a, b, spec) = &self.formula.nodes[x]
			&& let [first, second] = *spec.layout
			&& first.peer.is_none()
			&& second.peer.is_none()
			&& let (Tape::Lhs(lhs), Tape::Rhs(rhs)) | (Tape::Rhs(rhs), Tape::Lhs(lhs)) =
				(first.source, second.source)
		{
			let spec = SyncSpec {
				pairs: spec
					.pairs
					.iter()
					.copied()
					.chain([TapePair { lhs, rhs }])
					.collect(),
				layout: [ResultTape {
					source: Tape::Lhs(lhs),
					peer: Some(rhs),
				}]
				.into(),
			};

			return Decision::DiagonalSync(plain(u), plain(*a), plain(*b), Arc::new(spec));
		}

		Decision::Diagonal(plain(u), plain(x))
	}
}

/// The structure of a node, by which equal nodes are built once.
#[derive(PartialEq, Eq, Hash)]
enum Key {
	Leaf(LeafId),
	Union(Arc<[ExecId]>),
	Intersection(ExecId, ExecId),
	Complement(ExecId),
	Identity(ExecId),
	Project(ExecId, Arc<[usize]>),
	Sync(ExecId, ExecId, Arc<SyncSpec>),
	Diagonal(ExecId, ExecId),
	Explicit(ExecId, NodeId),
}

/// Depth and log2 of a bound on the number of macrostates, to order operands.
#[derive(Clone, Copy)]
struct Cost {
	depth: u32,
	log_states: f64,
}

impl Ord for Cost {
	fn cmp(&self, other: &Cost) -> Ordering {
		self.depth
			.cmp(&other.depth)
			.then(self.log_states.total_cmp(&other.log_states))
	}
}

impl PartialOrd for Cost {
	fn partial_cmp(&self, other: &Cost) -> Option<Ordering> {
		Some(self.cmp(other))
	}
}

impl PartialEq for Cost {
	fn eq(&self, other: &Cost) -> bool {
		self.cmp(other).is_eq()
	}
}

impl Eq for Cost {}

#[derive(Default)]
struct Emitter<'f> {
	nodes: TiVec<ExecId, ExecNode<'f>>,
	costs: TiVec<ExecId, Cost>,
	pads: TiVec<ExecId, bool>,
	interned: FxHashMap<Key, ExecId>,
}

impl<'f> Emitter<'f> {
	fn emit(&mut self, key: Key, op: Op<'f>, arity: usize, cost: Cost) -> ExecId {
		match self.interned.entry(key) {
			Entry::Occupied(entry) => *entry.get(),
			Entry::Vacant(entry) => {
				let pads = match &op {
					Op::Leaf(leaf) => leaf.has_padding,
					Op::Complement(_) => false,
					op => op.children().any(|child| self.pads[child]),
				};

				self.pads.push(pads);
				let id = self.nodes.push_and_get_key(ExecNode { op, arity });
				self.costs.push(cost);

				*entry.insert(id)
			}
		}
	}

	/// The node of `decision`, whose dependencies are `built`.
	fn emit_decision(
		&mut self,
		formula: &'f Formula,
		built: &FxHashMap<Src, ExecId>,
		decision: &Decision,
	) -> ExecId {
		match decision {
			Decision::Alias(src) => built[src],
			Decision::Leaf(index) => self.leaf(*index, &formula.leaves[*index]),
			Decision::Complement(src) => self.complement(built[src]),
			Decision::Boolean(op, a, b) => self.boolean(*op, built[a], built[b]),
			Decision::IdentityOf(op, lhs, rhs) => {
				let inner = self.boolean(*op, built[lhs], built[rhs]);

				self.identity(inner)
			}
			Decision::Identity(src) => self.identity(built[src]),
			Decision::Project(src, tapes) => self.project(built[src], tapes),
			Decision::Diagonal(u, x) => self.diagonal(built[u], built[x]),
			Decision::DiagonalSync(u, a, b, spec) => {
				let synced = self.sync(built[a], built[b], spec);

				self.intersection(built[u], synced)
			}
			Decision::Sync(a, b, spec) => self.sync(built[a], built[b], spec),
			Decision::Explicit(src, options, origin) => {
				self.explicit(built[src], *options, *origin)
			}
		}
	}

	fn unary_cost(&self, child: ExecId) -> Cost {
		let cost = self.costs[child];

		Cost {
			depth: cost.depth + 1,
			log_states: cost.log_states,
		}
	}

	fn product_cost(&self, a: ExecId, b: ExecId) -> Cost {
		let (a, b) = (self.costs[a], self.costs[b]);

		Cost {
			depth: a.depth.max(b.depth) + 1,
			log_states: a.log_states + b.log_states,
		}
	}

	fn leaf(&mut self, index: LeafId, leaf: &'f Leaf) -> ExecId {
		let states = u32::try_from(leaf.num_states().max(1)).unwrap_or(u32::MAX);
		let cost = Cost {
			depth: 0,
			log_states: f64::from(states).log2(),
		};
		let op = Op::Leaf(LeafRef::Borrowed(leaf));

		self.emit(Key::Leaf(index), op, leaf.arity(), cost)
	}

	fn boolean(&mut self, op: Boolean, a: ExecId, b: ExecId) -> ExecId {
		match op {
			Boolean::Union => self.union(a, b),
			Boolean::Intersection => self.intersection(a, b),
		}
	}

	fn intersection(&mut self, a: ExecId, b: ExecId) -> ExecId {
		if a == b {
			return a;
		}

		let (a, b) = if self.costs[a] <= self.costs[b] {
			(a, b)
		} else {
			(b, a)
		};

		let cost = self.product_cost(a, b);
		let key = Key::Intersection(a.min(b), a.max(b));
		let arity = self.nodes[a].arity;

		self.emit(key, Op::Intersection(a, b), arity, cost)
	}

	/// One union node over the branches of both operands, as a chain of binary unions would copy every
	/// macrostate into each union above it.
	fn union(&mut self, a: ExecId, b: ExecId) -> ExecId {
		let mut branches = Vec::new();
		for node in [a, b] {
			match &self.nodes[node].op {
				Op::Union(inner) => branches.extend_from_slice(inner),
				_ => branches.push(node),
			}
		}

		branches.sort_unstable();
		branches.dedup();
		if let [only] = branches[..] {
			return only;
		}

		let key = Key::Union(branches.as_slice().into());
		let costs = branches.iter().map(|&node| self.costs[node]);
		let cost = Cost {
			depth: costs.clone().map(|cost| cost.depth).max().unwrap_or(0) + 1,
			log_states: log2_sum(costs.map(|cost| cost.log_states)),
		};
		branches.sort_by_key(|&branch| self.costs[branch]);
		let arity = self.nodes[a].arity;

		self.emit(key, Op::Union(branches.into()), arity, cost)
	}

	fn identity(&mut self, child: ExecId) -> ExecId {
		let cost = self.unary_cost(child);

		self.emit(Key::Identity(child), Op::Identity(child), 2, cost)
	}

	fn complement(&mut self, child: ExecId) -> ExecId {
		if let Op::Complement(inner) = self.nodes[child].op
			&& !self.pads[inner]
		{
			return inner;
		}

		let mut cost = self.unary_cost(child);
		// Up to one macrostate per set of the child's states.
		cost.log_states = cost.log_states.exp2();
		let arity = self.nodes[child].arity;

		self.emit(Key::Complement(child), Op::Complement(child), arity, cost)
	}

	fn project(&mut self, child: ExecId, tapes: &Arc<[usize]>) -> ExecId {
		let cost = self.unary_cost(child);
		let key = Key::Project(child, Arc::clone(tapes));
		let op = Op::Project(child, Arc::clone(tapes));

		self.emit(key, op, tapes.len(), cost)
	}

	fn diagonal(&mut self, u: ExecId, x: ExecId) -> ExecId {
		let cost = self.product_cost(u, x);

		self.emit(Key::Diagonal(u, x), Op::Diagonal(u, x), 1, cost)
	}

	fn sync(&mut self, a: ExecId, b: ExecId, spec: &Arc<SyncSpec>) -> ExecId {
		let cost = self.product_cost(a, b);
		let key = Key::Sync(a, b, Arc::clone(spec));
		let op = Op::Sync(a, b, Arc::clone(spec));

		self.emit(key, op, spec.layout.len(), cost)
	}

	fn explicit(&mut self, child: ExecId, options: ExplicitOptions, origin: NodeId) -> ExecId {
		if matches!(self.nodes[child].op, Op::Explicit { .. }) {
			return child;
		}

		let cost = self.unary_cost(child);
		let op = Op::Explicit {
			child,
			options,
			origin,
		};
		let arity = self.nodes[child].arity;

		self.emit(Key::Explicit(child, origin), op, arity, cost)
	}
}

/// `log2(Σ 2^x)` over `logs`, taken relative to the largest so that it does not overflow.
fn log2_sum(logs: impl Iterator<Item = f64> + Clone) -> f64 {
	let max = logs.clone().fold(f64::NEG_INFINITY, f64::max);
	if max.is_infinite() {
		return max;
	}

	max + logs.map(|log| (log - max).exp2()).sum::<f64>().log2()
}

/// Builds the execution DAG of `root`, with the source nodes in `marked` made explicit.
pub(crate) fn build<'f>(
	formula: &'f Formula,
	root: NodeId,
	marked: &FxHashSet<NodeId>,
) -> Exec<'f> {
	let decisions = Rewriter { formula, marked }.decide_all(root);
	let mut emitter = Emitter::default();
	let mut built = FxHashMap::default();
	for (&src, decision) in &decisions {
		let id = emitter.emit_decision(formula, &built, decision);
		built.insert(src, id);
	}

	let root = built[&plain(root)];
	let mut nodes = emitter.nodes;
	nodes.truncate(root.index() + 1);
	let reachable = reachable(&nodes, root);

	Exec {
		nodes,
		root,
		reachable,
	}
}

/// Whether `from` reaches each of `nodes[..=from]`, where children come before their parents.
pub(crate) fn reachable(nodes: &TiVec<ExecId, ExecNode<'_>>, from: ExecId) -> TiVec<ExecId, bool> {
	let mut reachable = TiVec::from(vec![false; from.index() + 1]);
	reachable[from] = true;
	for node in reachable.keys().rev() {
		if reachable[node] {
			for child in nodes[node].op.children() {
				reachable[child] = true;
			}
		}
	}

	reachable
}
