//! Behaviour of the public API on hand-written cases.

use mata::{
	Alphabet, AlphabetError, CheckAlgorithm, Complementation, Determinization, Error,
	LimitExceeded, Limits, Minimization, Nfa, Side, State, Symbol, Transition, word_from_bytes,
};

fn q(index: u32) -> State {
	State::new(index)
}

#[test]
fn states_and_flags() -> Result<(), Error> {
	let mut nfa = Nfa::new();
	assert_eq!(nfa.num_states(), 0);
	let (q0, q1) = (nfa.add_state(), nfa.add_state());
	assert_eq!((q0, q1), (q(0), q(1)));
	assert_eq!(nfa.states().collect::<Vec<_>>(), [q0, q1]);

	nfa.add_initial(q0)?;
	nfa.add_final(q1)?;
	nfa.add_final(q0)?;
	assert!(nfa.is_initial(q0) && !nfa.is_initial(q1));
	assert_eq!(nfa.initial_states(), [q0]);
	let mut finals = nfa.final_states().to_vec();
	finals.sort();
	assert_eq!(finals, [q0, q1]);

	assert!(nfa.remove_final(q0));
	assert!(!nfa.remove_final(q0));
	assert_eq!(nfa.final_states(), [q1]);
	assert!(!nfa.is_final(q(1_000_000)));
	assert!(!nfa.remove_initial(q(u32::MAX)));
	Ok(())
}

#[test]
fn missing_states_are_errors() {
	let mut nfa = Nfa::with_states(2);
	let error = nfa.add_transition(q(0), Symbol::new(0), q(2)).unwrap_err();
	assert_eq!(
		error.to_string(),
		"state q2 does not exist (the automaton has 2 states)"
	);
	assert_eq!(nfa.add_initial(q(u32::MAX)).unwrap_err().state, q(u32::MAX));
	assert_eq!(nfa.add_final(q(2)).unwrap_err().state, q(2));
}

#[test]
fn add_transition_reports_the_state() {
	let mut nfa = Nfa::with_states(1);
	let error = nfa
		.add_transition(q(0), Symbol::new(3), q(u32::MAX))
		.unwrap_err();
	assert_eq!((error.state, error.num_states), (q(u32::MAX), 1));
	assert_eq!(nfa.num_states(), 1, "nothing was allocated");
	assert_eq!(nfa.num_transitions(), 0);
	let error: Error = error.into();
	assert!(error.to_string().contains("q4294967295"));
}

#[test]
fn transitions_add_remove_iterate() -> Result<(), Error> {
	let (a, b) = (Symbol::from(b'a'), Symbol::from(b'b'));
	let mut nfa = Nfa::with_states(3);
	nfa.add_transitions([
		Transition::new(q(2), a, q(0)),
		Transition::new(q(0), b, q(1)),
		Transition::new(q(0), a, q(2)),
		Transition::new(q(0), a, q(1)),
		Transition::new(q(0), a, q(1)),
	])?;
	assert_eq!(nfa.num_transitions(), 4);
	let listed: Vec<String> = nfa.transitions().map(|t| t.to_string()).collect();
	assert_eq!(listed, ["q0 97 q1", "q0 97 q2", "q0 98 q1", "q2 97 q0"]);
	assert_eq!(nfa.successors(q(0), a), [q(1), q(2)]);
	assert_eq!(nfa.successors(q(1), a), []);
	assert_eq!(nfa.successors(q(u32::MAX), a), []);
	assert_eq!(nfa.transitions_from(q(2)).count(), 1);
	assert_eq!(nfa.transitions_from(q(7)).count(), 0);
	assert_eq!(nfa.used_symbols(), [a, b]);

	assert!(nfa.remove_transition(q(0), a, q(2)));
	assert!(!nfa.remove_transition(q(0), a, q(2)), "already removed");
	assert!(
		!nfa.remove_transition(q(1), b, q(0)),
		"symbol absent at the source"
	);
	assert!(!nfa.remove_transition(q(9), b, q(0)), "no such source");
	assert!(!nfa.contains_transition(q(0), a, q(2)));
	assert_eq!(nfa.num_transitions(), 3);
	Ok(())
}

