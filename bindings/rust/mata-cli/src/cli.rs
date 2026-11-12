// Copyright (c) 2026 VeriFIT. GNU GPL v3. See LICENSE in the project root.

use std::collections::HashMap;
use std::fmt::Write as _;

use crate::eval::{Engine, Value};
use crate::ops::Operation;
use crate::scripts::{Inputs, Source};
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
		"script" => script(&arguments[1..]),
		"nfa" => direct(&arguments[1..], "nfa"),
		"nft" => direct(&arguments[1..], "nft"),
		"convert" => crate::convert::main(&arguments[1..]),
		"version" | "--version" => {
			print!("{}", version());
			Ok(Exit::Success)
		}
		_ => Err(Error::plain(format!("unknown command '{command}'"))),
	}
}

/// The versions of the command, of the Mata Script language it runs, and of the library it calls.
fn version() -> String {
	format!(
		"mata {}\nMata Script {}\nlibmata {}\n",
		env!("CARGO_PKG_VERSION"),
		crate::version::LANGUAGE,
		mata::sys::LIBRARY_VERSION
	)
}

fn help(arguments: &[String]) -> Result<Exit, Error> {
	match arguments.first().map(String::as_str) {
		None | Some("help") => println!("{}", main_help()),
		Some("script") => match arguments.get(1).map(String::as_str) {
			None => println!("{}", script_help()),
			Some("run") => println!("{}", run_help()),
			Some("check") => println!("{}", check_help()),
			Some("repl") => println!("{}", repl_help()),
			Some(other) => {
				return Err(Error::plain(format!("unknown script subcommand '{other}'")));
			}
		},
		Some("nfa") => print!("{}", direct_help("nfa")),
		Some("nft") => print!("{}", direct_help("nft")),
		Some("convert") => println!("{}", crate::convert::help()),
		Some("formats") => println!("{}", formats_help()),
		Some("version") => print!("{}", version()),
		Some(other) => {
			return Err(Error::plain(format!("unknown command '{other}'")));
		}
	}
	Ok(Exit::Success)
}

fn main_help() -> &'static str {
	"usage: mata <command> [options]

commands:
  script run|check|repl    run, check, or interactively evaluate a Mata Script
  nfa <operation> …        one NFA operation on automata in the .mata format
  nft <operation> …        one NFT operation on automata in the .mata format
  convert [options]        convert automata between the .mata and DOT formats
  formats                  list the supported formats, features, and every operation
  version                  print the versions of this command, the language, and the library
  help [command]           print this text, or the text of one command

Every command accepts --help."
}

fn script_help() -> &'static str {
	"usage: mata script <run|check|repl> [options] …

  run SCRIPT [INPUT…]   run a Mata Script and write its results to the output stream
  check [SCRIPT]        check a Mata Script without running it
  repl                  evaluate the Mata Script forms read from standard input

Every subcommand accepts --help."
}

fn run_help() -> &'static str {
	"usage: mata script run [options] [SCRIPT] [INPUT…]

runs SCRIPT, a .matasc file or `-` for standard input, and writes its results.

options:
  --script PATH       the script file, instead of the SCRIPT operand
  --expression TEXT   the script text, instead of a script file
  --input NAME=PATH   bind the input at PATH to NAME, which is $NAME in the script
  --param NAME=VALUE  give the parameter NAME that the script declares with declare-param
  --output PATH       write the output to PATH instead of standard output; `-` is standard output
  --no-check          skip the static check: run forms until one fails at run time
  --help              print this text
  --                  every following argument is an operand

INPUT operands are positional inputs: they become $1, $2, … and `inputs`. An INPUT
of `-` is standard input. Command-line paths are relative to the current directory;
paths written in the script are relative to the script's directory."
}

fn check_help() -> &'static str {
	"usage: mata script check [options] [SCRIPT | --script PATH | --expression TEXT]

checks SCRIPT, a .matasc file or `-` for standard input, without loading an input
or running an operation. It writes `checked: <path>` to standard output and the
report to standard error.

options:
  --script PATH       the script file, instead of the SCRIPT operand
  --expression TEXT   the script text, instead of a script file
  --help              print this text
  --                  every following argument is an operand"
}

fn repl_help() -> &'static str {
	"usage: mata script repl [--output PATH]

evaluates the Mata Script forms read from standard input and writes the value
of every top-level expression to the output stream; an automaton is shown by its
size, and `print` shows it in full. An error ends the current form only, and makes
the session end with status 2.

options:
  --output PATH   write the values to PATH instead of standard output; `-` is standard output
  --help          print this text"
}

