//! The join plan of each product, and a join by groups of moves for sync products that drop their only pair.

use std::cell::OnceCell;
use std::collections::hash_map::Entry;
use std::hash::Hasher;
use std::rc::Rc;

use hashbrown::HashTable;
use rustc_hash::{FxHashMap, FxHasher};
use typed_index_collections::TiVec;

use super::budget::Budget;
use super::exec::{Exec, Op};
use super::post::{Post, Target, index_u32};
use super::store::{Pair, Store};
use crate::formula::{SyncSpec, TapePair};
use crate::ids::{ExecId, StateId};
use crate::{Symbol, Tape};

/// A letter's code in a [`LetterIndex`].
type Code = u32;

/// A group of an operand's moves, those with the same letters on the kept tapes and the same target.
type Group = u32;

/// A move of a post, by its index.
type MoveIndex = u32;

/// A group's target, numbered densely among the targets of the operand's groups.
type DenseTarget = u32;

/// Universes larger than this get no letter index, so no group join.
const MAX_LETTERS: u32 = 1 << 20;

/// The group join is tried when the pair join would try at least this many pairs per move of the operands.
const MIN_PAIRS_PER_MOVE: usize = 2;

/// The group join is used when its mask words per left move cost less than this many pairs.
const MASK_WORDS_PER_PAIR: usize = 16;

/// The largest mask table, in words, that the group join builds.
const MAX_MASK_WORDS: usize = 1 << 22;

/// How many prepared right operands are kept.
const MAX_PREPARED: usize = 8;

/// How the join of a product works, decided once per node when the query starts.
#[derive(Debug, Default)]
pub(crate) struct JoinPlan {
	/// A sync product that drops tapes, where different pairs of moves may give the same move.
	pub(crate) dedup: bool,
	/// The group join of such a product that synchronizes only one pair and drops it.
	pub(crate) group: Option<GroupPlan>,
	/// The tapes of the operands whose letters must merge.
	pub(crate) keys: Box<[TapePair]>,
	/// The right tapes of `keys`, to index the right operand by.
	pub(crate) right_tapes: Box<[usize]>,
}

impl JoinPlan {
	fn new(keys: Box<[TapePair]>) -> JoinPlan {
		JoinPlan {
			right_tapes: keys.iter().map(|pair| pair.rhs).collect(),
			keys,
			..JoinPlan::default()
		}
	}
}

/// What the group join of a sync product needs, besides its operands.
#[derive(Debug)]
pub(crate) struct GroupPlan {
	/// The only synchronized pair, which the product drops.
	pub(crate) hidden: TapePair,
	/// The letters of the hidden tapes, numbered on first use.
	pub(crate) index: OnceCell<Option<LetterIndex>>,
	/// The tapes of each operand in the layout, in its order.
	kept_left: Box<[usize]>,
	kept_right: Box<[usize]>,
}

impl GroupPlan {
	fn new(hidden: TapePair, spec: &SyncSpec) -> GroupPlan {
		let (mut kept_left, mut kept_right) = (Vec::new(), Vec::new());
		for tape in &spec.layout {
			match tape.source {
				Tape::Lhs(i) => kept_left.push(i),
				Tape::Rhs(j) => kept_right.push(j),
			}
		}

		GroupPlan {
			hidden,
			index: OnceCell::new(),
			kept_left: kept_left.into(),
			kept_right: kept_right.into(),
		}
	}
}

/// The plan of every node.
pub(crate) fn plans(exec: &Exec<'_>) -> TiVec<ExecId, JoinPlan> {
	let shapes = shapes(exec);

	exec.nodes
		.iter()
		.map(|node| match &node.op {
			Op::Sync(a, b, spec) => {
				let dedup =
					hides(exec, *a, *b, spec) && !distinct_pairs(exec, &shapes, *a, *b, spec);
				let group = match hidden_pair(spec) {
					Some(hidden) if dedup => Some(GroupPlan::new(hidden, spec)),
					_ => None,
				};

				JoinPlan {
					dedup,
					group,
					..JoinPlan::new(spec.pairs.clone())
				}
			}
			Op::Intersection(..) => JoinPlan::new(
				(0..node.arity)
					.map(|tape| TapePair {
						lhs: tape,
						rhs: tape,
					})
					.collect(),
			),
			Op::Diagonal(..) => JoinPlan::new(Box::new([TapePair { lhs: 0, rhs: 0 }])),
			_ => JoinPlan::default(),
		})
		.collect()
}

