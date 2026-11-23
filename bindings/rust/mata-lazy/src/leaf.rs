//! The form in which the engine stores a leaf, and the import of an [`Automaton`] into it.

use std::sync::OnceLock;
use std::time::Instant;

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

	fn tuple(&self, edge: usize) -> &[Symbol] {
		&self.letters[edge * self.arity..(edge + 1) * self.arity]
	}
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
		// The states that silent moves reach from `first`; also the worklist.
		let mut closure: IndexSet<A::State, FxBuildHasher> = IndexSet::default();
		let mut next = 0;
		while let Some(&first) = states.get_index(next) {
			let source = StateId::new(next);
			next += 1;
			closure.clear();
			closure.insert(first);

			let mut is_final = false;
			let mut member = 0;
			while let Some(&state) = closure.get_index(member) {
				member += 1;
				is_final |= automaton.is_final(state);
				automaton.for_each_move(state, |tuple, target| {
					if tuple.len() != arity {
						wrong_width.get_or_insert(tuple.len());
					} else if arity > 0 && tuple.iter().all(|&symbol| symbol == Symbol::EPSILON) {
						closure.insert(target);
					} else {
						raw.push(source, tuple, StateId::new(states.insert_full(target).0));
					}
				});
			}

			if let Some(found) = wrong_width {
				return Err(Error::MoveWidth { arity, found });
			}

			raw.finals.push(is_final);
		}

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
