//! Behaviour of the transducer API on hand-written cases.

use mata::nft::{DONT_CARE, Nft, SyncLevel};
use mata::{Alphabet, Error, Nfa, NftError, State, Symbol, word_from_bytes};

/// The two-level transducer that replaces every `a` by `b` and keeps `b`.
fn a_to_b() -> Nft {
	let (a, b) = (Symbol::from(b'a'), Symbol::from(b'b'));
	let mut t = Nft::new(2).unwrap();
	let q = t.add_state();
	t.add_initial(q).unwrap();
	t.add_final(q).unwrap();
	t.add_transition(q, &[a, b], q).unwrap();
	t.add_transition(q, &[b, b], q).unwrap();
	t
}

#[test]
fn construction_and_structure() -> Result<(), Error> {
	let t = a_to_b();
	assert_eq!(t.num_levels(), 2);
	assert_eq!(t.num_states(), 3, "one state per level and transition");
	assert_eq!(t.num_edges(), 4);
	let levels: Vec<usize> = t.states().map(|q| t.level(q).unwrap()).collect();
	assert_eq!(levels, [0, 1, 1]);
	assert!(t.accepts(&[word_from_bytes(b"aab"), word_from_bytes(b"bbb")]));
	assert!(!t.accepts(&[word_from_bytes(b"aab"), word_from_bytes(b"bba")]));
	assert!(t.accepts(&[word_from_bytes(b""), word_from_bytes(b"")]));
	assert!(!t.is_lang_empty());
	assert!(Nft::new(3)?.is_lang_empty());
	Ok(())
}

#[test]
fn paths_pad_short_words_with_epsilon() -> Result<(), Error> {
	let mut t = Nft::new(2)?;
	let (p, q) = (t.add_state(), t.add_state());
	t.add_initial(p)?;
	t.add_final(q)?;
	t.add_path(p, &[word_from_bytes(b"abc"), word_from_bytes(b"x")], q)?;
	assert!(t.accepts(&[word_from_bytes(b"abc"), word_from_bytes(b"x")]));
	assert!(!t.accepts(&[word_from_bytes(b"abc"), word_from_bytes(b"")]));
	assert!(t.accepts(&[
		word_from_bytes(b"abc"),
		[Symbol::from(b'x'), Symbol::EPSILON].to_vec()
	]));
	Ok(())
}

#[test]
fn transition_needs_one_symbol_per_level() -> Result<(), Error> {
	let mut t = Nft::new(2)?;
	let q = t.add_state();
	assert_eq!(
		t.add_transition(q, &[Symbol::new(0)], q),
		Err(NftError::LevelMismatch {
			expected: 2,
			found: 1
		})
	);
	Ok(())
}

#[test]
fn initial_states_live_on_level_zero() {
	let mut t = a_to_b();
	let inner = t.states().find(|&q| t.level(q) == Some(1)).unwrap();
	assert_eq!(
		t.add_initial(inner),
		Err(NftError::NotLevelZero {
			state: inner,
			level: 1
		})
	);
	let missing = State::new(100);
	assert!(
		matches!(t.add_final(missing), Err(NftError::NoSuchState(error)) if error.state == missing)
	);
	assert_eq!(t.level(missing), None);
}

#[test]
fn invalid_numbers_of_levels_are_errors() {
	for num_levels in [0, mata::nft::MAX_LEVELS + 1] {
		assert_eq!(
			Nft::new(num_levels).unwrap_err(),
			NftError::InvalidNumLevels { num_levels }
		);
	}
}

#[test]
fn mismatched_levels_are_errors() -> Result<(), Error> {
	let three = Nft::new(3)?;
	let mismatch = NftError::LevelMismatch {
		expected: 2,
		found: 3,
	};
	assert_eq!(a_to_b().union(&three).unwrap_err(), mismatch);
	assert_eq!(a_to_b().is_included_in(&three), Err(mismatch));
	assert!(!a_to_b().accepts(&[word_from_bytes(b"a")]));
	Ok(())
}

