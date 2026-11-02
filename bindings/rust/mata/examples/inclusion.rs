//! Checks whether the language of one automaton is included in another's and prints a counterexample.
//!
//! ```text
//! cargo run --release --example inclusion -- SMALLER.mata BIGGER.mata
//! ```
//!
//! Both files are parsed with one alphabet, so equal symbol names mean equal symbols.

use std::{env, fs, process};

use mata::{Alphabet, Nfa};

fn main() -> Result<(), Box<dyn std::error::Error>> {
	let paths: Vec<String> = env::args().skip(1).collect();
	let [smaller, bigger] = paths.as_slice() else {
		eprintln!("usage: inclusion SMALLER.mata BIGGER.mata");
		process::exit(2);
	};
	let mut alphabet = Alphabet::new();
	let smaller = Nfa::parse_mata(&fs::read_to_string(smaller)?, &mut alphabet)?;
	let bigger = Nfa::parse_mata(&fs::read_to_string(bigger)?, &mut alphabet)?;
	match smaller.find_word_not_in(&bigger) {
		None => println!("included"),
		Some(word) => println!("not included: {}", alphabet.display_word(&word)),
	}
	Ok(())
}
