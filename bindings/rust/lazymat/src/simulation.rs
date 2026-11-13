//! Direct simulation of a leaf, over its tuples read as opaque letters.
//!
//! Reading tuples as opaque letters relates fewer states than the true simulation (a wildcard tuple only
//! matches the same wildcard tuple), so it still implies language inclusion.
//!
//! The states that simulate `p` are the greatest solution of `sim(p) = F(p) ∩ ⋂ {pre_a[sim(p')] | p -a-> p'}`,
//! where `F(p)` is the set of final states if `p` is final and of all states otherwise, and `pre_a[X]` is the
//! set of states with an `a`-move into `X`. The rows of a strongly connected component depend only on its own
//! rows and on those of the components it reaches, so the components are solved sinks first. One without a
//! cycle takes one step, and one with a cycle the refinement of Henzinger, Henzinger and Kopke (FOCS 1995).

use std::sync::OnceLock;
use std::time::Instant;

use rustc_hash::{FxHashMap, FxHashSet};

use crate::Symbol;
use crate::ids::{Id, StateId};
use crate::leaf::Leaf;

/// Leaves with more states get the identity relation (the matrix would take more than 128 MiB).
const MAX_STATES: usize = 1 << 15;

/// Work after which a leaf gets the identity relation.
const MAX_WORK: usize = 3 << 29;

/// Checks that refining pair by pair would make at least, after which a leaf gets the identity relation.
const MAX_PAIR_CHECKS: usize = 1 << 30;

/// Work between two reads of the clock.
const CLOCK_WORK: usize = 1 << 16;

/// The most counters of one component (32 MiB); past them, a group's targets are scanned at every removal.
const MAX_COUNTERS: usize = 1 << 22;

/// The most pairs of a relation that is transposed, which costs a step per pair.
const MAX_TRANSPOSED: usize = 1 << 20;

/// The relation of a leaf whose simulation is not computed.
pub(crate) static IDENTITY: Simulation = Simulation::new(None);

/// The simulation passed the query's deadline.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) struct TimedOut;

/// Why the refinement stopped.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum Stop {
	/// It passed [`MAX_WORK`] or [`MAX_PAIR_CHECKS`], so the leaf gets the identity.
	Work,
	Deadline,
}

/// The work and pair checks that one simulation has left, and its deadline.
#[derive(Debug)]
struct Work {
	left: usize,
	pair_checks: usize,
	until_clock: usize,
	deadline: Option<Instant>,
}

impl Work {
	fn new(deadline: Option<Instant>) -> Work {
		Work {
			left: MAX_WORK,
			pair_checks: MAX_PAIR_CHECKS,
			until_clock: CLOCK_WORK,
			deadline,
		}
	}

	/// Counts `checks` that refining pair by pair would make.
	fn check_pairs(&mut self, checks: usize) -> Result<(), Stop> {
		self.pair_checks = self.pair_checks.checked_sub(checks).ok_or(Stop::Work)?;

		Ok(())
	}

	/// Whether `units` are left, without spending them.
	fn afford(&self, units: usize) -> Result<(), Stop> {
		if units <= self.left {
			Ok(())
		} else {
			Err(Stop::Work)
		}
	}

	/// Spends `units`, reading the clock every [`CLOCK_WORK`] units.
	fn spend(&mut self, units: usize) -> Result<(), Stop> {
		self.left = self.left.checked_sub(units).ok_or(Stop::Work)?;
		if let Some(left) = self.until_clock.checked_sub(units) {
			self.until_clock = left;

			return Ok(());
		}

		self.until_clock = CLOCK_WORK;
		if self
			.deadline
			.is_some_and(|deadline| Instant::now() > deadline)
		{
			return Err(Stop::Deadline);
		}

		Ok(())
	}
}

/// A class of tuples with the same moves.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, PartialOrd, Ord)]
struct Letter(u32);

impl Letter {
	fn new(index: usize) -> Letter {
		Letter(u32::try_from(index).expect("fewer than 2^32 letters"))
	}

	fn index(self) -> usize {
		self.0 as usize
	}
}

/// The number of a source and a letter with several moves, which get counters.
#[derive(Clone, Copy, Debug)]
struct Group(u32);

impl Group {
	fn new(index: usize) -> Group {
		Group(u32::try_from(index).expect("fewer than 2^32 groups"))
	}

	fn index(self) -> usize {
		self.0 as usize
	}
}

const WORD_BITS: usize = u64::BITS as usize;

/// A square matrix of bits, `stride` words per row.
#[derive(Debug)]
struct BitMatrix {
	size: usize,
	stride: usize,
	words: Vec<u64>,
}

impl BitMatrix {
	fn new(size: usize) -> BitMatrix {
		let stride = size.div_ceil(WORD_BITS);

		BitMatrix {
			size,
			stride,
			words: vec![0; stride * size],
		}
	}

	/// The index of the word that holds bit `(row, col)`, and the mask of the bit in it.
	fn locate(&self, row: usize, col: usize) -> (usize, u64) {
		(row * self.stride + col / WORD_BITS, 1 << (col % WORD_BITS))
	}

	fn get(&self, row: usize, col: usize) -> bool {
		let (word, mask) = self.locate(row, col);

		self.words[word] & mask != 0
	}

	fn clear(&mut self, row: usize, col: usize) {
		let (word, mask) = self.locate(row, col);
		self.words[word] &= !mask;
	}

