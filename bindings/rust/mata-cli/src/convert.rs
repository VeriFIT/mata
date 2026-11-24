// Copyright (c) 2026 VeriFIT. GNU GPL v3. See LICENSE in the project root.

use std::path::Path;

use mata::Nfa;

use crate::cli::Exit;
use crate::eval::Engine;
use crate::{Error, display_value};

pub(crate) fn help() -> &'static str {
	"usage: mata convert [options] [INPUT…]

converts automata from the .mata format or DOT to the .mata format, DOT, or a
regular expression. INPUT of `-` reads standard input, which can be used only
once.

options:
  --from mata|dot    the format of the input (default: mata)
  --to mata|dot|regex
                     the format of the output (default: dot)
  --kind nfa|nft     parse every input as this kind (default: detect)
  --output PATH      write to PATH; a directory gets one file per input
  --help             print this text
  --                 every following argument is an operand

Every input must hold the same kind of automaton. Regex output is for NFAs only.
With several inputs and no --output, each converted automaton follows the
previous one."
}

/// The options of one `mata convert` invocation, after the arguments are read.
struct Options {
	from: String,
	to: String,
	kind: Option<String>,
	output: Option<String>,
	inputs: Vec<String>,
}

/// `mata convert [--from mata|dot] [--to mata|dot|regex] [--kind nfa|nft] [--output PATH] [INPUT…]`.
pub(crate) fn main(arguments: &[String]) -> Result<Exit, Error> {
	let Some(options) = parse_arguments(arguments)? else {
		crate::write_stdout(&format!("{}\n", help()))?;
		return Ok(Exit::Success);
	};
	convert(&options)
}

/// Reads the arguments; `None` means that `--help` was given.
fn parse_arguments(arguments: &[String]) -> Result<Option<Options>, Error> {
	let mut from = "mata".to_owned();
	let mut to = "dot".to_owned();
	let mut kind = None;
	let mut output = None;
	let mut inputs = Vec::new();
	let mut separator = false;
	let mut index = 0;
	while index < arguments.len() {
		let argument = &arguments[index];
		if separator {
			inputs.push(argument.clone());
			index += 1;
			continue;
		}
		match argument.as_str() {
			"--" => separator = true,
			"--from" => {
				index += 1;
				from.clone_from(value_of(arguments, index, "--from needs a format")?);
			}
			"--to" => {
				index += 1;
				to.clone_from(value_of(arguments, index, "--to needs a format")?);
			}
			"--kind" => {
				index += 1;
				let value = value_of(arguments, index, "--kind needs nfa or nft")?;
				if !matches!(value.as_str(), "nfa" | "nft") {
					return Err(Error::plain(format!("unknown kind '{value}'")));
				}
				kind = Some(value.clone());
			}
			"--output" | "-o" => {
				index += 1;
				output = Some(value_of(arguments, index, "--output needs a path")?.clone());
			}
			"--help" | "-h" => return Ok(None),
			_ if argument.starts_with("--") => {
				return Err(Error::plain(format!("unknown option '{argument}'")));
			}
			_ => inputs.push(argument.clone()),
		}
		index += 1;
	}
	if !matches!(from.as_str(), "mata" | "dot") {
		return Err(Error::plain(format!("unknown format '{from}' for --from")));
	}
	if !matches!(to.as_str(), "mata" | "dot" | "regex") {
		return Err(Error::plain(format!("unknown format '{to}' for --to")));
	}
	if inputs.is_empty() {
		inputs.push("-".to_owned());
	}
	Ok(Some(Options {
		from,
		to,
		kind,
		output,
		inputs,
	}))
}

/// The argument after an option, or an error naming the option.
fn value_of<'a>(arguments: &'a [String], index: usize, message: &str) -> Result<&'a String, Error> {
	arguments
		.get(index)
		.ok_or_else(|| Error::plain(message.to_owned()))
}

/// Converts every input, into one file per input or into one stream.
fn convert(options: &Options) -> Result<Exit, Error> {
	let directory = output_directory(options)?;
	let extension = match options.to.as_str() {
		"dot" => "dot",
		"regex" => "regex",
		_ => "mata",
	};
	if directory.is_some() {
		let mut targets = std::collections::HashMap::new();
		for input in &options.inputs {
			let target = target_name(input, extension)?;
			if let Some(first) = targets.insert(target.clone(), input) {
				return Err(Error::plain(format!(
					"'{first}' and '{input}' would both be written to '{target}'"
				)));
			}
		}
	}
	let mut text = String::new();
	let mut kind: Option<String> = None;
	let mut engine = Engine::new();
	for input in &options.inputs {
		if options.to == "regex" {
			// Regex output uses the current input's names, not names accumulated from earlier inputs.
			engine = Engine::new();
		}
		let source = crate::eval::read_text(input)?;
		let (value, detected) = parse_input(input, &source, options, &mut engine)?;
		match &kind {
			None => kind = Some(detected.clone()),
			Some(first) if first != &detected => {
				return Err(Error::plain(format!(
					"'{input}' holds an {}, but the first input holds an {}",
					detected.to_uppercase(),
					first.to_uppercase()
				)));
			}
			Some(_) => {}
		}
		let rendered = render(&value, &engine, &options.to)?;
		if let Some(directory) = &directory {
			let path = directory.join(target_name(input, extension)?);
			std::fs::write(&path, &rendered)
				.map_err(|e| Error::plain(format!("cannot write '{}': {e}", path.display())))?;
		} else {
			if !text.is_empty() {
				text.push('\n');
			}
			text.push_str(&rendered);
		}
	}
	if directory.is_none() {
		write_stream(&text, options.output.as_deref())?;
	}
	Ok(Exit::Success)
}

