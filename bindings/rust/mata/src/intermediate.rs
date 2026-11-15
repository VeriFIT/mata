//! Parsed `.mata` sections that are not constructed yet: Mata's `mata::IntermediateAut`.

use std::fmt;

use mata_sys::cxx::UniquePtr;
use mata_sys::ffi;

use crate::nft::Nft;
use crate::{Alphabet, IntermediateError, Nfa, ParseError, check};

/// One section of a `.mata` text as Mata's parser reads it, before an automaton is constructed from it:
/// Mata's `mata::IntermediateAut`.
///
/// [`Nfa::parse_mata`] does the three steps of loading in one call; they are available separately as
/// [`IntermediateAutomaton::parse_mata_all`], [`IntermediateAutomaton::mintermize`] for sections with a
/// bit-vector alphabet, and [`IntermediateAutomaton::construct_nfa`] or
/// [`IntermediateAutomaton::construct_nft`].
///
/// The [`Display`](fmt::Display) form is Mata's debugging dump of the section, not the `.mata` format.
///
/// # Examples
///
/// ```
/// use mata::{Alphabet, IntermediateAutomaton};
///
/// let text = "@NFA-bits\n%Alphabet-auto\n%Initial q0\n%Final q1\nq0 (a0 & !a1) q1\n";
/// let parsed = IntermediateAutomaton::parse_mata_all(text)?;
/// let explicit = IntermediateAutomaton::mintermize(&[&parsed[0]])?;
/// let mut sigma = Alphabet::new();
/// let nfa = explicit[0].construct_nfa(&mut sigma)?;
/// assert_eq!(nfa.num_transitions(), 1);
/// # Ok::<(), mata::Error>(())
/// ```
pub struct IntermediateAutomaton {
	/// Never null.
	inner: UniquePtr<ffi::IntermediateAut>,
}

// SAFETY: An `IntermediateAutomaton` owns its C++ object, which is plain data (strings and vectors) that
// no other object refers to; mintermization, the one step that uses process-wide state (CUDD), is
// serialized by the shim.
unsafe impl Send for IntermediateAutomaton {}

// SAFETY: The `&self` methods read the object through `const` references only.
unsafe impl Sync for IntermediateAutomaton {}

impl IntermediateAutomaton {
	/// Parses every section of a `.mata` text (Mata's `parser::parse_mf` followed by
	/// `IntermediateAut::parse_from_mf`). Sections with a bit-vector alphabet keep their formulas.
	///
	/// # Errors
	///
	/// [`ParseError`] if the text is not valid.
	pub fn parse_mata_all(text: &str) -> Result<Vec<IntermediateAutomaton>, ParseError> {
		let parsed =
			ffi::intermediate_parse_mata(text).map_err(|error| ParseError::new(error.what()))?;
		Ok(take_all(parsed))
	}

	/// Mintermizes `automata` together (Mata's `Mintermization::mintermize`): the formulas over bit
	/// vectors become explicit symbols `0`, `1`, … that stand for the same minterms in every result, in
	/// the order of `automata`. Automata mintermized in separate calls do not share their minterms.
	///
	/// # Errors
	///
	/// [`IntermediateError::NotMintermizable`] unless every automaton is an NFA with a bit-vector
	/// alphabet.
	pub fn mintermize(
		automata: &[&IntermediateAutomaton],
	) -> Result<Vec<IntermediateAutomaton>, IntermediateError> {
		let mut refs = ffi::intermediate_refs_new();
		for (index, automaton) in automata.iter().enumerate() {
			let raw = automaton.raw();
			if !raw.is_nfa() || !raw.is_bitvector() {
				return Err(IntermediateError::NotMintermizable { index });
			}
			ffi::intermediate_refs_push(refs.pin_mut(), raw);
		}
		let mintermized =
			ffi::intermediate_mintermize(&refs).map_err(|error| ParseError::new(error.what()))?;
		Ok(take_all(mintermized))
	}

