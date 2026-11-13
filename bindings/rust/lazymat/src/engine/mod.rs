//! One query over a formula.
//!
//! # Steps
//!
//! 1. `exec` rewrites the formula into an execution DAG (its docs list the rewrites), builds equal nodes once
//!    and puts the cheaper operand first. Rewrites do not look into explicit nodes.
//! 2. `alphabet` groups the tapes into classes that read the same symbols and gives each class its alphabet.
//! 3. Explicit nodes, children first, are explored completely, numbered densely, trimmed and replaced by the
//!    leaf they give. The formula keeps that leaf for later queries, keyed by the node and the alphabets below
//!    it. A node whose exploration passes its `max_states` stays lazy.
//! 4. The search explores the root's macrostates depth first until one accepts, and returns the path to it.
//!
//! | Node | Macrostate | Accepting | Successors |
//! |---|---|---|---|
//! | leaf, explicit | a state | final | its moves |
//! | union (n-ary) | (branch, state) | that state | the branch's moves |
//! | intersection, sync product, diagonal | (state, state) | both | pairs of moves whose tuples merge, once each when the product drops tapes |
//! | complement | a minimized set of child states | no member | one move per tuple of the alphabets |
//! | identity, projection | the child's | the child's | the child's moves, relabelled |
//!
//! # Successors
//!
//! The successors of a macrostate (its post) are built from the children's posts when first needed (`post`)
//! and cached per node, except for identities and along the root. A product joins each left move with the
//! right moves that agree on the synchronized tapes. The post of a complement is a table of all tuples in a
//! fixed order, so the matching moves follow from the letters. A sync product that drops its only synchronized
//! pair, as in a composition or an image, joins groups of moves instead (`join`). A complement over at most 256
//! child states builds its posts from bitset rows (`dense`). Every path gives the same posts and the same
//! macrostate numbers.
//!
//! # Subsumption
//!
//! `s ⊑ t` implies that `s` accepts a subset of what `t` accepts (`subsumption`). Leaves use their maximal
//! direct simulation (the `simulation` module), products compare componentwise, and complement sets
//! `S ⊑ T` when every member of `T` is below a member of `S`. Complement sets drop the members below another
//! member. Each node keeps the results it computed (`memo`), except a complement over a leaf, which compares
//! sets through the closures of the transposed simulation (`closure`). The root keeps an antichain of the
//! macrostates explored, grouped by the shape of the root so that a new macrostate is compared only with those
//! it can be below.
//!
//! # Initial macrostates
//!
//! Intersected unions multiply their initial macrostates (`initial`). A node with at most 2¹⁶ lists them, and
//! one with more numbers them on demand in the same order. The search then takes them one at a time, which can
//! change the witness and the statistics but not the answer.
//!
//! # Memory and limits
//!
//! Most of the memory is cached posts. A post stores its tuples flat and each target as a `u32` with the
//! accepting flag in the top bit, and the posts of one complement share their tuple table. Under a cache budget
//! the query drops posts by GreedyDual-Size with frequency (`cache`) and computes them again when needed.
//! `budget` counts macrostates and steps against the query's limits.
//!
//! Intersections stay binary, so `¬(A₁ ∪ … ∪ Aₙ)` builds quadratically many pair macrostates.

mod alphabet;
mod budget;
mod cache;
mod closure;
mod dense;
mod exec;
mod initial;
mod join;
mod memo;
mod post;
mod store;
mod subsumption;

use std::collections::hash_map::Entry;
use std::rc::Rc;
use std::sync::Arc;

use indexmap::IndexSet;
use rustc_hash::{FxBuildHasher, FxHashMap, FxHashSet};
use typed_index_collections::TiVec;

