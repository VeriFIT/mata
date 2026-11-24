//! The form in which the engine stores a leaf, and the import of an [`Automaton`] into it.

use std::hash::BuildHasher;
use std::sync::OnceLock;
use std::time::Instant;

use hashbrown::HashTable;
use indexmap::IndexSet;
use parking_lot::Mutex;
use rustc_hash::FxBuildHasher;

use crate::ids::{Id, StateId};
use crate::simulation::{Simulation, TimedOut};
use crate::{Automaton, Error, Symbol};

/// Transitions collected before trimming and sorting.
#[derive(Debug)]
pub(crate) struct Raw {
	arity: usize,
	pub(crate) initial: Vec<StateId>,
	pub(crate) finals: Vec<bool>,
	edges: Vec<(StateId, StateId)>,
	letters: Vec<Symbol>,
}

impl Raw {
	pub(crate) fn new(arity: usize) -> Raw {
		Raw {
			arity,
			initial: Vec::new(),
			finals: Vec::new(),
			edges: Vec::new(),
			letters: Vec::new(),
		}
	}

	pub(crate) fn push(&mut self, source: StateId, tuple: &[Symbol], target: StateId) {
		debug_assert_eq!(tuple.len(), self.arity);
		self.edges.push((source, target));
		self.letters.extend_from_slice(tuple);
	}

	/// Pushes a move unless `pushed`, the edges of `source` so far, has it.
	fn push_new(
		&mut self,
		pushed: &mut HashTable<usize>,
		source: StateId,
		tuple: &[Symbol],
		target: StateId,
	) {
		let hash = FxBuildHasher.hash_one((tuple, target));
		let same = |&edge: &usize| self.edges[edge].1 == target && self.tuple(edge) == tuple;
		if pushed.find(hash, same).is_some() {
			return;
		}

		let rehash = |&edge: &usize| FxBuildHasher.hash_one((self.tuple(edge), self.edges[edge].1));
		pushed.insert_unique(hash, self.edges.len(), rehash);
		self.push(source, tuple, target);
	}

	fn tuple(&self, edge: usize) -> &[Symbol] {
		&self.letters[edge * self.arity..(edge + 1) * self.arity]
	}

	/// Moves the moves of `moves` to the end of `self`, and returns where they are.
	fn append(&mut self, moves: &mut Raw) -> (usize, usize) {
		let start = self.edges.len();
		self.edges.append(&mut moves.edges);
		self.letters.append(&mut moves.letters);
		(start, self.edges.len())
	}
}

/// How far the import of a source (a state that a non-silent move or the start reaches) is.
#[derive(Clone, Copy)]
enum Import {
	Pending,
	Walking,
	/// Its finality and its moves, `raw.edges[start..end]`, with the silent moves removed.
	Done {
		is_final: bool,
		start: usize,
		end: usize,
	},
}

/// The walk of the silent closure of a source.
struct Walk<S> {
	source: usize,
	/// The states that silent moves reach from the source; also the worklist, expanded up to `next`.
	closure: IndexSet<S, FxBuildHasher>,
	next: usize,
	is_final: bool,
	/// The moves found so far; `pushed` holds them so that a move shared along the closure is pushed once.
	moves: Raw,
	pushed: HashTable<usize>,
}

impl<S: Copy + Eq + std::hash::Hash> Walk<S> {
	/// What to do with the next member of the closure, given the sources and how far their imports are.
	fn step(&self, states: &IndexSet<S, FxBuildHasher>, imports: &[Import]) -> Step {
		let Some(state) = self.closure.get_index(self.next) else {
			return Step::Finish;
		};
		if self.next == 0 {
			return Step::Expand;
		}

		let Some(index) = states.get_index_of(state) else {
			return Step::Expand;
		};
		match imports.get(index).copied().unwrap_or(Import::Pending) {
			Import::Pending => Step::Descend(index),
			// A silent cycle back to a source on the stack, whose closure is not complete yet.
			Import::Walking => Step::Expand,
			Import::Done {
				is_final,
				start,
				end,
			} => Step::Inherit {
				is_final,
				start,
				end,
			},
		}
	}
}