	fn row_words(&self, row: usize) -> &[u64] {
		&self.words[row * self.stride..(row + 1) * self.stride]
	}

	fn row_words_mut(&mut self, row: usize) -> &mut [u64] {
		&mut self.words[row * self.stride..(row + 1) * self.stride]
	}

	fn intersect_row(&mut self, row: usize, with: &[u64]) {
		self.row_words_mut(row)
			.iter_mut()
			.zip(with)
			.for_each(|(word, &kept)| *word &= kept);
	}

	/// The columns of the set bits of `row`, ascending.
	fn row(&self, row: usize) -> impl Iterator<Item = usize> {
		self.row_words(row)
			.iter()
			.copied()
			.enumerate()
			.flat_map(|(index, mut word)| {
				std::iter::from_fn(move || {
					if word == 0 {
						return None;
					}

					let bit = word.trailing_zeros() as usize;
					word &= word - 1;

					Some(index * WORD_BITS + bit)
				})
			})
	}

	fn count_row(&self, row: usize) -> usize {
		self.row_words(row)
			.iter()
			.map(|word| word.count_ones() as usize)
			.sum()
	}

	fn transposed(&self) -> BitMatrix {
		let mut result = BitMatrix::new(self.size);
		for row in 0..self.size {
			for col in self.row(row) {
				set_bit(result.row_words_mut(col), row);
			}
		}

		result
	}
}

fn set_bit(words: &mut [u64], bit: usize) {
	words[bit / WORD_BITS] |= 1 << (bit % WORD_BITS);
}

/// `p ≤ q` when every word accepted from `p` is accepted from `q`.
#[derive(Debug)]
pub(crate) struct Simulation {
	/// Row `p` holds the states `q` with `p ≤ q`; `None` is the identity.
	relation: Option<BitMatrix>,
	/// The number of states above each state.
	counts_above: OnceLock<Vec<usize>>,
	/// `relation` transposed, if it has at most [`MAX_TRANSPOSED`] pairs.
	transposed: OnceLock<Option<BitMatrix>>,
}

impl Simulation {
	const fn new(relation: Option<BitMatrix>) -> Simulation {
		Simulation {
			relation,
			counts_above: OnceLock::new(),
			transposed: OnceLock::new(),
		}
	}

	/// The maximal simulation of `leaf`, or the identity past the bounds; [`TimedOut`] once `deadline` passes.
	pub(crate) fn compute(leaf: &Leaf, deadline: Option<Instant>) -> Result<Simulation, TimedOut> {
		let relation = if leaf.num_states() > MAX_STATES {
			None
		} else {
			match maximal_simulation(leaf, &mut Work::new(deadline), MAX_COUNTERS) {
				Ok(relation) => Some(relation),
				Err(Stop::Work) => None,
				Err(Stop::Deadline) => return Err(TimedOut),
			}
		};

		Ok(Simulation::new(relation))
	}

	pub(crate) fn le(&self, p: StateId, q: StateId) -> bool {
		match &self.relation {
			Some(relation) => relation.get(p.index(), q.index()),
			None => p == q,
		}
	}

	/// The states `q` with `p ≤ q`, ascending; `None` for the identity.
	pub(crate) fn upward(&self, p: StateId) -> Option<impl Iterator<Item = StateId>> {
		let relation = self.relation.as_ref()?;

		Some(relation.row(p.index()).map(StateId::new))
	}

	/// The number of states `q` with `p ≤ q`.
	pub(crate) fn count_above(&self, p: StateId) -> usize {
		match &self.relation {
			Some(relation) => self.counts_above(relation)[p.index()],
			None => 1,
		}
	}

	/// The states `q` with `q ≤ p`, ascending; `None` for the identity and for a relation not transposed.
	pub(crate) fn downward(&self, p: StateId) -> Option<impl Iterator<Item = StateId>> {
		let relation = self.relation.as_ref()?;

		self.transposed(relation)
			.map(|transposed| transposed.row(p.index()).map(StateId::new))
	}

	pub(crate) fn is_identity(&self) -> bool {
		self.relation.is_none()
	}

	/// The rows of [`downward`](Self::downward), or `None` for the identity and for a relation not transposed.
	pub(crate) fn downward_rows(&self) -> Option<DownwardRows<'_>> {
		let relation = self.relation.as_ref()?;

		self.transposed(relation).map(DownwardRows)
	}

	fn transposed(&self, relation: &BitMatrix) -> Option<&BitMatrix> {
		self.transposed
			.get_or_init(|| {
				let pairs: usize = self.counts_above(relation).iter().sum();

				(pairs <= MAX_TRANSPOSED).then(|| relation.transposed())
			})
			.as_ref()
	}

	fn counts_above(&self, relation: &BitMatrix) -> &[usize] {
		self.counts_above.get_or_init(|| {
			(0..relation.size)
				.map(|row| relation.count_row(row))
				.collect()
		})
	}
}

/// The states below each state of a simulation other than the identity, one row of bits per state.
#[derive(Clone, Copy, Debug)]
pub(crate) struct DownwardRows<'a>(&'a BitMatrix);

impl<'a> DownwardRows<'a> {
	/// The states `q` with `q ≤ p`, as the words of a bitset over the leaf's states.
	pub(crate) fn row(self, p: StateId) -> &'a [u64] {
		self.0.row_words(p.index())
	}
}

