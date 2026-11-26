//! The successors of every node, built from those of its children.

use std::hash::BuildHasher;
use std::rc::Rc;
use std::time::Instant;

use hashbrown::HashTable;
use rustc_hash::FxBuildHasher;

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

/// One tape of a move. A set denotes every concrete symbol in it, sorted without repetitions.
#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub(crate) enum TapeLabel {
	Simple(Symbol),
	Set(Rc<[Symbol]>),
}

impl TapeLabel {
	fn set(mut symbols: Vec<Symbol>) -> Option<TapeLabel> {
		symbols.sort_unstable();
		symbols.dedup();
		debug_assert!(symbols.iter().all(|symbol| symbol.is_concrete()));

		(!symbols.is_empty()).then(|| TapeLabel::Set(symbols.into()))
	}

	pub(super) fn symbol(&self) -> Symbol {
		match self {
			TapeLabel::Simple(symbol) => *symbol,
			TapeLabel::Set(_) => panic!("a concrete-label join received a set"),
		}
	}

	/// The concrete `position`-th symbol, where padding and the wildcard each have at most one expansion.
	pub(super) fn symbol_at(&self, universe: &[Symbol], position: usize) -> Option<Symbol> {
		match self {
			TapeLabel::Simple(Symbol::ANY) => universe.get(position).copied(),
			TapeLabel::Simple(symbol) => (position == 0).then_some(*symbol),
			TapeLabel::Set(symbols) => symbols.get(position).copied(),
		}
	}

	fn intersects(&self, other: &TapeLabel) -> bool {
		match (self, other) {
			(TapeLabel::Simple(a), TapeLabel::Simple(b)) => merge(*a, *b).is_some(),
			(TapeLabel::Simple(Symbol::ANY), TapeLabel::Set(_))
			| (TapeLabel::Set(_), TapeLabel::Simple(Symbol::ANY)) => true,
			(TapeLabel::Simple(symbol), TapeLabel::Set(symbols))
			| (TapeLabel::Set(symbols), TapeLabel::Simple(symbol)) => {
				symbol.is_concrete() && symbols.binary_search(symbol).is_ok()
			}
			(TapeLabel::Set(a), TapeLabel::Set(b)) => {
				let (short, long) = if a.len() <= b.len() { (a, b) } else { (b, a) };
				short
					.iter()
					.any(|symbol| long.binary_search(symbol).is_ok())
			}
		}
	}

	fn intersection(&self, other: &TapeLabel) -> Option<TapeLabel> {
		match (self, other) {
			(TapeLabel::Simple(a), TapeLabel::Simple(b)) => merge(*a, *b).map(TapeLabel::Simple),
			(TapeLabel::Simple(Symbol::ANY), TapeLabel::Set(symbols))
			| (TapeLabel::Set(symbols), TapeLabel::Simple(Symbol::ANY)) => {
				Some(TapeLabel::Set(Rc::clone(symbols)))
			}
			(TapeLabel::Simple(symbol), TapeLabel::Set(symbols))
			| (TapeLabel::Set(symbols), TapeLabel::Simple(symbol)) => (symbol.is_concrete()
				&& symbols.binary_search(symbol).is_ok())
			.then_some(TapeLabel::Simple(*symbol)),
			(TapeLabel::Set(a), TapeLabel::Set(b)) => {
				let mut symbols = Vec::new();
				let (mut i, mut j) = (0, 0);
				while i < a.len() && j < b.len() {
					match a[i].cmp(&b[j]) {
						std::cmp::Ordering::Less => i += 1,
						std::cmp::Ordering::Greater => j += 1,
						std::cmp::Ordering::Equal => {
							symbols.push(a[i]);
							i += 1;
							j += 1;
						}
					}
				}

				TapeLabel::set(symbols)
			}
		}
	}

	/// Splits `part`, which this label overlaps, into the covered and uncovered concrete symbols.
	fn split(&self, part: &[Symbol]) -> (Vec<Symbol>, Vec<Symbol>) {
		match self {
			TapeLabel::Simple(Symbol::ANY) => (part.to_vec(), Vec::new()),
			TapeLabel::Simple(Symbol::EPSILON) => (Vec::new(), part.to_vec()),
			TapeLabel::Simple(symbol) => {
				let mut covered = Vec::new();
				let mut uncovered = Vec::new();
				for &letter in part {
					if letter == *symbol {
						covered.push(letter);
					} else {
						uncovered.push(letter);
					}
				}

				(covered, uncovered)
			}
			TapeLabel::Set(symbols) => {
				let mut covered = Vec::new();
				let mut uncovered = Vec::new();
				let mut next = 0;
				for &letter in part {
					while next < symbols.len() && symbols[next] < letter {
						next += 1;
					}
					if next < symbols.len() && symbols[next] == letter {
						covered.push(letter);
					} else {
						uncovered.push(letter);
					}
				}

				(covered, uncovered)
			}
		}
	}
}

