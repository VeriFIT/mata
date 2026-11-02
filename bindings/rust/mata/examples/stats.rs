//! Prints facts about the automata in `.mata` files.
//!
//! ```text
//! cargo run --release --example stats -- FILE...
//! ```
//!
//! For each automaton: states, transitions, symbols, whether the language is empty, and the time taken
//! to parse the file. Automata in one file share an alphabet.

use std::time::Instant;
use std::{env, fs, process};

use mata::{Alphabet, Nfa};

fn main() {
	let paths: Vec<String> = env::args().skip(1).collect();
	if paths.is_empty() {
		eprintln!("usage: stats FILE...");
		process::exit(2);
	}
	let mut failures = 0;
	for path in &paths {
		let text = match fs::read_to_string(path) {
			Ok(text) => text,
			Err(error) => {
				eprintln!("{path}: {error}");
				failures += 1;
				continue;
			}
		};
		let mut alphabet = Alphabet::new();
		let start = Instant::now();
		match Nfa::parse_mata_all(&text, &mut alphabet) {
			Ok(automata) => {
				let parse_ms = start.elapsed().as_secs_f64() * 1e3;
				for (i, nfa) in automata.iter().enumerate() {
					println!(
						"{path}#{i}\tstates={}\ttransitions={}\tsymbols={}\tempty={}\tparse_ms={parse_ms:.2}",
						nfa.num_states(),
						nfa.num_transitions(),
						nfa.used_symbols().len(),
						nfa.is_lang_empty(),
					);
				}
			}
			Err(error) => {
				eprintln!("{path}: {error}");
				failures += 1;
			}
		}
	}
	if failures > 0 {
		eprintln!("{failures} of {} files failed", paths.len());
		process::exit(1);
	}
}
