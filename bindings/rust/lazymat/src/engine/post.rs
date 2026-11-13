//! The successors of every node, built from those of its children.

use std::hash::BuildHasher;
use std::ops::Deref;
use std::rc::Rc;
use std::time::Instant;

use hashbrown::HashTable;
use rustc_hash::{FxBuildHasher, FxHashMap};

use super::budget::Limit;
use super::exec::Op;
use super::join::{GroupJoin, LetterIndex};
use super::store::{Pair, Store, Tagged};
use super::{Engine, deep};
use crate::formula::SyncSpec;
use crate::ids::{ExecId, Id, StateId};
use crate::leaf::Leaf;
use crate::symbol::merge;
use crate::{Symbol, Tape};

/// A count or index bounded by the moves of a post.
pub(super) fn index_u32(index: usize) -> u32 {
	u32::try_from(index).expect("a post has fewer than 2^32 moves")
}

#[derive(Clone, Copy, Debug)]
pub(crate) struct Target {
	pub(crate) state: StateId,
	pub(crate) accepting: bool,
}

impl Target {
	pub(super) fn of_leaf(leaf: &Leaf, state: StateId) -> Target {
		Target {
			state,
			accepting: leaf.is_final(state),
		}
	}
}

/// The tuples of a post, stored flat.
#[derive(Debug)]
enum Letters {
	Owned(Vec<Symbol>),
	/// Every tuple of a complement node (or of its identity), in the order of each of its posts.
	Shared(Rc<Table>),
}

impl Deref for Letters {
	type Target = [Symbol];

	fn deref(&self) -> &[Symbol] {
		match self {
			Letters::Owned(letters) => letters,
			Letters::Shared(table) => &table.letters,
		}
	}
}

/// The tuples of a complement node (or of its identity), flat, in odometer order of its digits.
#[derive(Debug)]
pub(crate) struct Table {
	letters: Box<[Symbol]>,
	digits: Box<[Digit]>,
	/// The digit that each tape reads.
	tapes: Box<[usize]>,
	len: usize,
}

/// One digit of a [`Table`], a position in a universe.
#[derive(Clone, Debug)]
struct Digit {
	universe: Rc<[Symbol]>,
	/// How many tuples one step of this digit skips.
	stride: usize,
	/// The value of the first symbol of the universe.
	first: u32,
	/// The position of each symbol by its value minus `first` (the largest `u32` if absent); empty if too sparse.
	positions: Box<[u32]>,
}

impl Digit {
	fn new(universe: &Rc<[Symbol]>, stride: usize) -> Digit {
		let (first, last) = match (universe.first(), universe.last()) {
			(Some(first), Some(last)) => (first.value(), last.value()),
			_ => (0, 0),
		};

		let span = (last - first) as usize + 1;

		let positions = if universe.len() > 1 && span <= 4 * universe.len() {
			let mut positions = vec![u32::MAX; span];
			for (position, letter) in (0_u32..).zip(universe.iter()) {
				positions[(letter.value() - first) as usize] = position;
			}

			positions.into()
		} else {
			Box::default()
		};

		Digit {
			universe: Rc::clone(universe),
			stride,
			first,
			positions,
		}
	}

	fn position(&self, letter: Symbol) -> Option<usize> {
		if self.positions.is_empty() {
			return self.universe.binary_search(&letter).ok();
		}

		let offset = letter.value().checked_sub(self.first)? as usize;
		let position = *self.positions.get(offset)?;

		(position != u32::MAX).then_some(position as usize)
	}
}

impl Table {
	fn new(universes: &[Rc<[Symbol]>], len: usize) -> Table {
		let mut letters = Vec::with_capacity(len * universes.len());
		let mut positions = vec![0; universes.len()];
		for _ in 0..len {
			letters.extend(
				positions
					.iter()
					.zip(universes)
					.map(|(&position, universe)| universe[position]),
			);
			for (position, universe) in positions.iter_mut().zip(universes).rev() {
				*position += 1;
				if *position < universe.len() {
					break;
				}

				*position = 0;
			}
		}

		let mut strides = vec![1; universes.len()];
		for tape in (1..universes.len()).rev() {
			strides[tape - 1] = strides[tape] * universes[tape].len();
		}

		Table {
			letters: letters.into(),
			digits: universes
				.iter()
				.zip(strides)
				.map(|(universe, stride)| Digit::new(universe, stride))
				.collect(),
			tapes: (0..universes.len()).collect(),
			len,
		}
	}