#[test]
fn composing_one_level_transducers_is_an_error() -> Result<(), Error> {
	let one = Nft::new(1)?;
	assert_eq!(one.compose(&one).unwrap_err(), NftError::NoLevelLeft);
	Ok(())
}

#[test]
fn composing_beyond_max_levels_is_an_error() -> Result<(), Error> {
	let half = Nft::empty_word(513)?;
	assert_eq!(half.compose(&half)?.num_levels(), 1024);
	assert_eq!(
		half.compose_on(&half, 512, 0, SyncLevel::Keep).unwrap_err(),
		NftError::InvalidNumLevels { num_levels: 1025 }
	);
	let big = Nft::empty_word(600)?;
	assert_eq!(
		big.compose(&big).unwrap_err(),
		NftError::InvalidNumLevels { num_levels: 1198 }
	);
	Ok(())
}

#[test]
fn unnamed_symbols_round_trip() -> Result<(), Error> {
	let mut sigma = Alphabet::from_names(["a"])?;
	let a = sigma.symbol("a").unwrap();
	let unnamed = Symbol::new(41);
	sigma.insert(unnamed)?;
	let mut t = Nft::new(2)?;
	let q = t.add_state();
	t.add_initial(q)?;
	t.add_final(q)?;
	t.add_transition(q, &[a, unnamed], q)?;
	let mut reread = sigma.clone();
	let parsed = Nft::parse_mata(&t.display_with(&sigma).to_string(), &mut reread)?;
	assert_eq!(reread, sigma, "41 is the unnamed symbol, not a new name");
	assert!(parsed.accepts(&[[a], [unnamed]]) && parsed.is_equivalent_to(&t)?);
	Ok(())
}

#[test]
fn projecting_out_everything_is_an_error() {
	assert_eq!(
		a_to_b().project_out(&[0, 1]).unwrap_err(),
		NftError::NoLevelLeft
	);
}

#[test]
fn unsorted_levels_are_an_error() -> Result<(), Error> {
	let t = Nft::new(3)?;
	assert_eq!(
		t.project_to(&[2, 0]).unwrap_err(),
		NftError::LevelsNotIncreasing
	);
	assert_eq!(
		t.project_to(&[3]).unwrap_err(),
		NftError::NoSuchLevel {
			level: 3,
			num_levels: 3
		}
	);
	Ok(())
}

#[test]
fn compose_apply_invert() -> Result<(), Error> {
	let a_to_b = a_to_b();
	// b ↦ c, keep everything else.
	let (a, b, c) = (Symbol::from(b'a'), Symbol::from(b'b'), Symbol::from(b'c'));
	let mut b_to_c = Nft::new(2)?;
	let state = b_to_c.add_state();
	b_to_c.add_initial(state)?;
	b_to_c.add_final(state)?;
	b_to_c.add_transition(state, &[a, a], state)?;
	b_to_c.add_transition(state, &[b, c], state)?;
	let both = a_to_b.compose(&b_to_c)?;
	assert!(both.accepts(&[word_from_bytes(b"ab"), word_from_bytes(b"cc")]));
	assert!(!both.accepts(&[word_from_bytes(b"ab"), word_from_bytes(b"bb")]));

	let kept = a_to_b.compose_on(&b_to_c, 1, 0, SyncLevel::Keep)?;
	assert_eq!(kept.num_levels(), 3);
	assert!(kept.accepts(&[
		word_from_bytes(b"ab"),
		word_from_bytes(b"bb"),
		word_from_bytes(b"cc")
	]));

	let image = a_to_b.apply(&Nfa::from_regex("a*")?, 0)?.to_nfa();
	assert!(image.is_equivalent_to(&Nfa::from_regex("b*")?));
	let preimage = a_to_b.apply(&Nfa::from_regex("b")?, 1)?.to_nfa();
	assert!(preimage.is_equivalent_to(&Nfa::from_regex("a|b")?));

	let inverse = a_to_b.invert_levels();
	assert!(inverse.accepts(&[word_from_bytes(b"bb"), word_from_bytes(b"ab")]));
	assert!(inverse.invert_levels().is_equivalent_to(&a_to_b)?);
	Ok(())
}

