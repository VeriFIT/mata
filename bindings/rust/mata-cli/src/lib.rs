// Copyright (c) 2026 VeriFIT. GNU GPL v3. See LICENSE in the project root.

//! Command-line front end for Mata: the `mata` binary with script execution and automata tools.
use crate::eval::{AutomatonKind, Value};

#[derive(Debug)]
pub(crate) struct Error {
	pub(crate) message: String,
	pub(crate) at: Option<(usize, usize)>,
	pub(crate) incomplete: bool,
}

impl Error {
	pub(crate) fn plain(message: String) -> Error {
		Error {
			message,
			at: None,
			incomplete: false,
		}
	}

	pub(crate) fn at(message: String, line: usize, column: usize) -> Error {
		Error {
			message,
			at: Some((line, column)),
			incomplete: false,
		}
	}

	pub(crate) fn render(&self) -> String {
		match self.at {
			Some((line, column)) => format!("{} at {line}:{column}", self.message),
			None => self.message.clone(),
		}
	}
}

impl From<String> for Error {
	fn from(message: String) -> Error {
		Error::plain(message)
	}
}

impl From<&str> for Error {
	fn from(message: &str) -> Error {
		Error::plain(message.to_owned())
	}
}

impl std::fmt::Display for Error {
	fn fmt(&self, formatter: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
		formatter.write_str(&self.render())
	}
}

pub(crate) mod ast;
pub(crate) mod check;
/// Command dispatch of the `mata` binary.
pub mod cli;
pub(crate) mod convert;
pub(crate) mod dot;
pub(crate) mod eval;
pub(crate) mod lexer;
pub(crate) mod literal;
pub(crate) mod ops;
pub(crate) mod scripts;
pub(crate) mod version;

/// The `.mata` text of one automaton, which the library prints with a trailing newline of its own.
fn automaton_text(text: &dyn std::fmt::Display) -> String {
	format!("{}\n", text.to_string().trim_end_matches('\n'))
}

/// Displays one value on one or more lines, as `cli.md` specifies.
pub(crate) fn display_value(value: &Value, engine: &eval::Engine) -> String {
	match value {
		Value::Bool(value) => format!("{value}\n"),
		Value::Int(value) => format!("{value}\n"),
		Value::String(value) => format!("{value}\n"),
		Value::Nfa(nfa) => automaton_text(&nfa.display_with(&engine.script_alphabet)),
		Value::Nft(nft) => automaton_text(&nft.display_with(&engine.script_alphabet)),
		Value::Automaton(automaton) => match &**automaton {
			AutomatonKind::Nfa(nfa) => automaton_text(&nfa.display_with(&engine.script_alphabet)),
			AutomatonKind::Nft(nft) => automaton_text(&nft.display_with(&engine.script_alphabet)),
		},
		Value::List(list) => list
			.iter()
			.map(|element| display_value(element, engine))
			.collect::<String>(),
		Value::Word(word) => format!("{}\n", engine.script_alphabet.display_word(word)),
		Value::Alphabet(alphabet) => format!(
			"{}\n",
			alphabet
				.symbols()
				.map(|symbol| {
					alphabet
						.name(symbol)
						.unwrap_or(&format!("{}", symbol.value()))
						.to_owned()
				})
				.collect::<Vec<_>>()
				.join(" ")
		),
		Value::Symbol(symbol) => format!(
			"{}\n",
			engine
				.script_alphabet
				.name(*symbol)
				.unwrap_or(&symbol.to_string())
		),
		Value::Run(run) => {
			let word = engine.script_alphabet.display_word(&run.word);
			let path = run
				.path
				.iter()
				.map(|state| state.index().to_string())
				.collect::<Vec<_>>()
				.join(" ");
			format!("(run :word ({word}) :path ({path}))\n")
		}
		Value::Function(_) => "#<function>\n".to_owned(),
		Value::None => "none\n".to_owned(),
	}
}
