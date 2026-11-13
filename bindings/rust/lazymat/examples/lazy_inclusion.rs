//! Language inclusion as emptiness, as `L1 ⊆ L2` iff `L1 ∩ ¬L2 = ∅`.

use lazymat::{Formula, Witness};
use mata::Nfa;

/// The word on tape 0, with bytes shown as characters.
fn text(witness: &Witness) -> String {
	witness
		.tape(0)
		.unwrap_or_default()
		.into_iter()
		.map(|symbol| char::from_u32(symbol.value()).unwrap_or('?'))
		.collect()
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
	let mut formula = Formula::new();
	let starts_a = formula.leaf(&Nfa::from_regex("a(a|b)*")?)?;
	let ends_b = formula.leaf(&Nfa::from_regex("(a|b)*b")?)?;
	let both = formula.leaf(&Nfa::from_regex("a(a|b)*b")?)?;

	for (name, lhs, rhs) in [
		("a(a|b)*b ⊆ a(a|b)*", both, starts_a),
		("a(a|b)* ⊆ a(a|b)*b", starts_a, both),
		("a(a|b)*b ⊆ (a|b)*b", both, ends_b),
	] {
		let not_rhs = formula.complement(rhs)?;
		let difference = formula.intersection(lhs, not_rhs)?;

		match formula.find_word(difference)? {
			None => println!("{name}: holds"),
			Some(witness) => println!("{name}: fails on {:?}", text(&witness)),
		}
	}

	Ok(())
}
