//! The symbolic formula, a DAG of operations over leaves, built without doing any automata work.

use std::sync::Arc;
use std::sync::atomic::{AtomicU32, Ordering};

use parking_lot::Mutex;
use rustc_hash::{FxHashMap, FxHashSet};
use typed_index_collections::TiVec;

use crate::engine;
use crate::ids::{Id, LeafId, NodeId};
use crate::leaf::Leaf;
use crate::{
	Automaton, Error, ExplicitAutomaton, ExplicitOptions, Options, Outcome, Symbol, Witness,
};

static NEXT_FORMULA: AtomicU32 = AtomicU32::new(0);

/// A node of a [`Formula`], which denotes a relation over tuples of words.
///
/// A term is only valid in the formula that created it; the methods of other formulas return
/// [`ForeignTerm`](Error::ForeignTerm).
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct Term {
	formula: u32,
	node: u32,
}

/// A tape of one operand of [`sync_product`](Formula::sync_product).
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum Tape {
	/// Tape of the left operand.
	Lhs(usize),
	/// Tape of the right operand.
	Rhs(usize),
}

impl Tape {
	/// The tape's index in its operand.
	#[must_use]
	pub fn index(self) -> usize {
		match self {
			Tape::Lhs(index) | Tape::Rhs(index) => index,
		}
	}
}

/// The step that [`pad_closure`](Formula::pad_closure) appends.
#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub enum Pad {
	/// The same symbols every time, one per tape; [`ANY`](Symbol::ANY) reads any symbol of its tape's alphabet.
	Step(Box<[Symbol]>),
	/// The last step of the word, again (sign extension); the empty word gets none.
	Last,
}

#[derive(Debug)]
pub(crate) enum Node {
	Leaf(LeafId),
	Union(NodeId, NodeId),
	Intersection(NodeId, NodeId),
	Complement(NodeId),
	Identity(NodeId),
	Project(NodeId, Arc<[usize]>),
	PadClosure(NodeId, Arc<Pad>),
	Sync(NodeId, NodeId, Arc<SyncSpec>),
	Explicit(NodeId, ExplicitOptions),
}

/// A synchronized pair of tapes, one of the left operand and one of the right.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub(crate) struct TapePair {
	pub(crate) lhs: usize,
	pub(crate) rhs: usize,
}

/// A tape of the result of a synchronized product.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub(crate) struct ResultTape {
	pub(crate) source: Tape,
	/// The tape of the other operand that `source` is synchronized with.
	pub(crate) peer: Option<usize>,
}

/// A checked synchronized product.
#[derive(Debug, PartialEq, Eq, Hash)]
pub(crate) struct SyncSpec {
	pub(crate) pairs: Box<[TapePair]>,
	pub(crate) layout: Box<[ResultTape]>,
}

/// A DAG of Boolean and relational operations over automata, checked for emptiness lazily.
///
/// Building a formula does no automata work. [`leaf`](Formula::leaf) copies an automaton, and the other builders add a node. [`is_empty`](Formula::is_empty) and [`check`](Formula::check) explore only the macrostates they need.
///
/// A term of arity *n* denotes a relation over *n*-tuples of words of equal length, read in lock step, where one
/// step reads one symbol on every tape ([`EPSILON`](Symbol::EPSILON) pads a shorter word). The steps are what
/// count: a step that pads every tape still is one (only a leaf's own moves that pad every tape are silent), and
/// a term of arity 0 holds a number of steps. The complement is taken over the tuples of concrete symbols of each
/// tape's alphabet, which a query computes from the leaves that share the tape.
#[derive(Debug)]
pub struct Formula {
	id: u32,
	pub(crate) nodes: TiVec<NodeId, Node>,
	pub(crate) arities: TiVec<NodeId, usize>,
	/// Whether a word of the node may read padding (a complement never does).
	pub(crate) pads: TiVec<NodeId, bool>,
	pub(crate) leaves: TiVec<LeafId, Leaf>,
	pub(crate) explicit_cache: Mutex<engine::ExplicitCache>,
}

