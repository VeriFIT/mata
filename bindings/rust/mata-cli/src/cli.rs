// Copyright (c) 2026 VeriFIT. GNU GPL v3. See LICENSE in the project root.

use std::collections::HashMap;
use std::fmt::Write as _;
use std::io::Read;

use crate::eval::Engine;
use crate::ops::Operation;
use crate::{Error, display_value};

/// Exit status of the `mata` binary.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Exit {
	/// The command succeeded, and a query answered true.
	Success,
	/// A query answered false.
	False,
	/// An `assert` in a script failed.
	Assertion,
	/// Wrong usage or a runtime error.
	Usage,
}

/// Runs the command given as the process arguments and returns its exit status.
#[must_use]
pub fn main() -> Exit {
	let arguments: Vec<String> = std::env::args().skip(1).collect();
	let result = dispatch(&arguments);
	match result {
		Ok(exit) => exit,
		Err(error) => {
			eprintln!("mata: {}", error.render());
			Exit::Usage
		}
	}
}

fn dispatch(arguments: &[String]) -> Result<Exit, Error> {
	let Some(command) = arguments.first() else {
		println!("{}", main_help());
		return Ok(Exit::Success);
	};
	match command.as_str() {
		"help" | "--help" | "-h" => help(&arguments[1..]),
		"formats" => {
			formats();
			Ok(Exit::Success)
		}
		"scripts" => scripts(&arguments[1..]),
		"nfa" => direct(&arguments[1..], "nfa"),
		"nft" => direct(&arguments[1..], "nft"),
		"convert" => crate::convert::main(&arguments[1..]),
		"version" | "--version" => {
			println!("mata {}", env!("CARGO_PKG_VERSION"));
			Ok(Exit::Success)
		}
		_ => Err(Error::plain(format!("unknown command '{command}'"))),
	}
}

fn help(arguments: &[String]) -> Result<Exit, Error> {
	match arguments.first().map(String::as_str) {
		None | Some("help") => println!("{}", main_help()),
		Some("scripts") => match arguments.get(1).map(String::as_str) {
			None => println!("{}", scripts_help()),
			Some("run") => println!("{}", run_help()),
			Some("check") => println!("{}", check_help()),
			Some("repl") => println!("{}", repl_help()),
			Some(other) => {
				return Err(Error::plain(format!(
					"unknown scripts subcommand '{other}'"
				)));
			}
		},
		Some("nfa") => println!("{}", direct_help("nfa")),
		Some("nft") => println!("{}", direct_help("nft")),
		Some("convert") => println!("{}", crate::convert::help()),
		Some("formats") => println!("{}", formats_help()),
		Some("version") => {
			println!("mata {}", env!("CARGO_PKG_VERSION"));
		}
		Some(other) => {
			return Err(Error::plain(format!("unknown command '{other}'")));
		}
	}
	Ok(Exit::Success)
}

fn main_help() -> &'static str {
	"usage: mata <command> [options]

commands:
  scripts run|check|repl   run, check, or interactively evaluate a Mata Script
  nfa <operation> …        one NFA operation on automata in the .mata format
  nft <operation> …        one NFT operation on automata in the .mata format
  convert [options]        convert automata between the .mata and DOT formats
  formats                  list the supported formats and every operation
  version                  print the version of this command
  help [command]           print this text, or the text of one command

Every command accepts --help."
}

fn scripts_help() -> &'static str {
	"usage: mata scripts <run|check|repl> [options] …

  run SCRIPT [INPUT…]   run a Mata Script and write its results to the output stream
  check [SCRIPT]        check a Mata Script without running it
  repl                  evaluate the Mata Script forms read from standard input

Every subcommand accepts --help."
}

fn run_help() -> &'static str {
	"usage: mata scripts run [options] [SCRIPT] [INPUT…]

runs SCRIPT, a .matasc file or `-` for standard input, and writes its results.