#[test]
fn dont_care_matches_any_symbol() -> Result<(), Error> {
	let mut t = Nft::new(2)?;
	let q = t.add_state();
	t.add_initial(q)?;
	t.add_final(q)?;
	// Any symbol on tape 0, `x` on tape 1.
	t.add_transition(q, &[DONT_CARE, Symbol::from(b'x')], q)?;
	assert!(t.accepts(&[word_from_bytes(b"abc"), word_from_bytes(b"xxx")]));
	let sigma = Alphabet::from_symbols(word_from_bytes(b"ab"))?;
	let input_language = t.project_to(&[0])?.to_nfa_over(&sigma);
	assert!(input_language.is_equivalent_to(&Nfa::from_regex("(a|b)*")?));
	// Without an alphabet to expand it, DONT_CARE stays a symbol of its own.
	assert_eq!(t.project_to(&[0])?.to_nfa().used_symbols(), [DONT_CARE]);
	assert!(
		t.project_to(&[0])?
			.to_nfa_over(&Alphabet::new())
			.is_equivalent_to(&Nfa::empty_word())
	);
	Ok(())
}

#[test]
fn dont_care_overlaps_other_symbols_for_determinism() -> Result<(), Error> {
	let mut t = Nft::new(1)?;
	let (p, q, r) = (t.add_state(), t.add_state(), t.add_state());
	t.add_initial(p)?;
	t.add_final(q)?;
	t.add_final(r)?;
	t.add_transition(p, &[Symbol::from(b'a')], q)?;
	assert!(t.is_deterministic());
	// The encoding as an NFA keeps DONT_CARE a symbol of its own, so only the transducer sees the overlap.
	t.add_transition(p, &[DONT_CARE], r)?;
	assert!(t.to_nfa().is_deterministic());
	assert!(!t.is_deterministic());
	Ok(())
}

#[test]
fn identity_and_empty_word() -> Result<(), Error> {
	let nfa = Nfa::from_regex("ab*")?;
	let identity = Nft::identity(&nfa, 3)?;
	assert!(identity.accepts(&[
		word_from_bytes(b"abb"),
		word_from_bytes(b"abb"),
		word_from_bytes(b"abb")
	]));
	assert!(!identity.accepts(&[
		word_from_bytes(b"abb"),
		word_from_bytes(b"ab"),
		word_from_bytes(b"abb")
	]));
	let empty = Nft::empty_word(2)?;
	assert!(empty.accepts(&[word_from_bytes(b""), word_from_bytes(b"")]));
	assert!(!empty.accepts(&[word_from_bytes(b"a"), word_from_bytes(b"")]));
	Ok(())
}

#[test]
fn text_round_trips() -> Result<(), Error> {
	let t = a_to_b();
	let parsed: Nft = t.to_string().parse()?;
	assert!(parsed.is_equivalent_to(&t)?);

	let mut sigma = Alphabet::new();
	let named = Nft::parse_mata(
		"@NFT-explicit\n%Alphabet-auto\n%Initial q0\n%Final q0\n%Levels q0:0 q1:1\n%LevelsNum 2\nq0 a q1\nq1 b q0\n",
		&mut sigma,
	)?;
	let (a, b) = (sigma.symbol("a").unwrap(), sigma.symbol("b").unwrap());
	assert!(named.accepts(&[[a, a], [b, b]]));
	let text = named.display_with(&sigma).to_string();
	assert!(
		text.contains("q0 a q1") && text.contains("q1 b q0"),
		"{text}"
	);
	let again = Nft::parse_mata(&text, &mut sigma)?;
	assert!(again.is_equivalent_to(&named)?);
	assert!(named.dot().to_string().starts_with("digraph"));
	Ok(())
}