	/// The table of the identity of a post over this table, with each letter on both tapes.
	fn diagonal(&self) -> Table {
		debug_assert_eq!(self.tapes.len(), 1);

		Table {
			letters: self
				.letters
				.iter()
				.flat_map(|&letter| [letter, letter])
				.collect(),
			digits: self.digits.clone(),
			tapes: [0, 0].into(),
			len: self.len,
		}
	}

	/// The index of a concrete tuple of a complement's table, if each letter is in its universe.
	fn index(&self, tuple: &[Symbol]) -> Option<usize> {
		debug_assert_eq!(tuple.len(), self.digits.len());

		tuple
			.iter()
			.zip(&self.digits)
			.try_fold(0, |index, (&letter, digit)| {
				Some(index + digit.position(letter)? * digit.stride)
			})
	}

	/// Visits, in increasing order, the tuples whose letters on `tapes` merge with `letters`.
	pub(super) fn matching(
		&self,
		tapes: &[usize],
		letters: impl Iterator<Item = Symbol>,
		odometer: &mut Odometer,
		mut visit: impl FnMut(usize),
	) {
		const FREE: usize = usize::MAX;

		if self.len == 0 {
			return;
		}

		odometer.positions.clear();
		odometer.positions.resize(self.digits.len(), FREE);
		for (&tape, letter) in tapes.iter().zip(letters) {
			if letter == Symbol::ANY {
				continue;
			}

			let digit = self.tapes[tape];
			let Some(position) = self.digits[digit].position(letter) else {
				return;
			};

			if odometer.positions[digit] != FREE && odometer.positions[digit] != position {
				return;
			}

			odometer.positions[digit] = position;
		}

		// The free digits after the last fixed one vary fastest, so their tuples are consecutive.
		let fixed_end = odometer
			.positions
			.iter()
			.rposition(|&position| position != FREE)
			.map_or(0, |digit| digit + 1);
		let run = match fixed_end {
			0 => self.len,
			end => self.digits[end - 1].stride,
		};

		let mut start: usize = (0..fixed_end)
			.filter(|&digit| odometer.positions[digit] != FREE)
			.map(|digit| odometer.positions[digit] * self.digits[digit].stride)
			.sum();

		// The other free digits count in `positions`, from 0.
		odometer.free.clear();
		odometer
			.free
			.extend((0..fixed_end).filter(|&digit| odometer.positions[digit] == FREE));
		for &digit in &odometer.free {
			odometer.positions[digit] = 0;
		}

		loop {
			(start..start + run).for_each(&mut visit);

			// The free digits step like an odometer, and the tuples end when they all wrap.
			let mut wrapped = true;
			for &digit in odometer.free.iter().rev() {
				odometer.positions[digit] += 1;
				start += self.digits[digit].stride;
				if odometer.positions[digit] < self.digits[digit].universe.len() {
					wrapped = false;

					break;
				}

				start -= odometer.positions[digit] * self.digits[digit].stride;
				odometer.positions[digit] = 0;
			}

			if wrapped {
				return;
			}
		}
	}
}

/// Scratch space of [`matching`](Table::matching).
#[derive(Debug, Default)]
pub(super) struct Odometer {
	/// The position of each digit, or the largest `usize` while it is free.
	positions: Vec<usize>,
	/// The free digits before the last fixed one.
	free: Vec<usize>,
}

/// The moves from one macrostate, with the tuples stored flat, `arity` letters each, and their targets.
#[derive(Debug)]
pub(crate) struct Post {
	arity: usize,
	letters: Letters,
	/// A target's state and whether it accepts, packed by [`pack`](StateId::pack).
	targets: Vec<u32>,
}

impl Post {
	fn new(arity: usize) -> Post {
		Post {
			arity,
			letters: Letters::Owned(Vec::new()),
			targets: Vec::new(),
		}
	}

	pub(super) fn with_capacity(arity: usize, moves: usize) -> Post {
		Post {
			arity,
			letters: Letters::Owned(Vec::with_capacity(moves * arity)),
			targets: Vec::with_capacity(moves),
		}
	}

	/// A post whose tuples are `table`; its targets follow with [`push_target`](Post::push_target).
	pub(super) fn with_table(arity: usize, table: Rc<Table>) -> Post {
		Post {
			arity,
			targets: Vec::with_capacity(table.len),
			letters: Letters::Shared(table),
		}
	}