/// The directory to write one file per input into, or `None` when the output is one stream.
fn output_directory(options: &Options) -> Result<Option<std::path::PathBuf>, Error> {
	let Some(path) = options.output.as_deref() else {
		return Ok(None);
	};
	if path == "-" || !Path::new(path).is_dir() {
		return Ok(None);
	}
	if options.inputs.iter().any(|input| input == "-") {
		return Err(Error::plain(
			"standard input cannot be converted into a directory".into(),
		));
	}
	Ok(Some(Path::new(path).to_path_buf()))
}

/// The file name an input converts to inside an output directory.
fn target_name(input: &str, extension: &str) -> Result<String, Error> {
	let stem = Path::new(input)
		.file_stem()
		.and_then(|stem| stem.to_str())
		.ok_or_else(|| Error::plain(format!("'{input}' has no file name")))?;
	Ok(format!("{stem}.{extension}"))
}

/// Reads one input in its source format and returns it with the kind that was detected.
fn parse_input(
	input: &str,
	source: &str,
	options: &Options,
	engine: &mut Engine,
) -> Result<(crate::eval::Value, String), Error> {
	if options.from == "dot" {
		// DOT carries no heading, so the kind shows only once the graph is read: a state label or a
		// transducer's graph attribute makes it a transducer.
		let value =
			crate::dot::parse(source, engine).map_err(|e| Error::plain(format!("{input}: {e}")))?;
		let detected = kind_of(&value).to_owned();
		if let Some(given) = &options.kind
			&& given != &detected
		{
			return Err(Error::plain(format!(
				"'{input}' holds an {}, but --kind {given} was given",
				detected.to_uppercase()
			)));
		}
		return Ok((value, detected));
	}
	let detected = options.kind.clone().unwrap_or_else(|| detect_kind(source));
	let value = engine.parse_text(source, &detected)?;
	Ok((value, detected))
}

fn write_stream(text: &str, output: Option<&str>) -> Result<(), Error> {
	match output {
		None | Some("-") => crate::write_stdout(text),
		Some(path) => std::fs::write(path, text)
			.map_err(|e| Error::plain(format!("cannot write '{path}': {e}"))),
	}
}

fn render(value: &crate::eval::Value, engine: &Engine, to: &str) -> Result<String, Error> {
	if to == "regex" {
		return match value {
			crate::eval::Value::Nfa(nfa) => render_regex(nfa, engine),
			crate::eval::Value::Nft(_) => Err(Error::plain(
				"the input holds an NFT; regex output is for NFAs only".into(),
			)),
			crate::eval::Value::Automaton(automaton) => match &**automaton {
				crate::eval::AutomatonKind::Nfa(nfa) => render_regex(nfa, engine),
				crate::eval::AutomatonKind::Nft(_) => Err(Error::plain(
					"the input holds an NFT; regex output is for NFAs only".into(),
				)),
			},
			_ => Err(Error::plain("the input holds no automaton".into())),
		};
	}
	if to == "dot" {
		return Ok(match value {
			crate::eval::Value::Nfa(nfa) => crate::dot::write_nfa(nfa, engine),
			crate::eval::Value::Nft(nft) => crate::dot::write_nft(nft, engine),
			crate::eval::Value::Automaton(automaton) => match &**automaton {
				crate::eval::AutomatonKind::Nfa(nfa) => crate::dot::write_nfa(nfa, engine),
				crate::eval::AutomatonKind::Nft(nft) => crate::dot::write_nft(nft, engine),
			},
			_ => return Err(Error::plain("the input holds no automaton".into())),
		});
	}
	match value {
		crate::eval::Value::Nfa(_)
		| crate::eval::Value::Nft(_)
		| crate::eval::Value::Automaton(_) => Ok(display_value(value, engine)),
		_ => Err(Error::plain("the input holds no automaton".into())),
	}
}

fn render_regex(nfa: &Nfa, engine: &Engine) -> Result<String, Error> {
	let mut regex = nfa.regex();
	if engine
		.script_alphabet
		.iter()
		.any(|(_, name)| name.is_some())
	{
		regex = regex.alphabet(&engine.script_alphabet);
	}
	regex
		.render()
		.map(|regex| format!("{regex}\n"))
		.map_err(|error| Error::plain(error.to_string()))
}

fn kind_of(value: &crate::eval::Value) -> &'static str {
	match value {
		crate::eval::Value::Nft(_) => "nft",
		crate::eval::Value::Automaton(automaton) => match &**automaton {
			crate::eval::AutomatonKind::Nfa(_) => "nfa",
			crate::eval::AutomatonKind::Nft(_) => "nft",
		},
		_ => "nfa",
	}
}

fn detect_kind(source: &str) -> String {
	if crate::eval::holds_transducer(source) {
		"nft".to_owned()
	} else {
		"nfa".to_owned()
	}
}