options:
  --script PATH       the script file, instead of the SCRIPT operand
  --expression TEXT   the script text, instead of a script file
  --input NAME=PATH   bind the input at PATH to NAME, which is $NAME in the script
  --output PATH       write the output to PATH instead of standard output; `-` is standard output
  --help              print this text
  --                  every following argument is an operand

INPUT operands are positional inputs: they become $1, $2, … and `inputs`."
}

fn check_help() -> &'static str {
	"usage: mata scripts check [options] [SCRIPT | --script PATH | --expression TEXT]

checks SCRIPT without loading an input or running an operation. It writes
`checked: <path>` to standard output and the report to standard error.

options:
  --script PATH       the script file, instead of the SCRIPT operand
  --expression TEXT   the script text, instead of a script file
  --help              print this text
  --                  every following argument is an operand"
}

fn repl_help() -> &'static str {
	"usage: mata scripts repl [--output PATH]

evaluates the Mata Script forms read from standard input and writes the value
of every top-level expression to the output stream. An error ends the current
form only.

options:
  --output PATH   write the values to PATH instead of standard output; `-` is standard output
  --help          print this text"
}

fn direct_help(namespace: &str) -> String {
	let kind = if namespace == "nft" { "NFT" } else { "NFA" };
	let mut text = format!(
		"usage: mata {namespace} <operation> [options] [INPUT…]

performs one {kind} operation on automata read from INPUT operands, which are
`-` for standard input or files in the .mata format. A query exits 0 for true
and 1 for false.

options:
  --output PATH   write the result to PATH instead of standard output; `-` is standard output
  --help          print this text and the operations below
  --              every following argument is an operand

operations:"
	);
	for operation in crate::ops::OPERATIONS {
		if let Some(rest) = operation.name.strip_prefix(&format!("{namespace}:")) {
			writeln!(text, "\n  {namespace}:{rest}{}", operand_count(operation))
				.expect("writing to a String cannot fail");
		}
	}
	text.push('\n');
	text
}

fn formats_help() -> &'static str {
	"usage: mata formats

writes one line per supported format, `format <name>`, and one line per
operation, `operation <name> <operands>`."
}

fn operand_count(operation: &Operation) -> String {
	match operation.maximum {
		Some(maximum) if maximum == operation.minimum => match maximum {
			1 => " (1 operand)".to_owned(),
			count => format!(" ({count} operands)"),
		},
		Some(0) => String::new(),
		Some(maximum) => format!(" (at most {maximum} operands)"),
		None => match operation.minimum {
			0 => " (any operands)".to_owned(),
			1 => " (1 or more operands)".to_owned(),
			minimum => format!(" ({minimum} or more operands)"),
		},
	}
}

fn formats() {
	for format in ["mata", "dot"] {
		println!("format {format}");
	}
	for operation in crate::ops::OPERATIONS {
		println!("operation {}{}", operation.name, operand_count(operation));
	}
}

/// Splits an option argument list into the arguments before and after `--`.
fn split_separator(arguments: &[String]) -> (&[String], &[String]) {
	match arguments.iter().position(|argument| argument == "--") {
		Some(position) => (&arguments[..position], &arguments[position + 1..]),
		None => (arguments, &[]),
	}
}

fn scripts(arguments: &[String]) -> Result<Exit, Error> {
	let Some(subcommand) = arguments.first() else {
		return Err(Error::plain(
			"expected 'run', 'check', or 'repl' after 'scripts'".into(),
		));
	};
	match subcommand.as_str() {
		"run" => scripts_run(&arguments[1..]),
		"check" => scripts_check(&arguments[1..]),
		"repl" => scripts_repl(&arguments[1..]),
		"--help" | "-h" => {
			println!("{}", scripts_help());
			Ok(Exit::Success)
		}
		_ => Err(Error::plain(format!(
			"unknown scripts subcommand '{subcommand}'"
		))),
	}
}