impl Default for Formula {
	fn default() -> Formula {
		Formula::new()
	}
}

impl Formula {
	/// An empty formula.
	///
	/// # Panics
	///
	/// If 2³² − 1 formulas have already been created in this process; reusing an id would let a term of an earlier
	/// formula pass as one of the new formula.
	#[must_use]
	pub fn new() -> Formula {
		let id = NEXT_FORMULA
			.try_update(Ordering::Relaxed, Ordering::Relaxed, |id| id.checked_add(1))
			.expect("formula ids exhausted");
		Formula {
			id,
			nodes: TiVec::default(),
			arities: TiVec::default(),
			pads: TiVec::default(),
			leaves: TiVec::default(),
			explicit_cache: Mutex::new(engine::ExplicitCache::default()),
		}
	}

	/// A leaf with the language of `automaton`, copied now (later changes to `automaton` do not show).
	///
	/// # Errors
	///
	/// [`MoveWidth`](Error::MoveWidth) if a move of `automaton` does not read one symbol per tape.
	pub fn leaf(&mut self, automaton: &impl Automaton) -> Result<Term, Error> {
		self.leaf_over(automaton, std::iter::empty::<[Symbol; 0]>())
	}

	/// A leaf as [`leaf`](Formula::leaf), whose tape `i` also has the symbols of `alphabets[i]`, read or not, so
	/// that a complement over the leaf includes them. A single alphabet applies to every tape. The symbols of
	/// [`Automaton::alphabet`] join them, in `leaf` too.
	///
	/// ```
	/// use mata_lazy::{Automaton, Formula, Symbol};
	///
	/// /// The words a*.
	/// struct OnlyA;
	///
	/// impl Automaton for OnlyA {
	///     type State = ();
	///
	///     fn arity(&self) -> usize { 1 }
	///     fn initial_states(&self) -> impl IntoIterator<Item = ()> { [()] }
	///     fn is_final(&self, (): ()) -> bool { true }
	///     fn for_each_move(&self, (): (), mut visit: impl FnMut(&[Symbol], ())) {
	///         visit(&[Symbol::from(b'a')], ());
	///     }
	/// }
	///
	/// let mut formula = Formula::new();
	/// let leaf = formula.leaf(&OnlyA)?;
	/// // Without b in the alphabet, the complement of a* is empty.
	/// let not_a = formula.complement(leaf)?;
	/// assert!(formula.is_empty(not_a)?);
	/// let leaf = formula.leaf_over(&OnlyA, [*b"ab"])?;
	/// let not_a = formula.complement(leaf)?;
	/// assert!(!formula.is_empty(not_a)?);
	/// # Ok::<(), Box<dyn std::error::Error>>(())
	/// ```
	///
	/// # Errors
	///
	/// As [`leaf`](Formula::leaf), [`AlphabetCount`](Error::AlphabetCount) if `alphabets` has neither one alphabet
	/// nor one per tape, or [`ReservedSymbol`](Error::ReservedSymbol).
	pub fn leaf_over<I>(
		&mut self,
		automaton: &impl Automaton,
		alphabets: impl IntoIterator<Item = I>,
	) -> Result<Term, Error>
	where
		I: IntoIterator<Item: Into<Symbol>>,
	{
		let tapes = automaton.arity();
		let mut declared: Vec<Vec<Symbol>> = alphabets
			.into_iter()
			.map(|alphabet| alphabet.into_iter().map(Into::into).collect())
			.collect();

		match declared.len() {
			0 => {}
			1 => declared = vec![declared.remove(0); tapes],
			found if found == tapes => {}
			found => {
				return Err(Error::AlphabetCount {
					arity: tapes,
					found,
				});
			}
		}

		declared.resize(tapes, Vec::new());
		for (tape, symbols) in declared.iter_mut().enumerate() {
			symbols.extend(automaton.alphabet(tape));
		}

		if let Some(&symbol) = declared
			.iter()
			.flatten()
			.find(|symbol| !symbol.is_concrete())
		{
			return Err(Error::ReservedSymbol { symbol });
		}

		let leaf = Leaf::import(automaton, declared)?;
		let arity = leaf.arity();
		let leaf = self.leaves.push_and_get_key(leaf);

		Ok(self.push(Node::Leaf(leaf), arity))
	}

