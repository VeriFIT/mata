//! Loading in separate steps: parsing to intermediate automata, mintermizing, constructing.

use mata::{Alphabet, Error, IntermediateAutomaton, IntermediateError, Nfa};

const BITS_A: &str = "@NFA-bits\n%Alphabet-auto\n%Initial q0\n%Final q1\nq0 (a0 & !a1) q1\n";
const BITS_B: &str = "@NFA-bits\n%Alphabet-auto\n%Initial q0\n%Final q1\nq0 (!a0 & a1) q1\n";
const EXPLICIT: &str = "@NFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final q1\nq0 a q1\nq1 b q1\n";

fn parse_one(text: &str) -> IntermediateAutomaton {
	let mut parsed = IntermediateAutomaton::parse_mata_all(text).unwrap();
	assert_eq!(parsed.len(), 1);
	parsed.remove(0)
}

fn construct_all(automata: &[IntermediateAutomaton], alphabet: &mut Alphabet) -> Vec<Nfa> {
	automata
		.iter()
		.map(|automaton| automaton.construct_nfa(alphabet).unwrap())
		.collect()
}

#[test]
fn an_explicit_section_constructs_over_the_alphabet() -> Result<(), Error> {
	let parsed = IntermediateAutomaton::parse_mata_all(&format!("{EXPLICIT}{EXPLICIT}"))?;
	assert_eq!(parsed.len(), 2);
	let mut sigma = Alphabet::new();
	let nfa = parsed[0].construct_nfa(&mut sigma)?;
	let abb = sigma.encode(["a", "b", "b"]).unwrap();
	assert!(nfa.accepts(&abb));
	assert!(!nfa.accepts(&abb[1..]));
	assert!(nfa.is_equivalent_to(&Nfa::parse_mata(EXPLICIT, &mut sigma)?));
	Ok(())
}

#[test]
fn automata_mintermized_together_share_their_minterms() -> Result<(), Error> {
	let (a, b) = (parse_one(BITS_A), parse_one(BITS_B));
	let mut sigma = Alphabet::new();
	let together = construct_all(&IntermediateAutomaton::mintermize(&[&a, &b])?, &mut sigma);
	assert!(together[0].intersection(&together[1]).is_lang_empty());
	// Separately, each section's one minterm is symbol 0, so the languages coincide.
	let first = construct_all(&IntermediateAutomaton::mintermize(&[&a])?, &mut sigma);
	let second = construct_all(&IntermediateAutomaton::mintermize(&[&b])?, &mut sigma);
	assert!(first[0].is_equivalent_to(&second[0]));
	Ok(())
}

#[test]
fn each_step_rejects_a_section_it_cannot_take() {
	let bits = parse_one(BITS_A);
	let explicit = parse_one(EXPLICIT);
	let mut sigma = Alphabet::new();
	assert_eq!(
		bits.construct_nfa(&mut sigma).unwrap_err(),
		IntermediateError::NeedsMintermization
	);
	assert_eq!(
		IntermediateAutomaton::mintermize(&[&bits, &explicit]).unwrap_err(),
		IntermediateError::NotMintermizable { index: 1 }
	);
	assert_eq!(
		explicit.construct_nft(&mut sigma).unwrap_err(),
		IntermediateError::WrongKind { expected: "NFT" }
	);
	assert!(IntermediateAutomaton::parse_mata_all("@NFA-explicit\n%Initial\n q0 a").is_err());
}

#[test]
fn a_transducer_section_constructs_on_level_zero() -> Result<(), Error> {
	let text = "@NFT-explicit\n%Alphabet-auto\n%Initial q0\n%Final q1\nq0 a q1\n";
	let mut sigma = Alphabet::new();
	let nft = parse_one(text).construct_nft(&mut sigma)?;
	let a = sigma.encode(["a"]).unwrap();
	assert_eq!(nft.num_levels(), 2);
	assert!(nft.accepts(&[&a, &a]));
	assert!(!nft.accepts(&[&a[..], &[]]));
	assert_eq!(
		parse_one(text).construct_nfa(&mut sigma).unwrap_err(),
		IntermediateError::WrongKind { expected: "NFA" }
	);
	Ok(())
}

#[test]
fn display_is_the_library_dump() {
	let text = parse_one(EXPLICIT).to_string();
	assert!(
		text.starts_with("Intermediate automaton type NFA\n"),
		"{text}"
	);
	assert!(text.contains("Alphabet 0\n"), "{text}");
	assert!(
		text.contains("Transitions: \nq0 -> a & q1\nq1 -> b & q1\n"),
		"{text}"
	);
	assert!(
		!text.contains("@NFA"),
		"the .mata format, not the dump: {text}"
	);
}