/// Items per key, where those of key `k` are `items[starts[k]..starts[k + 1]]`.
#[derive(Debug)]
struct Lists<T> {
	starts: Vec<usize>,
	items: Vec<T>,
}

impl<T> Default for Lists<T> {
	fn default() -> Lists<T> {
		Lists {
			starts: vec![0],
			items: Vec::new(),
		}
	}
}

impl<T: Copy + Default> Lists<T> {
	/// The lists of `len` keys from the `(key, item)` pairs of `pairs`, in their order; `pairs` is called twice.
	fn grouped<I: Iterator<Item = (usize, T)>>(len: usize, pairs: impl Fn() -> I) -> Lists<T> {
		let mut starts = vec![0; len + 1];
		for (key, _) in pairs() {
			starts[key + 1] += 1;
		}

		for key in 0..len {
			starts[key + 1] += starts[key];
		}

		let mut items = vec![T::default(); starts[len]];
		let mut next = starts.clone();
		for (key, item) in pairs() {
			items[next[key]] = item;
			next[key] += 1;
		}

		Lists { starts, items }
	}
}

impl<T> Lists<T> {
	fn len(&self) -> usize {
		self.starts.len() - 1
	}

	fn of(&self, key: usize) -> &[T] {
		&self.items[self.starts[key]..self.starts[key + 1]]
	}
}

/// A source with no move on `letter` into `row` any more, whose consequences are not drawn yet.
#[derive(Clone, Copy, Debug)]
struct Lost {
	row: StateId,
	source: StateId,
	letter: Letter,
}

/// A move seen from its target.
#[derive(Clone, Copy, Debug, Default)]
struct MoveIn {
	letter: Letter,
	source: StateId,
	/// The counters of the source's moves on the letter, if it has several.
	group: Option<Group>,
}

/// The moves of a leaf, by letter.
#[derive(Debug)]
struct Moves {
	/// From each state, by letter, then target.
	post: Lists<(Letter, StateId)>,
	/// Into each state, by letter, then source.
	pre: Lists<MoveIn>,
	/// On each letter, by source, then target.
	on: Lists<(StateId, StateId)>,
	/// The number of states with moves on each letter.
	sources_on: Vec<usize>,
	/// The number of sources and letters with several moves.
	groups: usize,
	/// The cost of a binary search among the moves into a state, about four moves tried per halving.
	search_steps: usize,
}

impl Moves {
	/// Every list comes out sorted, as it is filled in the order of its keys.
	fn new(leaf: &Leaf) -> Moves {
		let on = moves_by_letter(leaf);

		// A source with several moves on a letter gets counters.
		let mut groups = 0;
		let mut group_of = Vec::with_capacity(on.items.len());
		let mut sources_on = vec![0; on.len()];
		for (letter, sources) in sources_on.iter_mut().enumerate() {
			for run in on.of(letter).chunk_by(|a, b| a.0 == b.0) {
				let mut group = None;
				if run.len() > 1 {
					group = Some(Group::new(groups));
					groups += 1;
				}

				*sources += 1;
				group_of.extend(std::iter::repeat_n(group, run.len()));
			}
		}

		let on_letters = || {
			(0..on.len()).flat_map(|index| {
				let letter = Letter::new(index);

				on.of(index)
					.iter()
					.map(move |&(source, target)| (letter, source, target))
			})
		};
		let size = leaf.num_states();
		let post = Lists::grouped(size, || {
			on_letters().map(|(letter, source, target)| (source.index(), (letter, target)))
		});
		let pre = Lists::grouped(size, || {
			on_letters()
				.zip(&group_of)
				.map(|((letter, source, target), &group)| {
					let moved = MoveIn {
						letter,
						source,
						group,
					};

					(target.index(), moved)
				})
		});

		let average_into = (pre.items.len() / size.max(1)).max(1);

		Moves {
			post,
			pre,
			on,
			sources_on,
			groups,
			search_steps: 4 * (1 + average_into.ilog2() as usize),
		}
	}

	/// The targets of the moves of `state` on `letter`.
	fn targets(&self, state: StateId, letter: Letter) -> impl Iterator<Item = StateId> {
		let moves = self.post.of(state.index());
		let start = moves.partition_point(|&(other, _)| other < letter);

		moves[start..]
			.iter()
			.take_while(move |&&(other, _)| other == letter)
			.map(|&(_, target)| target)
	}

	/// The moves into `state` on `letter`.
	fn sources(&self, state: StateId, letter: Letter) -> &[MoveIn] {
		let moves = self.pre.of(state.index());
		let start = moves.partition_point(|moved| moved.letter < letter);
		let len = moves[start..]
			.iter()
			.take_while(|moved| moved.letter == letter)
			.count();

		&moves[start..start + len]
	}

	/// The moves into `state`, one run per letter.
	fn runs_into(&self, state: StateId) -> impl Iterator<Item = &[MoveIn]> {
		self.pre
			.of(state.index())
			.chunk_by(|a, b| a.letter == b.letter)
	}
}

