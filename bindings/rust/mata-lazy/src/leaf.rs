//! The form in which the engine stores a leaf, and the import of an [`Automaton`] into it.

use std::hash::BuildHasher;
use std::sync::OnceLock;
use std::time::Instant;

use hashbrown::HashTable;
use indexmap::IndexSet;
use parking_lot::Mutex;
use rustc_hash::{FxBuildHasher, FxHashMap};

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

	/// The quotient by the coarsest forward bisimulation, which keeps the language on every tape. It refines a
	/// partition by finality: when a block splits, only the predecessors of its smaller part are compared again,
	/// so every state is revisited O(log n) times. [`Stopped`] when `stop` says so (it is asked once per split
	/// block processed).
	pub(crate) fn quotient(&self, stop: &mut dyn FnMut() -> bool) -> Result<Raw, Stopped> {
		let num_states = self.finals.len();
		let mut outgoing: Vec<Vec<usize>> = vec![Vec::new(); num_states];
		let mut predecessors: Vec<Vec<usize>> = vec![Vec::new(); num_states];
		for (edge, &(source, target)) in self.edges.iter().enumerate() {
			outgoing[source.index()].push(edge);
			predecessors[target.index()].push(source.index());
		}

		let mut blocks = Partition::new(num_states);
		for state in (0..num_states).filter(|&state| self.finals[state]) {
			blocks.mark(state);
		}
		blocks.split();
		// Every block once, so that every state with a move is compared once.
		let mut work: Vec<usize> = (0..blocks.sets).collect();
		blocks.created.clear();

		// The round in which a state was last touched.
		let mut stamp = vec![usize::MAX; num_states];
		let mut round = 0;
		while let Some(changed) = work.pop() {
			if stop() {
				return Err(Stopped);
			}
			round += 1;

			// The states with a move into `changed`, by their block.
			let mut touched: FxHashMap<usize, Vec<usize>> = FxHashMap::default();
			for position in blocks.first[changed]..blocks.past[changed] {
				for &source in &predecessors[blocks.elements[position]] {
					if stamp[source] != round {
						stamp[source] = round;
						touched.entry(blocks.set[source]).or_default().push(source);
					}
				}
			}

			let signature = |state: usize, blocks: &Partition| {
				let mut signature: Vec<(&[Symbol], usize)> = outgoing[state]
					.iter()
					.map(|&edge| (self.tuple(edge), blocks.set[self.edges[edge].1.index()]))
					.collect();
				signature.sort_unstable();
				signature.dedup();
				signature
			};
			for members in touched.into_values() {
				let reference = blocks
					.other_member(&members)
					.map(|other| signature(other, &blocks));
				let mut groups: FxHashMap<Vec<(&[Symbol], usize)>, Vec<usize>> =
					FxHashMap::default();
				for &state in &members {
					groups
						.entry(signature(state, &blocks))
						.or_default()
						.push(state);
				}
				// The states with the reference signature, or else the largest group, keep the block.
				let kept = reference.or_else(|| {
					groups
						.iter()
						.max_by_key(|(_, group)| group.len())
						.map(|(signature, _)| signature.clone())
				});
				for (signature, group) in groups {
					if Some(&signature) == kept.as_ref() {
						continue;
					}
					for state in group {
						blocks.mark(state);
					}
					blocks.split();
				}
			}
			work.append(&mut blocks.created);
		}

		let mut quotient = Raw::new(self.arity);
		quotient.finals = vec![false; blocks.sets];
		for state in (0..num_states).filter(|&state| self.finals[state]) {
			quotient.finals[blocks.set[state]] = true;
		}
		quotient.initial = self
			.initial
			.iter()
			.map(|state| StateId::new(blocks.set[state.index()]))
			.collect();
		quotient.initial.sort_unstable();
		quotient.initial.dedup();
		let mut moves: Vec<(usize, &[Symbol], usize)> = (0..self.edges.len())
			.map(|edge| {
				let (source, target) = self.edges[edge];
				(
					blocks.set[source.index()],
					self.tuple(edge),
					blocks.set[target.index()],
				)
			})
			.collect();
		moves.sort_unstable();
		moves.dedup();
		for (source, tuple, target) in moves {
			quotient.push(StateId::new(source), tuple, StateId::new(target));
		}

		Ok(quotient)
	}

	/// The subset construction over the tuples of `self`: `Ok(None)` once it has more than `max_states` states,
	/// [`Stopped`] when `stop` says so (it is asked once per subset).
	pub(crate) fn determinize(
		&self,
		max_states: usize,
		stop: &mut dyn FnMut() -> bool,
	) -> Result<Option<Raw>, Stopped> {
		let mut outgoing: Vec<Vec<usize>> = vec![Vec::new(); self.finals.len()];
		for (edge, &(source, _)) in self.edges.iter().enumerate() {
			outgoing[source.index()].push(edge);
		}

		let mut start = self.initial.clone();
		start.sort_unstable();
		start.dedup();
		let mut subsets: IndexSet<Vec<StateId>, FxBuildHasher> = IndexSet::default();
		subsets.insert(start);
		let mut dfa = Raw::new(self.arity);
		dfa.initial.push(StateId::new(0));
		let mut next = 0;
		while next < subsets.len() {
			if subsets.len() > max_states {
				return Ok(None);
			}
			if stop() {
				return Err(Stopped);
			}

			let mut moves: Vec<(&[Symbol], StateId)> = subsets[next]
				.iter()
				.flat_map(|state| &outgoing[state.index()])
				.map(|&edge| (self.tuple(edge), self.edges[edge].1))
				.collect();
			moves.sort_unstable();
			moves.dedup();
			dfa.finals
				.push(subsets[next].iter().any(|state| self.finals[state.index()]));
			for group in moves.chunk_by(|a, b| a.0 == b.0) {
				let target: Vec<StateId> = group.iter().map(|&(_, target)| target).collect();
				let (index, _) = subsets.insert_full(target);
				dfa.push(StateId::new(next), group[0].0, StateId::new(index));
			}
			next += 1;
		}

		Ok(Some(dfa))
	}

	/// The minimal automaton of `self`, which must be trimmed and deterministic over its tuples: the partition
	/// refinement of Valmari and Lehtinen for partial DFAs (STACS 2008), in O(m log n) for m moves and n states.
	/// [`Stopped`] when `stop` says so (it is asked once per class of moves).
	pub(crate) fn minimize(&self, stop: &mut dyn FnMut() -> bool) -> Result<Raw, Stopped> {
		let num_states = self.finals.len();
		let num_edges = self.edges.len();
		let mut minimal = Raw::new(self.arity);
		if num_states == 0 {
			return Ok(minimal);
		}
		debug_assert_eq!(self.initial.len(), 1, "a deterministic automaton");

		// Letters are the distinct tuples, numbered in their order.
		let mut tuples: Vec<&[Symbol]> = (0..num_edges).map(|edge| self.tuple(edge)).collect();
		tuples.sort_unstable();
		tuples.dedup();
		let label = |edge: usize| {
			tuples
				.binary_search(&self.tuple(edge))
				.expect("the tuple of a move")
		};
		let tail = |edge: usize| self.edges[edge].0.index();
		let head = |edge: usize| self.edges[edge].1.index();

		// The moves into each state.
		let mut incoming_start = vec![0; num_states + 1];
		for edge in 0..num_edges {
			incoming_start[head(edge) + 1] += 1;
		}
		for state in 0..num_states {
			incoming_start[state + 1] += incoming_start[state];
		}
		let mut incoming = vec![0; num_edges];
		let mut fill = incoming_start.clone();
		for edge in 0..num_edges {
			incoming[fill[head(edge)]] = edge;
			fill[head(edge)] += 1;
		}

		// Blocks of states, first split by finality.
		let mut blocks = Partition::new(num_states);
		for state in (0..num_states).filter(|&state| self.finals[state]) {
			blocks.mark(state);
		}
		blocks.split();

		// Cords: classes of moves, first by label.
		let mut by_label: Vec<(usize, usize)> =
			(0..num_edges).map(|edge| (label(edge), edge)).collect();
		by_label.sort_unstable();
		let mut cords = Partition::new(num_edges);
		cords.sets = 0;
		for (position, &(letter, edge)) in by_label.iter().enumerate() {
			if position == 0 || by_label[position - 1].0 != letter {
				cords.first[cords.sets] = position;
				cords.sets += 1;
			}
			cords.past[cords.sets - 1] = position + 1;
			cords.elements[position] = edge;
			cords.location[edge] = position;
			cords.set[edge] = cords.sets - 1;
		}

		let mut block = 1;
		let mut cord = 0;
		while cord < cords.sets {
			if stop() {
				return Err(Stopped);
			}
			for position in cords.first[cord]..cords.past[cord] {
				blocks.mark(tail(cords.elements[position]));
			}
			blocks.split();
			cord += 1;

			while block < blocks.sets {
				for position in blocks.first[block]..blocks.past[block] {
					let state = blocks.elements[position];
					for &edge in &incoming[incoming_start[state]..incoming_start[state + 1]] {
						cords.mark(edge);
					}
				}
				cords.split();
				block += 1;
			}
		}

		minimal.finals = vec![false; blocks.sets];
		for state in (0..num_states).filter(|&state| self.finals[state]) {
			minimal.finals[blocks.set[state]] = true;
		}
		minimal
			.initial
			.push(StateId::new(blocks.set[self.initial[0].index()]));
		let mut moves: Vec<(usize, usize, usize)> = (0..num_edges)
			.map(|edge| (blocks.set[tail(edge)], label(edge), blocks.set[head(edge)]))
			.collect();
		moves.sort_unstable();
		moves.dedup();
		for (source, letter, target) in moves {
			minimal.push(StateId::new(source), tuples[letter], StateId::new(target));
		}

		Ok(minimal)
	}

	/// The useful part of `self`: states reachable from an initial one that reach a final one.
	pub(crate) fn trimmed(&self) -> Raw {
		let renumbered = useful_states(self);
		let mut trimmed = Raw::new(self.arity);
		trimmed.finals = vec![false; renumbered.iter().flatten().count()];
		for (&new, &is_final) in renumbered.iter().zip(&self.finals) {
			if let Some(new) = new {
				trimmed.finals[new.index()] = is_final;
			}
		}
		trimmed.initial = self
			.initial
			.iter()
			.filter_map(|&state| renumbered[state.index()])
			.collect();
		for edge in 0..self.edges.len() {
			let (source, target) = self.edges[edge];
			if let (Some(source), Some(target)) =
				(renumbered[source.index()], renumbered[target.index()])
			{
				trimmed.push(source, self.tuple(edge), target);
			}
		}

		trimmed
	}
}

