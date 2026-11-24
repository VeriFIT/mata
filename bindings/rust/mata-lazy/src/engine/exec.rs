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
//! - `op(X, X) = X`, `prefixes(prefixes(X)) = prefixes(X)`, and structurally equal nodes are built once;
//! - the cheaper operand of a union or intersection goes left.
//!
//! Explicit nodes stop the rewrites from looking into them. The query's annotations make nodes explicit, keep
//! explicit nodes of the formula lazy, and fix the operand order of an intersection (not one that De Morgan
//! turns into a union) or of a synchronized product.

use std::cmp::Ordering;
use std::collections::hash_map::Entry;
use std::collections::{BTreeMap, btree_map};
use std::ops::Deref;
use std::sync::Arc;

use rustc_hash::{FxHashMap, FxHashSet};
use typed_index_collections::TiVec;

use crate::formula::{Formula, Node, Pad, ResultTape, SyncSpec, TapePair};
use crate::ids::{ExecId, Id, LeafId, NodeId};
use crate::leaf::Leaf;
use crate::plan::{Annotations, Order};
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
	/// `{w | w·p^k ∈ child}`; its macrostates are a state of the child and the number of the last step read
	/// (see [`PadSteps`](super::pad::PadSteps)).
	PadClosure(ExecId, Arc<Pad>),
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
	/// The prefix closure, whose macrostates are its child's, accepting when they reach an accepting one.
	Prefixes(ExecId),
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
			| Op::PadClosure(a, _)
			| Op::Explicit { child: a, .. }
			| Op::Transparent(a)
			| Op::Prefixes(a) => ([Some(*a), None], &[]),
		};

		operands
			.into_iter()
			.flatten()
			.chain(branches.iter().copied())
	}

	/// The child whose macrostates this node shares, if any.
	pub(crate) fn transparent_child(&self) -> Option<ExecId> {
		match *self {
			Op::Identity(child)
			| Op::Project(child, _)
			| Op::Transparent(child)
			| Op::Prefixes(child) => Some(child),
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
	/// The operation, its operands, and whether their order is fixed.
	Boolean(Boolean, Src, Src, bool),
	IdentityOf(Boolean, Src, Src, bool),
	Identity(Src),
	Project(Src, Arc<[usize]>),
	PadClosure(Src, Arc<Pad>),
	Diagonal(Src, Src),
	/// `U ∩ X'` for the diagonal of `U` and a sync product, with the product's two tapes synchronized.
	DiagonalSync(Src, Src, Src, Arc<SyncSpec>),
	Sync(Src, Src, Arc<SyncSpec>),
	Explicit(Src, ExplicitOptions, NodeId),
	Prefixes(Src),
}

impl Decision {
	fn deps(&self) -> Vec<Src> {
		match self {
			Decision::Leaf(_) => Vec::new(),
			Decision::Alias(s)
			| Decision::Complement(s)
			| Decision::Identity(s)
			| Decision::Project(s, _)
			| Decision::PadClosure(s, _)
			| Decision::Explicit(s, ..)
			| Decision::Prefixes(s) => vec![*s],
			Decision::Boolean(_, a, b, _)
			| Decision::IdentityOf(_, a, b, _)
			| Decision::Diagonal(a, b)
			| Decision::Sync(a, b, _) => vec![*a, *b],
			Decision::DiagonalSync(u, a, b, _) => vec![*u, *a, *b],
		}
	}
}

struct Rewriter<'a> {
	formula: &'a Formula,
	annotations: &'a Annotations,
}