	/// Constructs the NFA of an explicit NFA section (Mata's `nfa::builder::construct`). Symbol names
	/// are read through `alphabet` as in [`Nfa::parse_mata`], which adds the new ones; the numbers that
	/// mintermization gives the minterms of a section stay names of their symbols.
	///
	/// # Errors
	///
	/// [`IntermediateError::WrongKind`] if the section is not an NFA,
	/// [`IntermediateError::NeedsMintermization`] if it still has a bit-vector alphabet, and
	/// [`IntermediateError::Parse`] if it has a transition Mata cannot construct or `alphabet` has no
	/// symbol left for a new name.
	pub fn construct_nfa(&self, alphabet: &mut Alphabet) -> Result<Nfa, IntermediateError> {
		self.require("NFA", self.raw().is_nfa())?;
		let (names, symbols) = alphabet.raw_parse_names();
		let mut new_names = Vec::new();
		let mut new_symbols = Vec::new();
		let nfa = ffi::intermediate_construct_nfa(
			self.raw(),
			&names,
			&symbols,
			alphabet.raw_next_symbol(),
			&mut new_names,
			&mut new_symbols,
		)
		.map_err(|error| ParseError::new(error.what()))?;
		alphabet.add_minterm_names(new_names, &new_symbols);
		Ok(Nfa::from_owned(nfa))
	}

	/// Constructs the transducer of an explicit NFT section (Mata's `nft::builder::construct`), with
	/// symbol names as in [`IntermediateAutomaton::construct_nfa`]. Mata reads no `%Levels` or
	/// `%LevelsNum` on this path: every state is on level 0 of a two-level transducer, so each
	/// transition reads its symbol on both levels. [`Nft::parse_mata`] reads the levels.
	///
	/// # Errors
	///
	/// [`IntermediateError::WrongKind`] if the section is not an NFT,
	/// [`IntermediateError::NeedsMintermization`] if it has a bit-vector alphabet, and
	/// [`IntermediateError::Parse`] if it has a transition Mata cannot construct or `alphabet` has no
	/// symbol left for a new name.
	pub fn construct_nft(&self, alphabet: &mut Alphabet) -> Result<Nft, IntermediateError> {
		self.require("NFT", self.raw().is_nft())?;
		let (names, symbols) = alphabet.raw_parse_names();
		let mut new_names = Vec::new();
		let mut new_symbols = Vec::new();
		let nft = mata_sys::nft::ffi::intermediate_construct_nft(
			self.raw(),
			&names,
			&symbols,
			alphabet.raw_next_symbol(),
			&mut new_names,
			&mut new_symbols,
		)
		.map_err(|error| ParseError::new(error.what()))?;
		alphabet.add_minterm_names(new_names, &new_symbols);
		Ok(Nft::from_owned(nft))
	}

	/// The underlying C++ object, for passing to other C++ code through [`mata_sys`].
	#[must_use]
	pub fn as_ffi(&self) -> &ffi::IntermediateAut {
		self.raw()
	}

	fn raw(&self) -> &ffi::IntermediateAut {
		&self.inner
	}

	/// Checks that the section is of the kind a construction builds, with an explicit alphabet.
	fn require(&self, expected: &'static str, kind: bool) -> Result<(), IntermediateError> {
		if !kind {
			return Err(IntermediateError::WrongKind { expected });
		}
		if self.raw().is_bitvector() {
			return Err(IntermediateError::NeedsMintermization);
		}
		Ok(())
	}
}

fn take_all(mut automata: UniquePtr<ffi::IntermediateAuts>) -> Vec<IntermediateAutomaton> {
	(0..ffi::intermediate_auts_len(&automata))
		.map(|index| {
			let inner = check(ffi::intermediate_auts_take(automata.pin_mut(), index));
			assert!(
				!inner.is_null(),
				"mata-sys returned a null intermediate automaton"
			);
			IntermediateAutomaton { inner }
		})
		.collect()
}

/// Mata's `operator<<`: the type, naming, alphabet, initial and final states, and transitions.
impl fmt::Display for IntermediateAutomaton {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		f.write_str(&check(ffi::intermediate_print(self.raw())))
	}
}

impl fmt::Debug for IntermediateAutomaton {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		f.debug_struct("IntermediateAutomaton")
			.finish_non_exhaustive()
	}
}