/// Whether a sync product drops a tape of an operand.
fn hides(exec: &Exec<'_>, a: ExecId, b: ExecId, spec: &SyncSpec) -> bool {
	let kept: usize = spec
		.layout
		.iter()
		.map(|tape| 1 + usize::from(tape.peer.is_some()))
		.sum();

	kept < exec.nodes[a].arity + exec.nodes[b].arity
}

/// The only synchronized pair, if the product drops it.
fn hidden_pair(spec: &SyncSpec) -> Option<TapePair> {
	match *spec.pairs {
		[pair] if spec.layout.iter().all(|tape| tape.peer.is_none()) => Some(pair),
		_ => None,
	}
}

/// What every post of a node is known to satisfy.
#[derive(Clone, Debug)]
struct Shape {
	/// No two moves have the same tuple and target.
	distinct: bool,
	/// The tapes that may read [`ANY`](Symbol::ANY).
	wild: Vec<bool>,
}

fn shapes(exec: &Exec<'_>) -> TiVec<ExecId, Shape> {
	let mut shapes: TiVec<ExecId, Shape> = Vec::with_capacity(exec.nodes.len()).into();
	for node in &exec.nodes {
		let arity = node.arity;
		let shape = match &node.op {
			Op::Leaf(leaf) => Shape {
				distinct: true,
				wild: leaf.tape_has_any.clone(),
			},
			Op::Union(branches) => Shape {
				distinct: branches.iter().all(|&branch| shapes[branch].distinct),
				wild: (0..arity)
					.map(|tape| branches.iter().any(|&branch| shapes[branch].wild[tape]))
					.collect(),
			},
			Op::Intersection(a, b) => {
				let (a, b) = (&shapes[*a], &shapes[*b]);

				Shape {
					distinct: a.distinct && b.distinct && !a.wild.iter().chain(&b.wild).any(|&w| w),
					wild: (0..arity)
						.map(|tape| a.wild[tape] && b.wild[tape])
						.collect(),
				}
			}
			Op::Complement(_) => Shape {
				distinct: true,
				wild: vec![false; arity],
			},
			Op::Identity(child) => Shape {
				distinct: shapes[*child].distinct && !shapes[*child].wild[0],
				wild: vec![false; 2],
			},
			Op::Project(child, tapes) => Shape {
				distinct: true,
				wild: tapes
					.iter()
					.map(|&tape| shapes[*child].wild[tape])
					.collect(),
			},
			Op::Sync(a, b, spec) => Shape {
				distinct: hides(exec, *a, *b, spec) || distinct_pairs(exec, &shapes, *a, *b, spec),
				wild: spec
					.layout
					.iter()
					.map(|tape| match (tape.source, tape.peer) {
						(Tape::Lhs(i), None) => shapes[*a].wild[i],
						(Tape::Rhs(j), None) => shapes[*b].wild[j],
						(Tape::Lhs(i), Some(j)) | (Tape::Rhs(j), Some(i)) => {
							shapes[*a].wild[i] && shapes[*b].wild[j]
						}
					})
					.collect(),
			},
			Op::Diagonal(u, x) => {
				let wild = [shapes[*u].wild[0], shapes[*x].wild[0], shapes[*x].wild[1]];

				Shape {
					distinct: shapes[*u].distinct
						&& shapes[*x].distinct
						&& !wild.iter().any(|&w| w),
					wild: vec![wild.iter().all(|&w| w)],
				}
			}
			// It may become a leaf cached by another query, whose wildcards this one cannot see.
			Op::Explicit { child, .. } => Shape {
				distinct: shapes[*child].distinct,
				wild: vec![true; arity],
			},
			Op::Transparent(child) => shapes[*child].clone(),
		};

		shapes.push(shape);
	}

	shapes
}

