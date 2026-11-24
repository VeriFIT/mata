//! Behaviour of the string-solving operations (`mata::strings`) on hand-written cases.

use mata::nft::{DONT_CARE, Nft};
use mata::strings::{self, BEGIN_MARKER, END_MARKER, ReplaceMode};
use mata::{Alphabet, Error, Nfa, NftError, ReplaceError, State, Symbol, Word, word_from_bytes};

fn words(texts: &[&str]) -> Vec<Word> {
	texts.iter().map(word_from_bytes).collect()
}

fn abc() -> Alphabet {
	Alphabet::from_symbols(b"abc".map(Symbol::from)).unwrap()
}

/// Whether `nft` relates `input` to `output`.
fn relates(nft: &Nft, input: &str, output: &str) -> bool {
	nft.accepts(&[word_from_bytes(input), word_from_bytes(output)])
}

#[test]
fn word_queries_read_the_language() -> Result<(), Error> {
	let nfa = Nfa::from_regex("ab|ba|b|abc")?;
	assert_eq!(strings::shortest_words(&nfa), words(&["b"]));
	assert_eq!(strings::longest_words(&nfa), Some(words(&["abc"])));
	assert_eq!(strings::accepted_symbols(&nfa), word_from_bytes("b"));
	assert_eq!(strings::word_lengths(&nfa), [(1, 0), (2, 0), (3, 0)]);
	assert!(!strings::is_lang_eps(&nfa));

	let infinite = Nfa::from_regex("a(bb)*")?;
	assert_eq!(strings::longest_words(&infinite), None);
	assert_eq!(strings::word_lengths(&infinite), [(1, 2)]);

	let empty = Nfa::new();
	assert_eq!(strings::shortest_words(&empty), Vec::<Word>::new());
	assert_eq!(strings::longest_words(&empty), Some(Vec::new()));
	assert_eq!(strings::word_lengths(&empty), Vec::<(usize, usize)>::new());

	// Epsilon transitions are not letters: q0 -ε-> q1 is the language {ε}.
	let mut epsilon = Nfa::new();
	let (q0, q1) = (epsilon.add_state(), epsilon.add_state());
	epsilon.add_initial(q0)?;
	epsilon.add_final(q1)?;
	epsilon.add_transition(q0, Symbol::EPSILON, q1)?;
	assert!(strings::is_lang_eps(&epsilon));
	assert_eq!(strings::shortest_words(&epsilon), [Word::new()]);
	assert_eq!(strings::accepted_symbols(&epsilon), Vec::<Symbol>::new());
	Ok(())
}

#[test]
fn segments_split_at_the_given_symbols() -> Result<(), Error> {
	let (a, b, separator) = (Symbol::from(b'a'), Symbol::from(b'b'), Symbol::new(100));
	// a+ · separator · b
	let mut nfa = Nfa::new();
	let q: Vec<State> = (0..4).map(|_| nfa.add_state()).collect();
	nfa.add_initial(q[0])?;
	nfa.add_final(q[3])?;
	nfa.add_transition(q[0], a, q[1])?;
	nfa.add_transition(q[1], a, q[1])?;
	nfa.add_transition(q[1], separator, q[2])?;
	nfa.add_transition(q[2], b, q[3])?;
	let segments = strings::segments(&nfa, &[separator]);
	assert_eq!(segments.len(), 2);
	assert!(segments[0].is_equivalent_to(&Nfa::from_regex("a+")?));
	assert!(segments[1].is_equivalent_to(&Nfa::from_regex("b")?));
	// Without separators, the one segment is the trimmed automaton.
	assert_eq!(strings::segments(&nfa, &[]).len(), 1);
	Ok(())
}

#[test]
fn reluctant_nfa_stops_at_the_first_final_state() -> Result<(), Error> {
	let reluctant = strings::reluctant_nfa(&Nfa::from_regex("a+|b")?);
	assert!(reluctant.is_equivalent_to(&Nfa::from_regex("a|b")?));
	Ok(())
}

#[test]
fn words_of_lengths_finds_one_word_per_level() -> Result<(), Error> {
	let identity = strings::create_identity(&abc(), 2)?;
	let found = strings::words_of_lengths(&identity, &[2, 2])?.expect("a pair of words");
	assert_eq!(found.len(), 2);
	assert_eq!(found[0], found[1]);
	assert_eq!(found[0].len(), 2);
	assert_eq!(strings::words_of_lengths(&identity, &[1, 2])?, None);
	assert_eq!(
		strings::words_of_lengths(&identity, &[1]),
		Err(NftError::LevelMismatch {
			expected: 2,
			found: 1
		})
	);
	Ok(())
}