/// Calls `visit` for each concrete tuple of `labels`, without collecting their Cartesian product.
pub(super) fn for_each_concrete(
	universes: &[Rc<[Symbol]>],
	labels: &[TapeLabel],
	mut visit: impl FnMut(&[Symbol]) -> Result<(), Limit>,
) -> Result<(), Limit> {
	debug_assert_eq!(universes.len(), labels.len());
	let mut positions = vec![0; labels.len()];
	let mut tuple: Vec<Symbol> = match labels
		.iter()
		.zip(universes)
		.map(|(label, universe)| label.symbol_at(universe, 0))
		.collect()
	{
		Some(tuple) => tuple,
		None => return Ok(()),
	};

	loop {
		visit(&tuple)?;
		let mut wrapped = true;
		for tape in (0..labels.len()).rev() {
			positions[tape] += 1;
			if let Some(symbol) = labels[tape].symbol_at(&universes[tape], positions[tape]) {
				tuple[tape] = symbol;
				wrapped = false;
				break;
			}

			positions[tape] = 0;
			tuple[tape] = labels[tape]
				.symbol_at(&universes[tape], 0)
				.expect("the first concrete tuple exists");
		}

		if wrapped {
			return Ok(());
		}
	}
}

/// One child move that overlaps the concrete region being partitioned.
#[derive(Clone, Copy)]
struct ActiveMove<'a> {
	tuple: &'a [TapeLabel],
	target: Target,
}

/// The moves from one macrostate, with symbolic Cartesian labels of `arity` tape labels each.
#[derive(Debug)]
pub(crate) struct Post {
	arity: usize,
	labels: Vec<TapeLabel>,
	/// A target's state and whether it accepts, packed by [`pack`](StateId::pack).
	targets: Vec<u32>,
}

impl Post {
	pub(super) fn new(arity: usize) -> Post {
		Post {
			arity,
			labels: Vec::new(),
			targets: Vec::new(),
		}
	}

	pub(super) fn with_capacity(arity: usize, moves: usize) -> Post {
		Post {
			arity,
			labels: Vec::with_capacity(moves * arity),
			targets: Vec::with_capacity(moves),
		}
	}

	pub(crate) fn len(&self) -> usize {
		self.targets.len()
	}

	pub(super) fn wholly_simple(&self) -> bool {
		self.labels
			.iter()
			.all(|label| matches!(label, TapeLabel::Simple(_)))
	}

	/// The same moves, each target accepting as `accepting` decides, or the limit that it passed.
	pub(super) fn reflagged(
		&self,
		mut accepting: impl FnMut(Target) -> Result<bool, Limit>,
	) -> Result<Post, Limit> {
		let targets = (0..self.len())
			.map(|index| {
				let target = self.target(index);

				Ok(target.state.pack(accepting(target)?))
			})
			.collect::<Result<Vec<u32>, Limit>>()?;

		Ok(Post {
			arity: self.arity,
			labels: self.labels.clone(),
			targets,
		})
	}

	/// Distinct split symbol arrays are charged once by the cache budget.
	pub(super) fn bytes(&self) -> usize {
		size_of::<Post>()
			+ self.labels.capacity() * size_of::<TapeLabel>()
			+ self.targets.capacity() * size_of::<u32>()
	}

	fn shrink_to_fit(&mut self) {
		self.labels.shrink_to_fit();
		self.targets.shrink_to_fit();
	}

	pub(super) fn push(&mut self, tuple: &[Symbol], target: Target) {
		debug_assert_eq!(tuple.len(), self.arity);
		self.labels
			.extend(tuple.iter().copied().map(TapeLabel::Simple));
		self.targets.push(target.state.pack(target.accepting));
	}

	pub(super) fn push_labels(&mut self, tuple: &[TapeLabel], target: Target) {
		debug_assert_eq!(tuple.len(), self.arity);
		self.labels.extend_from_slice(tuple);
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
		tuple: &[TapeLabel],
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
		self.push_labels(tuple, Target { state, accepting });
	}

	pub(super) fn symbol(&self, index: usize, tape: usize) -> Symbol {
		self.tuple(index)[tape].symbol()
	}