#[test]
fn malformed_transducers_are_rejected() {
	let cases = [
		// A level beyond %LevelsNum.
		"@NFT-explicit\n%Alphabet-auto\n%Initial q0\n%Levels q0:0 q1:5\n%LevelsNum 2\nq0 0 q1\nq1 0 q0\n",
		// An edge that goes back from level 1 to level 1.
		"@NFT-explicit\n%Alphabet-auto\n%Initial q0\n%Levels q0:0 q1:1 q2:1\n%LevelsNum 2\nq0 0 q1\nq1 0 q2\n",
		// An initial state on level 1.
		"@NFT-explicit\n%Alphabet-auto\n%Initial q1\n%Levels q0:0 q1:1\n%LevelsNum 2\nq0 0 q1\n",
		// No levels at all.
		"@NFT-explicit\n%Alphabet-auto\n%Initial q0\n%Levels q0:0\n%LevelsNum 0\n",
		// Not a transducer.
		"@NFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final q0\nq0 0 q0\n",
		// Malformed lines.
		"@NFT-explicit\n%Alphabet-auto\n%Initial q0\nq0 0\n",
		"@NFT-explicit\n%Alphabet-auto\n%Levels q0:x\n",
		"@NFT-explicit\n%Alphabet-auto\n%Levels q0:1:2\n",
		"@NFT-explicit\n%Alphabet-auto\n%LevelsNum -3\n",
	];
	for text in cases {
		assert!(text.parse::<Nft>().is_err(), "accepted {text:?}");
		assert!(
			Nft::parse_mata(text, &mut Alphabet::new()).is_err(),
			"accepted {text:?}"
		);
	}
}

#[test]
fn parsed_jumps_are_unwound() -> Result<(), Error> {
	// q0 -a-> q0 with both states on level 0 is a jump: it reads `a` on both tapes.
	let text = "@NFT-explicit\n%Alphabet-auto\n%Initial q0\n%Final q0\n%Levels q0:0\n%LevelsNum 2\nq0 7 q0\n";
	let t: Nft = text.parse()?;
	let seven = Symbol::new(7);
	assert!(t.accepts(&[[seven, seven], [seven, seven]]));
	assert!(!t.accepts(&[vec![seven], vec![]]));
	assert_eq!(t.num_edges(), 2, "one edge per level");
	for edge in t.edges() {
		assert_eq!(
			t.level(edge.target),
			t.level(edge.source).map(|level| (level + 1) % 2)
		);
	}
	Ok(())
}

#[test]
fn raw_round_trip_and_clone() -> Result<(), Error> {
	let t = a_to_b();
	let raw = t.clone().into_ffi();
	let back = Nft::from_ffi(raw)?;
	assert!(back.is_equivalent_to(&t)?);
	assert!(Nft::from_ffi(mata::sys::cxx::UniquePtr::null()).is_err());
	let mut copy = t.clone();
	let extra = copy.add_state();
	copy.add_initial(extra)?;
	copy.add_final(extra)?;
	assert_eq!(t.num_states(), 3);
	assert!(copy.accepts(&[word_from_bytes(b""), word_from_bytes(b"")]));
	Ok(())
}

#[test]
fn nft_is_send_and_sync() {
	fn assert_send_sync<T: Send + Sync>() {}
	assert_send_sync::<Nft>();
	assert_send_sync::<mata::nft::Transitions<'_>>();
	assert_send_sync::<mata::nft::Dot<'_, Nft>>();
}