fn direct_help(namespace: &str) -> String {
	let kind = if namespace == "nft" { "NFT" } else { "NFA" };
	let mut text = format!(
		"usage: mata {namespace} <operation> [options] [OPERAND…]

performs one {kind} operation. An automaton operand is a file in the .mata format,
or `-` for standard input; a file with several automata gives one operand each.
A Word operand is symbol names separated by spaces, a String operand is the text
itself, an Int operand a number. A query exits 0 for true and 1 for false; its name
may leave out the trailing `?`.

options:
  --output PATH        write the result to PATH instead of standard output; `-` is standard output
  --hint NAME=VALUE    a hint of the operation, as `--hint algorithm=naive`
  --help               print this text and the operations below
  --                   every following argument is an operand

operations:
"
	);
	for operation in crate::ops::OPERATIONS {
		if let Some(rest) = operation.name.strip_prefix(&format!("{namespace}:"))
			&& direct_operand_sorts(operation).is_some()
		{
			write!(text, "  {rest}{}", operand_list(operation))
				.expect("writing to a String cannot fail");
			for hint in crate::ops::hints_of(operation.name) {
				write!(text, " [--hint {}={}]", hint.name, hint.values.join("|"))
					.expect("writing to a String cannot fail");
			}
			text.push('\n');
		}
	}
	text
}

fn formats_help() -> &'static str {
	"usage: mata formats

writes one line per supported format, `format <name>`, one line per language
feature, `feature <name> implemented|not-implemented`, and one line per
operation, `operation <name> <operands>`."
}

/// The operand sorts an operation takes from the command line, `None` for an operation whose operands
/// cannot be written there, such as a function or a literal.
fn direct_operand_sorts(operation: &Operation) -> Option<Vec<&'static str>> {
	if operation.handler.ends_with("_literal") {
		return None;
	}
	let count = operation.maximum.unwrap_or(operation.minimum);
	let mut sorts = Vec::with_capacity(count);
	for index in 0..count {
		let sort = crate::ops::operand_sort(operation, index)?;
		if !matches!(
			sort,
			"Nfa" | "Nft" | "Automaton" | "Word" | "String" | "Int" | "Nfa|Word" | "(List Int)"
		) {
			return None;
		}
		sorts.push(sort);
	}
	Some(sorts)
}

/// The operands of an operation, as `--help` lists them.
fn operand_list(operation: &Operation) -> String {
	let Some(sorts) = direct_operand_sorts(operation) else {
		return String::new();
	};
	let mut text = String::new();
	for sort in &sorts {
		write!(text, " {}", sort.to_uppercase()).expect("writing to a String cannot fail");
	}
	if operation.maximum.is_none()
		&& let Some(last) = sorts.last()
	{
		write!(text, " [{}…]", last.to_uppercase()).expect("writing to a String cannot fail");
	}
	text
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
	for feature in crate::version::FEATURES {
		println!(
			"feature {} {}",
			feature.name,
			if feature.implemented {
				"implemented"
			} else {
				"not-implemented"
			}
		);
	}
	for operation in crate::ops::OPERATIONS {
		println!("operation {}{}", operation.name, operand_count(operation));
	}
}

fn script(arguments: &[String]) -> Result<Exit, Error> {
	let Some(subcommand) = arguments.first() else {
		return Err(Error::plain(
			"expected 'run', 'check', or 'repl' after 'script'".into(),
		));
	};
	match subcommand.as_str() {
		"run" => script_run(&arguments[1..]),
		"check" => script_check(&arguments[1..]),
		"repl" => script_repl(&arguments[1..]),
		"--help" | "-h" => {
			println!("{}", script_help());
			Ok(Exit::Success)
		}
		_ => Err(Error::plain(format!(
			"unknown script subcommand '{subcommand}'"
		))),
	}
}

/// The value of an option such as `--input`, which takes the next argument.
fn option_value<'a>(
	arguments: &'a [String],
	index: usize,
	option: &str,
) -> Result<&'a String, Error> {
	arguments
		.get(index)
		.ok_or_else(|| Error::plain(format!("{option} needs a value")))
}

/// `NAME=VALUE` of `--input`, `--param`, or `--hint`, with a name each may give once.
fn insert_pair(pairs: &mut HashMap<String, String>, text: &str, option: &str) -> Result<(), Error> {
	let (name, value) = text
		.split_once('=')
		.ok_or_else(|| Error::plain(format!("{option} needs NAME=VALUE, found '{text}'")))?;
	if name.is_empty() {
		return Err(Error::plain(format!(
			"{option} needs a name, found '{text}'"
		)));
	}
	if pairs.insert(name.to_owned(), value.to_owned()).is_some() {
		return Err(Error::plain(format!("{option} {name} given twice")));
	}
	Ok(())
}

/// The script given as `SCRIPT`, `--script PATH`, or `--expression TEXT`: exactly one of them.
#[derive(Default)]
struct ScriptChoice {
	operand: Option<String>,
	path: Option<String>,
	expression: Option<String>,
}

