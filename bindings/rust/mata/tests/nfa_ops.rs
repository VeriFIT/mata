//! The further NFA operations of the library, on hand-written cases.

use mata::{
	Alphabet, Direction, Error, Nfa, ProductFinalCondition, Requirement, Symbol, word_from_bytes,
};

#[test]
fn concatenation_over_epsilon_keeps_the_joining_epsilon_transitions() -> Result<(), Error> {
	let (a, b) = (Nfa::from_regex("a")?, Nfa::from_regex("b")?);
	let joined = a.concat_over_epsilon(&b);
	assert!(joined.has_epsilon_transitions());
	assert!(!a.concat(&b).has_epsilon_transitions());
	assert!(joined.is_equivalent_to(&a.concat(&b)));
	assert!(joined.accepts(&word_from_bytes("ab")) && !joined.accepts(&[Symbol::from(b'a')]));
	Ok(())
}

#[test]
fn as_dfa_is_a_deterministic_automaton_of_the_same_language() -> Result<(), Error> {
	let nfa = Nfa::from_regex("a(b|b)c")?;
	let dfa = nfa.as_dfa();
	assert!(dfa.is_deterministic());
	assert!(dfa.is_equivalent_to(&nfa));
	Ok(())
}

#[test]
fn concatenate_nth_power_repeats_the_language() {
	let word = Nfa::from_word(&[Symbol::from(b'a'), Symbol::from(b'b')]);
	let squared = word.concatenate_nth_power(2);
	assert!(squared.accepts(&word_from_bytes("abab")));
	assert!(!squared.accepts(&word_from_bytes("ab")));
	assert!(word.concatenate_nth_power(0).accepts(&[]));
}

/// A complete automaton for one word over `alphabet`: the word, plus a sink.
fn complete(word: &str, alphabet: &Alphabet) -> Nfa {
	let symbols: Vec<Symbol> = word.bytes().map(Symbol::from).collect();
	let mut nfa = Nfa::from_word(&symbols);
	nfa.make_complete(alphabet);
	nfa
}

#[test]
fn union_det_complete_is_the_union_of_complete_deterministic_automata() -> Result<(), Error> {
	let alphabet = Alphabet::from_symbols([Symbol::from(b'a'), Symbol::from(b'b')])?;
	let (a, b) = (complete("a", &alphabet), complete("b", &alphabet));
	let union = a.union_det_complete(&b, &alphabet)?;
	assert!(union.is_deterministic());
	assert!(union.accepts(&word_from_bytes("a")) && union.accepts(&word_from_bytes("b")));
	assert!(!union.accepts(&word_from_bytes("ab")));

	let incomplete = Nfa::from_word(&[Symbol::from(b'a')]);
	let error = incomplete.union_det_complete(&a, &alphabet).unwrap_err();
	assert_eq!(error.requirement(), Requirement::Complete);
	// Two initial states.
	let nondeterministic = complete("a", &alphabet).union(&complete("b", &alphabet));
	let error = a
		.union_det_complete(&nondeterministic, &alphabet)
		.unwrap_err();
	assert_eq!(error.requirement(), Requirement::Deterministic);
	// A transition under a symbol outside the alphabet is not complete over it, not a panic.
	let outside = complete("c", &Alphabet::from_symbols([Symbol::from(b'c')])?);
	let error = outside.union_det_complete(&a, &alphabet).unwrap_err();
	assert_eq!(error.requirement(), Requirement::Complete);
	Ok(())
}

#[test]
fn product_takes_the_final_state_condition_of_the_pairs() -> Result<(), Error> {
	let alphabet = Alphabet::from_symbols([Symbol::from(b'a'), Symbol::from(b'b')])?;
	let (a, b) = (complete("a", &alphabet), complete("b", &alphabet));
	let intersection = a.product(&b, ProductFinalCondition::And, Symbol::EPSILON);
	assert!(intersection.is_lang_empty());
	let union = a.product(&b, ProductFinalCondition::Or, Symbol::EPSILON);
	assert!(union.accepts(&word_from_bytes("b")) && union.accepts(&word_from_bytes("a")));
	Ok(())
}