fn scripts_run(arguments: &[String]) -> Result<Exit, Error> {
	let mut script = None;
	let mut script_path = None;
	let mut expression = None;
	let mut output = None;
	let mut named = HashMap::new();
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
			"--expression" | "-e" => {
				index += 1;
				expression = Some(
					arguments
						.get(index)
						.ok_or_else(|| Error::plain("--expression needs a script".into()))?
						.clone(),
				);
			}
			"--script" => {
				index += 1;
				script_path = Some(
					arguments
						.get(index)
						.ok_or_else(|| Error::plain("--script needs a path".into()))?
						.clone(),
				);
			}
			"--output" | "-o" => {
				index += 1;
				output = Some(
					arguments
						.get(index)
						.ok_or_else(|| Error::plain("--output needs a path".into()))?
						.clone(),
				);
			}
			"--input" => {
				index += 1;
				let pair = arguments
					.get(index)
					.ok_or_else(|| Error::plain("--input needs NAME=PATH".into()))?
					.clone();
				let (name, path) = pair.split_once('=').ok_or_else(|| {
					Error::plain(format!("--input needs NAME=PATH, found '{pair}'"))
				})?;
				if named.insert(name.to_owned(), path.to_owned()).is_some() {
					return Err(Error::plain(format!("input '{name}' given twice")));
				}
			}
			"--help" | "-h" => {
				println!("{}", run_help());
				return Ok(Exit::Success);
			}
			_ if argument.starts_with("--") => {
				return Err(Error::plain(format!("unknown option '{argument}'")));
			}
			_ if script.is_none() && expression.is_none() && script_path.is_none() => {
				script = Some(argument.clone());
			}
			_ => inputs.push(argument.clone()),
		}
		index += 1;
	}
	if script.is_some() && (script_path.is_some() || expression.is_some()) {
		return Err(Error::plain(
			"give the script once, as SCRIPT, --script, or --expression".into(),
		));
	}
	if script_path.is_some() && expression.is_some() {
		return Err(Error::plain(
			"give the script once, as --script or --expression".into(),
		));
	}
	crate::scripts::run_scripts_run(
		script.as_deref(),
		script_path.as_deref(),
		expression.as_deref(),
		&inputs,
		named,
		output.as_deref(),
	)
}

fn scripts_check(arguments: &[String]) -> Result<Exit, Error> {
	let mut script = None;
	let mut script_path = None;
	let mut expression = None;
	let mut separator = false;
	let mut index = 0;
	while index < arguments.len() {
		let argument = &arguments[index];
		if separator {
			if script.is_some() || script_path.is_some() || expression.is_some() {
				return Err(Error::plain(format!("unexpected argument '{argument}'")));
			}
			script = Some(argument.clone());
			index += 1;
			continue;
		}
		match argument.as_str() {
			"--" => separator = true,
			"--expression" | "-e" => {
				index += 1;
				expression = Some(
					arguments
						.get(index)
						.ok_or_else(|| Error::plain("--expression needs a script".into()))?
						.clone(),
				);
			}
			"--script" => {
				index += 1;
				script_path = Some(
					arguments
						.get(index)
						.ok_or_else(|| Error::plain("--script needs a path".into()))?
						.clone(),
				);
			}
			"--help" | "-h" => {
				println!("{}", check_help());
				return Ok(Exit::Success);
			}
			_ if argument.starts_with("--") => {
				return Err(Error::plain(format!("unknown option '{argument}'")));
			}
			_ if script.is_none() && expression.is_none() && script_path.is_none() => {
				script = Some(argument.clone());
			}
			_ => return Err(Error::plain(format!("unexpected argument '{argument}'"))),
		}
		index += 1;
	}
	let given = usize::from(script.is_some())
		+ usize::from(script_path.is_some())
		+ usize::from(expression.is_some());
	if given > 1 {
		return Err(Error::plain(
			"give the script once, as SCRIPT, --script, or --expression".into(),
		));
	}
	let source = if let Some(expression) = expression {
		expression
	} else if let Some(path) = script.as_ref().or(script_path.as_ref()) {
		std::fs::read_to_string(path)
			.map_err(|e| Error::plain(format!("cannot read '{path}': {e}")))?
	} else {
		return Err(Error::plain("no script given".into()));
	};
	let label = script
		.as_deref()
		.or(script_path.as_deref())
		.unwrap_or("<expression>");
	crate::scripts::run_scripts_check(label, &source)
}