/// Whether different pairs of moves of the operands give different moves of the sync product.
fn distinct_pairs(
	exec: &Exec<'_>,
	shapes: &TiVec<ExecId, Shape>,
	a: ExecId,
	b: ExecId,
	spec: &SyncSpec,
) -> bool {
	let kept_alone = |tape: Tape| {
		spec.layout
			.iter()
			.any(|result| result.source == tape && result.peer.is_none())
	};
	// The other tape of an identity, which reads the same letter, never the wildcard.
	let twin_kept = |node: ExecId, tape: usize, side: fn(usize) -> Tape| {
		matches!(exec.nodes[node].op, Op::Identity(_)) && kept_alone(side(1 - tape))
	};
	let pair_ok = |pair: &TapePair| {
		let kept = spec.layout.iter().any(|result| {
			matches!(
				(result.source, result.peer),
				(Tape::Lhs(i), Some(j)) | (Tape::Rhs(j), Some(i)) if i == pair.lhs && j == pair.rhs
			)
		});
		let (wild_a, wild_b) = (shapes[a].wild[pair.lhs], shapes[b].wild[pair.rhs]);

		if kept {
			!wild_a && !wild_b
		} else {
			(twin_kept(b, pair.rhs, Tape::Rhs) && !wild_a)
				|| (twin_kept(a, pair.lhs, Tape::Lhs) && !wild_b)
		}
	};
	let synced_lhs = |i: usize| spec.pairs.iter().any(|pair| pair.lhs == i);
	let synced_rhs = |j: usize| spec.pairs.iter().any(|pair| pair.rhs == j);

	// Distinct moves of each operand, and a product move determines the tuples of the two it came from.
	shapes[a].distinct
		&& shapes[b].distinct
		&& spec.pairs.iter().all(pair_ok)
		&& (0..exec.nodes[a].arity).all(|i| synced_lhs(i) || kept_alone(Tape::Lhs(i)))
		&& (0..exec.nodes[b].arity).all(|j| synced_rhs(j) || kept_alone(Tape::Rhs(j)))
}

/// The letters of a universe numbered `0..len`; padding is `len` and the wildcard `len + 1`.
#[derive(Debug)]
pub(crate) struct LetterIndex {
	len: u32,
	codes: Codes,
}

#[derive(Debug)]
enum Codes {
	/// The universe is `first..first + len`.
	Range {
		first: u32,
	},
	Map(FxHashMap<Symbol, Code>),
}

impl LetterIndex {
	/// The index of a sorted universe; `None` if it has more than [`MAX_LETTERS`] letters.
	pub(crate) fn new(universe: &[Symbol]) -> Option<LetterIndex> {
		let len = u32::try_from(universe.len())
			.ok()
			.filter(|&len| len <= MAX_LETTERS)?;
		let codes = match (universe.first(), universe.last()) {
			(Some(first), Some(last)) if last.value() - first.value() + 1 != len => {
				Codes::Map(universe.iter().copied().zip(0..).collect())
			}
			(first, _) => Codes::Range {
				first: first.map_or(0, |first| first.value()),
			},
		};

		Some(LetterIndex { len, codes })
	}

	fn len(&self) -> usize {
		self.len as usize
	}

	fn code(&self, letter: Symbol) -> Option<Code> {
		match letter {
			Symbol::EPSILON => Some(self.len),
			Symbol::ANY => Some(self.len + 1),
			_ => match &self.codes {
				Codes::Range { first } => letter
					.value()
					.checked_sub(*first)
					.filter(|&code| code < self.len),
				Codes::Map(codes) => codes.get(&letter).copied(),
			},
		}
	}
}