	/// The union.
	///
	/// # Errors
	///
	/// [`Arity`](Error::Arity) if the arities differ, or [`ForeignTerm`](Error::ForeignTerm).
	pub fn union(&mut self, lhs: Term, rhs: Term) -> Result<Term, Error> {
		let (lhs, rhs, arity) = self.same_arity("union", lhs, rhs)?;

		Ok(self.push(Node::Union(lhs, rhs), arity))
	}

	/// The intersection.
	///
	/// # Errors
	///
	/// [`Arity`](Error::Arity) if the arities differ, or [`ForeignTerm`](Error::ForeignTerm).
	pub fn intersection(&mut self, lhs: Term, rhs: Term) -> Result<Term, Error> {
		let (lhs, rhs, arity) = self.same_arity("intersection", lhs, rhs)?;

		Ok(self.push(Node::Intersection(lhs, rhs), arity))
	}

	/// The complement over the tuples of the tapes' alphabets.
	///
	/// # Errors
	///
	/// [`ForeignTerm`](Error::ForeignTerm).
	pub fn complement(&mut self, term: Term) -> Result<Term, Error> {
		let node = self.node(term)?;

		Ok(self.push(Node::Complement(node), self.arities[node]))
	}

	/// The identity relation `{(w, w) | w ∈ term}` of a language.
	///
	/// # Errors
	///
	/// [`Arity`](Error::Arity) if `term` does not have arity 1, or [`ForeignTerm`](Error::ForeignTerm).
	pub fn identity(&mut self, term: Term) -> Result<Term, Error> {
		let node = self.node_of_arity("identity", term, 1)?;

		Ok(self.push(Node::Identity(node), 2))
	}

	/// The projection to `tapes`, in that order; the other tapes are dropped (existentially).
	///
	/// # Errors
	///
	/// [`TapeOutOfRange`](Error::TapeOutOfRange), [`RepeatedTape`](Error::RepeatedTape), or
	/// [`ForeignTerm`](Error::ForeignTerm).
	pub fn project(&mut self, term: Term, tapes: &[usize]) -> Result<Term, Error> {
		let node = self.node(term)?;
		let arity = self.arities[node];

		let mut seen = FxHashSet::default();
		for &tape in tapes {
			if tape >= arity {
				return Err(Error::TapeOutOfRange { tape, arity });
			}

			if !seen.insert(tape) {
				return Err(Error::RepeatedTape { tape });
			}
		}

		Ok(self.push(Node::Project(node, tapes.into()), tapes.len()))
	}

	/// The words of `term` with padding steps cut off the end: `{w | w·p^k ∈ term for some k ≥ 0}`, where `p`
	/// is `pad`. Logics that encode numbers as words need it after a projection, as the dropped tapes may have
	/// needed longer words than the kept ones. Symbols of a [`Pad::Step`] join the alphabets of their tapes.
	///
	/// # Errors
	///
	/// [`Arity`](Error::Arity) if a [`Pad::Step`] does not have one symbol per tape, or
	/// [`ForeignTerm`](Error::ForeignTerm).
	pub fn pad_closure(&mut self, term: Term, pad: Pad) -> Result<Term, Error> {
		let node = self.node(term)?;
		let arity = self.arities[node];
		if let Pad::Step(step) = &pad
			&& step.len() != arity
		{
			return Err(Error::Arity {
				operation: "pad_closure",
				expected: arity,
				found: step.len(),
			});
		}

		Ok(self.push(Node::PadClosure(node, Arc::new(pad)), arity))
	}

