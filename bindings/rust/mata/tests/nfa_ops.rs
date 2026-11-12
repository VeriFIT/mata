//! The further NFA operations of the library, on hand-written cases.

use mata::{Error, Nfa, Symbol, word_from_bytes};

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