/// The moves on each letter, by source, then target, the letters numbered in the order of their first tuples.
fn moves_by_letter(leaf: &Leaf) -> Lists<(StateId, StateId)> {
	let leaf_moves = || {
		(0..leaf.num_states()).map(StateId::new).flat_map(|source| {
			leaf.moves(source)
				.map(move |(tuple, target)| (tuple, source, target))
		})
	};

	let mut tuples: FxHashMap<&[Symbol], u32> = FxHashMap::default();
	let mut tuple_of = Vec::with_capacity(leaf.num_moves());
	for (tuple, _, _) in leaf_moves() {
		let next = u32::try_from(tuples.len()).expect("fewer than 2^32 tuples");
		tuple_of.push(*tuples.entry(tuple).or_insert(next));
	}

	let by_tuple = Lists::grouped(tuples.len(), || {
		leaf_moves()
			.zip(&tuple_of)
			.map(|((_, source, target), &tuple)| (tuple as usize, (source, target)))
	});

	// Tuples with the same moves impose the same conditions, so they share a letter.
	let mut seen: FxHashSet<&[(StateId, StateId)]> = FxHashSet::default();
	let mut on = Lists::default();
	for tuple in 0..by_tuple.len() {
		let moves = by_tuple.of(tuple);
		if seen.insert(moves) {
			on.items.extend_from_slice(moves);
			on.starts.push(on.items.len());
		}
	}

	on
}

/// The strongly connected components of the moves, each after the components it reaches.
#[derive(Debug)]
struct Components {
	members: Lists<StateId>,
	/// The component of each state.
	of: Vec<usize>,
	/// Whether each component has a cycle.
	cyclic: Vec<bool>,
}

/// Tarjan's search for strongly connected components.
#[derive(Debug)]
struct Tarjan {
	/// The order in which the search reached each state.
	index: Vec<Option<usize>>,
	/// The lowest index that a state reaches back to.
	low: Vec<usize>,
	on_stack: Vec<bool>,
	stack: Vec<usize>,
	/// The states being searched, each with its next move.
	path: Vec<(usize, usize)>,
	reached: usize,
}

impl Tarjan {
	fn enter(&mut self, state: usize) {
		self.index[state] = Some(self.reached);
		self.low[state] = self.reached;
		self.reached += 1;
		self.on_stack[state] = true;
		self.stack.push(state);
		self.path.push((state, 0));
	}
}

impl Components {
	fn new(post: &Lists<(Letter, StateId)>) -> Components {
		let size = post.len();
		let mut members = Lists::default();
		let mut of = vec![0; size];
		let mut search = Tarjan {
			index: vec![None; size],
			low: vec![0; size],
			on_stack: vec![false; size],
			stack: Vec::new(),
			path: Vec::new(),
			reached: 0,
		};

		for root in 0..size {
			if search.index[root].is_some() {
				continue;
			}

			search.enter(root);
			// Depth first, one move at a time; a state finishes when its moves are done.
			while let Some(&(state, next)) = search.path.last() {
				if let Some(&(_, target)) = post.of(state).get(next) {
					let top = search.path.len() - 1;
					search.path[top].1 += 1;
					let target = target.index();
					match search.index[target] {
						None => search.enter(target),
						Some(index) if search.on_stack[target] => {
							search.low[state] = search.low[state].min(index);
						}
						Some(_) => {}
					}

					continue;
				}

				search.path.pop();
				if let Some(&(parent, _)) = search.path.last() {
					search.low[parent] = search.low[parent].min(search.low[state]);
				}

				if Some(search.low[state]) == search.index[state] {
					let component = members.len();
					loop {
						let member = search
							.stack
							.pop()
							.expect("a component's root is on the stack");
						search.on_stack[member] = false;
						of[member] = component;
						members.items.push(StateId::new(member));
						if member == state {
							break;
						}
					}

					members.starts.push(members.items.len());
				}
			}
		}

		let cyclic = (0..members.len())
			.map(|component| match *members.of(component) {
				[state] => post
					.of(state.index())
					.iter()
					.any(|&(_, target)| target == state),
				_ => true,
			})
			.collect();

		Components {
			members,
			of,
			cyclic,
		}
	}

	/// Whether `state` is on a cycle, that is, its component has one.
	fn on_cycle(&self, state: StateId) -> bool {
		self.cyclic[self.of[state.index()]]
	}
}

/// The rows of one leaf while they are refined.
#[derive(Debug)]
struct Solver<'a> {
	moves: &'a Moves,
	components: &'a Components,
	/// Row `p` holds the states that simulate `p`.
	relation: BitMatrix,
	work: &'a mut Work,
	scratch: Vec<u64>,
	/// The position of each state in its component.
	local: Vec<usize>,
	/// The component being refined.
	refining: usize,
	/// The number of members of the component being refined.
	width: usize,
	max_counters: usize,
	/// Whether the component being refined has counters.
	counting: bool,
	/// Per group and member of the component being refined, how many targets of the group the member's row holds.
	counts: Vec<Option<u32>>,
	/// The counters in use, cleared when the component is solved.
	counted: Vec<usize>,
	/// The lost sources whose consequences are not drawn yet, each at most once.
	pending: Vec<Lost>,
}