	/// The synchronized product, which holds the tuples of both operands whose `synchronized` tape pairs read
	/// the same words, arranged by `layout`. Tapes missing from `layout` are dropped.
	///
	/// ```
	/// use mata_lazy::{Automaton, Formula, Symbol, Tape};
	///
	/// /// The single pair (x, y).
	/// struct Pair(u8, u8);
	///
	/// impl Automaton for Pair {
	///     type State = bool;
	///
	///     fn arity(&self) -> usize { 2 }
	///     fn initial_states(&self) -> impl IntoIterator<Item = bool> { [false] }
	///     fn is_final(&self, read: bool) -> bool { read }
	///     fn for_each_move(&self, read: bool, mut visit: impl FnMut(&[Symbol], bool)) {
	///         if !read {
	///             visit(&[self.0.into(), self.1.into()], true);
	///         }
	///     }
	/// }
	///
	/// let mut formula = Formula::new();
	/// let ab = formula.leaf(&Pair(b'a', b'b'))?;
	/// let bc = formula.leaf(&Pair(b'b', b'c'))?;
	/// // {(a, b, c)}: tape 1 of `ab` meets tape 0 of `bc`.
	/// let layout = [Tape::Lhs(0), Tape::Lhs(1), Tape::Rhs(1)];
	/// let abc = formula.sync_product(ab, bc, &[(1, 0)], &layout)?;
	/// assert_eq!(formula.arity(abc)?, 3);
	/// assert!(!formula.is_empty(abc)?);
	/// // Tape 0 of `ab` reads a, tape 0 of `bc` reads b.
	/// let none = formula.sync_product(ab, bc, &[(0, 0)], &[])?;
	/// assert!(formula.is_empty(none)?);
	/// # Ok::<(), mata_lazy::Error>(())
	/// ```
	///
	/// # Errors
	///
	/// [`SyncTapeOutOfRange`](Error::SyncTapeOutOfRange), [`RepeatedSyncTape`](Error::RepeatedSyncTape) if a tape is
	/// synchronized twice or a result tape (or its synchronized peer) appears twice in `layout`, or
	/// [`ForeignTerm`](Error::ForeignTerm).
	pub fn sync_product(
		&mut self,
		lhs: Term,
		rhs: Term,
		synchronized: &[(usize, usize)],
		layout: &[Tape],
	) -> Result<Term, Error> {
		let (lhs, rhs) = (self.node(lhs)?, self.node(rhs)?);
		let checked = |tape: Tape| {
			let arity = match tape {
				Tape::Lhs(_) => self.arities[lhs],
				Tape::Rhs(_) => self.arities[rhs],
			};

			if tape.index() < arity {
				Ok(tape)
			} else {
				Err(Error::SyncTapeOutOfRange { tape, arity })
			}
		};

		let mut peers: FxHashMap<Tape, Tape> = FxHashMap::default();
		for &(i, j) in synchronized {
			let (lhs_tape, rhs_tape) = (checked(Tape::Lhs(i))?, checked(Tape::Rhs(j))?);
			for (tape, peer) in [(lhs_tape, rhs_tape), (rhs_tape, lhs_tape)] {
				if peers.insert(tape, peer).is_some() {
					return Err(Error::RepeatedSyncTape { tape });
				}
			}
		}

		// A synchronized pair gives at most one result tape.
		let mut used = FxHashSet::default();
		let mut result_tapes = Vec::with_capacity(layout.len());
		for &tape in layout {
			let tape = checked(tape)?;
			if !used.insert(tape) {
				return Err(Error::RepeatedSyncTape { tape });
			}

			let peer = peers.get(&tape).copied();
			if let Some(peer) = peer
				&& !used.insert(peer)
			{
				return Err(Error::RepeatedSyncTape { tape });
			}

			result_tapes.push(ResultTape {
				source: tape,
				peer: peer.map(Tape::index),
			});
		}

		let arity = result_tapes.len();
		let spec = SyncSpec {
			pairs: synchronized
				.iter()
				.map(|&(lhs, rhs)| TapePair { lhs, rhs })
				.collect(),
			layout: result_tapes.into(),
		};

		Ok(self.push(Node::Sync(lhs, rhs, Arc::new(spec)), arity))
	}