/// Buffers of the group join, kept between posts.
#[derive(Debug, Default)]
pub(crate) struct Scratch {
	lhs: Side,
	recent: Recent,
	buffers: Buffers,
	keys: FxHashMap<u64, u32>,
	table: HashTable<Group>,
}

/// The pairs of groups that meet, and what finding and emitting them needs.
#[derive(Debug, Default)]
struct Buffers {
	/// The right groups that each left group met, `words` words per left group.
	found: Vec<u64>,
	fresh: Vec<u64>,
	outputs: Vec<(Group, Group)>,
	pair_ids: Vec<StateId>,
}

/// The moves of one operand, with the code of each one's letter on the hidden tape and its group.
#[derive(Debug, Default)]
struct Side {
	codes: Vec<Code>,
	groups: Vec<Group>,
	/// The first move of each group.
	first: Vec<MoveIndex>,
	/// The target of each group, numbered densely.
	targets: Vec<DenseTarget>,
	num_targets: usize,
}

/// A right operand ready to join, with its groups, its moves by code, and the groups that each code merges with.
#[derive(Debug)]
struct Prepared {
	post: Rc<Post>,
	tape: usize,
	kept: Vec<usize>,
	side: Side,
	/// The moves with code `c` are `by_code[starts[c]..starts[c + 1]]`, in increasing order.
	starts: Vec<usize>,
	by_code: Vec<MoveIndex>,
	/// `words` words per code, the wildcard's groups included; the last row is every non-padding group.
	masks: Option<Vec<u64>>,
	words: usize,
}

impl Prepared {
	/// `post` grouped by its letters on `kept` and by its codes on `tape`; `None` if a letter is not indexed.
	fn new(
		post: &Rc<Post>,
		tape: usize,
		kept: &[usize],
		index: &LetterIndex,
		keys: &mut FxHashMap<u64, u32>,
		table: &mut HashTable<Group>,
	) -> Option<Prepared> {
		let letters = index.len();
		let mut side = Side::default();
		side.read_codes(post, tape, index)?;
		let words = side.group(post, kept, keys, table).div_ceil(64);
		side.number_targets(post, keys);
		let masks = side.masks(letters, words);

		// The moves sorted by code, by counting.
		let mut starts = vec![0; letters + 3];
		for &code in &side.codes {
			starts[code as usize + 1] += 1;
		}

		for code in 1..starts.len() {
			starts[code] += starts[code - 1];
		}

		let mut next = starts.clone();
		let mut by_code = vec![0; post.len()];
		for (move_index, &code) in (0_u32..).zip(&side.codes) {
			let slot = &mut next[code as usize];
			by_code[*slot] = move_index;
			*slot += 1;
		}

		Some(Prepared {
			post: Rc::clone(post),
			tape,
			kept: kept.to_vec(),
			side,
			starts,
			by_code,
			masks,
			words,
		})
	}

	fn with_code(&self, code: usize) -> &[MoveIndex] {
		&self.by_code[self.starts[code]..self.starts[code + 1]]
	}

	/// The right moves that may merge with a left `code`, in the pair join's order, until `visit` returns false.
	fn partners(
		&self,
		code: usize,
		letters: usize,
		indexed: bool,
		visit: impl FnMut(MoveIndex) -> bool,
	) {
		let (padding, wildcard) = (letters, letters + 1);
		if !indexed || code == wildcard {
			let merges = |other: usize| {
				if code < letters {
					other == code || other == wildcard
				} else if code == padding {
					other == padding
				} else {
					other != padding
				}
			};
			let all = (0..self.side.codes.len())
				.filter(|&index| merges(self.side.codes[index] as usize))
				.map(index_u32);
			visit_while(all, visit);
		} else if code < letters {
			let letter_or_wildcard = self.with_code(code).iter().chain(self.with_code(wildcard));
			visit_while(letter_or_wildcard.copied(), visit);
		} else {
			visit_while(self.with_code(padding).iter().copied(), visit);
		}
	}
}

/// The right operands prepared recently, the latest last.
#[derive(Debug, Default)]
struct Recent(Vec<Prepared>);