impl ScriptChoice {
	fn source(self) -> Result<Source, Error> {
		match (self.operand, self.path, self.expression) {
			(Some(operand), None, None) if operand == "-" => Ok(Source::Stdin),
			(Some(path), None, None) | (None, Some(path), None) => Ok(Source::File(path)),
			(None, None, Some(text)) => Ok(Source::Expression(text)),
			(None, None, None) => Err(Error::plain(
				"no script given; give SCRIPT, --script PATH, or --expression TEXT".into(),
			)),
			_ => Err(Error::plain(
				"give the script once, as SCRIPT, --script, or --expression".into(),
			)),
		}
	}

	fn given(&self) -> bool {
		self.operand.is_some() || self.path.is_some() || self.expression.is_some()
	}
}

fn script_run(arguments: &[String]) -> Result<Exit, Error> {
	let mut choice = ScriptChoice::default();
	let mut output = None;
	let mut checked = true;
	let mut inputs = Inputs {
		positional: Vec::new(),
		named: HashMap::new(),
		parameters: HashMap::new(),
	};
	let mut separator = false;
	let mut index = 0;
	while index < arguments.len() {
		let argument = &arguments[index];
		index += 1;
		if separator {
			if choice.given() {
				inputs.positional.push(argument.clone());
			} else {
				choice.operand = Some(argument.clone());
			}
			continue;
		}
		match argument.as_str() {
			"--" => separator = true,
			"--expression" | "-e" => {
				choice.expression = Some(option_value(arguments, index, "--expression")?.clone());
				index += 1;
			}
			"--script" => {
				choice.path = Some(option_value(arguments, index, "--script")?.clone());
				index += 1;
			}
			"--output" | "-o" => {
				output = Some(option_value(arguments, index, "--output")?.clone());
				index += 1;
			}
			"--no-check" => checked = false,
			"--input" => {
				insert_pair(
					&mut inputs.named,
					option_value(arguments, index, "--input")?,
					"--input",
				)?;
				index += 1;
			}
			"--param" => {
				insert_pair(
					&mut inputs.parameters,
					option_value(arguments, index, "--param")?,
					"--param",
				)?;
				index += 1;
			}
			"--help" | "-h" => {
				println!("{}", run_help());
				return Ok(Exit::Success);
			}
			_ if argument.starts_with("--") => {
				return Err(Error::plain(format!("unknown option '{argument}'")));
			}
			_ if !choice.given() => choice.operand = Some(argument.clone()),
			_ => inputs.positional.push(argument.clone()),
		}
	}
	let source = choice.source()?;
	crate::scripts::run(&source, inputs, output.as_deref(), checked)
}

fn script_check(arguments: &[String]) -> Result<Exit, Error> {
	let mut choice = ScriptChoice::default();
	let mut separator = false;
	let mut index = 0;
	while index < arguments.len() {
		let argument = &arguments[index];
		index += 1;
		if separator || argument == "-" || !argument.starts_with('-') {
			if choice.given() {
				return Err(Error::plain(format!(
					"unexpected argument '{argument}'; check takes no inputs"
				)));
			}
			choice.operand = Some(argument.clone());
			continue;
		}
		match argument.as_str() {
			"--expression" | "-e" => {
				choice.expression = Some(option_value(arguments, index, "--expression")?.clone());
				index += 1;
			}
			"--script" => {
				choice.path = Some(option_value(arguments, index, "--script")?.clone());
				index += 1;
			}
			"--help" | "-h" => {
				println!("{}", check_help());
				return Ok(Exit::Success);
			}
			"--" => separator = true,
			_ => return Err(Error::plain(format!("unknown option '{argument}'"))),
		}
	}
	crate::scripts::check(&choice.source()?)
}

fn script_repl(arguments: &[String]) -> Result<Exit, Error> {
	let mut output = None;
	let mut index = 0;
	while index < arguments.len() {
		let argument = &arguments[index];
		index += 1;
		match argument.as_str() {
			"--output" | "-o" => {
				output = Some(option_value(arguments, index, "--output")?.clone());
				index += 1;
			}
			"--help" | "-h" => {
				println!("{}", repl_help());
				return Ok(Exit::Success);
			}
			_ if argument.starts_with("--") => {
				return Err(Error::plain(format!("unknown option '{argument}'")));
			}
			_ => {
				return Err(Error::plain(format!(
					"unexpected argument '{argument}'; a session takes no inputs"
				)));
			}
		}
	}
	crate::scripts::repl(output.as_deref())
}

/// The catalogue operation `mata nfa NAME` or `mata nft NAME` names; a query may leave out its `?`,
/// which a shell would otherwise read as a pattern.
fn direct_operation(namespace: &str, name: &str) -> Result<&'static Operation, Error> {
	let full = format!("{namespace}:{name}");
	crate::ops::operation(&full)
		.or_else(|| {
			(!name.ends_with('?'))
				.then(|| crate::ops::operation(&format!("{full}?")))
				.flatten()
		})
		.ok_or_else(|| Error::plain(format!("unknown operation '{full}'")))
}