#[test]
fn quotients_remove_the_prefixes_and_the_suffixes() {
	let language = Nfa::from_word(&[Symbol::from(b'a'), Symbol::from(b'b')]);
	let prefixes = Nfa::from_word(&[Symbol::from(b'a')]);
	let suffixes = Nfa::from_word(&[Symbol::from(b'b')]);
	assert!(
		language
			.left_quotient(&prefixes, Symbol::EPSILON)
			.accepts(&[Symbol::from(b'b')])
	);
	assert!(
		language
			.right_quotient(&suffixes, Symbol::EPSILON)
			.accepts(&[Symbol::from(b'a')])
	);
}

#[test]
fn decode_utf8_turns_the_bytes_of_a_symbol_into_one_symbol() -> Result<(), Error> {
	let bytes = Nfa::from_word(&[Symbol::new(0xC4), Symbol::new(0x85)]);
	let decoded = bytes.decode_utf8()?;
	assert!(decoded.accepts(&[Symbol::new(0x105)]));
	for broken in [
		// The last byte is not a continuation byte.
		vec![0xC4, u32::from(b'A')],
		// A middle byte is not one: E1 41 80 is not UTF-8, though its last byte is a continuation byte.
		vec![0xE1, u32::from(b'A'), 0x80],
		vec![0xF0, 0x90, u32::from(b'A'), 0x80],
		// A sequence starts with a continuation byte, or a symbol is not a byte.
		vec![0x80, 0x80],
		vec![0x1C4, 0x85],
	] {
		let nfa = Nfa::from_word(&broken.into_iter().map(Symbol::new).collect::<Vec<_>>());
		assert_eq!(
			nfa.decode_utf8().unwrap_err().requirement(),
			Requirement::Utf8
		);
	}
	Ok(())
}

#[test]
fn one_letter_automaton_renames_every_symbol() {
	let word = Nfa::from_word(&[Symbol::from(b'a'), Symbol::from(b'b')]);
	let abstracted = word.one_letter_automaton(Symbol::from(b'x'));
	assert!(abstracted.accepts(&[Symbol::from(b'x'), Symbol::from(b'x')]));
	assert!(!abstracted.accepts(&[Symbol::from(b'a'), Symbol::from(b'b')]));
}

#[test]
fn collapse_by_length_over_approximates_until_the_length_separates_states() -> Result<(), Error> {
	let four = Nfa::from_regex("aaaa")?;
	let coarse = four.collapse_by_length(1, false, Direction::Forward);
	assert!(four.is_included_in(&coarse));
	assert!(coarse.accepts(&word_from_bytes("aa")) && !coarse.accepts(&word_from_bytes("a")));
	assert!(
		four.collapse_by_length(4, false, Direction::Forward)
			.is_equivalent_to(&four)
	);
	let prefixes = four.collapse_by_length(4, true, Direction::Forward);
	assert!(prefixes.accepts(&word_from_bytes("aa")));
	let backward = Nfa::from_regex("aaaab")?.collapse_by_length(1, false, Direction::Backward);
	assert!(backward.accepts(&word_from_bytes("aab")));
	Ok(())
}

#[test]
fn collapse_by_length_reads_epsilon_as_the_empty_word() -> Result<(), Error> {
	// Through the binding, an epsilon transition is no letter: at a length that separates every state, the result
	// is the language, not the epsilon-free words around it.
	let joined = Nfa::from_regex("a")?.concat_over_epsilon(&Nfa::from_regex("b")?);
	let abstraction = joined.collapse_by_length(3, false, Direction::Forward);
	assert!(abstraction.is_equivalent_to(&Nfa::from_regex("ab")?));
	assert!(!abstraction.accepts(&word_from_bytes("b")));
	Ok(())
}

#[test]
fn collapse_by_predicates_over_approximates_and_preserves_disjointness() -> Result<(), Error> {
	// Every accepted word has `a` in the penultimate position; the bad predicate requires `b` there.
	let subject = Nfa::from_regex("(a|b)*a(a|b)")?;
	let bad = Nfa::from_regex("(a|b)*b(a|b)")?;
	let abstraction = subject.collapse_by_predicates(&[&bad]);
	assert!(subject.is_included_in(&abstraction));
	assert!(abstraction.is_disjoint_from(&bad));
	Ok(())
}