/// What the walk on top of the stack does with the next member of its closure.
enum Step {
	Expand,
	/// Takes over the moves of a source that is done.
	Inherit {
		is_final: bool,
		start: usize,
		end: usize,
	},
	/// Walks this pending source first.
	Descend(usize),
	Finish,
}

/// A trimmed automaton over tuples of symbols with dense states and sorted moves.
#[derive(Debug)]
pub(crate) struct Leaf {
	arity: usize,
	initial: Vec<StateId>,
	finals: Vec<bool>,
	/// The moves of state `s` are `offsets[s]..offsets[s + 1]`.
	offsets: Vec<usize>,
	letters: Vec<Symbol>,
	targets: Vec<StateId>,
	/// The concrete symbols of every tape, the ones read before trimming and the declared ones.
	pub(crate) tape_symbols: Vec<Vec<Symbol>>,
	/// Whether a move reads the wildcard on each tape.
	pub(crate) tape_has_any: Vec<bool>,
	/// Whether a move reads padding on some tape.
	pub(crate) has_padding: bool,
	simulation: OnceLock<Simulation>,
	/// Held while a query computes the simulation, so that others wait for it instead of computing it again.
	computing: Mutex<()>,
}

impl Leaf {
	/// Copies the reachable part of `automaton`, without silent moves; `declared[tape]` joins `tape`'s symbols.
	///
	/// A closure that silently reaches another source takes over that source's moves instead of walking past it, and
	/// walks a pending source first, on a stack, so that a chain of `n` sources joined by silent moves calls
	/// `for_each_move` `n` times rather than `n^2 / 2`.
	pub(crate) fn import<A: Automaton>(
		automaton: &A,
		declared: Vec<Vec<Symbol>>,
	) -> Result<Leaf, Error> {
		let arity = automaton.arity();
		let mut states: IndexSet<A::State, FxBuildHasher> = IndexSet::default();
		let mut raw = Raw::new(arity);
		for state in automaton.initial_states() {
			raw.initial.push(StateId::new(states.insert_full(state).0));
		}

		let mut wrong_width = None;
		let mut imports: Vec<Import> = Vec::new();
		let mut walks: Vec<Walk<A::State>> = Vec::new();
		// Finished walks, kept for their allocations.
		let mut spare: Vec<Walk<A::State>> = Vec::new();
		let mut first = 0;
		while first < states.len() {
			imports.resize(states.len(), Import::Pending);
			if matches!(imports[first], Import::Pending) {
				imports[first] = Import::Walking;
				walks.push(start_walk(&mut spare, arity, first, states[first]));
			}

			first += 1;
			while let Some(walk) = walks.last_mut() {
				let step = walk.step(&states, &imports);

				match step {
					Step::Expand => {
						let state = walk.closure[walk.next];
						walk.next += 1;
						walk.is_final |= automaton.is_final(state);
						let source = StateId::new(walk.source);
						automaton.for_each_move(state, |tuple, target| {
							if tuple.len() != arity {
								wrong_width.get_or_insert(tuple.len());
							} else if arity > 0
								&& tuple.iter().all(|&symbol| symbol == Symbol::EPSILON)
							{
								walk.closure.insert(target);
							} else {
								let target = StateId::new(states.insert_full(target).0);
								walk.moves.push_new(&mut walk.pushed, source, tuple, target);
							}
						});

						if let Some(found) = wrong_width {
							return Err(Error::MoveWidth { arity, found });
						}
					}
					Step::Inherit {
						is_final,
						start,
						end,
					} => {
						walk.next += 1;
						walk.is_final |= is_final;
						let source = StateId::new(walk.source);
						for edge in start..end {
							walk.moves.push_new(
								&mut walk.pushed,
								source,
								raw.tuple(edge),
								raw.edges[edge].1,
							);
						}
					}
					Step::Descend(index) => {
						imports.resize(states.len(), Import::Pending);
						imports[index] = Import::Walking;
						walks.push(start_walk(&mut spare, arity, index, states[index]));
					}
					Step::Finish => {
						let mut walk = walks.pop().expect("the loop has a walk");
						let (start, end) = raw.append(&mut walk.moves);
						imports[walk.source] = Import::Done {
							is_final: walk.is_final,
							start,
							end,
						};
						spare.push(walk);
					}
				}
			}
		}

		raw.finals = imports
			.iter()
			.map(|import| match import {
				Import::Done { is_final, .. } => *is_final,
				Import::Pending | Import::Walking => unreachable!("every source is imported"),
			})
			.collect();

		Ok(Leaf::build(&raw, declared))
	}

