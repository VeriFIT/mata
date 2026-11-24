//! The `.mata` parser must turn any input into an automaton or an error, never a crash.

use mata::{Alphabet, Nfa};
use proptest::prelude::*;

/// Lines built from the tokens of the format, so that most inputs get deep into the parser.
fn line() -> impl Strategy<Value = String> {
	let token = prop_oneof![
		Just("@NFA-explicit".to_owned()),
		Just("@NFA-bits".to_owned()),
		Just("@NFT-explicit".to_owned()),
		Just("%Alphabet-auto".to_owned()),
		Just("%Alphabet-enum".to_owned()),
		Just("%Initial".to_owned()),
		Just("%Final".to_owned()),
		Just("%States-enum".to_owned()),
		Just("%Levels".to_owned()),
		Just("q0".to_owned()),
		Just("q1".to_owned()),
		Just("q".to_owned()),
		Just("a".to_owned()),
		Just("0".to_owned()),
		Just("<eps>".to_owned()),
		Just("&".to_owned()),
		Just("|".to_owned()),
		Just("!".to_owned()),
		Just("(".to_owned()),
		Just(")".to_owned()),
		Just("\"".to_owned()),
		Just("\\".to_owned()),
		Just("#".to_owned()),
		Just("true".to_owned()),
		Just("false".to_owned()),
		"[ -~]{0,3}",
	];
	proptest::collection::vec(token, 0..6).prop_map(|tokens| tokens.join(" "))
}

fn text() -> impl Strategy<Value = String> {
	let header = prop_oneof![
		Just("@NFA-explicit\n%Alphabet-auto\n"),
		Just("@NFA-bits\n%Alphabet-auto\n"),
		Just("@NFA-explicit\n"),
		Just(""),
	];
	(header, proptest::collection::vec(line(), 0..8))
		.prop_map(|(header, lines)| format!("{header}{}\n", lines.join("\n")))
}

proptest! {
	#![proptest_config(ProptestConfig::with_cases(2000))]

	#[test]
	fn parsing_never_crashes(text in text()) {
		let mut sigma = Alphabet::new();
		if let Ok(nfa) = Nfa::parse_mata(&text, &mut sigma) {
			// Whatever was accepted is a usable automaton.
			let _ = nfa.is_lang_empty();
			let _ = nfa.display_with(&sigma).to_string();
		}
		let _ = text.parse::<Nfa>();
	}

	#[test]
	fn regex_parsing_never_crashes(pattern in "[ab()|*+?{}\\[\\]^$\\\\.,0-9-]{0,12}") {
		if let Ok(nfa) = Nfa::from_regex(&pattern) {
			let _ = nfa.minimize();
		}
	}
}

/// Lines of transducer text.
fn nft_line() -> impl Strategy<Value = String> {
	let token = prop_oneof![
		Just("%Initial".to_owned()),
		Just("%Final".to_owned()),
		Just("%Levels".to_owned()),
		Just("%LevelsNum".to_owned()),
		Just("%Alphabet-auto".to_owned()),
		"q[0-3]",
		"q[0-3]:[0-3]",
		"q[0-3]:[0-9:x-]{0,3}",
		"-?[0-4]",
		Just("<eps>".to_owned()),
		Just("<dntcr>".to_owned()),
		Just("a".to_owned()),
		"[ -~]{0,3}",
	];
	proptest::collection::vec(token, 0..6).prop_map(|tokens| tokens.join(" "))
}

proptest! {
	#![proptest_config(ProptestConfig::with_cases(4000))]

	#[test]
	fn transducer_parsing_never_crashes(
		header in prop_oneof![Just("@NFT-explicit\n"), Just("@NFT\n"), Just("@NFT-bits\n"), Just("")],
		lines in proptest::collection::vec(nft_line(), 0..8),
	) {
		let text = format!("{header}{}\n", lines.join("\n"));
		let mut sigma = Alphabet::new();
		if let Ok(nft) = mata::nft::Nft::parse_mata(&text, &mut sigma) {
			// Whatever was accepted is a usable transducer.
			let _ = nft.is_lang_empty();
			let _ = nft.display_with(&sigma).to_string();
			let _ = nft.invert_levels();
		}
		let _ = text.parse::<mata::nft::Nft>();
	}
}