#[test]
fn epsilon_is_handled_as_the_empty_word() -> Result<(), Error> {
	// ε·a, with the language of `a`.
	let a = Symbol::from(b'a');
	let mut eps_a = Nfa::with_states(3);
	eps_a.add_initial(q(0))?;
	eps_a.add_final(q(2))?;
	eps_a.add_transition(q(0), Symbol::EPSILON, q(1))?;
	eps_a.add_transition(q(1), a, q(2))?;
	assert!(eps_a.has_epsilon_transitions());
	assert!(!eps_a.is_deterministic());
	assert!(eps_a.accepts(&[a]));

	let just_a = Nfa::from_word(&[a]);
	assert!(just_a.is_included_in(&eps_a));
	assert!(eps_a.is_equivalent_to(&just_a));
	let dfa = eps_a.determinize();
	assert!(dfa.is_deterministic() && !dfa.has_epsilon_transitions());
	assert!(dfa.is_equivalent_to(&just_a));
	assert_eq!(eps_a.minimize().num_states(), 2);
	let sigma = Alphabet::from_symbols([a])?;
	assert!(eps_a.complement(&sigma).accepts(&[]));
	assert!(!eps_a.complement(&sigma).accepts(&[a]));
	let words: Vec<_> = eps_a.words_up_to(4).collect();
	assert_eq!(words, [vec![a]]);
	Ok(())
}

/// (a|b)* a (a|b)^k over a = 0 and b = 1.
fn nth_symbol_from_the_end(k: u32) -> Result<Nfa, Error> {
	let (a, b) = (Symbol::new(0), Symbol::new(1));
	let mut nfa = Nfa::with_states(k as usize + 2);
	nfa.add_initial(q(0))?;
	nfa.add_final(q(k + 1))?;
	nfa.add_transition(q(0), a, q(0))?;
	nfa.add_transition(q(0), b, q(0))?;
	nfa.add_transition(q(0), a, q(1))?;
	for state in 1..=k {
		nfa.add_transition(q(state), a, q(state + 1))?;
		nfa.add_transition(q(state), b, q(state + 1))?;
	}
	Ok(nfa)
}

#[test]
fn determinization_algorithms() -> Result<(), Error> {
	// The same DFA up to the numbering of its states; the default is Four Russians.
	let nfa = nth_symbol_from_the_end(3)?;
	let four_russians = nfa.determinize_with(Determinization::FourRussians);
	let classical = nfa.determinize_with(Determinization::Classical);
	for dfa in [&four_russians, &classical] {
		assert!(dfa.is_deterministic());
		assert_eq!(dfa.num_states(), 16);
		assert!(dfa.is_equivalent_to(&nfa));
	}
	assert!(nfa.determinize().is_identical(&four_russians));
	Ok(())
}

#[test]
fn determinize_the_largest_symbol() -> Result<(), Error> {
	// The largest symbol but epsilon: its bit in the used symbols must not overflow when usize has
	// 32 bits (wasm32).
	let symbol = Symbol::new(u32::MAX - 1);
	let mut nfa = Nfa::with_states(2);
	nfa.add_initial(q(0))?;
	nfa.add_final(q(1))?;
	nfa.add_transition(q(0), symbol, q(0))?;
	nfa.add_transition(q(0), symbol, q(1))?;
	for algorithm in [Determinization::FourRussians, Determinization::Classical] {
		let dfa = nfa.determinize_with(algorithm);
		assert!(dfa.is_deterministic());
		assert_eq!(dfa.num_states(), 2, "{algorithm:?}");
		assert!(dfa.accepts(&[symbol]) && dfa.accepts(&[symbol, symbol]) && !dfa.accepts(&[]));
	}
	Ok(())
}

#[test]
fn hopcroft_works_on_nondeterministic_input() -> Result<(), Error> {
	// Mata's Hopcroft minimization needs a trimmed DFA.
	let nfa = Nfa::from_regex("(a|b)*a(a|b)")?;
	assert!(!nfa.is_deterministic());
	let hopcroft = nfa.minimize_with(Minimization::Hopcroft);
	assert!(hopcroft.is_deterministic());
	assert!(hopcroft.is_equivalent_to(&nfa));
	assert_eq!(hopcroft.num_states(), nfa.minimize().num_states());
	assert_eq!(hopcroft.num_states(), 4);
	assert!(hopcroft.is_minimal());
	assert!(!nfa.is_minimal());

	// The empty language: a single non-final initial state, from both algorithms.
	let empty = Nfa::with_states(3);
	for algorithm in [Minimization::Brzozowski, Minimization::Hopcroft] {
		let minimal = empty.minimize_with(algorithm);
		assert_eq!(
			(minimal.num_states(), minimal.initial_states().len()),
			(1, 1),
			"{algorithm:?}"
		);
		assert!(minimal.final_states().is_empty() && minimal.num_transitions() == 0);
		assert!(minimal.is_minimal());
	}
	Ok(())
}