	pub(crate) fn len(&self) -> usize {
		self.targets.len()
	}

	/// The bytes it takes, without a shared table.
	fn bytes(&self) -> usize {
		let letters = match &self.letters {
			Letters::Owned(letters) => letters.capacity() * size_of::<Symbol>(),
			Letters::Shared(_) => 0,
		};

		size_of::<Post>() + letters + self.targets.capacity() * size_of::<u32>()
	}

	fn shrink_to_fit(&mut self) {
		if let Letters::Owned(letters) = &mut self.letters {
			letters.shrink_to_fit();
		}

		self.targets.shrink_to_fit();
	}

	pub(super) fn push(&mut self, tuple: &[Symbol], target: Target) {
		debug_assert_eq!(tuple.len(), self.arity);
		let Letters::Owned(letters) = &mut self.letters else {
			unreachable!("a post with a shared table gets only targets")
		};

		letters.extend_from_slice(tuple);
		self.push_target(target);
	}

	/// The target of the next tuple of a shared table.
	pub(super) fn push_target(&mut self, target: Target) {
		self.targets.push(target.state.pack(target.accepting));
	}

	pub(super) fn state(&self, index: usize) -> StateId {
		StateId::unpack(self.targets[index])
	}

	pub(super) fn target(&self, index: usize) -> Target {
		Target {
			state: self.state(index),
			accepting: StateId::unpack_accepting(self.targets[index]),
		}
	}

	/// Pushes a move to `pair` unless the post has it; `seen` holds the index and pair of each move.
	fn push_new(
		&mut self,
		seen: &mut HashTable<(u32, Pair)>,
		store: &mut Store,
		tuple: &[Symbol],
		pair: Pair,
		accepting: bool,
	) {
		let hash = FxBuildHasher.hash_one((tuple, pair));
		let same =
			|&(index, other): &(u32, Pair)| other == pair && self.tuple(index as usize) == tuple;
		if seen.find(hash, same).is_some() {
			return;
		}

		let rehash = |&(index, other): &(u32, Pair)| {
			FxBuildHasher.hash_one((self.tuple(index as usize), other))
		};
		let index = index_u32(self.len());
		seen.insert_unique(hash, (index, pair), rehash);

		let state = store.intern_pair(pair);
		self.push(tuple, Target { state, accepting });
	}

	/// The arity and the tuples stored flat.
	pub(super) fn flat(&self) -> (usize, &[Symbol]) {
		(self.arity, &self.letters)
	}

	pub(super) fn tuple(&self, index: usize) -> &[Symbol] {
		&self.letters[index * self.arity..(index + 1) * self.arity]
	}

	pub(crate) fn iter(&self) -> impl Iterator<Item = (&[Symbol], Target)> {
		(0..self.len()).map(|index| (self.tuple(index), self.target(index)))
	}

	/// Sorts the moves by tuple and target, without repeats.
	fn dedup(&mut self) {
		let key = |index: usize| (self.tuple(index), self.state(index));
		let mut order: Vec<usize> = (0..self.len()).collect();
		order.sort_unstable_by_key(|&index| key(index));
		order.dedup_by_key(|index| key(*index));

		let mut unique = Post::with_capacity(self.arity, order.len());
		for index in order {
			unique.push(self.tuple(index), self.target(index));
		}

		*self = unique;
	}
}

/// The most moves that the table of new moves of a join is sized for up front.
const SEEN_CAPACITY: usize = 1 << 16;

/// The moves of a post grouped by their letters on some tapes, to join without trying every pair.
#[derive(Debug)]
struct ByKey {
	/// The keys, sorted; the moves of `keys[i]` are `moves[starts[i]..starts[i + 1]]`, in increasing order.
	keys: Vec<u64>,
	starts: Vec<u32>,
	moves: Vec<u32>,
	/// The moves that read the wildcard on a key tape.
	any: Vec<u32>,
	len: usize,
}

