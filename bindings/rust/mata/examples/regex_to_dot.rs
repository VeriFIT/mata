//! Builds the minimal DFA of a regular expression and prints it in the DOT format of Graphviz.
//!
//! ```text
//! cargo run --example regex_to_dot -- '(a|b)*abb' | dot -Tsvg > dfa.svg
//! ```

use std::{env, process};

use mata::Nfa;

fn main() {
    let Some(pattern) = env::args().nth(1) else {
        eprintln!("usage: regex_to_dot PATTERN");
        process::exit(2);
    };
    let nfa = match Nfa::from_regex(&pattern) {
        Ok(nfa) => nfa,
        Err(error) => {
            eprintln!("{error}");
            process::exit(1);
        }
    };
    let dfa = nfa.minimize();
    print!("{}", dfa.dot().ascii_labels(true).intervals(true));
}