	/// Trims `raw` to the states that reach a final state, and sorts and deduplicates its moves.
	pub(crate) fn build(raw: &Raw, declared: Vec<Vec<Symbol>>) -> Leaf {
		let arity = raw.arity;
		let mut tape_symbols = declared;
		tape_symbols.resize(arity, Vec::new());
		for (tape, symbols) in tape_symbols.iter_mut().enumerate() {
			symbols.extend(column(&raw.letters, arity, tape).filter(|letter| letter.is_concrete()));
			symbols.sort_unstable();
			symbols.dedup();
		}

		let renumbered = useful_states(raw);
		let num_states = renumbered.iter().flatten().count();
		let mut initial: Vec<StateId> = raw
			.initial
			.iter()
			.filter_map(|&state| renumbered[state.index()])
			.collect();
		initial.sort_unstable();
		initial.dedup();

		let mut finals = vec![false; num_states];
		for (&new, &is_final) in renumbered.iter().zip(&raw.finals) {
			if let Some(new) = new {
				finals[new.index()] = is_final;
			}
		}

		let mut edges: Vec<(StateId, usize, StateId)> = raw
			.edges
			.iter()
			.enumerate()
			.filter_map(|(edge, &(source, target))| {
				Some((
					renumbered[source.index()]?,
					edge,
					renumbered[target.index()]?,
				))
			})
			.collect();
		let key =
			|&(source, edge, target): &(StateId, usize, StateId)| (source, raw.tuple(edge), target);
		edges.sort_unstable_by_key(key);
		edges.dedup_by_key(|edge| key(edge));

		let mut offsets = vec![0; num_states + 1];
		for &(source, _, _) in &edges {
			offsets[source.index() + 1] += 1;
		}

		for state in 0..num_states {
			offsets[state + 1] += offsets[state];
		}

		let mut letters = Vec::with_capacity(edges.len() * arity);
		let mut targets = Vec::with_capacity(edges.len());
		for &(_, edge, target) in &edges {
			letters.extend_from_slice(raw.tuple(edge));
			targets.push(target);
		}

		let tape_has_any = (0..arity)
			.map(|tape| column(&letters, arity, tape).any(|letter| letter == Symbol::ANY))
			.collect();
		let has_padding = letters.contains(&Symbol::EPSILON);

		Leaf {
			arity,
			initial,
			finals,
			offsets,
			letters,
			targets,
			tape_symbols,
			tape_has_any,
			has_padding,
			simulation: OnceLock::new(),
			computing: Mutex::new(()),
		}
	}

	pub(crate) fn arity(&self) -> usize {
		self.arity
	}

	pub(crate) fn num_states(&self) -> usize {
		self.finals.len()
	}

	pub(crate) fn num_moves(&self) -> usize {
		self.targets.len()
	}

	pub(crate) fn initial(&self) -> &[StateId] {
		&self.initial
	}

	pub(crate) fn is_final(&self, state: StateId) -> bool {
		self.finals[state.index()]
	}