	/// The composition on the `synchronized` tape pairs, which are dropped. The result has the other tapes of
	/// `lhs`, then the other tapes of `rhs`.
	///
	/// # Errors
	///
	/// As [`sync_product`](Formula::sync_product).
	pub fn compose_on(
		&mut self,
		lhs: Term,
		rhs: Term,
		synchronized: &[(usize, usize)],
	) -> Result<Term, Error> {
		let (lhs_arity, rhs_arity) = (self.arity(lhs)?, self.arity(rhs)?);

		let kept_lhs = (0..lhs_arity)
			.filter(|&i| !synchronized.iter().any(|&(l, _)| l == i))
			.map(Tape::Lhs);
		let kept_rhs = (0..rhs_arity)
			.filter(|&j| !synchronized.iter().any(|&(_, r)| r == j))
			.map(Tape::Rhs);
		let layout: Vec<Tape> = kept_lhs.chain(kept_rhs).collect();

		self.sync_product(lhs, rhs, synchronized, &layout)
	}

	/// The composition of two binary relations, the pairs `(u, w)` with `(u, v) ∈ lhs` and `(v, w) ∈ rhs`.
	///
	/// # Errors
	///
	/// [`Arity`](Error::Arity) if a term does not have arity 2, or [`ForeignTerm`](Error::ForeignTerm).
	pub fn compose(&mut self, lhs: Term, rhs: Term) -> Result<Term, Error> {
		self.node_of_arity("compose", lhs, 2)?;
		self.node_of_arity("compose", rhs, 2)?;

		self.compose_on(lhs, rhs, &[(1, 0)])
	}

	/// The image `{w | ∃u ∈ language: (u, w) ∈ relation}`.
	///
	/// # Errors
	///
	/// [`Arity`](Error::Arity) if `language` does not have arity 1 or `relation` arity 2, or
	/// [`ForeignTerm`](Error::ForeignTerm).
	pub fn post_image(&mut self, language: Term, relation: Term) -> Result<Term, Error> {
		self.node_of_arity("post_image", language, 1)?;
		self.node_of_arity("post_image", relation, 2)?;

		self.sync_product(language, relation, &[(0, 0)], &[Tape::Rhs(1)])
	}

	/// The preimage `{u | ∃w ∈ language: (u, w) ∈ relation}`.
	///
	/// # Errors
	///
	/// As [`post_image`](Formula::post_image).
	pub fn pre_image(&mut self, language: Term, relation: Term) -> Result<Term, Error> {
		self.node_of_arity("pre_image", language, 1)?;
		self.node_of_arity("pre_image", relation, 2)?;

		self.sync_product(language, relation, &[(0, 1)], &[Tape::Rhs(0)])
	}

	/// The same relation, built as an explicit automaton when a query starts; see
	/// [`explicit_with`](Formula::explicit_with).
	///
	/// # Errors
	///
	/// [`ForeignTerm`](Error::ForeignTerm).
	pub fn explicit(&mut self, term: Term) -> Result<Term, Error> {
		self.explicit_with(term, ExplicitOptions::new())
	}

	/// The same relation, built as an explicit automaton when a query starts.
	///
	/// The query explores `term` completely, numbers its macrostates densely, trims the result and reads it like a
	/// leaf, with a simulation of its own. Rewrites stop at this node. Materialized automata are reused by later
	/// queries over the same alphabets (see [`clear_cache`](Formula::clear_cache)). When the exploration passes
	/// `options.max_states`, the node stays lazy.
	///
	/// # Errors
	///
	/// [`ForeignTerm`](Error::ForeignTerm).
	pub fn explicit_with(&mut self, term: Term, options: ExplicitOptions) -> Result<Term, Error> {
		let node = self.node(term)?;

		Ok(self.push(Node::Explicit(node, options), self.arities[node]))
	}

	/// The number of tapes of `term`.
	///
	/// # Errors
	///
	/// [`ForeignTerm`](Error::ForeignTerm).
	pub fn arity(&self, term: Term) -> Result<usize, Error> {
		Ok(self.arities[self.node(term)?])
	}

	/// Whether the relation of `term` is empty.
	///
	/// # Errors
	///
	/// [`ComplementTooLarge`](Error::ComplementTooLarge), or [`ForeignTerm`](Error::ForeignTerm).
	pub fn is_empty(&self, term: Term) -> Result<bool, Error> {
		Ok(self.find_word(term)?.is_none())
	}