/// The maximal simulation of `leaf`; a component gets counters if it needs at most `max_counters`.
fn maximal_simulation(
	leaf: &Leaf,
	work: &mut Work,
	max_counters: usize,
) -> Result<BitMatrix, Stop> {
	// Building the moves costs a few units per move, so past the bound nothing is built.
	let size = leaf.num_states();
	work.spend(4 * leaf.num_moves() + size)?;
	let moves = Moves::new(leaf);
	// Every move will cost at least a row of work, an intersection or the removals that keep a row.
	work.afford(moves.post.items.len() * size.div_ceil(WORD_BITS))?;
	let components = Components::new(&moves.post);

	// Refining pair by pair would start the row of a state on a cycle by matching its moves against every state.
	let start_checks: usize = (0..size)
		.filter(|&state| components.on_cycle(StateId::new(state)))
		.map(|state| size * (1 + moves.post.of(state).len()))
		.sum();
	work.check_pairs(start_checks)?;

	// Every row starts as the final states if its state is final, else as all states.
	let mut relation = BitMatrix::new(size);
	let stride = relation.stride;
	let (mut finals, mut all) = (vec![0; stride], vec![0; stride]);
	for state in 0..size {
		set_bit(&mut all, state);
		if leaf.is_final(StateId::new(state)) {
			set_bit(&mut finals, state);
		}
	}

	for state in 0..size {
		let start = if leaf.is_final(StateId::new(state)) {
			&finals
		} else {
			&all
		};

		relation.row_words_mut(state).copy_from_slice(start);
	}

	work.spend(size * stride)?;

	let mut solver = Solver {
		moves: &moves,
		components: &components,
		relation,
		work,
		scratch: vec![0; stride],
		local: vec![0; size],
		refining: 0,
		width: 0,
		max_counters,
		counting: false,
		counts: Vec::new(),
		counted: Vec::new(),
		pending: Vec::new(),
	};
	solver.require_letters()?;

	// Then it would check each state that such a row starts with.
	let row_checks: usize = (0..size)
		.filter(|&state| components.on_cycle(StateId::new(state)))
		.map(|state| solver.relation.count_row(state))
		.sum();
	solver.work.spend(size * stride)?;
	solver.work.check_pairs(row_checks)?;

	// Sinks first, so the rows of the components that a component reaches are final when it is solved.
	for (component, &cyclic) in components.cyclic.iter().enumerate() {
		if cyclic {
			solver.refine(component)?;
		}

		solver.constrain_predecessors(component)?;
	}

	Ok(solver.relation)
}