use self::alphabet::Alphabets;
use self::budget::{Budget, Limit};
use self::cache::CacheBudget;
use self::dense::Dense;
use self::exec::{Exec, LeafRef, Op};
use self::initial::{INITIAL_BATCH, batches};
use self::join::{JoinPlan, Scratch};
use self::post::{JoinBuffers, Post, Table, Target};
use self::store::Store;
use self::subsumption::{Antichain, Subsumption};
use crate::formula::Formula;
use crate::ids::{ExecId, Id, NodeId, StateId};
use crate::leaf::{Leaf, Raw};
use crate::options::{ExplicitOptions, Materialized, Stats, Tapes};
use crate::{Error, Options, Symbol, Witness};

/// Successors, initial macrostates and subsumption recurse once per level of the DAG; the stack grows by
/// this much whenever less than [`RED_ZONE`] is left.
const STACK_GROWTH: usize = 1 << 20;
const RED_ZONE: usize = 64 << 10;

fn deep<T>(work: impl FnOnce() -> T) -> T {
	stacker::maybe_grow(RED_ZONE, STACK_GROWTH, work)
}

/// An explored node, or the number of macrostates at which it passed its budget.
enum Explored {
	Done(Raw),
	OverBudget(usize),
}

/// How the search first reached a macrostate, from `parent` by the tuple at `offset` in its tuple buffer.
#[derive(Clone, Copy, Debug)]
struct Step {
	parent: StateId,
	offset: usize,
}

#[derive(Debug)]
pub(crate) struct Engine<'f> {
	formula: &'f Formula,
	exec: Exec<'f>,
	alphabets: Alphabets,
	stores: TiVec<ExecId, Store>,
	posts: TiVec<ExecId, Vec<Option<Rc<Post>>>>,
	/// The root and the nodes sharing its macrostates expand each macrostate once, so they keep no posts. An
	/// identity relabels its child's post, which is kept, as a copy would double it.
	cached: TiVec<ExecId, bool>,
	/// The tuples of the posts of each complement node, and of each identity of one.
	tables: TiVec<ExecId, Option<Rc<Table>>>,
	cache_budget: Option<CacheBudget>,
	/// The time spent computing the children's posts of the post being computed, under a cache budget.
	nested_nanos: u64,
	/// The number of initial macrostates of each node, once counted.
	initial_counts: TiVec<ExecId, Option<usize>>,
	/// The initial macrostates of the nodes that list them.
	initials: TiVec<ExecId, Option<Rc<[Target]>>>,
	/// Where each branch's initial macrostates start, for the unions numbered on demand.
	union_starts: TiVec<ExecId, Option<Rc<[usize]>>>,
	/// Per node, whether one of its initial macrostates accepts, once asked.
	accepting_initials: TiVec<ExecId, Option<bool>>,
	/// What each product's join may skip.
	plans: TiVec<ExecId, JoinPlan>,
	join_scratch: Scratch,
	join_buffers: JoinBuffers,
	subsumption: Subsumption,
	/// Whether the search keeps an antichain of the root's macrostates.
	antichain: bool,
	/// Whether the posts of each complement node use bitsets over its child's states.
	dense: TiVec<ExecId, Option<Box<Dense>>>,
	budget: Budget,
	stats: Stats,
}

impl<'f> Engine<'f> {
	/// Prepares the query of `root` and builds its explicit nodes.
	pub(crate) fn new(
		formula: &'f Formula,
		root: NodeId,
		options: &Options,
	) -> Result<Engine<'f>, Error> {
		let budget = Budget::new(options);
		let marked = options
			.explicit
			.iter()
			.map(|&term| formula.node(term))
			.collect::<Result<FxHashSet<NodeId>, Error>>()?;
		let exec = exec::build(formula, root, &marked);
		let alphabets = Alphabets::new(&exec, &root_symbols(options, exec.nodes[exec.root].arity)?);

		for (id, node) in exec.nodes.iter_enumerated() {
			if matches!(node.op, Op::Complement(_))
				&& exec.reachable[id]
				&& alphabets.tuple_count(id).is_none()
			{
				return Err(Error::ComplementTooLarge { arity: node.arity });
			}
		}