#[test]
fn common_words() -> Result<(), Error> {
	let ends_with_a = Nfa::from_regex("(a|b)*a")?;
	let starts_with_b = Nfa::from_regex("b(a|b)*")?;
	let word = ends_with_a.find_common_word(&starts_with_b).unwrap();
	assert!(ends_with_a.accepts(&word) && starts_with_b.accepts(&word));
	assert!(!ends_with_a.is_disjoint_from(&starts_with_b));
	assert!(ends_with_a.is_disjoint_from(&Nfa::from_regex("(a|b)*b")?));

	// Epsilon transitions move one automaton only and are not part of the common word.
	let mut eps_a = Nfa::with_states(3);
	eps_a.add_initial(q(0))?;
	eps_a.add_transition(q(0), Symbol::EPSILON, q(1))?;
	eps_a.add_transition(q(1), Symbol::from(b'a'), q(2))?;
	eps_a.add_final(q(2))?;
	let word = eps_a
		.find_common_word(&Nfa::from_word(&word_from_bytes(b"a")))
		.unwrap();
	assert_eq!(word, word_from_bytes(b"a"));
	Ok(())
}

#[test]
fn regex_construction_and_errors() -> Result<(), Error> {
	let nfa = Nfa::from_regex("h(e|a)llo")?;
	assert!(nfa.accepts(&word_from_bytes(b"hello")) && nfa.accepts(&word_from_bytes(b"hallo")));
	assert!(!nfa.accepts(&word_from_bytes(b"hullo")));
	assert!(!nfa.has_epsilon_transitions());

	let error = Nfa::from_regex("(ab").unwrap_err();
	assert!(error.message().contains("missing )"), "{error}");
	Ok(())
}

#[test]
fn oversized_regex_is_an_error() {
	// Beyond RE2's memory budget.
	let pattern = "[a-z]{1000}".repeat(1000);
	let error = Nfa::from_regex(&pattern).unwrap_err();
	assert!(error.message().contains("memory budget"), "{error}");
}

#[test]
fn malformed_mata_input_is_an_error() {
	let cases = [
		"",
		"@NFA-explicit\n%Alphabet-auto\n%Initial q0\nq0\n",
		"@NFA-explicit\n%Alphabet-auto\n%Initial q0\nq0 1\n",
		"@NFA-explicit\n%Alphabet-auto\n%Initial q0 q1\n%Final\nq0 a b q1\n",
		"@NFA-explicit\n%Initial (q0\n",
		"@NFT-explicit\n%Alphabet-auto\n%Initial q0\n%Final q0\n",
		"@NFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final q0\n@NFA-explicit\n%Alphabet-auto\n%Initial q0\n",
		"%Initial q0\n",
		"@\n",
		"@NFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final q0\nq0 \"a q0\n",
	];
	for text in cases {
		let mut sigma = Alphabet::new();
		assert!(
			Nfa::parse_mata(text, &mut sigma).is_err(),
			"accepted {text:?}"
		);
		assert!(text.parse::<Nfa>().is_err(), "accepted {text:?}");
	}
	let error = "@NFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final q1\nq0 a q1\n"
		.parse::<Nfa>()
		.unwrap_err();
	assert!(error.message().contains("Cannot translate"), "{error}");
}

#[test]
fn numeric_names_are_rejected() -> Result<(), Error> {
	// A name that consists only of digits would print as the number of a symbol, so a round trip
	// through the `.mata` format would resolve it to that symbol and change the language (issue:
	// alphabet {42, 43} + the name "42" for a new symbol used to re-write [42, 42] as [44, 44]).
	let mut sigma = Alphabet::from_symbols([Symbol::new(42), Symbol::new(43)])?;
	assert_eq!(
		sigma.insert_name("42"),
		Err(AlphabetError::NumericName { name: "42".into() })
	);
	assert_eq!(
		Alphabet::from_names(["7"]),
		Err(AlphabetError::NumericName { name: "7".into() })
	);
	// `insert_named` binds a numeric name (the "bytes" script alphabet does), so it must reload.
	let mut bytes = Alphabet::new();
	bytes.insert_named("1", Symbol::new(49))?;
	#[cfg(feature = "serde")]
	{
		let text = serde_json::to_string(&bytes).unwrap();
		assert_eq!(serde_json::from_str::<Alphabet>(&text).unwrap(), bytes);
	}
	// The parser resolves a name before a number, so the `.mata` text keeps symbol 49.
	let nfa = Nfa::from_word(&[Symbol::new(49)]);
	let back = Nfa::parse_mata(&nfa.display_with(&bytes).to_string(), &mut bytes)?;
	assert!(back.accepts(&[Symbol::new(49)]));
	assert!(!back.accepts(&[Symbol::new(1)]));
	// The empty name is digits-and-nothing: not numeric, still rejected by round-trip rules as before.
	let mut empty = Alphabet::new();
	let symbol = empty.insert_name("")?;
	assert_eq!(u32::from(symbol), 0);
	Ok(())
}