	/// A tuple of words in the relation of `term`, if there is one.
	///
	/// # Errors
	///
	/// [`ComplementTooLarge`](Error::ComplementTooLarge), or [`ForeignTerm`](Error::ForeignTerm).
	pub fn find_word(&self, term: Term) -> Result<Option<Witness>, Error> {
		Ok(self.check(term, &Options::new())?.witness)
	}

	/// Checks `term` for emptiness with `options`, and reports a witness and statistics.
	///
	/// # Errors
	///
	/// [`StateLimit`](Error::StateLimit) or [`TimeLimit`](Error::TimeLimit) (with the statistics so far) if the query
	/// passes a limit of `options`, [`TapeOutOfRange`](Error::TapeOutOfRange) or
	/// [`ReservedSymbol`](Error::ReservedSymbol) for an alphabet of `options`,
	/// [`ComplementTooLarge`](Error::ComplementTooLarge), or [`ForeignTerm`](Error::ForeignTerm) for `term` or an
	/// explicit term of `options`.
	pub fn check(&self, term: Term, options: &Options) -> Result<Outcome, Error> {
		let mut engine = engine::Engine::new(self, self.node(term)?, options)?;
		let witness = engine.search()?;

		Ok(Outcome {
			witness,
			stats: engine.stats(),
		})
	}

	/// Builds the whole relation of `term` as an explicit automaton (trimmed).
	///
	/// Other libraries copy it into their own automata; with Mata, `Nft::try_from(&explicit)` (in `mata::lazy`)
	/// gives a transducer with one level per tape. [`leaf`](Formula::leaf) reads it back into a formula.
	///
	/// # Errors
	///
	/// As [`check`](Formula::check).
	pub fn materialize(&self, term: Term, options: &Options) -> Result<ExplicitAutomaton, Error> {
		let node = self.node(term)?;
		let mut engine = engine::Engine::new(self, node, options)?;

		Ok(ExplicitAutomaton::new(engine.materialize_root()?))
	}

	/// Forgets the explicit automata kept for later queries.
	pub fn clear_cache(&self) {
		self.explicit_cache.lock().clear();
	}

	pub(crate) fn node(&self, term: Term) -> Result<NodeId, Error> {
		if term.formula == self.id {
			Ok(NodeId::new(term.node as usize))
		} else {
			Err(Error::ForeignTerm)
		}
	}

	pub(crate) fn term(&self, node: NodeId) -> Term {
		Term {
			formula: self.id,
			node: u32::try_from(node.index()).expect("fewer than 2^32 nodes"),
		}
	}

	fn push(&mut self, node: Node, arity: usize) -> Term {
		let pads = match node {
			Node::Leaf(leaf) => self.leaves[leaf].has_padding,
			Node::Complement(_) => false,
			Node::Identity(child)
			| Node::Project(child, _)
			| Node::PadClosure(child, _)
			| Node::Explicit(child, _) => self.pads[child],
			Node::Union(a, b) | Node::Intersection(a, b) | Node::Sync(a, b, _) => {
				self.pads[a] || self.pads[b]
			}
		};

		self.pads.push(pads);
		let id = self.nodes.push_and_get_key(node);
		self.arities.push(arity);

		self.term(id)
	}

	fn node_of_arity(
		&self,
		operation: &'static str,
		term: Term,
		expected: usize,
	) -> Result<NodeId, Error> {
		let node = self.node(term)?;
		let found = self.arities[node];
		if found != expected {
			return Err(Error::Arity {
				operation,
				expected,
				found,
			});
		}

		Ok(node)
	}

	/// The nodes of two operands of equal arity, and that arity.
	fn same_arity(
		&self,
		operation: &'static str,
		lhs: Term,
		rhs: Term,
	) -> Result<(NodeId, NodeId, usize), Error> {
		let lhs = self.node(lhs)?;
		let arity = self.arities[lhs];
		let rhs = self.node_of_arity(operation, rhs, arity)?;

		Ok((lhs, rhs, arity))
	}
}