fn scripts_repl(arguments: &[String]) -> Result<Exit, Error> {
	let (options, operands) = split_separator(arguments);
	let mut output = None;
	let mut index = 0;
	while index < options.len() {
		match options[index].as_str() {
			"--output" | "-o" => {
				index += 1;
				output = Some(
					arguments
						.get(index)
						.ok_or_else(|| Error::plain("--output needs a path".into()))?
						.clone(),
				);
			}
			"--help" | "-h" => {
				println!("{}", repl_help());
				return Ok(Exit::Success);
			}
			argument => return Err(Error::plain(format!("unknown option '{argument}'"))),
		}
		index += 1;
	}
	if let Some(operand) = operands.first() {
		return Err(Error::plain(format!("unexpected argument '{operand}'")));
	}
	crate::scripts::run_scripts_repl(output.as_deref())
}

fn direct(arguments: &[String], namespace: &str) -> Result<Exit, Error> {
	let Some(operation) = arguments.first() else {
		return Err(Error::plain(format!(
			"expected an operation after 'mata {namespace}'"
		)));
	};
	if operation == "--help" || operation == "-h" {
		println!("{}", direct_help(namespace));
		return Ok(Exit::Success);
	}
	let name = format!("{namespace}:{operation}");
	let operation = crate::ops::operation(&name)
		.ok_or_else(|| Error::plain(format!("unknown operation '{name}'")))?;
	let mut inputs = Vec::new();
	let mut output = None;
	let mut separator = false;
	let mut index = 1;
	while index < arguments.len() {
		let argument = &arguments[index];
		if separator {
			inputs.push(argument.clone());
			index += 1;
			continue;
		}
		match argument.as_str() {
			"--" => separator = true,
			"--output" | "-o" => {
				index += 1;
				output = Some(
					arguments
						.get(index)
						.ok_or_else(|| Error::plain("--output needs a path".into()))?
						.clone(),
				);
			}
			"--help" | "-h" => {
				println!("{}", direct_help(namespace));
				return Ok(Exit::Success);
			}
			_ if argument.starts_with("--") => {
				return Err(Error::plain(format!("unknown option '{argument}'")));
			}
			_ => inputs.push(argument.clone()),
		}
		index += 1;
	}
	if !crate::ops::accepts(operation, inputs.len()) {
		return Err(Error::plain(crate::ops::arity_message(
			operation,
			inputs.len(),
		)));
	}
	let mut engine = Engine::new();
	let values = load_direct(&mut engine, &inputs, namespace)?;
	let value = crate::ops::call(operation.name, values, &mut engine)?;
	let text = display_value(&value, &engine);
	match output.as_deref() {
		None | Some("-") => print!("{text}"),
		Some(path) => std::fs::write(path, text)
			.map_err(|e| Error::plain(format!("cannot write '{path}': {e}")))?,
	}
	if let crate::eval::Value::Bool(verdict) = value {
		Ok(if verdict { Exit::Success } else { Exit::False })
	} else {
		Ok(Exit::Success)
	}
}

fn load_direct(
	engine: &mut Engine,
	inputs: &[String],
	namespace: &str,
) -> Result<Vec<crate::eval::Value>, Error> {
	let kind = if namespace == "nft" { "nft" } else { "nfa" };
	let mut values = Vec::new();
	for input in inputs {
		if input == "-" {
			let mut text = String::new();
			std::io::stdin()
				.read_to_string(&mut text)
				.map_err(|e| Error::plain(format!("cannot read standard input: {e}")))?;
			values.push(engine.parse_text(&text, kind)?);
		} else {
			values.push(engine.load_file(input, kind)?);
		}
	}
	Ok(values)
}
