//! One formula over leaves from three libraries, and a result handed back to Mata.
//!
//! - a regex-automata DFA gives the inputs,
//! - a rustfst transducer rewrites them,
//! - a Mata NFA states a property of the outputs.

use std::error::Error;

use lazymat::regex::Regex;
use lazymat::{Formula, Options, Witness};
use mata::{Alphabet, Nfa, NoSuchState, State, Symbol};
use rustfst::Tr;
use rustfst::fst_impls::VectorFst;
use rustfst::fst_traits::MutableFst;
use rustfst::semirings::{Semiring, TropicalWeight};

/// The word on tape 0, with bytes shown as characters.
fn text(witness: &Witness) -> String {
	witness
		.tape(0)
		.unwrap_or_default()
		.into_iter()
		.map(|symbol| char::from_u32(symbol.value()).unwrap_or('?'))
		.collect()
}

/// Replaces a by b and keeps c.
fn replace_a_by_b() -> Result<VectorFst<TropicalWeight>, Box<dyn Error>> {
	let mut fst = VectorFst::<TropicalWeight>::new();
	let state = fst.add_state();
	fst.set_start(state)?;
	fst.set_final(state, TropicalWeight::one())?;
	for (input, output) in [(b'a', b'b'), (b'c', b'c')] {
		fst.add_tr(
			state,
			Tr::new(input.into(), output.into(), TropicalWeight::one(), state),
		)?;
	}

	Ok(fst)
}

/// The words over {a, b, c} without two c in a row.
fn no_two_c() -> Result<Nfa, NoSuchState> {
	let [a, b, c] = b"abc".map(Symbol::from);
	let mut nfa = Nfa::with_states(2);
	let (free, after_c) = (State::new(0), State::new(1));
	nfa.add_initial(free)?;
	nfa.add_final(free)?;
	nfa.add_final(after_c)?;
	nfa.add_transition(free, c, after_c)?;
	for state in [free, after_c] {
		nfa.add_transition(state, a, free)?;
		nfa.add_transition(state, b, free)?;
	}

	Ok(nfa)
}

fn main() -> Result<(), Box<dyn Error>> {
	let mut formula = Formula::new();
	let inputs = formula.leaf(&Regex::new("(a|c)*")?)?;
	let rewrite = formula.leaf(&replace_a_by_b()?)?;
	let property = formula.leaf(&no_two_c()?)?;
	let outputs = formula.post_image(inputs, rewrite)?;
	let not_property = formula.complement(property)?;
	let violations = formula.intersection(outputs, not_property)?;

	let options = Options::new().alphabet(*b"abc");
	match formula.check(violations, &options)?.witness {
		Some(witness) => println!("violated by the output {:?}", text(&witness)),
		None => println!("the property holds"),
	}

	// The outputs as an explicit automaton, minimized by Mata.
	let explicit = formula.materialize(outputs, &options)?;
	let alphabet = Alphabet::from_symbols(b"abc".map(Symbol::from))?;
	let minimal = explicit.to_nfa_over(&alphabet).minimize();
	println!(
		"outputs: {} states explicit, {} after Mata's minimization",
		explicit.num_states(),
		minimal.num_states()
	);

	Ok(())
}