#[test]
fn collapse_by_predicates_granularity_and_direction_control_precision() -> Result<(), Error> {
	let suffix = Nfa::from_regex("ac")?;
	let suffix_predicate = Nfa::from_regex("ac")?;
	let states = suffix.collapse_by_predicates(&[&suffix_predicate]);
	let languages =
		suffix.collapse_by_predicates_with(&[&suffix_predicate], true, Direction::Forward);
	assert!(states.is_equivalent_to(&suffix));
	assert!(suffix.is_included_in(&languages));
	assert!(!languages.accepts(&[]) && languages.accepts(&word_from_bytes("a")));
	assert!(languages.accepts(&word_from_bytes("acc")));
	assert!(!states.accepts(&word_from_bytes("a")));

	let prefix = Nfa::from_regex("ca")?;
	let prefix_predicate = Nfa::from_regex("ca")?;
	let backward =
		prefix.collapse_by_predicates_with(&[&prefix_predicate], true, Direction::Backward);
	assert!(prefix.is_included_in(&backward));
	assert!(backward.accepts(&word_from_bytes("a")));
	assert!(backward.accepts(&word_from_bytes("cca")));
	assert!(!backward.accepts(&word_from_bytes("c")));
	Ok(())
}

#[test]
fn collapse_by_predicates_accepts_empty_lists_and_removes_epsilon() -> Result<(), Error> {
	let empty_word = Nfa::empty_word();
	assert!(
		empty_word
			.collapse_by_predicates(&[])
			.is_equivalent_to(&empty_word)
	);
	let no_states = Nfa::new();
	assert!(
		empty_word
			.collapse_by_predicates(&[&no_states])
			.is_equivalent_to(&empty_word)
	);

	let subject = Nfa::from_regex("a")?.concat_over_epsilon(&Nfa::from_regex("b")?);
	let predicate = Nfa::from_regex("a")?.concat_over_epsilon(&Nfa::from_regex("b")?);
	let abstraction = subject.collapse_by_predicates(&[&predicate]);
	assert!(abstraction.is_equivalent_to(&Nfa::from_regex("ab")?));
	assert!(!abstraction.accepts(&word_from_bytes("b")));
	Ok(())
}

#[test]
fn is_flat_separates_one_cycle_per_state_from_two() -> Result<(), Error> {
	assert!(Nfa::from_word(&[Symbol::from(b'a'), Symbol::from(b'b')]).is_flat());
	assert!(!Nfa::from_regex("(a|b)*")?.is_flat());
	Ok(())
}

#[test]
fn is_in_language_prefix_finds_an_accepted_prefix_of_the_word() {
	let language = Nfa::from_word(&word_from_bytes("abc"));
	assert!(language.is_in_language_prefix(&word_from_bytes("abcd"), false));
	assert!(!language.is_in_language_prefix(&word_from_bytes("ab"), false));
	assert!(!language.is_in_language_prefix(&word_from_bytes("xbc"), false));
}

#[test]
fn find_word_from_complement_finds_a_word_outside_the_language() -> Result<(), Error> {
	let alphabet = Alphabet::from_symbols([Symbol::from(b'a'), Symbol::from(b'b')])?;
	let language = Nfa::from_word(&word_from_bytes("ab"));
	let word = language
		.find_word_from_complement(&alphabet)
		.expect("the language is not universal");
	assert!(!language.accepts(&word));
	assert!(
		Nfa::sigma_star(&alphabet)
			.find_word_from_complement(&alphabet)
			.is_none()
	);
	Ok(())
}

#[test]
fn find_word_from_language_difference_witnesses_the_difference() -> Result<(), Error> {
	let included = Nfa::from_word(&word_from_bytes("a"));
	let excluded = Nfa::from_word(&word_from_bytes("ab"));
	let word = included
		.find_word_from_language_difference(&excluded)
		.expect("the first language is not included in the second");
	assert!(included.accepts(&word) && !excluded.accepts(&word));
	let alphabet = Alphabet::from_symbols([Symbol::from(b'a'), Symbol::from(b'b')])?;
	assert!(
		included
			.find_word_from_language_difference(&Nfa::sigma_star(&alphabet))
			.is_none()
	);
	Ok(())
}

#[test]
fn random_tabakov_vardi_is_reproducible_from_its_seed() -> Result<(), Error> {
	let random = Nfa::random_tabakov_vardi(5, 2, 1.0, 0.5, Some(42))?;
	assert_eq!(random.num_states(), 5);
	assert!(random.is_identical(&Nfa::random_tabakov_vardi(5, 2, 1.0, 0.5, Some(42))?));
	assert!(Nfa::random_tabakov_vardi(5, 2, 9.0, 0.5, Some(1)).is_err());
	Ok(())
}