	pub(super) fn tuple(&self, index: usize) -> &[TapeLabel] {
		&self.labels[index * self.arity..(index + 1) * self.arity]
	}

	pub(crate) fn iter(&self) -> impl Iterator<Item = (&[TapeLabel], Target)> {
		(0..self.len()).map(|index| (self.tuple(index), self.target(index)))
	}

	/// Sorts the moves by exact structural label and target, without repeats.
	pub(super) fn dedup(&mut self) {
		let key = |index: usize| (self.tuple(index), self.state(index));
		let mut order: Vec<usize> = (0..self.len()).collect();
		order.sort_unstable_by_key(|&index| key(index));
		order.dedup_by_key(|index| key(*index));

		let mut unique = Post::with_capacity(self.arity, order.len());
		for index in order {
			unique.push_labels(self.tuple(index), self.target(index));
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
			match key(tapes.iter().map(|&tape| tuple[tape].symbol())) {
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
	/// Writes the symbolic tuple that `lhs` and `rhs` read together into `out`, if they overlap.
	fn merge(self, lhs: &[TapeLabel], rhs: &[TapeLabel], out: &mut Vec<TapeLabel>) -> bool {
		out.clear();
		match self {
			Product::Intersection => {
				for (a, b) in lhs.iter().zip(rhs) {
					let Some(label) = a.intersection(b) else {
						return false;
					};

					out.push(label);
				}
			}
			Product::Sync(spec) => {
				if !spec
					.pairs
					.iter()
					.all(|pair| lhs[pair.lhs].intersects(&rhs[pair.rhs]))
				{
					return false;
				}

				for tape in &spec.layout {
					let label = match (tape.source, tape.peer) {
						(Tape::Lhs(i), None) => lhs[i].clone(),
						(Tape::Rhs(j), None) => rhs[j].clone(),
						(Tape::Lhs(i), Some(j)) | (Tape::Rhs(j), Some(i)) => lhs[i]
							.intersection(&rhs[j])
							.expect("every synchronized pair overlaps"),
					};

					out.push(label);
				}
			}
			Product::Diagonal => {
				let Some(label) = lhs[0]
					.intersection(&rhs[0])
					.and_then(|label| label.intersection(&rhs[1]))
				else {
					return false;
				};

				out.push(label);
			}
		}

		true
	}
}

/// How a product finds the moves of its right operand that may merge with a left move.
enum Candidates {
	ByKey(ByKey),
	/// Every move of the right operand.
	All(usize),
}

impl Candidates {
	/// The candidates of `right` on `tapes`, indexed by key if `indexed` and wholly simple.
	fn new(right: &Post, tapes: &[usize], indexed: bool) -> Candidates {
		if indexed {
			Candidates::ByKey(ByKey::new(right, tapes))
		} else {
			Candidates::All(right.len())
		}
	}

	/// Visits the moves that may merge with `letters`, and maybe others; the caller merges them.
	fn visit(&mut self, letters: impl Iterator<Item = Symbol>, visit: impl FnMut(usize)) {
		match self {
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
	tuple: Vec<TapeLabel>,
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
		self.post_counts[node] += 1;
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
			let dropped = budget.admit(node.index(), state, nanos, &post);
			self.evict(dropped);
		}

		Ok(post)
	}

	/// Drops the posts that the cache budget evicted.
	pub(super) fn evict(&mut self, dropped: Vec<(usize, StateId)>) {
		for (key, state) in dropped {
			let removed = self.posts[ExecId::new(key)][state.index()].take();
			if let (Some(budget), Some(post)) = (&mut self.cache_budget, &removed) {
				budget.retire(post);
			}
		}
		if let Some(budget) = &mut self.cache_budget {
			budget.reclaim();
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
			Op::PadClosure(child, pad) => self.pad_closure_post(node, child, &pad, state),
			Op::Prefixes(child) => self.prefixes_post(node, child, state),
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
			post.push_labels(tuple, self.tagged_target(node, branch, target));
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

	/// The child's moves with each concrete letter on both tapes. Sets expand on one tape, never as a product.
	fn identity_post(
		&mut self,
		node: ExecId,
		child: ExecId,
		state: StateId,
	) -> Result<Post, Limit> {
		let child_post = self.post(child, state)?;
		let universe = self.alphabets.universe(node, 0);
		let mut post = Post::with_capacity(2, child_post.len());
		for (tuple, target) in child_post.iter() {
			match &tuple[0] {
				TapeLabel::Simple(Symbol::ANY) => {
					for &symbol in universe.iter() {
						post.push(&[symbol, symbol], target);
					}
				}
				TapeLabel::Set(symbols) => {
					for &symbol in symbols.iter() {
						post.push(&[symbol, symbol], target);
					}
				}
				TapeLabel::Simple(symbol) => post.push(&[*symbol, *symbol], target),
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
			projected.extend(tapes.iter().map(|&tape| tuple[tape].clone()));
			post.push_labels(&projected, target);
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
		let simple = left.wholly_simple() && right.wholly_simple();
		let indexed =
			simple && !plan.keys.is_empty() && left.len() > 1 && right.len() > INDEX_THRESHOLD;

		if simple
			&& let Product::Sync(spec) = product
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
		let mut candidates = Candidates::new(right, &plan.right_tapes, indexed);
		let store = &mut self.stores[node];
		let start = store.len();

		for (left_tuple, left_target) in left.iter() {
			self.budget.step(store.len() - start);
			if self.budget.passed() {
				break;
			}

			let letters = plan.keys.iter().map(|pair| left_tuple[pair.lhs].symbol());
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
					post.push_labels(&self.join_buffers.tuple, Target { state, accepting });
				}
			});
		}

		self.budget.add(store.len() - start);

		post
	}

	/// Partitions the concrete tuples into symbolic Cartesian regions, each to its minimized successor set.
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
		if universes.iter().any(|universe| universe.is_empty()) {
			return Ok(Post::new(arity));
		}

		let posts = members
			.iter()
			.map(|&member| self.post(child, member))
			.collect::<Result<Vec<Rc<Post>>, Limit>>()?;
		let mut region: Vec<TapeLabel> = universes.into_iter().map(TapeLabel::Set).collect();
		let active: Vec<ActiveMove> = posts
			.iter()
			.flat_map(|post| post.iter())
			.filter(|(tuple, _)| !tuple.contains(&TapeLabel::Simple(Symbol::EPSILON)))
			.filter(|(tuple, _)| {
				tuple
					.iter()
					.zip(&region)
					.all(|(label, region)| label.intersects(region))
			})
			.map(|(tuple, target)| ActiveMove { tuple, target })
			.collect();

		let mut post = Post::new(arity);
		let start = self.stores[node].len();
		self.complement_region(node, &mut region, &active, start, &mut post, 0)?;
		self.budget.add(self.stores[node].len() - start);

		Ok(post)
	}

	/// Splits one tape at a time into the minterms of the active moves; no product of tape minterms is built.
	fn complement_region(
		&mut self,
		node: ExecId,
		region: &mut [TapeLabel],
		active: &[ActiveMove],
		start: usize,
		post: &mut Post,
		next_tape: usize,
	) -> Result<(), Limit> {
		self.budget.step(self.stores[node].len() - start);
		if self.budget.passed() {
			return Ok(());
		}
		let Op::Complement(child) = self.exec.nodes[node].op else {
			unreachable!("only complements partition concrete regions")
		};

		// Earlier tapes are already disjoint minterms of every active move, so they cannot split again.
		if !active.is_empty() {
			for tape in next_tape..region.len() {
				let TapeLabel::Set(current) = &region[tape] else {
					unreachable!("a complement region is a concrete set on every tape")
				};
				let mut parts = vec![Rc::clone(current)];
				for active_move in active {
					let mut next = Vec::with_capacity(parts.len() + 1);
					for part in &parts {
						let (covered, uncovered) = active_move.tuple[tape].split(part);
						if covered.is_empty() || uncovered.is_empty() {
							next.push(Rc::clone(part));
							continue;
						}

						next.push(covered.into());
						next.push(uncovered.into());
					}
					parts = next;
				}

				if parts.len() == 1 {
					continue;
				}

				let previous = region[tape].clone();
				for part in parts {
					region[tape] = TapeLabel::Set(part);
					let next: Vec<ActiveMove> = active
						.iter()
						.filter(|active_move| active_move.tuple[tape].intersects(&region[tape]))
						.copied()
						.collect();
					deep(|| self.complement_region(node, region, &next, start, post, tape + 1))?;
					if self.budget.passed() {
						break;
					}
				}
				region[tape] = previous;

				return Ok(());
			}
		}

		let mut successors = Vec::with_capacity(active.len());
		let mut accepting = true;
		for active_move in active {
			successors.push(active_move.target.state);
			accepting &= !active_move.target.accepting;
		}
		self.subsumption
			.minimize(&self.exec, &self.stores, child, &mut successors);
		let state = self.stores[node].intern_set(&successors);
		post.push_labels(region, Target { state, accepting });

		Ok(())
	}
}