impl Recent {
	/// The prepared right operand, built unless it was prepared recently.
	fn prepare(
		&mut self,
		post: &Rc<Post>,
		tape: usize,
		kept: &[usize],
		index: &LetterIndex,
		keys: &mut FxHashMap<u64, u32>,
		table: &mut HashTable<Group>,
	) -> Option<&Prepared> {
		let Recent(prepared) = self;
		let found = prepared.iter().position(|entry| {
			Rc::ptr_eq(&entry.post, post) && entry.tape == tape && entry.kept == kept
		});
		let latest = match found {
			Some(position) => prepared.remove(position),
			None => Prepared::new(post, tape, kept, index, keys, table)?,
		};

		if prepared.len() == MAX_PREPARED {
			prepared.remove(0);
		}

		prepared.push(latest);

		prepared.last()
	}
}

impl Side {
	/// Reads the code of each move's letter on `tape`; `None` if one is not indexed.
	fn read_codes(&mut self, post: &Post, tape: usize, index: &LetterIndex) -> Option<()> {
		// The flat letters, as `tuple` would reload a shared table's letters after every push.
		let (arity, letters) = post.flat();
		self.codes.clear();
		for move_index in 0..post.len() {
			self.codes
				.push(index.code(letters[move_index * arity + tape])?);
		}

		Some(())
	}

	/// Numbers the groups' targets densely.
	fn number_targets(&mut self, post: &Post, numbers: &mut FxHashMap<u64, u32>) {
		numbers.clear();
		self.targets.clear();
		for &first in &self.first {
			let fresh = index_u32(numbers.len());
			let state = post.state(first as usize);
			self.targets
				.push(*numbers.entry(u64::from(state)).or_insert(fresh));
		}

		self.num_targets = numbers.len();
	}

	/// Groups the moves by their letters on `kept` and their target; returns the number of groups.
	fn group(
		&mut self,
		post: &Post,
		kept: &[usize],
		keys: &mut FxHashMap<u64, u32>,
		table: &mut HashTable<Group>,
	) -> usize {
		self.groups.clear();
		self.first.clear();

		if kept.len() <= 1 {
			// One letter and the state fit a key exactly.
			keys.clear();
			for index in 0..post.len() {
				let letter = kept
					.first()
					.map_or(0, |&tape| post.tuple(index)[tape].value());
				let key = u64::from(letter) << 32 | u64::from(post.state(index));
				let group = match keys.entry(key) {
					Entry::Occupied(entry) => *entry.get(),
					Entry::Vacant(entry) => {
						let group = index_u32(self.first.len());
						self.first.push(index_u32(index));

						*entry.insert(group)
					}
				};

				self.groups.push(group);
			}

			return self.first.len();
		}

		let hash = |index: usize| {
			let mut hasher = FxHasher::default();
			for &tape in kept {
				hasher.write_u32(post.tuple(index)[tape].value());
			}

			hasher.write_u32(u32::from(post.state(index)));

			hasher.finish()
		};
		let same = |a: usize, b: usize| {
			post.state(a) == post.state(b)
				&& kept
					.iter()
					.all(|&tape| post.tuple(a)[tape] == post.tuple(b)[tape])
		};
		table.clear();
		for index in 0..post.len() {
			let hashed = hash(index);
			let found = table.find(hashed, |&group| {
				same(self.first[group as usize] as usize, index)
			});
			let group = if let Some(&group) = found {
				group
			} else {
				let group = index_u32(self.first.len());
				self.first.push(index_u32(index));
				table.insert_unique(hashed, group, |&group| {
					hash(self.first[group as usize] as usize)
				});

				group
			};

			self.groups.push(group);
		}

		self.first.len()
	}