fn direct(arguments: &[String], namespace: &str) -> Result<Exit, Error> {
	let Some(name) = arguments.first() else {
		return Err(Error::plain(format!(
			"expected an operation after 'mata {namespace}'"
		)));
	};
	if name == "--help" || name == "-h" {
		print!("{}", direct_help(namespace));
		return Ok(Exit::Success);
	}
	let operation = direct_operation(namespace, name)?;
	let Some(sorts) = direct_operand_sorts(operation) else {
		return Err(Error::plain(format!(
			"operation '{}' takes operands that a command line cannot give; use it in a script",
			operation.name
		)));
	};
	let mut operands = Vec::new();
	let mut output = None;
	let mut hints = HashMap::new();
	let mut separator = false;
	let mut index = 1;
	while index < arguments.len() {
		let argument = &arguments[index];
		index += 1;
		if separator {
			operands.push(argument.clone());
			continue;
		}
		match argument.as_str() {
			"--" => separator = true,
			"--output" | "-o" => {
				output = Some(option_value(arguments, index, "--output")?.clone());
				index += 1;
			}
			"--hint" => {
				insert_pair(
					&mut hints,
					option_value(arguments, index, "--hint")?,
					"--hint",
				)?;
				index += 1;
			}
			"--help" | "-h" => {
				print!("{}", direct_help(namespace));
				return Ok(Exit::Success);
			}
			_ if argument.starts_with("--") => {
				return Err(Error::plain(format!("unknown option '{argument}'")));
			}
			_ => operands.push(argument.clone()),
		}
	}
	let mut engine = Engine::new();
	let values = direct_values(&mut engine, operation, &sorts, &operands)?;
	if !crate::ops::accepts(operation, values.len()) {
		return Err(Error::plain(crate::ops::arity_message(
			operation,
			values.len(),
		)));
	}
	let mut hints: Vec<(String, String)> = hints.into_iter().collect();
	hints.sort();
	let value = crate::ops::call(operation.name, values, &hints, &mut engine)?;
	let text = display_value(&value, &engine);
	match output.as_deref() {
		None | Some("-") => print!("{text}"),
		Some(path) => std::fs::write(path, text)
			.map_err(|e| Error::plain(format!("cannot write '{path}': {e}")))?,
	}
	if let Value::Bool(verdict) = value {
		Ok(if verdict { Exit::Success } else { Exit::False })
	} else {
		Ok(Exit::Success)
	}
}

/// The values of the operands of a direct command, each read as the sort its position takes. An
/// automaton operand that is a file with several automata gives one value each; standard input can
/// be read once.
fn direct_values(
	engine: &mut Engine,
	operation: &Operation,
	sorts: &[&str],
	operands: &[String],
) -> Result<Vec<Value>, Error> {
	let mut values = Vec::new();
	let mut stdin_used = false;
	for operand in operands {
		let index = values.len();
		let sort = sorts
			.get(index)
			.copied()
			.or_else(|| {
				operation
					.maximum
					.is_none()
					.then(|| sorts.last().copied())
					.flatten()
			})
			.ok_or_else(|| {
				Error::plain(crate::ops::arity_message(
					operation,
					operands.len().max(index + 1),
				))
			})?;
		match sort {
			"Nfa" | "Nft" | "Automaton" | "Nfa|Word" => {
				if operand == "-" {
					if stdin_used {
						return Err(Error::plain("standard input can be read only once".into()));
					}
					stdin_used = true;
				}
				let kind = match sort {
					"Nft" => "nft",
					"Automaton" => "automaton",
					_ => "nfa",
				};
				values.extend(engine.load_inputs(operand, kind)?);
			}
			"Word" => {
				let mut symbols = Vec::new();
				for name in operand.split_whitespace() {
					symbols.push(engine.intern_name(name).map_err(Error::plain)?);
				}
				values.push(Value::Word(mata::Word::from(symbols)));
			}
			"String" => values.push(Value::String(operand.clone())),
			"Int" => values.push(Value::Int(
				operand
					.parse()
					.map_err(|_| Error::plain(format!("'{operand}' is not an Int")))?,
			)),
			"(List Int)" => {
				let mut list = Vec::new();
				for part in operand.split([',', ' ']).filter(|part| !part.is_empty()) {
					list.push(Value::Int(
						part.parse()
							.map_err(|_| Error::plain(format!("'{part}' is not an Int")))?,
					));
				}
				values.push(Value::List(std::rc::Rc::new(list)));
			}
			_ => unreachable!("direct_operand_sorts admits only the sorts above"),
		}
	}
	Ok(values)
}