#[test]
fn huge_level_counts_are_rejected() -> Result<(), Error> {
	// Claims 4294967295 levels.
	let text = "@NFT-explicit\n%LevelsNum 4294967295\nq99999:1 q99999:1 -1\n";
	let error = Nft::parse_mata(text, &mut Alphabet::new()).unwrap_err();
	assert!(
		error
			.message()
			.contains("more than the 1024 supported levels allow"),
		"{error}"
	);
	assert!(text.parse::<Nft>().is_err());
	let max = Nft::new(mata::nft::MAX_LEVELS)?;
	assert_eq!(max.num_levels(), 1024);
	Ok(())
}

/// Mata decides inclusion on the interleaved encodings, where epsilon is a symbol: two transducers for the
/// same relation that read epsilon at different places are not included in each other.
#[test]
fn inclusion_compares_encodings() -> Result<(), Error> {
	let (a, b) = (Symbol::from(b'a'), Symbol::from(b'b'));
	let mut split = Nft::new(2)?;
	let (start, middle, end) = (split.add_state(), split.add_state(), split.add_state());
	split.add_initial(start)?;
	split.add_final(end)?;
	split.add_transition(start, &[a, Symbol::EPSILON], middle)?;
	split.add_transition(middle, &[Symbol::EPSILON, b], end)?;
	let mut joint = Nft::new(2)?;
	let (source, target) = (joint.add_state(), joint.add_state());
	joint.add_initial(source)?;
	joint.add_final(target)?;
	joint.add_transition(source, &[a, b], target)?;
	assert!(split.accepts(&[[a], [b]]) && joint.accepts(&[[a], [b]]));
	assert!(!split.is_included_in(&joint)? && !joint.is_included_in(&split)?);
	assert!(split.intersection(&joint)?.is_lang_empty());
	// Each is still included in, and equivalent to, itself.
	assert!(split.is_included_in(&split)? && joint.is_equivalent_to(&joint)?);
	Ok(())
}

#[test]
fn reverse_determinize_remove_epsilon() -> Result<(), Error> {
	let (a, b) = (Symbol::from(b'a'), Symbol::from(b'b'));
	let mut nft = Nft::new(2)?;
	let (start, middle, end) = (nft.add_state(), nft.add_state(), nft.add_state());
	nft.add_initial(start)?;
	nft.add_final(end)?;
	nft.add_path(
		start,
		&[word_from_bytes(b"ab"), word_from_bytes(b"b")],
		middle,
	)?;
	nft.add_transition(middle, &[Symbol::EPSILON, Symbol::EPSILON], end)?;
	nft.add_transition(middle, &[a, b], end)?;
	assert!(nft.accepts(&[word_from_bytes(b"ab"), word_from_bytes(b"b")]));
	let reversed = nft.reverse();
	assert!(reversed.accepts(&[word_from_bytes(b"ba"), word_from_bytes(b"b")]));
	assert!(reversed.accepts(&[word_from_bytes(b"aba"), word_from_bytes(b"bb")]));
	assert!(!reversed.accepts(&[word_from_bytes(b"ab"), word_from_bytes(b"b")]));
	let deterministic = nft.determinize();
	assert!(deterministic.is_equivalent_to(&nft)?);
	assert_eq!(deterministic.initial_states().len(), 1);
	let mut without_epsilon = nft.clone();
	without_epsilon.remove_epsilon();
	assert!(without_epsilon.accepts(&[word_from_bytes(b"ab"), word_from_bytes(b"b")]));
	let reads_epsilon_on_both_tapes = without_epsilon.edges().any(|first| {
		without_epsilon.level(first.source) == Some(0)
			&& first.symbol == Symbol::EPSILON
			&& without_epsilon
				.edges()
				.any(|second| second.source == first.target && second.symbol == Symbol::EPSILON)
	});
	assert!(!reads_epsilon_on_both_tapes);
	Ok(())
}