impl Solver<'_> {
	/// Keeps in the row of each state on a cycle the states with moves on its letters; pre-images do it for others.
	fn require_letters(&mut self) -> Result<(), Stop> {
		let (moves, components) = (self.moves, self.components);
		for letter in 0..moves.on.len() {
			let on = moves.on.of(letter);
			if !on.iter().any(|&(source, _)| components.on_cycle(source)) {
				continue;
			}

			self.scratch.fill(0);
			for &(source, _) in on {
				set_bit(&mut self.scratch, source.index());
			}

			let mut cost = self.scratch.len() + on.len();
			for run in on.chunk_by(|a, b| a.0 == b.0) {
				let source = run[0].0;
				if components.on_cycle(source) {
					self.relation.intersect_row(source.index(), &self.scratch);
					cost += self.scratch.len();
				}
			}

			self.work.spend(cost)?;
		}

		Ok(())
	}

	/// Sets the scratch row to the states with a `letter`-move into row `target`, by the cheapest of three ways.
	fn pre_image(&mut self, target: StateId, letter: Letter) -> Result<(), Stop> {
		let target = target.index();
		self.scratch.fill(0);
		let mut cost = 2 * self.scratch.len();

		// In moves tried; source by source tries about `states / row_len` moves per source if the row is spread out.
		let moves = self.moves.on.of(letter.index());
		let row_len = self.relation.count_row(target);
		let by_row = self.moves.search_steps * row_len;
		let by_letter = moves.len();
		let num_sources = self.moves.sources_on[letter.index()];
		let by_source =
			num_sources * (self.moves.search_steps + self.relation.size / row_len.max(1));
		if by_source < by_letter.min(by_row) {
			// A search finds where a source's moves end, so that the rest are skipped after a hit.
			let mut rest = moves;
			while let Some(&(source, _)) = rest.first() {
				let len = rest.partition_point(|&(other, _)| other == source);
				let (run, after) = rest.split_at(len);
				let hit = run
					.iter()
					.position(|&(_, simulator)| self.relation.get(target, simulator.index()));
				if hit.is_some() {
					set_bit(&mut self.scratch, source.index());
				}

				cost += self.moves.search_steps + hit.map_or(len, |position| position + 1);
				rest = after;
			}
		} else if by_letter <= by_row {
			// A source is done once one of its moves reaches the row.
			let mut reached = None;
			for &(source, simulator) in moves {
				if reached != Some(source) && self.relation.get(target, simulator.index()) {
					set_bit(&mut self.scratch, source.index());
					reached = Some(source);
				}
			}

			cost += moves.len();
		} else {
			for simulator in self.relation.row(target) {
				let sources = self.moves.sources(StateId::new(simulator), letter);
				for moved in sources {
					set_bit(&mut self.scratch, moved.source.index());
				}

				cost += self.moves.search_steps + sources.len();
			}
		}

		self.work.spend(cost)
	}

	/// Keeps in the rows of the predecessors outside the solved `component` what its rows allow.
	fn constrain_predecessors(&mut self, component: usize) -> Result<(), Stop> {
		let (moves, components) = (self.moves, self.components);
		let outside = |moved: &MoveIn| components.of[moved.source.index()] != component;
		for &target in components.members.of(component) {
			for run in moves.runs_into(target) {
				if !run.iter().any(outside) {
					continue;
				}

				self.pre_image(target, run[0].letter)?;
				for moved in run.iter().filter(|&moved| outside(moved)) {
					let source = moved.source.index();
					self.relation.intersect_row(source, &self.scratch);
				}

				self.work.spend(run.len() * self.scratch.len())?;
			}
		}

		Ok(())
	}

	/// Refines the rows of `component`, which has a cycle, by the counters of Henzinger, Henzinger and Kopke.
	fn refine(&mut self, component: usize) -> Result<(), Stop> {
		let (moves, components) = (self.moves, self.components);
		let members = components.members.of(component);
		for (position, &member) in members.iter().enumerate() {
			self.local[member.index()] = position;
		}

		self.refining = component;
		self.width = members.len();
		let slots = moves.groups * self.width;
		self.counting = slots <= self.max_counters;
		if self.counting && self.counts.len() < slots {
			self.work.spend(slots - self.counts.len())?;
			self.counts.resize(slots, None);
		}

		// Every move `u -a-> v` inside the component keeps in row `u` the states with an `a`-move into row `v`.
		let inside = |moved: &MoveIn| components.of[moved.source.index()] == component;
		for &target in members {
			for run in moves.runs_into(target) {
				if !run.iter().any(inside) {
					continue;
				}

				self.pre_image(target, run[0].letter)?;
				for moved in run.iter().filter(|&moved| inside(moved)) {
					self.keep_scratch(moved.source)?;
				}
			}
		}

		for slot in self.counted.drain(..) {
			self.counts[slot] = None;
		}

		Ok(())
	}

	/// Henzinger, Henzinger and Kopke's `remove` sets, where each lost source leaves the rows of its row's
	/// predecessors.
	fn draw_consequences(&mut self) -> Result<(), Stop> {
		let (moves, components) = (self.moves, self.components);
		while let Some(lost) = self.pending.pop() {
			let predecessors = moves.sources(lost.row, lost.letter);
			self.work.spend(1 + predecessors.len())?;
			// Refining pair by pair would check each of them against the source.
			self.work.check_pairs(predecessors.len())?;
			for predecessor in predecessors {
				let state = predecessor.source;
				if components.of[state.index()] == self.refining
					&& self.relation.get(state.index(), lost.source.index())
				{
					self.relation.clear(state.index(), lost.source.index());
					self.removed(state, lost.source)?;
				}
			}
		}

		Ok(())
	}

	/// Keeps in row `state` the states of the scratch row, removing the others one by one for the counters.
	fn keep_scratch(&mut self, state: StateId) -> Result<(), Stop> {
		let (row, stride) = (state.index(), self.relation.stride);
		for word in 0..stride {
			let mut gone = self.relation.row_words(row)[word] & !self.scratch[word];
			while gone != 0 {
				let simulator = word * WORD_BITS + gone.trailing_zeros() as usize;
				gone &= gone - 1;
				// The consequences of an earlier removal may have removed it already.
				if !self.relation.get(row, simulator) {
					continue;
				}

				self.relation.clear(row, simulator);
				self.removed(state, StateId::new(simulator))?;
				// Right away, so that only the consequences of one removal wait.
				self.draw_consequences()?;
			}
		}

		self.work.spend(stride)
	}

	/// Records the sources that lose their last move on a letter into row `state` as `simulator` leaves it.
	fn removed(&mut self, state: StateId, simulator: StateId) -> Result<(), Stop> {
		let into = self.moves.pre.of(simulator.index());
		let mut cost = 1 + into.len();
		for moved in into {
			if self.lost_all(moved, state, &mut cost) {
				self.pending.push(Lost {
					row: state,
					source: moved.source,
					letter: moved.letter,
				});
			}
		}

		self.work.spend(cost)
	}

	/// Whether the source of `moved`, whose target just left row `row`, has no move on the letter into it any more.
	fn lost_all(&mut self, moved: &MoveIn, row: StateId, cost: &mut usize) -> bool {
		match moved.group {
			// Its only move on the letter is `moved`.
			None => true,
			Some(group) if self.counting => self.uncount(group, row, moved, cost) == 0,
			// Without counters, the group's targets are scanned.
			Some(_) => {
				for target in self.moves.targets(moved.source, moved.letter) {
					*cost += 1;
					if self.relation.get(row.index(), target.index()) {
						return false;
					}
				}

				true
			}
		}
	}

	/// Counts that `group` lost a target in row `state` and returns how many are left; the first time, by counting.
	fn uncount(&mut self, group: Group, state: StateId, moved: &MoveIn, cost: &mut usize) -> u32 {
		let slot = group.index() * self.width + self.local[state.index()];
		let count = if let Some(count) = self.counts[slot] {
			count - 1
		} else {
			self.counted.push(slot);
			let mut count = 0;
			for target in self.moves.targets(moved.source, moved.letter) {
				*cost += 1;
				count += u32::from(self.relation.get(state.index(), target.index()));
			}

			count
		};

		self.counts[slot] = Some(count);

		count
	}
}

#[cfg(test)]
mod tests {
	use std::time::Instant;

	use rand::rngs::StdRng;
	use rand::{RngExt, SeedableRng};

	use super::{
		BitMatrix, MAX_COUNTERS, MAX_STATES, MAX_WORK, Simulation, Stop, TimedOut, Work,
		maximal_simulation, set_bit,
	};
	use crate::Symbol;
	use crate::ids::{Id, StateId};
	use crate::leaf::{Leaf, Raw};