impl Rewriter<'_> {
	fn is_marked(&self, node: NodeId) -> bool {
		self.annotations.explicit.contains_key(&node)
			&& !matches!(self.formula.nodes[node], Node::Explicit(..))
	}

	fn order(&self, node: NodeId) -> Order {
		self.annotations
			.order
			.get(&node)
			.copied()
			.unwrap_or_default()
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
					let options = self.annotations.explicit[&node];

					return Decision::Explicit((node, Form::Inner), options, node);
				}
				Form::Negated => return Decision::Complement(plain(node)),
				Form::Inner => {}
			}
		}

		let negated = form == Form::Negated;

		match &self.formula.nodes[node] {
			Node::Explicit(child, _) if self.annotations.lazy.contains(&node) => {
				Decision::Alias((*child, form))
			}
			Node::Complement(child) if negated && self.formula.pads[*child] => {
				Decision::Complement(plain(node))
			}
			Node::Complement(child) => {
				Decision::Alias((*child, if negated { Form::Plain } else { Form::Negated }))
			}
			Node::Union(a, b) => self.decide_boolean(Boolean::Union, *a, *b, negated, Order::Auto),
			Node::Intersection(a, b) => {
				let order = self.order(node);

				self.decide_boolean(Boolean::Intersection, *a, *b, negated, order)
			}
			_ if negated => Decision::Complement(plain(node)),
			Node::Leaf(leaf) => Decision::Leaf(*leaf),
			Node::Identity(child) => Decision::Identity(plain(*child)),
			Node::Project(child, tapes) => self.decide_project(*child, tapes),
			Node::PadClosure(child, pad) => Decision::PadClosure(plain(*child), Arc::clone(pad)),
			Node::Sync(a, b, spec) if self.order(node) == Order::Swapped => {
				Decision::Sync(plain(*b), plain(*a), Arc::new(spec.swapped()))
			}
			Node::Sync(a, b, spec) => Decision::Sync(plain(*a), plain(*b), Arc::clone(spec)),
			Node::Explicit(child, options) => {
				let options = self.annotations.explicit.get(&node).unwrap_or(options);

				Decision::Explicit(plain(*child), *options, node)
			}
			Node::Prefixes(child) => Decision::Prefixes(plain(*child)),
		}
	}

	fn decide_boolean(
		&self,
		op: Boolean,
		a: NodeId,
		b: NodeId,
		negated: bool,
		order: Order,
	) -> Decision {
		if negated {
			return Decision::Boolean(op.dual(), (a, Form::Negated), (b, Form::Negated), false);
		}

		let (a, b) = if order == Order::Swapped {
			(b, a)
		} else {
			(a, b)
		};
		let fixed = order != Order::Auto;

		match (self.identity_child(a), self.identity_child(b)) {
			(Some(lhs), Some(rhs)) => Decision::IdentityOf(op, plain(lhs), plain(rhs), fixed),
			_ => Decision::Boolean(op, plain(a), plain(b), fixed),
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
	/// An intersection whose operands an annotation ordered.
	OrderedIntersection(ExecId, ExecId),
	Complement(ExecId),
	Identity(ExecId),
	Project(ExecId, Arc<[usize]>),
	PadClosure(ExecId, Arc<Pad>),
	Sync(ExecId, ExecId, Arc<SyncSpec>),
	Diagonal(ExecId, ExecId),
	Explicit(ExecId, NodeId),
	Prefixes(ExecId),
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
			Decision::Boolean(op, a, b, fixed) => self.boolean(*op, built[a], built[b], *fixed),
			Decision::IdentityOf(op, lhs, rhs, fixed) => {
				let inner = self.boolean(*op, built[lhs], built[rhs], *fixed);

				self.identity(inner)
			}
			Decision::Identity(src) => self.identity(built[src]),
			Decision::Project(src, tapes) => self.project(built[src], tapes),
			Decision::PadClosure(src, pad) => self.pad_closure(built[src], pad),
			Decision::Diagonal(u, x) => self.diagonal(built[u], built[x]),
			Decision::DiagonalSync(u, a, b, spec) => {
				let synced = self.sync(built[a], built[b], spec);

				self.intersection(built[u], synced, false)
			}
			Decision::Sync(a, b, spec) => self.sync(built[a], built[b], spec),
			Decision::Prefixes(src) => self.prefixes(built[src]),
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

	/// `fixed` keeps the order of the operands of an intersection.
	fn boolean(&mut self, op: Boolean, a: ExecId, b: ExecId, fixed: bool) -> ExecId {
		match op {
			Boolean::Union => self.union(a, b),
			Boolean::Intersection => self.intersection(a, b, fixed),
		}
	}

	/// `fixed` keeps the order of the operands, which otherwise puts the cheaper one left.
	fn intersection(&mut self, a: ExecId, b: ExecId, fixed: bool) -> ExecId {
		if a == b {
			return a;
		}

		let (a, b, key) = if fixed {
			(a, b, Key::OrderedIntersection(a, b))
		} else if self.costs[a] <= self.costs[b] {
			(a, b, Key::Intersection(a.min(b), a.max(b)))
		} else {
			(b, a, Key::Intersection(a.min(b), a.max(b)))
		};

		let cost = self.product_cost(a, b);
		let arity = self.nodes[a].arity;

		self.emit(key, Op::Intersection(a, b), arity, cost)
	}

	/// The branches that `node` adds to a union above it.
	fn push_branches(&self, node: ExecId, branches: &mut Vec<ExecId>) {
		match &self.nodes[node].op {
			Op::Union(inner) => branches.extend_from_slice(inner),
			_ => branches.push(node),
		}
	}

	fn union(&mut self, a: ExecId, b: ExecId) -> ExecId {
		let mut branches = Vec::new();
		self.push_branches(a, &mut branches);
		self.push_branches(b, &mut branches);

		self.union_of(branches)
	}

	/// One union node over `branches`, as a chain of binary unions would copy every macrostate into each union
	/// above it.
	fn union_of(&mut self, mut branches: Vec<ExecId>) -> ExecId {
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
		let arity = self.nodes[branches[0]].arity;

		self.emit(key, Op::Union(branches.into()), arity, cost)
	}

	fn identity(&mut self, child: ExecId) -> ExecId {
		let cost = self.unary_cost(child);

		self.emit(Key::Identity(child), Op::Identity(child), 2, cost)
	}

	fn prefixes(&mut self, child: ExecId) -> ExecId {
		if matches!(self.nodes[child].op, Op::Prefixes(_)) {
			return child;
		}

		let cost = self.unary_cost(child);
		let arity = self.nodes[child].arity;

		self.emit(Key::Prefixes(child), Op::Prefixes(child), arity, cost)
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

	fn pad_closure(&mut self, child: ExecId, pad: &Arc<Pad>) -> ExecId {
		let cost = self.unary_cost(child);
		let key = Key::PadClosure(child, Arc::clone(pad));
		let op = Op::PadClosure(child, Arc::clone(pad));
		let arity = self.nodes[child].arity;

		self.emit(key, op, arity, cost)
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
		// The outer node adds nothing, unless the inner one may stay lazy at fewer states than it allows.
		if let Op::Explicit { options: inner, .. } = self.nodes[child].op
			&& inner
				.max_states
				.is_none_or(|inner| options.max_states.is_some_and(|outer| outer <= inner))
		{
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

/// Builds the execution DAG of `root`, with the query's `annotations`.
pub(crate) fn build<'f>(formula: &'f Formula, root: NodeId, annotations: &Annotations) -> Exec<'f> {
	let decisions = Rewriter {
		formula,
		annotations,
	}
	.decide_all(root);
	let aliases = aliases(&decisions);
	let resolve = |src: Src| aliases.get(&src).copied().unwrap_or(src);
	let inner_unions = inner_unions(&decisions, &aliases, resolve(plain(root)));
	let mut emitter = Emitter::default();
	let mut built = FxHashMap::default();
	// The branches of the unions that only one union above uses, which get no node of their own.
	let mut pending: FxHashMap<Src, Vec<ExecId>> = FxHashMap::default();
	for (&src, decision) in &decisions {
		let Decision::Boolean(Boolean::Union, a, b, _) = decision else {
			// The union above an alias of a pending union takes its branches.
			if !(matches!(decision, Decision::Alias(_)) && inner_unions.contains(&resolve(src))) {
				let id = emitter.emit_decision(formula, &built, decision);
				built.insert(src, id);
			}

			continue;
		};

		let [first, second] = [a, b].map(|operand| {
			pending.remove(&resolve(*operand)).unwrap_or_else(|| {
				let mut branches = Vec::new();
				emitter.push_branches(built[operand], &mut branches);

				branches
			})
		});

		// The larger list takes the smaller, so that a chain of unions costs no more than sorting its branches.
		let (mut branches, smaller) = if first.len() >= second.len() {
			(first, second)
		} else {
			(second, first)
		};

		branches.extend(smaller);
		if inner_unions.contains(&src) {
			pending.insert(src, branches);
		} else {
			built.insert(src, emitter.union_of(branches));
		}
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

/// The node that each alias stands for, past chains of aliases.
fn aliases(decisions: &BTreeMap<Src, Decision>) -> FxHashMap<Src, Src> {
	let mut aliases = FxHashMap::default();
	for (&src, decision) in decisions {
		if let Decision::Alias(target) = decision {
			let target = aliases.get(target).copied().unwrap_or(*target);
			aliases.insert(src, target);
		}
	}

	aliases
}

/// The unions, other than `root`, whose only user is one union, counting the users of their aliases.
fn inner_unions(
	decisions: &BTreeMap<Src, Decision>,
	aliases: &FxHashMap<Src, Src>,
	root: Src,
) -> FxHashSet<Src> {
	let mut users: FxHashMap<Src, (usize, bool)> = FxHashMap::default();
	for decision in decisions.values() {
		if matches!(decision, Decision::Alias(_)) {
			continue;
		}

		let by_union = matches!(decision, Decision::Boolean(Boolean::Union, ..));
		for dep in decision.deps() {
			let dep = aliases.get(&dep).copied().unwrap_or(dep);
			let (count, all_unions) = users.entry(dep).or_insert((0, true));
			*count += 1;
			*all_unions &= by_union;
		}
	}

	decisions
		.iter()
		.filter(|&(&src, decision)| {
			src != root
				&& matches!(decision, Decision::Boolean(Boolean::Union, ..))
				&& users.get(&src) == Some(&(1, true))
		})
		.map(|(&src, _)| src)
		.collect()
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

#[cfg(test)]
mod tests {
	use super::{Op, build};
	use crate::plan::Annotations;
	use crate::{Automaton, Formula, Symbol};

	/// The empty word.
	struct Epsilon;

	impl Automaton for Epsilon {
		type State = ();

		fn arity(&self) -> usize {
			1
		}

		fn initial_states(&self) -> impl IntoIterator<Item = ()> {
			[()]
		}

		fn is_final(&self, (): ()) -> bool {
			true
		}

		fn for_each_move(&self, (): (), _: impl FnMut(&[Symbol], ())) {}
	}

	#[test]
	fn a_chain_of_unions_through_dropped_projections_holds_each_branch_once() {
		const LEAVES: usize = 100;
		let mut formula = Formula::new();
		let mut root = formula.leaf(&Epsilon).expect("a unary leaf");
		for _ in 1..LEAVES {
			let leaf = formula.leaf(&Epsilon).expect("a unary leaf");
			let union = formula.union(root, leaf).expect("unary operands");
			root = formula
				.project(union, &[0])
				.expect("tape 0 of a unary term");
		}

		let root = formula.node(root).expect("a term of the formula");
		let exec = build(&formula, root, &Annotations::default());
		let branches: usize = exec
			.nodes
			.iter()
			.map(|node| match &node.op {
				Op::Union(branches) => branches.len(),
				_ => 0,
			})
			.sum();

		assert_eq!(branches, LEAVES);
	}
}