/// A one-transition transducer from an initial to a final state, with one transition per element of
/// `reads` (one symbol per level each).
fn single(levels: usize, reads: &[&[Symbol]]) -> Nft {
	let mut t = Nft::new(levels).unwrap();
	let (p, q) = (t.add_state(), t.add_state());
	t.add_initial(p).unwrap();
	t.add_final(q).unwrap();
	for symbols in reads {
		t.add_transition(p, symbols, q).unwrap();
	}
	t
}

#[test]
fn dont_care_is_any_symbol_in_inclusion() -> Result<(), Error> {
	// "Any symbol" is more than `a`, even where no other symbol occurs.
	let (a, x) = (Symbol::from(b'a'), Symbol::from(b'x'));
	let any = single(1, &[&[DONT_CARE]]);
	let only_a = single(1, &[&[a]]);
	assert!(only_a.is_included_in(&any)?);
	assert!(!any.is_included_in(&only_a)?);
	assert!(!any.is_equivalent_to(&only_a)?);
	let any_x = single(2, &[&[DONT_CARE, x]]);
	let a_or_x_then_x = single(2, &[&[a, x], &[x, x]]);
	assert!(a_or_x_then_x.is_included_in(&any_x)?);
	assert!(!any_x.is_included_in(&a_or_x_then_x)?);
	Ok(())
}

#[test]
fn compose_with_a_dead_end_on_the_synchronized_level() -> Result<(), Error> {
	// `q8 <dntcr> q14` ends on level 1 without outgoing edges: a dead end for the fast composition.
	let nft: Nft =
		"@NFT-explicit\n%Alphabet-auto\n%Initial q2\n%Levels q10:1 q14:1\n%LevelsNum 2\n\
                    q2 <eps> q7\nq7 2 q8\nq8 2 q10\nq8 <dntcr> q14\nq10 <eps> q11\n"
			.parse()?;
	let nfa: Nfa = "@NFA-explicit\n%Alphabet-auto\n%Initial q0\nq0 2 q0\n".parse()?;
	assert!(nft.restrict(&nfa, 1)?.is_lang_empty());
	assert!(nft.apply(&nfa, 1)?.is_lang_empty());
	assert!(nft.compose(&nft)?.is_lang_empty());
	Ok(())
}

#[test]
fn level_numbers_are_range_checked() {
	// 4294967297 must not wrap around to 1.
	for text in [
		"@NFT-explicit\n%LevelsNum 4294967297\n%Initial q0\n%Final q0\n",
		"@NFT-explicit\n%LevelsNum 2\n%Levels q0:0 q1:4294967296\n%Initial q0\n%Final q0\nq0 1 q1\nq1 1 q0\n",
		"@NFT-explicit\n%LevelsNum two\n%Initial q0\n",
	] {
		assert!(
			Nft::parse_mata(text, &mut Alphabet::new()).is_err(),
			"{text}"
		);
	}
}

#[test]
fn determinize_gives_one_initial_state() -> Result<(), Error> {
	assert_eq!(Nft::new(2)?.determinize().initial_states().len(), 1);
	assert_eq!(a_to_b().determinize().initial_states().len(), 1);
	Ok(())
}

#[test]
fn flags_edges_and_printing() {
	let mut t = a_to_b();
	let q = t.initial_states()[0];
	assert!(t.is_initial(q) && t.is_final(q));
	assert_eq!(t.edges_from(q).count(), 2);
	assert_eq!(t.edges_from(State::new(99)).count(), 0);
	assert!(!t.is_initial(State::new(99)));

	let debug = format!("{t:?}");
	assert!(
		debug.contains("q0 97 q1") && debug.contains("initial: [q0]"),
		"{debug}"
	);
	let dot = t.dot().ascii_labels(true).to_string();
	assert!(dot.starts_with("digraph"), "{dot}");

	assert!(t.remove_final(q) && !t.is_final(q) && !t.remove_final(q));
	assert!(t.remove_initial(q) && !t.is_initial(q));
	assert!(t.is_lang_empty());
}