	/// The masks of [`masks`](Prepared::masks) over `letters` letters, unless they pass [`MAX_MASK_WORDS`].
	fn masks(&self, letters: usize, words: usize) -> Option<Vec<u64>> {
		let (padding, wildcard, every) = (letters, letters + 1, letters + 2);
		if words.saturating_mul(letters + 3) > MAX_MASK_WORDS {
			return None;
		}

		let mut masks = vec![0; (letters + 3) * words];
		for (&code, &group) in self.codes.iter().zip(&self.groups) {
			let code = code as usize;
			let (word, bit) = bit_of(group);
			masks[code * words + word] |= bit;
			if code != padding {
				masks[every * words + word] |= bit;
			}
		}

		for code in 0..letters {
			for word in 0..words {
				masks[code * words + word] |= masks[wildcard * words + word];
			}
		}

		Some(masks)
	}
}

/// The word and the bit of `group` in a mask.
fn bit_of(group: Group) -> (usize, u64) {
	(group as usize / 64, 1 << (group % 64))
}

/// The operands of a sync product with a [`GroupPlan`].
pub(crate) struct GroupJoin<'a> {
	pub(crate) left: &'a Post,
	pub(crate) right: &'a Rc<Post>,
	pub(crate) spec: &'a SyncSpec,
	pub(crate) plan: &'a GroupPlan,
	/// The plan's letter index.
	pub(crate) index: &'a LetterIndex,
	/// Whether the pair join would index the right moves, which sets the order it visits them in.
	pub(crate) indexed: bool,
}