impl ByKey {
	fn new(post: &Post, tapes: &[usize]) -> ByKey {
		let mut keyed: Vec<(u64, u32)> = Vec::with_capacity(post.len());
		let mut any = Vec::new();
		for (index, (tuple, _)) in (0_u32..).zip(post.iter()) {
			match key(tapes.iter().map(|&tape| tuple[tape])) {
				Some(key) => keyed.push((key, index)),
				None => any.push(index),
			}
		}

		keyed.sort_unstable();
		let mut keys = Vec::new();
		let mut starts = Vec::new();
		for (at, &(key, _)) in (0_u32..).zip(&keyed) {
			if keys.last() != Some(&key) {
				keys.push(key);
				starts.push(at);
			}
		}

		starts.push(index_u32(keyed.len()));

		ByKey {
			keys,
			starts,
			moves: keyed.into_iter().map(|(_, index)| index).collect(),
			any,
			len: post.len(),
		}
	}

	fn candidates(&self, letters: impl Iterator<Item = Symbol>, mut visit: impl FnMut(usize)) {
		let Some(key) = key(letters) else {
			(0..self.len).for_each(visit);

			return;
		};

		if let Ok(at) = self.keys.binary_search(&key) {
			let range = self.starts[at] as usize..self.starts[at + 1] as usize;
			for &index in &self.moves[range] {
				visit(index as usize);
			}
		}

		for &index in &self.any {
			visit(index as usize);
		}
	}
}

/// `letters` folded into one number, `None` if one is the wildcard; past two letters, keys may collide.
fn key(mut letters: impl Iterator<Item = Symbol>) -> Option<u64> {
	letters.try_fold(0_u64, |key, letter| {
		(letter != Symbol::ANY).then(|| key.rotate_left(32) ^ u64::from(letter.value()))
	})
}

#[derive(Clone, Copy)]
enum Product<'s> {
	Intersection,
	Sync(&'s SyncSpec),
	Diagonal,
}

impl Product<'_> {
	/// Writes the tuple that `lhs` and `rhs` read together into `out`, if they agree.
	fn merge(self, lhs: &[Symbol], rhs: &[Symbol], out: &mut Vec<Symbol>) -> bool {
		out.clear();
		match self {
			Product::Intersection => {
				for (&a, &b) in lhs.iter().zip(rhs) {
					let Some(letter) = merge(a, b) else {
						return false;
					};

					out.push(letter);
				}
			}
			Product::Sync(spec) => {
				if !spec
					.pairs
					.iter()
					.all(|pair| merge(lhs[pair.lhs], rhs[pair.rhs]).is_some())
				{
					return false;
				}

				for tape in &spec.layout {
					let letter = match (tape.source, tape.peer) {
						(Tape::Lhs(i), None) => lhs[i],
						(Tape::Rhs(j), None) => rhs[j],
						(Tape::Lhs(i), Some(j)) | (Tape::Rhs(j), Some(i)) => {
							merge(lhs[i], rhs[j]).unwrap_or(Symbol::ANY)
						}
					};

					out.push(letter);
				}
			}
			Product::Diagonal => {
				let Some(letter) = merge(lhs[0], rhs[0]).and_then(|letter| merge(letter, rhs[1]))
				else {
					return false;
				};

				out.push(letter);
			}
		}

		true
	}
}

/// How a product finds the moves of its right operand that may merge with a left move.
enum Candidates<'a> {
	/// The right operand's shared table, read on `tapes`; it needs no index.
	Table {
		table: &'a Table,
		tapes: &'a [usize],
		scratch: &'a mut Odometer,
	},
	ByKey(ByKey),
	/// Every move of the right operand.
	All(usize),
}

impl<'a> Candidates<'a> {
	/// The candidates of `right` on `tapes`, indexed by key if `indexed` and it has no table.
	fn new(
		right: &'a Post,
		tapes: &'a [usize],
		indexed: bool,
		scratch: &'a mut Odometer,
	) -> Candidates<'a> {
		match &right.letters {
			Letters::Shared(table) => Candidates::Table {
				table,
				tapes,
				scratch,
			},
			Letters::Owned(_) if indexed => Candidates::ByKey(ByKey::new(right, tapes)),
			Letters::Owned(_) => Candidates::All(right.len()),
		}
	}

	/// Visits the moves that may merge with `letters`, and maybe others; the caller merges them.
	fn visit(&mut self, letters: impl Iterator<Item = Symbol>, visit: impl FnMut(usize)) {
		match self {
			Candidates::Table {
				table,
				tapes,
				scratch,
			} => table.matching(tapes, letters, scratch, visit),
			Candidates::ByKey(by_key) => by_key.candidates(letters, visit),
			Candidates::All(len) => (0..*len).for_each(visit),
		}
	}
}