	/// A leaf with a state per entry of `finals`, trimmed to those that reach a final state.
	fn leaf(arity: usize, finals: &[bool], moves: &[(usize, Vec<Symbol>, usize)]) -> Leaf {
		let mut raw = Raw::new(arity);
		raw.initial.push(StateId::new(0));
		raw.finals = finals.to_vec();
		for (source, tuple, target) in moves {
			raw.push(StateId::new(*source), tuple, StateId::new(*target));
		}

		Leaf::build(&raw, Vec::new())
	}

	/// The greatest simulation by the definition, dropping pairs with an unmatched move until none is left.
	fn reference(leaf: &Leaf) -> BitMatrix {
		let size = leaf.num_states();
		let is_final = |state: usize| leaf.is_final(StateId::new(state));
		let mut relation = BitMatrix::new(size);
		for p in 0..size {
			for q in 0..size {
				if !is_final(p) || is_final(q) {
					set_bit(relation.row_words_mut(p), q);
				}
			}
		}

		let mut changed = true;
		while changed {
			changed = false;
			for p in 0..size {
				for q in 0..size {
					let matched = leaf.moves(StateId::new(p)).all(|(tuple, p_next)| {
						leaf.moves(StateId::new(q)).any(|(other, q_next)| {
							other == tuple && relation.get(p_next.index(), q_next.index())
						})
					});
					if relation.get(p, q) && !matched {
						relation.clear(p, q);
						changed = true;
					}
				}
			}
		}

		relation
	}

	/// Checks the computed relation against [`reference`], with and without counters.
	fn assert_same(leaf: &Leaf) {
		let expected = reference(leaf).words;
		for max_counters in [MAX_COUNTERS, 0] {
			let computed = maximal_simulation(leaf, &mut Work::new(None), max_counters)
				.expect("within the work bound");
			assert_eq!(computed.words, expected, "{leaf:?}");
		}
	}

	/// A random leaf, with or without cycles, sometimes with tuples that move alike.
	fn random_leaf(rng: &mut StdRng) -> Leaf {
		let num_states = rng.random_range(1..=24);
		let arity = rng.random_range(1..=2);
		let num_symbols = rng.random_range(1..=3);
		let acyclic = rng.random_bool(0.3);
		let finals: Vec<bool> = (0..num_states).map(|_| rng.random_bool(0.3)).collect();

		let mut moves = Vec::new();
		for _ in 0..rng.random_range(0..=3 * num_states) {
			let source = rng.random_range(0..num_states);
			let target = if acyclic {
				if source + 1 == num_states {
					continue;
				}

				rng.random_range(source + 1..num_states)
			} else {
				rng.random_range(0..num_states)
			};

			let tuple = (0..arity)
				.map(|_| Symbol::new(rng.random_range(0..num_symbols)))
				.collect();
			moves.push((source, tuple, target));
		}

		if rng.random_bool(0.3)
			&& let Some((_, copied, _)) = moves.first().cloned()
		{
			let alike = vec![Symbol::new(9); arity];
			let copies: Vec<_> = moves
				.iter()
				.filter(|(_, tuple, _)| *tuple == copied)
				.map(|&(source, _, target)| (source, alike.clone(), target))
				.collect();
			moves.extend(copies);
		}

		leaf(arity, &finals, &moves)
	}

	#[test]
	fn random_leaves_get_the_greatest_simulation() {
		let mut rng = StdRng::seed_from_u64(7);
		for _ in 0..3000 {
			assert_same(&random_leaf(&mut rng));
		}
	}

	/// A SAT encoding's falsifier, the words of length `length` that falsify a clause, guessed at its first literal.
	fn falsifier(rng: &mut StdRng, length: usize, clauses: usize) -> Leaf {
		let symbols = [Symbol::new(0), Symbol::new(1)];
		// States `0..=length` choose no clause yet; `length + 1 + i` has a false clause at position `i`.
		let falsified = |position: usize| length + 1 + position;
		let mut num_states = 2 * (length + 1);
		let mut moves = Vec::new();
		for position in 0..length {
			for symbol in symbols {
				moves.push((position, vec![symbol], position + 1));
				moves.push((falsified(position), vec![symbol], falsified(position + 1)));
			}
		}

		// A clause has a state per position after its first literal, up to its last one.
		for _ in 0..clauses {
			let mut positions: Vec<usize> = (0..3).map(|_| rng.random_range(0..length)).collect();
			positions.sort_unstable();
			positions.dedup();
			let values: Vec<Symbol> = positions
				.iter()
				.map(|_| symbols[rng.random_range(0..2)])
				.collect();
			let (first, last) = (positions[0], positions[positions.len() - 1]);
			let base = num_states;
			num_states += last - first;
			let at = |position: usize| {
				if position > last {
					falsified(position)
				} else {
					base + position - first - 1
				}
			};

			moves.push((first, vec![values[0]], at(first + 1)));
			for position in first + 1..=last {
				match positions.iter().position(|&literal| literal == position) {
					Some(index) => {
						moves.push((at(position), vec![values[index]], at(position + 1)));
					}
					None => {
						for symbol in symbols {
							moves.push((at(position), vec![symbol], at(position + 1)));
						}
					}
				}
			}
		}

		let mut finals = vec![false; num_states];
		finals[falsified(length)] = true;

		leaf(1, &finals, &moves)
	}