impl GroupJoin<'_> {
	/// The pair join's post, interned in the same order; `None` if that looks cheaper or a letter is not indexed.
	pub(crate) fn run(
		&self,
		store: &mut Store,
		budget: &mut Budget,
		scratch: &mut Scratch,
	) -> Option<Post> {
		let (num_left, num_right) = (self.left.len(), self.right.len());
		// Tables over the letters stay in proportion to the moves.
		if self.index.len() > 4 * (num_left + num_right) + 4096 {
			return None;
		}

		scratch
			.lhs
			.read_codes(self.left, self.plan.hidden.lhs, self.index)?;
		let rhs = scratch.recent.prepare(
			self.right,
			self.plan.hidden.rhs,
			&self.plan.kept_right,
			self.index,
			&mut scratch.keys,
			&mut scratch.table,
		)?;

		let masks = rhs.masks.as_deref()?;
		let pairs = self.pairs_tried(&scratch.lhs, rhs);
		if pairs < MIN_PAIRS_PER_MOVE.saturating_mul(num_left + num_right)
			|| rhs.words.saturating_mul(num_left) > MASK_WORDS_PER_PAIR.saturating_mul(pairs)
		{
			return None;
		}

		let num_left_groups = scratch.lhs.group(
			self.left,
			&self.plan.kept_left,
			&mut scratch.keys,
			&mut scratch.table,
		);
		if rhs.words.saturating_mul(num_left_groups) > MAX_MASK_WORDS {
			return None;
		}

		self.meet(&scratch.lhs, rhs, masks, &mut scratch.buffers, budget);
		scratch.lhs.number_targets(self.left, &mut scratch.keys);

		Some(self.emit(&scratch.lhs, rhs, &mut scratch.buffers, store, budget))
	}

	/// How many pairs of moves merge, which is the pair join's work.
	fn pairs_tried(&self, lhs: &Side, rhs: &Prepared) -> usize {
		let letters = self.index.len();
		let (padding, wildcard) = (letters, letters + 1);
		let paddings = rhs.with_code(padding).len();
		let wildcards = rhs.with_code(wildcard).len();
		let partners = |code: usize| {
			if code < letters {
				rhs.with_code(code).len() + wildcards
			} else if code == padding {
				paddings
			} else {
				self.right.len() - paddings
			}
		};

		lhs.codes
			.iter()
			.map(|&code| partners(code as usize))
			.fold(0, usize::saturating_add)
	}

	/// The pairs of groups that merge, each once, in the pair join's order; stops past a limit.
	fn meet(
		&self,
		lhs: &Side,
		rhs: &Prepared,
		masks: &[u64],
		buffers: &mut Buffers,
		budget: &mut Budget,
	) {
		let letters = self.index.len();
		let (wildcard, every) = (letters + 1, letters + 2);
		let words = rhs.words;
		buffers.found.clear();
		buffers.found.resize(lhs.first.len() * words, 0);
		buffers.fresh.clear();
		buffers.fresh.resize(words, 0);
		buffers.outputs.clear();

		for (&code, &group) in lhs.codes.iter().zip(&lhs.groups) {
			budget.step(0);
			if budget.passed() {
				return;
			}

			let (code, row) = (code as usize, group as usize * words);
			let mask_row = if code == wildcard { every } else { code };
			let mask = mask_row * words;

			let mut missing = if words == 1 {
				buffers.fresh[0] = masks[mask] & !buffers.found[row];

				buffers.fresh[0].count_ones()
			} else {
				let mut missing = 0;
				for word in 0..words {
					buffers.fresh[word] = masks[mask + word] & !buffers.found[row + word];
					missing += buffers.fresh[word].count_ones();
				}

				missing
			};

			if missing == 0 {
				continue;
			}

			let found_row = &mut buffers.found[row..row + words];
			rhs.partners(code, letters, self.indexed, |index| {
				let right_group = rhs.side.groups[index as usize];
				let (word, bit) = bit_of(right_group);
				if buffers.fresh[word] & bit != 0 {
					buffers.fresh[word] &= !bit;
					found_row[word] |= bit;
					buffers.outputs.push((group, right_group));
					missing -= 1;
				}

				missing > 0
			});
		}
	}

	/// The post of the pairs of groups met, a pair of targets interned at its first output; stops past a limit.
	fn emit(
		&self,
		lhs: &Side,
		rhs: &Prepared,
		buffers: &mut Buffers,
		store: &mut Store,
		budget: &mut Budget,
	) -> Post {
		let (left, right) = (self.left, &**self.right);
		let (num_left_targets, num_right_targets) = (lhs.num_targets, rhs.side.num_targets);
		let cached =
			num_left_targets.saturating_mul(num_right_targets) <= 4 * buffers.outputs.len() + 4096;
		buffers.pair_ids.clear();
		if cached {
			buffers
				.pair_ids
				.resize(num_left_targets * num_right_targets, StateId::NONE);
		}

		let arity = self.spec.layout.len();
		let mut post = Post::with_capacity(arity, buffers.outputs.len());
		let mut tuple = Vec::with_capacity(arity);
		let start = store.len();
		for &(left_group, right_group) in &buffers.outputs {
			budget.step(store.len() - start);
			if budget.passed() {
				break;
			}

			let left_move = lhs.first[left_group as usize] as usize;
			let right_move = rhs.side.first[right_group as usize] as usize;
			let (left_tuple, right_tuple) = (left.tuple(left_move), right.tuple(right_move));
			tuple.clear();
			tuple.extend(self.spec.layout.iter().map(|tape| match tape.source {
				Tape::Lhs(i) => left_tuple[i],
				Tape::Rhs(j) => right_tuple[j],
			}));

			let (left_target, right_target) = (left.target(left_move), right.target(right_move));
			let pair = Pair {
				lhs: left_target.state,
				rhs: right_target.state,
			};
			let state = if cached {
				let slot = lhs.targets[left_group as usize] as usize * num_right_targets
					+ rhs.side.targets[right_group as usize] as usize;
				if buffers.pair_ids[slot] == StateId::NONE {
					buffers.pair_ids[slot] = store.intern_pair(pair);
				}

				buffers.pair_ids[slot]
			} else {
				store.intern_pair(pair)
			};

			let accepting = left_target.accepting && right_target.accepting;
			post.push(&tuple, Target { state, accepting });
		}

		budget.add(store.len() - start);

		post
	}
}

/// Visits `indices` until `visit` returns false.
fn visit_while(indices: impl Iterator<Item = MoveIndex>, mut visit: impl FnMut(MoveIndex) -> bool) {
	for index in indices {
		if !visit(index) {
			return;
		}
	}
}