#[test]
fn identity_replaces_one_symbol() -> Result<(), Error> {
	let (a, b) = (Symbol::from(b'a'), Symbol::from(b'b'));
	let identity = strings::create_identity(&abc(), 2)?;
	assert!(relates(&identity, "abc", "abc"));
	assert!(!relates(&identity, "abc", "abb"));
	assert_eq!(strings::create_identity(&abc(), 3)?.num_levels(), 3);

	let all =
		strings::create_identity_with_single_symbol_replace(&abc(), a, &[b, b], ReplaceMode::All)?;
	assert!(relates(&all, "cab", "cbbb"));
	assert!(relates(&all, "aa", "bbbb"));
	let single =
		strings::create_identity_with_single_symbol_replace(&abc(), a, &[], ReplaceMode::Single)?;
	assert!(relates(&single, "aca", "ca"));
	assert!(!relates(&single, "aca", "c"));
	assert_eq!(
		strings::create_identity_with_single_symbol_replace(
			&abc(),
			Symbol::new(7),
			&[a],
			ReplaceMode::All
		)
		.unwrap_err(),
		ReplaceError::SymbolNotInAlphabet {
			symbol: Symbol::new(7)
		}
	);
	Ok(())
}

#[test]
fn reluctant_replaces_take_the_shortest_leftmost_match() -> Result<(), Error> {
	let (a, b, c) = (Symbol::from(b'a'), Symbol::from(b'b'), Symbol::from(b'c'));
	let regex =
		strings::replace_reluctant_regex("a+", &[c], &abc(), ReplaceMode::All, BEGIN_MARKER)?;
	assert!(relates(&regex, "aab", "ccb"));
	assert!(!relates(&regex, "aab", "cb"));
	let first =
		strings::replace_reluctant_regex("a+", &[c], &abc(), ReplaceMode::Single, BEGIN_MARKER)?;
	assert!(relates(&first, "aab", "cab"));
	let from_nfa = strings::replace_reluctant_regex_nfa(
		&Nfa::from_regex("a+")?,
		&[c],
		&abc(),
		ReplaceMode::All,
		BEGIN_MARKER,
	)?;
	assert!(relates(&from_nfa, "aab", "ccb"));
	assert!(matches!(
		strings::replace_reluctant_regex("a(", &[c], &abc(), ReplaceMode::All, BEGIN_MARKER),
		Err(ReplaceError::Regex(_))
	));

	let literal =
		strings::replace_reluctant_literal(&[a, b], &[c], &abc(), ReplaceMode::All, END_MARKER)?;
	assert!(relates(&literal, "aabab", "acc"));
	assert!(!relates(&literal, "aabab", "aabab"));
	let literal =
		strings::replace_reluctant_literal(&[a, b], &[c], &abc(), ReplaceMode::Single, END_MARKER)?;
	assert!(relates(&literal, "aabab", "acab"));

	let symbol = strings::replace_reluctant_single_symbol(a, &[b], &abc(), ReplaceMode::All)?;
	assert!(relates(&symbol, "aca", "bcb"));
	let symbol = strings::replace_reluctant_single_symbol(a, &[b, c], &abc(), ReplaceMode::Single)?;
	assert!(relates(&symbol, "aca", "bcca"));
	Ok(())
}

#[test]
fn replace_rejects_reserved_symbols() -> Result<(), Error> {
	let c = Symbol::from(b'c');
	let mut with_marker = abc();
	with_marker.insert(BEGIN_MARKER)?;
	assert_eq!(
		strings::replace_reluctant_regex("a", &[c], &with_marker, ReplaceMode::All, BEGIN_MARKER)
			.unwrap_err(),
		ReplaceError::ReservedSymbol {
			symbol: BEGIN_MARKER
		}
	);
	let mut with_dont_care = abc();
	with_dont_care.insert(DONT_CARE)?;
	assert_eq!(
		strings::create_identity(&with_dont_care, 2).unwrap_err(),
		ReplaceError::ReservedSymbol { symbol: DONT_CARE }
	);
	assert_eq!(
		strings::create_identity(&abc(), 0).unwrap_err(),
		ReplaceError::Nft(NftError::InvalidNumLevels { num_levels: 0 })
	);
	// Epsilon and DONT_CARE read as moves, so neither can mark a position.
	for marker in [Symbol::EPSILON, DONT_CARE] {
		assert_eq!(
			strings::replace_reluctant_literal(&[c], &[c], &abc(), ReplaceMode::All, marker)
				.unwrap_err(),
			ReplaceError::ReservedMarker { marker }
		);
	}
	Ok(())
}
