//! The form in which the engine stores a leaf, and the import of an [`Automaton`] into it.

use std::hash::{BuildHasher, Hash, Hasher};
use std::sync::OnceLock;
use std::time::{Duration, Instant};

use hashbrown::HashTable;
use indexmap::IndexSet;
use parking_lot::Mutex;
use rustc_hash::{FxBuildHasher, FxHashMap, FxHashSet, FxHasher};

use crate::ids::{Id, StateId};
use crate::simulation::{Simulation, TimedOut, Until};
use crate::{Automaton, Error, Symbol};

/// How long a query waits for another one's simulation before it reads its cancellation again.
const LOCK_SLICE: Duration = Duration::from_millis(10);

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

	pub(crate) fn num_moves(&self) -> usize {
		self.edges.len()
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

	/// The tuple of every move, numbered in the order of first appearance, and the distinct tuples.
	/// One linear pass, with a check per move, so cancellation never waits for a sort.
	fn labels(
		&self,
		stop: &mut dyn FnMut() -> bool,
	) -> Result<(Vec<usize>, Vec<&[Symbol]>), Stopped> {
		let mut labels = Vec::with_capacity(self.edges.len());
		let mut tuples = Vec::new();
		let mut positions: FxHashMap<&[Symbol], usize> = FxHashMap::default();
		for edge in 0..self.edges.len() {
			if stop() {
				return Err(Stopped);
			}
			let label = *positions.entry(self.tuple(edge)).or_insert_with_key(|_| {
				tuples.push(self.tuple(edge));
				tuples.len() - 1
			});
			labels.push(label);
		}
		Ok((labels, tuples))
	}

	/// Moves sorted by their dense `labels`: a counting sort, which checks `stop` per label and per move.
	fn moves_by_label(
		&self,
		labels: &[usize],
		stop: &mut dyn FnMut() -> bool,
	) -> Result<Vec<(usize, usize)>, Stopped> {
		let letters = labels.iter().copied().max().map_or(0, |max| max + 1);
		let mut starts = vec![0; letters + 1];
		for &letter in labels {
			starts[letter + 1] += 1;
		}
		for letter in 0..letters {
			if stop() {
				return Err(Stopped);
			}
			starts[letter + 1] += starts[letter];
		}
		let mut moves = Vec::new();
		moves.resize(self.edges.len(), (0, 0));
		for edge in 0..self.edges.len() {
			if stop() {
				return Err(Stopped);
			}
			moves[starts[labels[edge]]] = (labels[edge], edge);
			starts[labels[edge]] += 1;
		}
		Ok(moves)
	}

	/// The quotient by the coarsest forward bisimulation, which keeps the language on every tape: the partition
	/// refinement of Paige and Tarjan, by letter. Blocks are kept stable with respect to a coarser partition of
	/// compound sets, counting for every state and letter its moves into each compound set; a compound set of
	/// several blocks loses its smaller one, and only the moves into that block are visited, so every move is
	/// visited O(log n) times. [`Stopped`] when `stop` says so (it is asked once per block taken out, and per move
	/// visited).
	pub(crate) fn quotient(&self, stop: &mut dyn FnMut() -> bool) -> Result<Raw, Stopped> {
		let num_states = self.finals.len();
		let num_edges = self.edges.len();
		let (label, _) = self.labels(stop)?;
		let tail = |edge: usize| self.edges[edge].0.index();
		let mut incoming: Vec<Vec<usize>> = vec![Vec::new(); num_states];
		for (edge, &(_, target)) in self.edges.iter().enumerate() {
			if stop() {
				return Err(Stopped);
			}
			incoming[target.index()].push(edge);
		}
		let mut blocks = self.blocks_by_letters(&label, stop)?;

		// The moves of a state with a letter into a compound set, counted in one cell.
		let mut counts: Vec<usize> = Vec::new();
		let mut cells: FxHashMap<(usize, usize), usize> = FxHashMap::default();
		let mut edge_cell: Vec<usize> = (0..num_edges)
			.map(|edge| {
				let cell = *cells.entry((tail(edge), label[edge])).or_insert_with(|| {
					counts.push(0);
					counts.len() - 1
				});
				counts[cell] += 1;
				cell
			})
			.collect();

		let mut compound_of: Vec<usize> = vec![0; num_states.max(1)];
		let mut compounds: Vec<Vec<usize>> = vec![(0..blocks.sets).collect()];
		let mut splittable: Vec<usize> = if blocks.sets > 1 { vec![0] } else { Vec::new() };
		let size = |blocks: &Partition, block: usize| blocks.past[block] - blocks.first[block];
		while let Some(&compound) = splittable.last() {
			if stop() {
				return Err(Stopped);
			}
			let last = compounds[compound].len() - 1;
			let (one, other) = (compounds[compound][last], compounds[compound][last - 1]);
			let splitter =
				compounds[compound].swap_remove(if size(&blocks, one) <= size(&blocks, other) {
					last
				} else {
					last - 1
				});
			if compounds[compound].len() < 2 {
				splittable.pop();
			}
			compound_of[splitter] = compounds.len();
			compounds.push(vec![splitter]);

			// Moves into the splitter get new cells; per state and letter, the old cell then counts its moves
			// into the rest of the compound set.
			// A fresh map: clearing the first one, with a slot per move, would cost that much per splitter.
			let mut cells: FxHashMap<(usize, usize), usize> = FxHashMap::default();
			let mut entries: Vec<(usize, usize, usize)> = Vec::new();
			for position in blocks.first[splitter]..blocks.past[splitter] {
				for &edge in &incoming[blocks.elements[position]] {
					if stop() {
						return Err(Stopped);
					}
					let old = edge_cell[edge];
					let cell = *cells.entry((tail(edge), label[edge])).or_insert_with(|| {
						entries.push((tail(edge), label[edge], old));
						counts.push(0);
						counts.len() - 1
					});
					counts[old] -= 1;
					counts[cell] += 1;
					edge_cell[edge] = cell;
				}
			}

			// A state with moves into the splitter is told apart by the letters of those moves and whether it
			// has a move with the same letter into the rest; the states without such moves keep their block.
			let by_state = canonical_cells(entries, &counts, stop)?;
			let mut touched: FxHashMap<usize, KeyGroups> = FxHashMap::default();
			for (state, key) in by_state {
				touched
					.entry(blocks.set[state])
					.or_default()
					.entry(key)
					.or_default()
					.push(state);
			}
			for (block, groups) in touched {
				let all = groups.values().map(Vec::len).sum::<usize>() == size(&blocks, block);
				for group in groups.into_values().skip(usize::from(all)) {
					for state in group {
						blocks.mark(state);
					}
					blocks.split();
				}
			}
			for (new, parent) in blocks.created.drain(..) {
				let compound = compound_of[parent];
				compound_of[new] = compound;
				compounds[compound].push(new);
				if compounds[compound].len() == 2 {
					splittable.push(compound);
				}
			}
		}

		Ok(self.merged(&blocks))
	}

	/// The states split by finality and the letters of their moves, so that the blocks are stable with respect to
	/// the one compound set of all states.
	fn blocks_by_letters(
		&self,
		label: &[usize],
		stop: &mut dyn FnMut() -> bool,
	) -> Result<Partition, Stopped> {
		let num_states = self.finals.len();
		let mut letters: Vec<Vec<usize>> = vec![Vec::new(); num_states];
		for (edge, &(source, _)) in self.edges.iter().enumerate() {
			if stop() {
				return Err(Stopped);
			}
			letters[source.index()].push(label[edge]);
		}
		let mut blocks = Partition::new(num_states);
		let mut groups: FxHashMap<(bool, LetterSet), Vec<usize>> = FxHashMap::default();
		for (state, state_letters) in letters.into_iter().enumerate() {
			if stop() {
				return Err(Stopped);
			}
			let state_letters = LetterSet::new(state_letters);
			groups
				.entry((self.finals[state], state_letters))
				.or_default()
				.push(state);
		}
		for group in groups.into_values().skip(1) {
			for state in group {
				blocks.mark(state);
			}
			blocks.split();
		}
		blocks.created.clear();
		Ok(blocks)
	}

	/// `self` with the states of every block of `blocks` merged.
	fn merged(&self, blocks: &Partition) -> Raw {
		let mut quotient = Raw::new(self.arity);
		quotient.finals = vec![false; blocks.sets];
		for state in (0..self.finals.len()).filter(|&state| self.finals[state]) {
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
		quotient
	}

	/// The subset construction over the tuples of `self`: `Ok(None)` once it has more than `max_states` states, its
	/// subsets more than `max_members` members in all or it more than `max_moves` moves, [`Stopped`] when `stop`
	/// says so (it is asked once per subset and per move).
	pub(crate) fn determinize(
		&self,
		max_states: usize,
		max_members: usize,
		max_moves: usize,
		stop: &mut dyn FnMut() -> bool,
	) -> Result<Option<Raw>, Stopped> {
		let mut outgoing: Vec<Vec<usize>> = vec![Vec::new(); self.finals.len()];
		for (edge, &(source, _)) in self.edges.iter().enumerate() {
			outgoing[source.index()].push(edge);
		}

		let mut start = self.initial.clone();
		start.sort_unstable();
		start.dedup();
		let mut members = start.len();
		let mut subsets: IndexSet<Vec<StateId>, FxBuildHasher> = IndexSet::default();
		subsets.insert(start);
		let mut dfa = Raw::new(self.arity);
		dfa.initial.push(StateId::new(0));
		let mut next = 0;
		while next < subsets.len() {
			if stop() {
				return Err(Stopped);
			}

			let mut moves: FxHashMap<&[Symbol], Vec<StateId>> = FxHashMap::default();
			for state in &subsets[next] {
				for &edge in &outgoing[state.index()] {
					if stop() {
						return Err(Stopped);
					}
					moves
						.entry(self.tuple(edge))
						.or_default()
						.push(self.edges[edge].1);
				}
			}
			dfa.finals
				.push(subsets[next].iter().any(|state| self.finals[state.index()]));
			// Grouped without a sort. A subset is canonical: its members in state order, each once.
			for (tuple, mut targets) in moves {
				if stop() {
					return Err(Stopped);
				}
				if dfa.edges.len() == max_moves {
					return Ok(None);
				}
				// Only the target subset is sorted to get one canonical key, not all the moves.
				targets.sort_unstable();
				targets.dedup();
				let target = targets;
				let size = target.len();
				let (index, new) = subsets.insert_full(target);
				// Checked as each subset is found, so a state with many new targets does not emit them all first.
				if new {
					members += size;
					if subsets.len() > max_states || members > max_members {
						return Ok(None);
					}
				}
				dfa.push(StateId::new(next), tuple, StateId::new(index));
			}
			next += 1;
		}

		Ok(Some(dfa))
	}

	/// The minimal automaton of `self`, which must be trimmed and deterministic over its tuples: the partition
	/// refinement of Valmari and Lehtinen for partial DFAs (STACS 2008), in O(m log n) for m moves and n states.
	/// [`Stopped`] when `stop` says so (it is asked once per class of moves, per move of it and per new block).
	pub(crate) fn minimize(&self, stop: &mut dyn FnMut() -> bool) -> Result<Raw, Stopped> {
		let num_states = self.finals.len();
		let num_edges = self.edges.len();
		let mut minimal = Raw::new(self.arity);
		if num_states == 0 {
			return Ok(minimal);
		}
		debug_assert_eq!(self.initial.len(), 1, "a deterministic automaton");

		// Letters are the distinct tuples, numbered in their order; one linear pass per move.
		let (label, tuples) = self.labels(stop)?;
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

		// Cords: classes of moves, first by label; a linear counting sort.
		let by_label = self.moves_by_label(&label, stop)?;
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
			for &edge in &cords.elements[cords.first[cord]..cords.past[cord]] {
				if stop() {
					return Err(Stopped);
				}
				blocks.mark(tail(edge));
			}
			blocks.split();
			cord += 1;

			while block < blocks.sets {
				if stop() {
					return Err(Stopped);
				}
				for &state in &blocks.elements[blocks.first[block]..blocks.past[block]] {
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
			.map(|edge| (blocks.set[tail(edge)], label[edge], blocks.set[head(edge)]))
			.collect();
		moves.sort_unstable();
		moves.dedup();
		for (source, letter, target) in moves {
			minimal.push(StateId::new(source), tuples[letter], StateId::new(target));
		}

		Ok(minimal)
	}

	/// The useful part of `self`: states reachable from an initial one that reach a final one. [`Stopped`] when
	/// `stop` says so (it is asked per state of the useful-state search, and per move).
	pub(crate) fn trimmed(&self, stop: &mut dyn FnMut() -> bool) -> Result<Raw, Stopped> {
		let renumbered = useful_states_until(self, stop)?;
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
			if stop() {
				return Err(Stopped);
			}
			let (source, target) = self.edges[edge];
			if let (Some(source), Some(target)) =
				(renumbered[source.index()], renumbered[target.index()])
			{
				trimmed.push(source, self.tuple(edge), target);
			}
		}

		Ok(trimmed)
	}
}

/// A reduction gave up, as its query passed a limit.
#[derive(Debug)]
pub(crate) struct Stopped;

/// The states of one block with moves into a splitter, by their letters and whether they also reach the rest.
type KeyGroups = FxHashMap<Vec<(usize, bool)>, Vec<usize>>;

/// The `(letter, whether a move remains)` cells for every state, sorted within each state. The grouping is the same
/// as sorting every entry together, but each sort covers only one state's cells and `stop` is asked per state.
fn canonical_cells(
	entries: Vec<(usize, usize, usize)>,
	counts: &[usize],
	stop: &mut dyn FnMut() -> bool,
) -> Result<FxHashMap<usize, Vec<(usize, bool)>>, Stopped> {
	let mut by_state: FxHashMap<usize, Vec<(usize, bool)>> = FxHashMap::default();
	for (state, letter, old) in entries {
		by_state
			.entry(state)
			.or_default()
			.push((letter, counts[old] > 0));
	}
	for key in by_state.values_mut() {
		if stop() {
			return Err(Stopped);
		}
		key.sort_unstable();
	}
	Ok(by_state)
}

/// A set of move labels, for grouping states by their outgoing letters without sorting them.
#[derive(PartialEq, Eq)]
struct LetterSet {
	letters: FxHashSet<usize>,
	/// The commutative hash: set equality is independent of insertion order.
	fingerprint: u64,
}

impl LetterSet {
	fn new(letters: Vec<usize>) -> LetterSet {
		let mut fingerprint = 0;
		let mut set = FxHashSet::default();
		for letter in letters {
			if set.insert(letter) {
				let mut hash = FxHasher::default();
				letter.hash(&mut hash);
				fingerprint ^= hash.finish();
			}
		}
		LetterSet {
			letters: set,
			fingerprint,
		}
	}
}

impl Hash for LetterSet {
	fn hash<H: Hasher>(&self, state: &mut H) {
		state.write_u64(self.fingerprint);
	}
}

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
	/// The sets that splits created, in order, each with the set it was split from.
	created: Vec<(usize, usize)>,
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
			self.created.push((new, set));
			self.sets += 1;
		}
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

	/// The simulation, computed on first use; [`TimedOut`] if computing or waiting passes `until` (nothing kept).
	pub(crate) fn simulation(&self, until: &Until) -> Result<&Simulation, TimedOut> {
		match self.simulation.get() {
			Some(simulation) => Ok(simulation),
			None => self.compute_simulation(until),
		}
	}

	/// Computes the simulation or waits for the query computing it; apart, so that the lookup above inlines.
	#[cold]
	fn compute_simulation(&self, until: &Until) -> Result<&Simulation, TimedOut> {
		let computing = if until.is_never() {
			self.computing.lock()
		} else {
			// Waits in slices, to see a cancellation that comes before the deadline.
			loop {
				let slice = Instant::now() + LOCK_SLICE;
				let wait = until.deadline.map_or(slice, |deadline| deadline.min(slice));
				if let Some(computing) = self.computing.try_lock_until(wait) {
					break computing;
				}
				if until.passed() {
					return Err(TimedOut);
				}
			}
		};

		// The query that held the lock may have computed it.
		if let Some(simulation) = self.simulation.get() {
			return Ok(simulation);
		}

		let computed = Simulation::compute(self, until)?;
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
	useful_states_until(raw, &mut || false).expect("nothing stops")
}

/// [`useful_states`], stopping once `stop` says so; asked per edge and per state visited.
fn useful_states_until(
	raw: &Raw,
	stop: &mut dyn FnMut() -> bool,
) -> Result<Vec<Option<StateId>>, Stopped> {
	let num_states = raw.finals.len();
	let mut predecessors = vec![Vec::new(); num_states];
	for &(source, target) in &raw.edges {
		if stop() {
			return Err(Stopped);
		}
		predecessors[target.index()].push(source);
	}

	let mut useful = raw.finals.clone();
	let mut stack: Vec<StateId> = (0..num_states)
		.filter(|&state| useful[state])
		.map(StateId::new)
		.collect();
	while let Some(state) = stack.pop() {
		if stop() {
			return Err(Stopped);
		}
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

	Ok(renumbered)
}