	/// Whether it has at most one initial state and no state has two moves on one tuple or a wildcard move, which
	/// overlaps the others.
	pub(crate) fn is_deterministic(&self) -> bool {
		let tuple = |edge: usize| &self.letters[edge * self.arity..(edge + 1) * self.arity];

		self.initial.len() <= 1
			&& !self.letters.contains(&Symbol::ANY)
			// The moves of a state are sorted by tuple, so equal tuples are adjacent.
			&& (0..self.num_states()).all(|state| {
				(self.offsets[state] + 1..self.offsets[state + 1])
					.all(|edge| tuple(edge - 1) != tuple(edge))
			})
	}

	/// The moves from `state`, sorted by tuple, then target.
	pub(crate) fn moves(
		&self,
		state: StateId,
	) -> impl ExactSizeIterator<Item = (&[Symbol], StateId)> {
		let state = state.index();
		let range = self.offsets[state]..self.offsets[state + 1];

		range.map(|edge| {
			let tuple = &self.letters[edge * self.arity..(edge + 1) * self.arity];

			(tuple, self.targets[edge])
		})
	}

	/// The simulation, computed on first use; [`TimedOut`] if computing or waiting passes `deadline` (nothing kept).
	pub(crate) fn simulation(&self, deadline: Option<Instant>) -> Result<&Simulation, TimedOut> {
		match self.simulation.get() {
			Some(simulation) => Ok(simulation),
			None => self.compute_simulation(deadline),
		}
	}

	/// Computes the simulation or waits for the query computing it; apart, so that the lookup above inlines.
	#[cold]
	fn compute_simulation(&self, deadline: Option<Instant>) -> Result<&Simulation, TimedOut> {
		let computing = match deadline {
			Some(deadline) => self.computing.try_lock_until(deadline).ok_or(TimedOut)?,
			None => self.computing.lock(),
		};

		// The query that held the lock may have computed it.
		if let Some(simulation) = self.simulation.get() {
			return Ok(simulation);
		}

		let computed = Simulation::compute(self, deadline)?;
		let simulation = self.simulation.get_or_init(|| computed);
		drop(computing);

		Ok(simulation)
	}

	/// The simulation, if a query computed it.
	pub(crate) fn computed_simulation(&self) -> Option<&Simulation> {
		self.simulation.get()
	}
}

/// A walk from `source`, whose state is `state`, reusing the allocations of a finished walk.
fn start_walk<S: Copy + Eq + std::hash::Hash>(
	spare: &mut Vec<Walk<S>>,
	arity: usize,
	source: usize,
	state: S,
) -> Walk<S> {
	let mut walk = spare.pop().unwrap_or_else(|| Walk {
		source,
		closure: IndexSet::default(),
		next: 0,
		is_final: false,
		moves: Raw::new(arity),
		pushed: HashTable::new(),
	});
	walk.source = source;
	walk.closure.clear();
	walk.closure.insert(state);
	walk.next = 0;
	walk.is_final = false;
	walk.pushed.clear();
	walk
}

/// The letters that the moves of `letters` read on `tape`.
fn column(letters: &[Symbol], arity: usize, tape: usize) -> impl Iterator<Item = Symbol> {
	letters.chunks_exact(arity).map(move |tuple| tuple[tape])
}

/// The new number of every state that can reach a final state, in the old order.
fn useful_states(raw: &Raw) -> Vec<Option<StateId>> {
	let num_states = raw.finals.len();
	let mut predecessors = vec![Vec::new(); num_states];
	for &(source, target) in &raw.edges {
		predecessors[target.index()].push(source);
	}

	let mut useful = raw.finals.clone();
	let mut stack: Vec<StateId> = (0..num_states)
		.filter(|&state| useful[state])
		.map(StateId::new)
		.collect();
	while let Some(state) = stack.pop() {
		for &source in &predecessors[state.index()] {
			if !useful[source.index()] {
				useful[source.index()] = true;
				stack.push(source);
			}
		}
	}

	let mut renumbered = vec![None; num_states];
	for (new, old) in (0..num_states).filter(|&state| useful[state]).enumerate() {
		renumbered[old] = Some(StateId::new(new));
	}

	renumbered
}