/// A reduction gave up, as its query passed a limit.
#[derive(Debug)]
pub(crate) struct Stopped;

/// A partition of `0..n` whose sets are contiguous ranges of `elements`, refined by marking elements and splitting
/// every touched set into its marked and unmarked part; the smaller part gets the new number.
struct Partition {
	sets: usize,
	elements: Vec<usize>,
	location: Vec<usize>,
	set: Vec<usize>,
	first: Vec<usize>,
	past: Vec<usize>,
	marked: Vec<usize>,
	touched: Vec<usize>,
	/// The sets that splits created, in order.
	created: Vec<usize>,
}

impl Partition {
	/// One set of all `n` elements, or none for `n == 0`.
	fn new(n: usize) -> Partition {
		let mut partition = Partition {
			sets: usize::from(n > 0),
			elements: (0..n).collect(),
			location: (0..n).collect(),
			set: vec![0; n],
			first: vec![0; n + 1],
			past: vec![0; n + 1],
			marked: vec![0; n + 1],
			touched: Vec::new(),
			created: Vec::new(),
		};
		partition.past[0] = n;
		partition
	}

	fn mark(&mut self, element: usize) {
		let set = self.set[element];
		let position = self.location[element];
		let boundary = self.first[set] + self.marked[set];
		if position < boundary {
			return; // Marked already.
		}
		self.elements[position] = self.elements[boundary];
		self.location[self.elements[position]] = position;
		self.elements[boundary] = element;
		self.location[element] = boundary;
		if self.marked[set] == 0 {
			self.touched.push(set);
		}
		self.marked[set] += 1;
	}

	fn split(&mut self) {
		while let Some(set) = self.touched.pop() {
			let boundary = self.first[set] + self.marked[set];
			self.marked[set] = 0;
			if boundary == self.past[set] {
				continue;
			}

			let new = self.sets;
			if boundary - self.first[set] <= self.past[set] - boundary {
				self.first[new] = self.first[set];
				self.past[new] = boundary;
				self.first[set] = boundary;
			} else {
				self.past[new] = self.past[set];
				self.first[new] = boundary;
				self.past[set] = boundary;
			}
			for position in self.first[new]..self.past[new] {
				self.set[self.elements[position]] = new;
			}
			self.created.push(new);
			self.sets += 1;
		}
	}

	/// A member of the set of `marked`'s elements that is not among them, if there is one; marks nothing.
	fn other_member(&mut self, marked: &[usize]) -> Option<usize> {
		let set = self.set[marked[0]];
		for &element in marked {
			self.mark(element);
		}
		let boundary = self.first[set] + self.marked[set];
		self.marked[set] = 0;
		self.touched.clear();
		(boundary < self.past[set]).then(|| self.elements[boundary])
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