		let num_nodes = exec.nodes.len();
		let stores = exec
			.nodes
			.iter()
			.map(|node| match node.op {
				Op::Union(_) => Store::Tagged(IndexSet::default()),
				Op::Intersection(..) | Op::Sync(..) | Op::Diagonal(..) => {
					Store::Pair(IndexSet::default())
				}
				Op::Complement(_) => Store::Set(IndexSet::default()),
				_ => Store::None,
			})
			.collect();
		let mut cached: TiVec<ExecId, bool> = exec
			.nodes
			.iter()
			.map(|node| !matches!(node.op, Op::Identity(_)))
			.collect();
		for node in exec.sharing(exec.root) {
			cached[node] = false;
		}

		let plans = join::plans(&exec);
		let dense = exec
			.nodes
			.iter()
			.map(|node| matches!(node.op, Op::Complement(_)).then(Box::default))
			.collect();

		let mut engine = Engine {
			formula,
			exec,
			alphabets,
			stores,
			posts: vec![Vec::new(); num_nodes].into(),
			cached,
			tables: vec![None; num_nodes].into(),
			// Rows of dense complements are keyed after the nodes.
			cache_budget: options
				.cache_budget
				.map(|limit| CacheBudget::new(limit, 2 * num_nodes)),
			nested_nanos: 0,
			initial_counts: vec![None; num_nodes].into(),
			initials: vec![None; num_nodes].into(),
			union_starts: vec![None; num_nodes].into(),
			accepting_initials: vec![None; num_nodes].into(),
			plans,
			join_scratch: Scratch::default(),
			join_buffers: JoinBuffers::default(),
			subsumption: Subsumption::new(options.subsumption, num_nodes, budget.deadline()),
			antichain: options.subsumption && options.antichain,
			dense,
			budget,
			stats: Stats::default(),
		};

		engine
			.materialize_explicit()
			.map_err(|limit| engine.limit_error(limit))?;

