//! The regex-automata backend alone, and the errors that need no other backend.

use lazymat::regex::{self, Regex};
use lazymat::{Error, Formula, Options, Symbol, Tape};
use regex_automata::dfa::{StartKind, dense};

#[test]
fn delayed_matches_add_no_symbols() -> Result<(), Box<dyn std::error::Error>> {
	let mut formula = Formula::new();
	let a_star = formula.leaf(&Regex::new("a*")?)?;
	let rest = formula.complement(a_star)?;

	assert!(formula.is_empty(rest)?);

	Ok(())
}

#[test]
fn patterns_parse_with_from_str() -> Result<(), Box<dyn std::error::Error>> {
	let mut formula = Formula::new();
	let parsed = formula.leaf(&"a|b".parse::<Regex>()?)?;
	let built = formula.leaf(&Regex::new("[ab]")?)?;
	let not_built = formula.complement(built)?;
	let difference = formula.intersection(parsed, not_built)?;

	assert!(formula.is_empty(difference)?);

	Ok(())
}

#[test]
fn unanchored_dfas_are_rejected() -> Result<(), Box<dyn std::error::Error>> {
	let dfa = dense::Builder::new()
		.configure(dense::Config::new().start_kind(StartKind::Unanchored))
		.build("a")?;

	assert!(matches!(Regex::from_dfa(dfa), Err(regex::Error::Start(_))));

	Ok(())
}

#[test]
fn alphabets_reject_reserved_symbols() -> Result<(), Box<dyn std::error::Error>> {
	let mut formula = Formula::new();
	let a = Regex::new("a")?;

	assert_eq!(
		formula.leaf_over(&a, [[Symbol::ANY]]),
		Err(Error::ReservedSymbol {
			symbol: Symbol::ANY
		})
	);

	let leaf = formula.leaf(&a)?;
	let options = Options::new().alphabet([Symbol::EPSILON]);
	assert_eq!(
		formula.check(leaf, &options),
		Err(Error::ReservedSymbol {
			symbol: Symbol::EPSILON
		})
	);

	Ok(())
}

#[test]
fn complements_too_large_to_enumerate_are_errors() -> Result<(), Box<dyn std::error::Error>> {
	let mut formula = Formula::new();
	let bytes = formula.leaf(&Regex::new("(?s).*")?)?;
	let two = formula.identity(bytes)?;
	let four = formula.sync_product(
		two,
		two,
		&[],
		&[Tape::Lhs(0), Tape::Lhs(1), Tape::Rhs(0), Tape::Rhs(1)],
	)?;
	let all: Vec<Tape> = (0..4).map(Tape::Lhs).chain((0..4).map(Tape::Rhs)).collect();
	let eight = formula.sync_product(four, four, &[], &all)?;
	let complement = formula.complement(eight)?;

	// 256 symbols on each of 8 tapes give 2^64 tuples per step.
	assert_eq!(
		formula.is_empty(complement),
		Err(Error::ComplementTooLarge { arity: 8 })
	);

	Ok(())
}