	#[test]
	fn falsifiers_get_the_greatest_simulation() {
		let mut rng = StdRng::seed_from_u64(11);
		for _ in 0..50 {
			let length = rng.random_range(3..=12);
			let clauses = rng.random_range(1..=4 * length);
			assert_same(&falsifier(&mut rng, length, clauses));
		}
	}

	/// States `0..size`, where `i` moves to `i + 1` on 0 and to `2i` on 1 (modulo `size`), and every state but 0 is
	/// final.
	fn doubling(size: usize) -> Leaf {
		let finals: Vec<bool> = (0..size).map(|state| state != 0).collect();
		let moves: Vec<_> = (0..size)
			.flat_map(|state| {
				[
					(state, vec![Symbol::new(0)], (state + 1) % size),
					(state, vec![Symbol::new(1)], (2 * state) % size),
				]
			})
			.collect();

		leaf(1, &finals, &moves)
	}

	#[test]
	fn one_large_cycle_gets_the_greatest_simulation() {
		for size in [2, 3, 17, 64, 65, 200] {
			assert_same(&doubling(size));
		}
	}

	/// A cycle of `size` states, each move on its own tuple, all final, and a state `size` that moves like state 0.
	fn distinct_cycle(size: usize) -> Leaf {
		let tuple = |state: usize| vec![Symbol::new(u32::try_from(state).expect("a small state"))];
		let mut moves: Vec<_> = (0..size)
			.map(|state| (state, tuple(state), (state + 1) % size))
			.collect();
		moves.push((size, tuple(0), 1));

		leaf(1, &vec![true; size + 1], &moves)
	}

	/// States `0..k` move on tuple `t` to `k + t`, for every `t < k`; state `k + t` moves to `t`. All are final.
	fn bipartite(k: usize) -> Leaf {
		let tuple = |t: usize| vec![Symbol::new(u32::try_from(t).expect("a small tuple"))];
		let mut moves = Vec::new();
		for source in 0..k {
			for t in 0..k {
				moves.push((source, tuple(t), k + t));
			}
		}

		for t in 0..k {
			moves.push((k + t, tuple(k), t));
		}

		leaf(1, &vec![true; 2 * k], &moves)
	}

	#[test]
	fn large_components_start_from_the_states_with_the_same_letters() {
		let size = 12_000;
		let leaf = distinct_cycle(size);
		let relation = maximal_simulation(&leaf, &mut Work::new(None), MAX_COUNTERS)
			.expect("within the work bound");
		assert!(relation.get(size, 0) && relation.get(0, size));
		assert_eq!(relation.count_row(1), 1);

		let k = 300;
		let leaf = bipartite(k);
		let relation = maximal_simulation(&leaf, &mut Work::new(None), MAX_COUNTERS)
			.expect("within the work bound");
		// The first states all move alike, and so do the others.
		for p in 0..2 * k {
			let same_side = |q: usize| (p < k) == (q < k);
			assert!((0..2 * k).all(|q| relation.get(p, q) == same_side(q)));
		}
	}

	/// Hubs 0 and 3, `k` spokes each; only hub 3 reaches a non-final state, so one removal removes `k²` pairs.
	fn cascade(k: usize) -> Leaf {
		let tuple = |symbol: u32| vec![Symbol::new(symbol)];
		let mut moves = Vec::new();
		for (hub, next, end) in [(0, 1, 2), (3, 4, 5)] {
			moves.push((hub, tuple(2), next));
			moves.push((next, tuple(3), end));
			moves.push((next, tuple(1), hub));
			moves.push((end, tuple(1), hub));
			let first = if hub == 0 { 6 } else { 6 + k };
			for spoke in first..first + k {
				moves.push((spoke, tuple(0), hub));
				moves.push((hub, tuple(1), spoke));
			}
		}

		let finals: Vec<bool> = (0..6 + 2 * k).map(|state| state != 5).collect();

		leaf(1, &finals, &moves)
	}

	#[test]
	fn one_removal_that_removes_many_pairs_gets_the_greatest_simulation() {
		for k in [1, 2, 7, 30] {
			assert_same(&cascade(k));
		}
	}

	#[test]
	fn leaves_that_need_too_many_pair_checks_get_the_identity_early() {
		// Too many checks to start the rows, and too many once pairs leave them.
		for leaf in [cascade(16_380), doubling(16_384)] {
			let mut work = Work::new(None);
			assert!(matches!(
				maximal_simulation(&leaf, &mut work, MAX_COUNTERS),
				Err(Stop::Work)
			));
			assert!(work.left > MAX_WORK / 2);
		}
	}

	#[test]
	fn leaves_up_to_max_states_get_their_simulation() {
		let leaf = falsifier(&mut StdRng::seed_from_u64(13), 100, 400);
		assert!(leaf.num_states() > 1 << 14 && leaf.num_states() <= MAX_STATES);
		let simulation = Simulation::compute(&leaf, None).expect("no deadline");
		assert!(!simulation.is_identity());
	}

	#[test]
	fn a_simulation_stops_at_the_deadline() {
		let leaf = doubling(4000);
		assert!(matches!(
			Simulation::compute(&leaf, Some(Instant::now())),
			Err(TimedOut)
		));

		let simulation = Simulation::compute(&leaf, None).expect("no deadline");
		assert!(!simulation.is_identity());
	}
}