		Ok(engine)
	}

	/// Builds every explicit node, children first.
	fn materialize_explicit(&mut self) -> Result<(), Limit> {
		for node in self.exec.nodes.keys() {
			if let Op::Explicit {
				child,
				options,
				origin,
			} = self.exec.nodes[node].op
				&& self.exec.reachable[node]
			{
				self.materialize_node(node, child, options, origin)?;
			}
		}

		Ok(())
	}

	/// Replaces the explicit `node` by its automaton, or by its child once the exploration passes its budget.
	fn materialize_node(
		&mut self,
		node: ExecId,
		child: ExecId,
		options: ExplicitOptions,
		origin: NodeId,
	) -> Result<(), Limit> {
		let term = self.formula.term(origin);
		let key = (origin, self.signature(node));
		// Looked up before the match, whose scrutinee would hold the lock through the insert.
		let hit = self.formula.explicit_cache.lock().get(&key).cloned();

		let (leaf, cached) = match hit {
			Some(leaf) => (leaf, true),
			None => match self.explore(child, options.max_states)? {
				Explored::Done(raw) => {
					let leaf = Arc::new(Leaf::build(&raw, Vec::new()));
					self.formula
						.explicit_cache
						.lock()
						.insert(key, Arc::clone(&leaf));

					(leaf, false)
				}
				Explored::OverBudget(states) => {
					self.stats.explicit.push(Materialized {
						term,
						states,
						transitions: 0,
						cached: false,
						lazy: true,
					});
					self.exec.nodes[node].op = Op::Transparent(child);

					return Ok(());
				}
			},
		};

		self.stats.explicit.push(Materialized {
			term,
			states: leaf.num_states(),
			transitions: leaf.num_moves(),
			cached,
			lazy: false,
		});
		self.exec.nodes[node].op = Op::Leaf(LeafRef::Shared(leaf));

		Ok(())
	}

	/// The alphabets that the language of `node` depends on.
	fn signature(&self, node: ExecId) -> Vec<Box<[Symbol]>> {
		let classes: FxHashSet<usize> = exec::reachable(&self.exec.nodes, node)
			.iter_enumerated()
			.filter(|&(_, &reached)| reached)
			.flat_map(|(below, _)| {
				(0..self.exec.nodes[below].arity).map(move |tape| self.alphabets.class(below, tape))
			})
			.collect();

		let mut signature: Vec<Box<[Symbol]>> = classes
			.into_iter()
			.map(|class| self.alphabets.class_universe(class).into())
			.collect();
		signature.sort_unstable();

		signature
	}

	/// Visits every macrostate of `node` reachable from its initial ones.
	fn explore(&mut self, node: ExecId, max_states: Option<usize>) -> Result<Explored, Limit> {
		fn intern(
			ids: &mut IndexSet<StateId, FxBuildHasher>,
			raw: &mut Raw,
			target: Target,
		) -> StateId {
			let (index, new) = ids.insert_full(target.state);
			if new {
				raw.finals.push(target.accepting);
			}

			StateId::new(index)
		}

		let mut ids = IndexSet::default();
		let mut raw = Raw::new(self.exec.nodes[node].arity);
		// The initial macrostates in batches, so that the node's budget stops a node with very many.
		for batch in batches(0..self.initial_count(node), INITIAL_BATCH) {
			if max_states.is_some_and(|max| ids.len() > max) {
				return Ok(Explored::OverBudget(ids.len()));
			}

			for index in batch {
				let target = self.initial_at(node, index)?;
				let state = intern(&mut ids, &mut raw, target);
				raw.initial.push(state);
			}
		}

		let mut next = 0;
		while next < ids.len() {
			if max_states.is_some_and(|max| ids.len() > max) {
				return Ok(Explored::OverBudget(ids.len()));
			}

			self.budget.step(0);
			self.stopped()?;

			let source = StateId::new(next);
			let post = self.post(node, ids[next])?;
			for (tuple, target) in post.iter() {
				let state = intern(&mut ids, &mut raw, target);
				raw.push(source, tuple, state);
			}

			next += 1;
		}

		Ok(Explored::Done(raw))
	}

	/// Searches the root for an accepting macrostate, depth first, skipping subsumed macrostates.
	pub(crate) fn search(&mut self) -> Result<Option<Witness>, Error> {
		self.find_accepting()
			.map_err(|limit| self.limit_error(limit))
	}

	fn find_accepting(&mut self) -> Result<Option<Witness>, Limit> {
		let root = self.exec.root;
		let mut antichain = self.antichain.then(|| Antichain::new(&self.exec));
		let mut parents: FxHashMap<StateId, Option<Step>> = FxHashMap::default();
		let mut tuples: Vec<Symbol> = Vec::new();
		let mut work = Vec::new();

		// An accepting initial macrostate gives the witness ε; with more than are listed, it is asked for first.
		if !self.is_listed(root) && self.has_accepting_initial(root) {
			return Ok(Some(Witness::new(self.exec.nodes[root].arity, Vec::new())));
		}

		// The initial macrostates in batches, each searched depth first before the next is numbered.
		for batch in self.search_order(root) {
			for index in batch {
				let target = self.initial_at(root, index)?;
				if target.accepting {
					return Ok(Some(Witness::new(self.exec.nodes[root].arity, Vec::new())));
				}

				if let Entry::Vacant(entry) = parents.entry(target.state) {
					entry.insert(None);
					self.enqueue(antichain.as_mut(), &mut work, target.state);
					self.stopped()?;
				}
			}

			while let Some(state) = work.pop() {
				if antichain
					.as_ref()
					.is_some_and(|antichain| antichain.is_pruned(state))
				{
					continue;
				}

				self.budget.step(0);
				self.stopped()?;

				let post = self.post(root, state)?;
				self.stats.explored += 1;
				for (tuple, target) in post.iter() {
					if target.accepting {
						return Ok(Some(self.witness(&parents, &tuples, state, tuple)));
					}

					let Entry::Vacant(entry) = parents.entry(target.state) else {
						continue;
					};

					entry.insert(Some(Step {
						parent: state,
						offset: tuples.len(),
					}));
					tuples.extend_from_slice(tuple);
					self.enqueue(antichain.as_mut(), &mut work, target.state);
					self.stopped()?;
				}
			}
		}

		Ok(None)
	}

	/// Pushes `state` to `work` unless the antichain prunes it.
	fn enqueue(
		&mut self,
		antichain: Option<&mut Antichain<'f>>,
		work: &mut Vec<StateId>,
		state: StateId,
	) {
		let added = match antichain {
			Some(antichain) => {
				self.budget.step(0);

				antichain.insert(&self.exec, &self.stores, &mut self.subsumption, state)
			}
			None => true,
		};

		if added {
			work.push(state);
		} else {
			self.stats.pruned += 1;
		}
	}

	/// The word to `state` and then `last`, with each wildcard as the first symbol of its tape.
	fn witness(
		&self,
		parents: &FxHashMap<StateId, Option<Step>>,
		tuples: &[Symbol],
		mut state: StateId,
		last: &[Symbol],
	) -> Witness {
		let root = self.exec.root;
		let arity = self.exec.nodes[root].arity;
		let mut steps = vec![last];
		while let Some(&Some(Step { parent, offset })) = parents.get(&state) {
			steps.push(&tuples[offset..offset + arity]);
			state = parent;
		}

		let symbols = steps
			.into_iter()
			.rev()
			.flat_map(|step| step.iter().enumerate())
			.map(|(tape, &symbol)| {
				if symbol == Symbol::ANY {
					let universe = self.alphabets.universe(root, tape);

					universe.first().copied().unwrap_or(Symbol::ANY)
				} else {
					symbol
				}
			})
			.collect();

		Witness::new(arity, symbols)
	}

	/// Builds the root as an explicit automaton.
	#[cfg(feature = "mata")]
	pub(crate) fn materialize_root(&mut self) -> Result<Leaf, Error> {
		let explored = self
			.explore(self.exec.root, None)
			.map_err(|limit| self.limit_error(limit))?;
		let Explored::Done(raw) = explored else {
			unreachable!("exploration without a state budget finishes")
		};

		Ok(Leaf::build(&raw, Vec::new()))
	}

	/// The limit that the query passed, if it did. The posts and initial macrostates computed since may then
	/// be partial, and the query stops.
	fn stopped(&self) -> Result<(), Limit> {
		if self.subsumption.is_late() {
			return Err(Limit::Time);
		}

		self.budget.check()
	}

	/// The error of passing `limit`, with the statistics so far.
	fn limit_error(&self, limit: Limit) -> Error {
		let stats = Box::new(self.stats());

		match limit {
			Limit::States => Error::StateLimit {
				max_states: self.budget.max_states.expect("only a state limit passes"),
				stats,
			},
			Limit::Time => Error::TimeLimit {
				limit: self.budget.time_limit().expect("only a time limit passes"),
				stats,
			},
		}
	}

	pub(crate) fn stats(&self) -> Stats {
		Stats {
			macrostates: self.stores.iter().map(Store::len).sum(),
			dropped: self
				.cache_budget
				.as_ref()
				.map_or(0, |budget| budget.dropped),
			..self.stats.clone()
		}
	}
}

/// The query's extra symbols of each tape of the root, whose arity is `arity`.
fn root_symbols(options: &Options, arity: usize) -> Result<Vec<Vec<Symbol>>, Error> {
	let mut root_symbols = vec![Vec::new(); arity];
	for extra in &options.alphabets {
		if let Some(&symbol) = extra.symbols.iter().find(|symbol| !symbol.is_concrete()) {
			return Err(Error::ReservedSymbol { symbol });
		}

		let tapes = match extra.tapes {
			Tapes::One(tape) if tape >= arity => return Err(Error::TapeOutOfRange { tape, arity }),
			Tapes::One(tape) => tape..tape + 1,
			Tapes::Every => 0..arity,
		};

		for tape in tapes {
			root_symbols[tape].extend_from_slice(&extra.symbols);
		}
	}

	Ok(root_symbols)
}