#[test]
fn insert_named_rejects_the_epsilon_name() {
	let mut sigma = Alphabet::new();
	assert_eq!(
		sigma.insert_named("<eps>", Symbol::new(5)),
		Err(AlphabetError::Epsilon)
	);
	assert_eq!(sigma.name(Symbol::new(5)), None);
	assert!(sigma.is_empty());
}

#[test]
fn a_numeric_name_bound_by_insert_named_is_found() -> Result<(), Error> {
	let mut sigma = Alphabet::new();
	sigma.insert_named("0", Symbol::new(48))?;
	assert_eq!(sigma.insert_name("0"), Ok(Symbol::new(48)));
	assert_eq!(
		sigma.insert_name("1"),
		Err(AlphabetError::NumericName { name: "1".into() })
	);
	Ok(())
}

#[test]
fn a_numeric_name_never_shadows_an_unnamed_symbol() -> Result<(), Error> {
	// Printing writes an unnamed symbol as its number, so that number cannot also name another symbol:
	// the transition over symbol 1 would read back over symbol 49.
	let mut sigma = Alphabet::from_symbols([Symbol::new(1)])?;
	assert_eq!(
		sigma.insert_named("1", Symbol::new(49)),
		Err(AlphabetError::NumericName { name: "1".into() })
	);
	let nfa = Nfa::from_word(&[Symbol::new(1)]);
	let back = Nfa::parse_mata(&nfa.display_with(&sigma).to_string(), &mut sigma)?;
	assert!(back.accepts(&[Symbol::new(1)]));

	let mut named = Alphabet::new();
	named.insert_named("1", Symbol::new(49))?;
	assert_eq!(
		named.insert(Symbol::new(1)),
		Err(AlphabetError::NumericName { name: "1".into() })
	);
	// Naming the symbol itself by its own number is no collision.
	sigma.insert_named("7", Symbol::new(7))?;
	#[cfg(feature = "serde")]
	assert!(serde_json::from_str::<Alphabet>(r#"[[49,"1"],[1,null]]"#).is_err());
	Ok(())
}

#[test]
fn parse_with_names_shares_symbols() -> Result<(), Error> {
	let mut sigma = Alphabet::from_names(["b"])?;
	let texts = [
		"@NFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final q1\nq0 a q1\nq1 b q1\n",
		"@NFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final q1\nq0 c q1\nq0 a q1\n",
	];
	let first = Nfa::parse_mata(texts[0], &mut sigma)?;
	let second = Nfa::parse_mata(texts[1], &mut sigma)?;
	assert_eq!(sigma.symbol("b"), Some(Symbol::new(0)), "kept");
	assert_eq!(sigma.symbol("a"), Some(Symbol::new(1)));
	assert_eq!(sigma.symbol("c"), Some(Symbol::new(2)));
	let a = sigma.encode(["a"]).unwrap();
	assert!(first.accepts(&a) && second.accepts(&a));
	assert_eq!(first.intersection(&second).find_shortest_word(), Some(a));

	let both = Nfa::parse_mata_all(&texts.concat(), &mut sigma)?;
	assert_eq!(both.len(), 2);
	assert_eq!(sigma.len(), 3);
	Ok(())
}

#[test]
fn parse_bitvector_sections() -> Result<(), Error> {
	let text =
		"@NFA-bits\n%Alphabet-auto\n%Initial q0\n%Final q1\nq0 (a0 & !a1) q1\nq1 (a0 | a1) q1\n";
	let mut sigma = Alphabet::new();
	let nfa = Nfa::parse_mata(text, &mut sigma)?;
	// The predicates a0 & !a1 and a0 | a1 split the valuations into the minterms a0 & !a1 (both hold),
	// a1 (only the second holds) and !a0 & !a1 (neither, unused): q0 reads the first, q1 loops on both.
	assert_eq!(nfa.used_symbols().len(), 2);
	assert_eq!(nfa.num_transitions(), 3);
	assert_eq!(nfa.find_shortest_word().map(|word| word.len()), Some(1));
	Ok(())
}

#[test]
fn named_printing() -> Result<(), Error> {
	let mut sigma = Alphabet::from_names(["x"])?;
	let x = sigma.symbol("x").unwrap();
	let unnamed = Symbol::new(41);
	sigma.insert(unnamed)?;
	let mut nfa = Nfa::with_states(2);
	nfa.add_initial(q(0))?;
	nfa.add_final(q(1))?;
	nfa.add_transition(q(0), x, q(1))?;
	nfa.add_transition(q(0), unnamed, q(1))?;
	nfa.add_transition(q(1), Symbol::EPSILON, q(0))?;
	let text = nfa.display_with(&sigma).to_string();
	assert_eq!(
		text,
		"@NFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final q1\nq0 x q1\nq0 41 q1\nq1 <eps> q0\n"
	);
	let mut reread = sigma.clone();
	let parsed = Nfa::parse_mata(&text, &mut reread)?;
	assert_eq!(reread, sigma, "41 is the unnamed symbol, not a new name");
	assert!(parsed.accepts(&[unnamed]) && parsed.is_equivalent_to(&nfa));
	let dot = nfa.dot().alphabet(&sigma).to_string();
	assert!(dot.contains('x'), "{dot}");
	assert_eq!(sigma.display_word(&[x, unnamed]).to_string(), "x 41");
	assert_eq!(sigma.display_word(&[]).to_string(), "ε");
	Ok(())
}

#[test]
fn checks_with_counterexamples() -> Result<(), Error> {
	let sigma = Alphabet::from_symbols(word_from_bytes(b"ab"))?;
	let ends_with_a = Nfa::from_regex("(a|b)*a")?;
	let rejected = ends_with_a.find_rejected_word(&sigma).unwrap();
	assert!(!ends_with_a.accepts(&rejected));
	assert!(Nfa::sigma_star(&sigma).is_universal(&sigma));
	assert!(!ends_with_a.is_universal(&sigma));
	assert!(Nfa::new().is_lang_empty());
	assert!(Nfa::empty_word().accepts(&[]));
	assert!(
		Nfa::empty_word()
			.complement(&Alphabet::new())
			.is_lang_empty()
	);

	let a_plus = Nfa::from_regex("a+")?;
	assert!(a_plus.is_included_in(&ends_with_a));
	let witness = ends_with_a.find_word_not_in(&a_plus).unwrap();
	assert!(ends_with_a.accepts(&witness) && !a_plus.accepts(&witness));
	let distinction = a_plus.find_distinguishing_word(&ends_with_a).unwrap();
	assert_eq!(distinction.accepted_by, Side::Right);
	Ok(())
}

#[test]
fn trim_and_complete() -> Result<(), Error> {
	let mut nfa = Nfa::with_states(4);
	nfa.add_initial(q(0))?;
	nfa.add_final(q(1))?;
	nfa.add_transition(q(0), Symbol::new(0), q(1))?;
	nfa.add_transition(q(0), Symbol::new(0), q(2))?; // q2 reaches no final state
	nfa.add_transition(q(3), Symbol::new(0), q(1))?; // q3 is unreachable
	assert_eq!(nfa.reachable_states(), [q(0), q(1), q(2)]);
	assert_eq!(nfa.useful_states(), [q(0), q(1)]);
	nfa.trim();
	assert_eq!(nfa.num_states(), 2);

	let sigma = Alphabet::from_symbols([Symbol::new(0), Symbol::new(1)])?;
	assert!(!nfa.is_complete(&sigma));
	assert_eq!(nfa.make_complete(&sigma), Some(q(2)));
	assert!(nfa.is_complete(&sigma));
	assert_eq!(nfa.make_complete(&sigma), None);
	assert!(!nfa.is_acyclic(), "the sink loops");
	Ok(())
}

#[test]
fn is_complete_rejects_outside_symbols() -> Result<(), Error> {
	// A transition over a symbol outside the alphabet is an error, as in Mata's own check.
	let mut nfa = Nfa::with_states(2);
	nfa.add_initial(q(0))?;
	nfa.add_final(q(1))?;
	nfa.add_transition(q(0), Symbol::new(5), q(1))?;
	let sigma = Alphabet::from_symbols([Symbol::new(0)])?;
	let result = std::panic::catch_unwind(|| nfa.is_complete(&sigma));
	assert!(
		result.is_err(),
		"a transition over symbol 5 must not pass silently"
	);

	// Epsilon is a move, never a letter outside the alphabet.
	let mut nfa = Nfa::with_states(2);
	nfa.add_initial(q(0))?;
	nfa.add_final(q(1))?;
	nfa.add_transition(q(0), Symbol::EPSILON, q(1))?;
	let sigma = Alphabet::from_symbols([Symbol::new(0)])?;
	assert!(!nfa.is_complete(&sigma));
	Ok(())
}

#[test]
fn debug_output() {
	let nfa = Nfa::from_word(&[Symbol::new(5)]);
	assert_eq!(
		format!("{nfa:?}"),
		"Nfa { num_states: 2, initial: [q0], final: [q1], transitions: [q0 5 q1] }"
	);
}

#[test]
fn raw_round_trip() -> Result<(), Error> {
	let nfa = Nfa::from_regex("ab|c")?;
	let raw = nfa.clone().into_ffi();
	let ffi_states =
		|raw: &mata::sys::ffi::Nfa| mata::sys::ffi::num_states(mata::sys::ffi::nfa_automaton(raw));
	assert_eq!(ffi_states(&raw), nfa.num_states());
	let back = Nfa::from_ffi(raw)?;
	assert!(back.is_identical(&nfa));
	assert!(Nfa::from_ffi(mata::sys::cxx::UniquePtr::null()).is_err());
	assert_eq!(ffi_states(back.as_ffi()), nfa.num_states());
	Ok(())
}

#[cfg(feature = "serde")]
#[test]
fn serde_round_trip() -> Result<(), Error> {
	let nfa = Nfa::from_regex("a(b|c)*")?;
	let json = serde_json::to_string(&nfa).unwrap();
	let back: Nfa = serde_json::from_str(&json).unwrap();
	assert!(back.is_identical(&nfa), "{json}");

	let bad = r#"{"num_states":1,"initial":[0],"final":[3],"transitions":[]}"#;
	let error = serde_json::from_str::<Nfa>(bad).unwrap_err();
	assert!(error.to_string().contains("q3"), "{error}");

	let sigma = Alphabet::from_names(["a", "b"])?;
	let json = serde_json::to_string(&sigma).unwrap();
	assert_eq!(json, r#"[[0,"a"],[1,"b"]]"#);
	assert_eq!(serde_json::from_str::<Alphabet>(&json).unwrap(), sigma);
	assert!(serde_json::from_str::<Alphabet>(r#"[[0,"<eps>"]]"#).is_err());
	assert!(serde_json::from_str::<Alphabet>(r#"[[0,"a"],[1,"a"]]"#).is_err());

	// A number of states that cannot be created is an error, not a panic.
	let huge = format!(
		r#"{{"num_states":{},"initial":[],"final":[],"transitions":[]}}"#,
		usize::MAX
	);
	let error = serde_json::from_str::<Nfa>(&huge).unwrap_err();
	assert!(error.to_string().contains("cannot create"), "{error}");
	Ok(())
}

#[test]
fn nfa_is_send_and_sync() {
	fn assert_send_sync<T: Send + Sync>() {}
	assert_send_sync::<Nfa>();
	assert_send_sync::<Alphabet>();
	assert_send_sync::<mata::nfa::Transitions<'_>>();
	assert_send_sync::<mata::nfa::Words<'_>>();
	assert_send_sync::<mata::nfa::States>();
	assert_send_sync::<mata::nfa::Dot<'_>>();
}

#[test]
fn is_minimal_needs_a_dfa() -> Result<(), Error> {
	// An epsilon transition is no DFA.
	let mut nfa = Nfa::with_states(2);
	nfa.add_initial(q(0))?;
	nfa.add_final(q(1))?;
	nfa.add_transition(q(0), Symbol::EPSILON, q(1))?;
	assert!(!nfa.is_minimal());
	assert!(nfa.minimize().is_minimal());
	Ok(())
}

#[test]
fn is_identical_compares_the_number_of_states() {
	assert!(Nfa::with_states(2).is_identical(&Nfa::with_states(2)));
	assert!(!Nfa::with_states(1).is_identical(&Nfa::with_states(2)));
}

#[test]
fn parsing_gives_no_name_a_reserved_symbol() -> Result<(), Error> {
	// The next symbols would be DONT_CARE and EPSILON.
	let text = "@NFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final q1\nq0 a q1\n";
	for top in [u32::MAX - 2, u32::MAX - 1] {
		let mut sigma = Alphabet::from_symbols([Symbol::new(top)])?;
		let error = Nfa::parse_mata(text, &mut sigma).unwrap_err();
		assert!(error.message().contains("no symbol left"), "{error}");
		assert_eq!(sigma.len(), 1, "the alphabet is unchanged");
	}
	Ok(())
}

#[test]
fn insert_name_gives_no_name_dont_care() -> Result<(), Error> {
	let mut sigma = Alphabet::from_symbols([Symbol::new(u32::MAX - 2)])?;
	assert_eq!(sigma.insert_name("x"), Err(AlphabetError::Full));
	Ok(())
}

#[test]
fn difference_operator() -> Result<(), Error> {
	let (a_star, a_plus) = (Nfa::from_regex("a*")?, Nfa::from_regex("a+")?);
	assert!((&a_star - &a_plus).is_equivalent_to(&Nfa::empty_word()));
	Ok(())
}

#[test]
fn determinize_within_stops_at_the_limit() -> Result<(), Error> {
	// The DFA must remember the last four letters: 16 states.
	let nfa = Nfa::from_regex("(a|b)*a(a|b)(a|b)(a|b)")?;
	let dfa = nfa.determinize();
	assert_eq!(dfa.num_states(), 16);
	let at = |max_states| Limits::new().max_states(max_states);
	assert!(nfa.determinize_within(&at(16))?.is_identical(&dfa));
	assert!(nfa.determinize_within(&Limits::new())?.is_identical(&dfa));
	assert_eq!(
		nfa.determinize_within(&at(15)).err(),
		Some(LimitExceeded::States { max_states: 15 })
	);

	// Even the DFA of an automaton without transitions has a state.
	assert!(Nfa::new().determinize_within(&at(0)).is_err());
	assert_eq!(Nfa::new().determinize_within(&at(1))?.num_states(), 1);

	let error = Error::from(nfa.determinize_within(&at(15)).unwrap_err());
	assert_eq!(
		error.to_string(),
		"the deterministic automaton would have more than 15 states"
	);
	Ok(())
}

#[test]
fn minimize_within_limits_each_determinization() -> Result<(), Error> {
	// The fourth letter is `a`: a small DFA, but Brzozowski's algorithm first builds the DFA of the
	// reversed language, which must remember the last four letters.
	let nfa = Nfa::from_regex("(a|b)(a|b)(a|b)a(a|b)*")?;
	let minimal = nfa.minimize();
	let forward = nfa.determinize().num_states();
	let backward = nfa.reverse().determinize().num_states();
	assert_eq!((forward, minimal.num_states(), backward), (5, 5, 16));

	let small = Limits::new().max_states(5);
	let hopcroft = nfa.minimize_within(Minimization::Hopcroft, &small);
	assert!(hopcroft?.is_identical(&minimal));
	assert_eq!(
		nfa.minimize_within(Minimization::Brzozowski, &small).err(),
		Some(LimitExceeded::States { max_states: 5 })
	);
	let brzozowski = nfa.minimize_within(Minimization::Brzozowski, &Limits::new().max_states(16));
	assert!(brzozowski?.is_identical(&nfa.minimize_with(Minimization::Brzozowski)));
	Ok(())
}

#[test]
fn complement_within_counts_the_sink_state() -> Result<(), Error> {
	// The DFA of `ab` has three states; its complement over {a, b, c} adds a sink.
	let nfa = Nfa::from_regex("ab")?;
	let sigma = Alphabet::from_symbols(word_from_bytes(b"abc"))?;
	for algorithm in [Complementation::Classical, Complementation::Brzozowski] {
		let complement = nfa.complement_with(&sigma, algorithm);
		assert_eq!(complement.num_states(), 4, "{algorithm:?}");
		let fits = nfa.complement_within(&sigma, algorithm, &Limits::new().max_states(4));
		assert!(fits?.is_identical(&complement), "{algorithm:?}");
		assert_eq!(
			nfa.complement_within(&sigma, algorithm, &Limits::new().max_states(3))
				.err(),
			Some(LimitExceeded::States { max_states: 3 }),
			"{algorithm:?}"
		);
	}
	Ok(())
}

#[test]
fn difference_within_limits_the_difference() -> Result<(), Error> {
	let nfa = Nfa::from_regex("(a|b)*a(a|b)(a|b)(a|b)")?;
	let other = Nfa::from_regex("(a|b)*b")?;
	let difference = nfa.difference(&other);
	let size = difference.num_states();
	let fits = nfa.difference_within(&other, &Limits::new().max_states(size));
	assert!(fits?.is_identical(&difference));
	assert_eq!(
		nfa.difference_within(&other, &Limits::new().max_states(size - 1))
			.err(),
		Some(LimitExceeded::States {
			max_states: size - 1
		})
	);
	Ok(())
}

#[test]
fn lazy_quantifiers_have_the_greedy_language() -> Result<(), Error> {
	for (lazy, greedy) in [("x*?", "x*"), ("x+?", "x+"), ("(?U)x*", "x*")] {
		let lazy_nfa = Nfa::from_regex(lazy)?;
		assert!(
			lazy_nfa.is_equivalent_to(&Nfa::from_regex(greedy)?),
			"{lazy}"
		);
	}
	Ok(())
}

#[test]
fn ids_and_conversions() {
	let state = State::try_from(7_usize).unwrap();
	assert_eq!(
		(state.index(), u32::from(state), usize::from(state)),
		(7, 7, 7)
	);
	assert_eq!(State::from(7_u32), state);
	// Only a `usize` wider than 32 bits can hold an index that is too big.
	if let Ok(too_big) = usize::try_from(u64::from(u32::MAX) + 1) {
		assert!(State::try_from(too_big).is_err());
	}
	assert!(Symbol::EPSILON.is_epsilon() && !Symbol::new(0).is_epsilon());
	assert_eq!(Symbol::EPSILON.to_string(), "<eps>");
	assert_eq!(u32::from(Symbol::from(3_u32)), 3);
	let transition: Transition = (q(0), Symbol::new(1), q(2)).into();
	assert_eq!(transition.to_string(), "q0 1 q2");
	assert_eq!(
		word_from_bytes("ab"),
		[Symbol::from(b'a'), Symbol::from(b'b')]
	);
}

#[test]
fn alphabet_collections() -> Result<(), Error> {
	let mut sigma = Alphabet::from_names(["a", "<eps>", "b"])?;
	assert_eq!(sigma.len(), 2, "<eps> is no member and takes no number");
	assert_eq!(sigma.symbol("b"), Some(Symbol::new(1)));
	assert_eq!(sigma.insert(Symbol::new(5)), Ok(true));
	assert_eq!(sigma.insert(Symbol::new(5)), Ok(false));
	assert_eq!(sigma.insert(Symbol::EPSILON), Err(AlphabetError::Epsilon));
	assert_eq!(
		sigma.insert_name("<dntcr>"),
		Err(AlphabetError::ReservedName)
	);
	assert_eq!(
		Alphabet::from_symbols([Symbol::EPSILON]),
		Err(AlphabetError::Epsilon)
	);
	assert!(sigma.contains(Symbol::new(5)) && !sigma.is_empty());
	assert_eq!(
		sigma.symbols().rev().collect::<Vec<_>>(),
		[Symbol::new(5), Symbol::new(1), Symbol::new(0)]
	);
	assert_eq!(sigma.iter().len(), 3);
	assert_eq!(
		(&sigma)
			.into_iter()
			.map(|(_, name)| name)
			.collect::<Vec<_>>(),
		[Some("a"), Some("b"), None]
	);
	assert!(Alphabet::new().is_empty());
	Ok(())
}

#[test]
fn states_iterator_and_adapters() -> Result<(), Error> {
	assert_eq!(Nfa::default().num_states(), 0);
	let states = Nfa::with_states(3).states();
	assert_eq!(states.len(), 3);
	assert_eq!(states.rev().collect::<Vec<_>>(), [q(2), q(1), q(0)]);

	let nfa = Nfa::from_regex("[a-c]x")?;
	let dot = nfa.dot().intervals(true).max_label_len(3).to_string();
	assert!(dot.starts_with("digraph") && dot.contains("->"), "{dot}");

	let text = "@NFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final q0\n\
                @NFA-explicit\n%Alphabet-auto\n%Initial q0\n";
	let both = Nfa::parse_mata_numeric_all(text)?;
	assert_eq!(both.len(), 2);
	assert!(text.parse::<Nfa>().is_err(), "one automaton expected");
	let distinction = both[0].find_distinguishing_word_with(&both[1], CheckAlgorithm::Naive);
	assert_eq!(distinction.map(|d| d.accepted_by), Some(Side::Left));
	Ok(())
}

/// The example of the repository README.
#[test]
fn readme_example() -> Result<(), Error> {
	let a = Nfa::from_regex("(a|b)*abb")?;
	let b = Nfa::from_regex("(a|b)*b")?;
	assert!(a.is_included_in(&b));
	let witness = b.find_word_not_in(&a).unwrap();
	assert!(b.accepts(&witness) && !a.accepts(&witness));
	assert_eq!(a.minimize().num_states(), 4);
	assert!(a.accepts(&word_from_bytes("aabb")));
	Ok(())
}

/// A number the alphabet has no name for is the symbol of that number in every parse, as it is printed.
#[test]
fn a_number_parses_as_its_own_symbol_in_every_parse() -> Result<(), Error> {
	let section = |symbol: &str| {
		format!("@NFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final q1\nq0 {symbol} q1\n")
	};
	let mut sigma = Alphabet::from_names(["a"])?;
	let five = Nfa::parse_mata(&section("5"), &mut sigma)?;
	let one = Nfa::parse_mata(&section("1"), &mut sigma)?;
	assert!(five.accepts(&[Symbol::new(5)]) && one.accepts(&[Symbol::new(1)]));
	assert!(Nfa::parse_mata(&section("5"), &mut sigma)?.is_equivalent_to(&five));
	assert_eq!((sigma.name(Symbol::new(5)), sigma.len()), (None, 3));
	Ok(())
}

/// A number whose own symbol has a name gets a new symbol once, and keeps it in later parses.
#[test]
fn a_number_whose_symbol_is_named_keeps_the_symbol_it_got() -> Result<(), Error> {
	let section = |symbol: &str| {
		format!("@NFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final q1\nq0 {symbol} q1\n")
	};
	let mut sigma = Alphabet::from_names(["a"])?;
	let zero = Nfa::parse_mata(&section("0"), &mut sigma)?;
	assert!(!zero.accepts(&[Symbol::new(0)]));
	Nfa::parse_mata(&section("b"), &mut sigma)?;
	assert!(Nfa::parse_mata(&section("0"), &mut sigma)?.is_equivalent_to(&zero));
	Ok(())
}