/// Joins with an index only when trying every pair would cost more than building it.
const INDEX_THRESHOLD: usize = 8;

/// Buffers of the pair join, kept between posts.
#[derive(Debug, Default)]
pub(crate) struct JoinBuffers {
	tuple: Vec<Symbol>,
	odometer: Odometer,
}

impl Engine<'_> {
	/// The moves from `state` of `node`, or the limit that the query passed while computing them.
	pub(crate) fn post(&mut self, node: ExecId, state: StateId) -> Result<Rc<Post>, Limit> {
		if let Op::Transparent(child) = self.exec.nodes[node].op {
			return self.post(child, state);
		}

		let index = state.index();
		if let Some(Some(post)) = self.posts[node].get(index) {
			if let Some(budget) = &mut self.cache_budget {
				budget.touch(node.index(), state);
			}

			return Ok(Rc::clone(post));
		}

		let (post, nanos) = self.timed(|engine| deep(|| engine.compute_post(node, state)));
		// Past a limit the post may be partial, so it is neither kept nor used.
		let mut post = post?;
		self.stopped()?;
		if !self.cached[node] {
			return Ok(Rc::new(post));
		}

		post.shrink_to_fit();
		let post = Rc::new(post);
		let slots = &mut self.posts[node];
		if slots.len() <= index {
			slots.resize(index + 1, None);
		}

		slots[index] = Some(Rc::clone(&post));
		if let Some(budget) = &mut self.cache_budget {
			let dropped = budget.admit(node.index(), state, nanos, post.bytes());
			self.evict(dropped);
		}

		Ok(post)
	}

	/// Drops the posts and the rows of dense complements (keyed after the nodes) that the cache budget evicted.
	pub(super) fn evict(&mut self, dropped: Vec<(usize, StateId)>) {
		let num_nodes = self.exec.nodes.len();
		for (key, state) in dropped {
			if key < num_nodes {
				self.posts[ExecId::new(key)][state.index()] = None;
			} else {
				self.evict_row(ExecId::new(key - num_nodes), state);
			}
		}
	}

	/// Runs `work` and, under a cache budget, times it without the posts it computed, which are cached on their own.
	pub(super) fn timed<T>(&mut self, work: impl FnOnce(&mut Self) -> T) -> (T, u64) {
		if self.cache_budget.is_none() {
			return (work(self), 0);
		}

		let start = Instant::now();
		let outer = self.nested_nanos;
		self.nested_nanos = 0;
		let result = work(self);
		let total = u64::try_from(start.elapsed().as_nanos()).unwrap_or(u64::MAX);
		let own = total.saturating_sub(self.nested_nanos);
		self.nested_nanos = outer.saturating_add(total);

		(result, own)
	}

	fn compute_post(&mut self, node: ExecId, state: StateId) -> Result<Post, Limit> {
		match self.exec.nodes[node].op.clone() {
			Op::Leaf(leaf) => Ok(self.leaf_post(node, &leaf, state)),
			Op::Union(branches) => self.union_post(node, &branches, state),
			Op::Intersection(a, b) => self.product_post(node, a, b, state, Product::Intersection),
			Op::Sync(a, b, spec) => self.product_post(node, a, b, state, Product::Sync(&spec)),
			Op::Diagonal(u, x) => self.product_post(node, u, x, state, Product::Diagonal),
			Op::Complement(child) => self.complement_post(node, child, state),
			Op::Identity(child) => self.identity_post(node, child, state),
			Op::Project(child, tapes) => self.project_post(node, child, &tapes, state),
			Op::Explicit { .. } | Op::Transparent(_) => {
				unreachable!("explicit nodes are replaced before the search")
			}
		}
	}

	/// The leaf's moves, except those that read the wildcard on a tape with an empty universe.
	fn leaf_post(&self, node: ExecId, leaf: &Leaf, state: StateId) -> Post {
		let arity = self.exec.nodes[node].arity;
		let dead: Vec<usize> = (0..arity)
			.filter(|&tape| {
				leaf.tape_has_any[tape] && self.alphabets.universe(node, tape).is_empty()
			})
			.collect();

		let mut post = Post::with_capacity(arity, leaf.moves(state).len());
		for (tuple, target) in leaf.moves(state) {
			if dead.iter().any(|&tape| tuple[tape] == Symbol::ANY) {
				continue;
			}

			post.push(tuple, Target::of_leaf(leaf, target));
		}

		post
	}

	fn union_post(
		&mut self,
		node: ExecId,
		branches: &[ExecId],
		state: StateId,
	) -> Result<Post, Limit> {
		let Tagged { branch, state } = self.stores[node].tagged(state);
		let child = self.post(branches[branch as usize], state)?;
		let start = self.stores[node].len();
		let mut post = Post::with_capacity(self.exec.nodes[node].arity, child.len());
		for (tuple, target) in child.iter() {
			post.push(tuple, self.tagged_target(node, branch, target));
		}

		self.budget.add(self.stores[node].len() - start);

		Ok(post)
	}

	pub(super) fn tagged_target(&mut self, node: ExecId, branch: u32, target: Target) -> Target {
		let state = self.stores[node].intern_tagged(Tagged {
			branch,
			state: target.state,
		});

		Target { state, ..target }
	}

	/// The child's moves with each letter on both tapes; a wildcard stands for every letter of the universe.
	fn identity_post(
		&mut self,
		node: ExecId,
		child: ExecId,
		state: StateId,
	) -> Result<Post, Limit> {
		let child_post = self.post(child, state)?;
		if let Letters::Shared(table) = &child_post.letters {
			let diagonal = self.tables[node].get_or_insert_with(|| Rc::new(table.diagonal()));

			return Ok(Post {
				arity: 2,
				letters: Letters::Shared(Rc::clone(diagonal)),
				targets: child_post.targets.clone(),
			});
		}

		let universe = self.alphabets.universe(node, 0);
		let mut post = Post::with_capacity(2, child_post.len());
		for (tuple, target) in child_post.iter() {
			if tuple[0] == Symbol::ANY {
				for &letter in universe.iter() {
					post.push(&[letter, letter], target);
				}
			} else {
				post.push(&[tuple[0], tuple[0]], target);
			}
		}

		Ok(post)
	}

	fn project_post(
		&mut self,
		node: ExecId,
		child: ExecId,
		tapes: &[usize],
		state: StateId,
	) -> Result<Post, Limit> {
		let arity = self.exec.nodes[node].arity;
		let child_post = self.post(child, state)?;
		let mut post = Post::with_capacity(arity, child_post.len());
		let mut projected = Vec::with_capacity(arity);
		for (tuple, target) in child_post.iter() {
			projected.clear();
			projected.extend(tapes.iter().map(|&tape| tuple[tape]));
			post.push(&projected, target);
		}

		post.dedup();

		Ok(post)
	}

	fn product_post(
		&mut self,
		node: ExecId,
		lhs: ExecId,
		rhs: ExecId,
		state: StateId,
		product: Product<'_>,
	) -> Result<Post, Limit> {
		let pair = self.stores[node].pair(state);
		let (left, right) = (self.post(lhs, pair.lhs)?, self.post(rhs, pair.rhs)?);
		let plan = &self.plans[node];
		let indexed = !plan.keys.is_empty() && left.len() > 1 && right.len() > INDEX_THRESHOLD;

		if let Product::Sync(spec) = product
			&& let Some(post) = self.group_join(node, lhs, &left, &right, spec, indexed)
		{
			return Ok(post);
		}

		Ok(self.pair_join(node, &left, &right, product, indexed))
	}

	/// The post by the group join, where the plan allows it and it looks cheaper than the pair join.
	fn group_join(
		&mut self,
		node: ExecId,
		lhs: ExecId,
		left: &Post,
		right: &Rc<Post>,
		spec: &SyncSpec,
		indexed: bool,
	) -> Option<Post> {
		let plan = self.plans[node].group.as_ref()?;
		let index = plan
			.index
			.get_or_init(|| LetterIndex::new(self.alphabets.universe(lhs, plan.hidden.lhs)))
			.as_ref()?;
		let join = GroupJoin {
			left,
			right,
			spec,
			plan,
			index,
			indexed,
		};

		join.run(
			&mut self.stores[node],
			&mut self.budget,
			&mut self.join_scratch,
		)
	}

	/// Merges each left move with the right moves that may merge with it.
	fn pair_join(
		&mut self,
		node: ExecId,
		left: &Post,
		right: &Post,
		product: Product<'_>,
		indexed: bool,
	) -> Post {
		let plan = &self.plans[node];
		// Sized for one move per left move, as a join that drops tapes has at least as many.
		let mut seen = plan
			.dedup
			.then(|| HashTable::with_capacity(left.len().min(SEEN_CAPACITY)));
		let mut post = Post::new(self.exec.nodes[node].arity);
		let mut candidates = Candidates::new(
			right,
			&plan.right_tapes,
			indexed,
			&mut self.join_buffers.odometer,
		);
		let store = &mut self.stores[node];
		let start = store.len();

		for (left_tuple, left_target) in left.iter() {
			self.budget.step(store.len() - start);
			if self.budget.passed() {
				break;
			}

			let letters = plan.keys.iter().map(|pair| left_tuple[pair.lhs]);
			candidates.visit(letters, |index| {
				if !product.merge(left_tuple, right.tuple(index), &mut self.join_buffers.tuple) {
					return;
				}

				let right_target = right.target(index);
				let pair = Pair {
					lhs: left_target.state,
					rhs: right_target.state,
				};
				let accepting = left_target.accepting && right_target.accepting;
				if let Some(seen) = &mut seen {
					post.push_new(seen, store, &self.join_buffers.tuple, pair, accepting);
				} else {
					let state = store.intern_pair(pair);
					post.push(&self.join_buffers.tuple, Target { state, accepting });
				}
			});
		}

		self.budget.add(store.len() - start);

		post
	}

	/// One move per tuple of the tapes' alphabets, to the minimized set of the members' successors.
	fn complement_post(
		&mut self,
		node: ExecId,
		child: ExecId,
		state: StateId,
	) -> Result<Post, Limit> {
		let arity = self.exec.nodes[node].arity;
		let members = self.stores[node].set(state).to_vec();
		let universes: Vec<Rc<[Symbol]>> = (0..arity)
			.map(|tape| Rc::clone(self.alphabets.universe(node, tape)))
			.collect();
		let num_tuples = self
			.alphabets
			.tuple_count(node)
			.expect("the tuple count is checked when the query starts");
		if let Some(post) = self.dense_post(node, child, &members, &universes, num_tuples)? {
			return Ok(post);
		}

		let posts = members
			.iter()
			.map(|&member| self.post(child, member))
			.collect::<Result<Vec<Rc<Post>>, Limit>>()?;
		let table = self.tuple_table(node, &universes, num_tuples);
		let mut exact: FxHashMap<usize, Vec<Target>> = FxHashMap::default();
		let mut wildcards: Vec<(&[Symbol], Target)> = Vec::new();
		for (tuple, target) in posts.iter().flat_map(|post| post.iter()) {
			if tuple.contains(&Symbol::EPSILON) {
				continue;
			}

			if tuple.contains(&Symbol::ANY) {
				wildcards.push((tuple, target));
			} else if let Some(index) = table.index(tuple) {
				exact.entry(index).or_default().push(target);
			}
		}

		let mut post = Post::with_table(arity, Rc::clone(&table));
		let mut successors = Vec::new();
		let start = self.stores[node].len();
		for index in 0..num_tuples {
			self.budget.step(self.stores[node].len() - start);
			if self.budget.passed() {
				break;
			}

			let tuple = &table.letters[index * arity..(index + 1) * arity];
			let wildcard_targets = wildcards
				.iter()
				.filter(|(letters, _)| {
					letters
						.iter()
						.zip(tuple)
						.all(|(&letter, &concrete)| letter == Symbol::ANY || letter == concrete)
				})
				.map(|(_, target)| target);
			let targets = exact
				.get(&index)
				.into_iter()
				.flatten()
				.chain(wildcard_targets);

			successors.clear();
			let mut accepting = true;
			for target in targets {
				successors.push(target.state);
				accepting &= !target.accepting;
			}

			self.subsumption
				.minimize(&self.exec, &self.stores, child, &mut successors);
			let state = self.stores[node].intern_set(&successors);
			post.push_target(Target { state, accepting });
		}

		self.budget.add(self.stores[node].len() - start);

		Ok(post)
	}

	/// Every tuple of the universes of the complement `node`, in odometer order (last tape fastest), built once.
	pub(super) fn tuple_table(
		&mut self,
		node: ExecId,
		universes: &[Rc<[Symbol]>],
		num_tuples: usize,
	) -> Rc<Table> {
		Rc::clone(
			self.tables[node].get_or_insert_with(|| Rc::new(Table::new(universes, num_tuples))),
		)
	}
}
