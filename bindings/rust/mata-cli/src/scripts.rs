// Copyright (c) 2026 VeriFIT. GNU GPL v3. See LICENSE in the project root.

//! `mata script run`, `mata script check`, and `mata script repl`.

use std::collections::HashMap;
use std::io::{BufRead, IsTerminal};

use crate::ast::{self, FormKind, Spanned};
use crate::cli::Exit;
use crate::eval::{AutomatonKind, Engine, Sink, Value, kind_of};
use crate::{Error, display_value};

/// Where a script comes from: a file, standard input, or the text of `--expression`.
pub(crate) enum Source {
	File(String),
	Stdin,
	Expression(String),
}

impl Source {
	/// The name `check` reports and the text of the script, with the directory its paths are relative to.
	fn read(&self) -> Result<(String, String, Option<std::path::PathBuf>), Error> {
		match self {
			Source::Expression(text) => Ok(("<expression>".into(), text.clone(), None)),
			Source::Stdin => Ok(("<stdin>".into(), crate::eval::read_text("-")?, None)),
			Source::File(path) => {
				let text = std::fs::read_to_string(path)
					.map_err(|e| Error::plain(format!("cannot read '{path}': {e}")))?;
				let base = std::path::Path::new(path)
					.parent()
					.map(std::path::Path::to_path_buf)
					.filter(|parent| !parent.as_os_str().is_empty());
				Ok((path.clone(), text, base))
			}
		}
	}
}

/// What one `mata script run` is given besides the script.
pub(crate) struct Inputs {
	pub(crate) positional: Vec<String>,
	pub(crate) named: HashMap<String, String>,
	pub(crate) parameters: HashMap<String, String>,
}

/// Parses a script after reading the language version of its header, and checks the version first, so
/// a script for another version is rejected before syntax this interpreter does not know is read.
fn parse(source: &str) -> Result<(Vec<Spanned<FormKind>>, crate::version::Version), Error> {
	let version = crate::version::declared(source)?.unwrap_or(crate::version::LANGUAGE);
	Ok((ast::parse(source)?, version))
}

/// Runs a script; `checked` runs the static check of `mata script check` first, so nothing runs
/// when the check fails.
pub(crate) fn run(
	source: &Source,
	inputs: Inputs,
	output: Option<&str>,
	checked: bool,
) -> Result<Exit, Error> {
	let (_, text, base_dir) = source.read()?;
	if matches!(source, Source::Stdin) && inputs.positional.iter().any(|input| input == "-") {
		return Err(Error::plain(
			"standard input holds the script, so no input can be '-'".into(),
		));
	}
	let (forms, version) = parse(&text)?;
	let mut engine = Engine::for_language(version);
	if checked {
		crate::check::check(&forms, &engine, Some(&inputs))?;
	}
	// The script resolves the relative paths it loads against its own directory, but a path given on
	// the command line is relative to the current one.
	let anchor = |path: &String| anchored(path, base_dir.as_deref());
	let positional = inputs
		.positional
		.iter()
		.map(anchor)
		.collect::<Result<Vec<_>, _>>()?;
	let named = inputs
		.named
		.iter()
		.map(|(name, path)| Ok((name.clone(), anchor(path)?)))
		.collect::<Result<HashMap<_, _>, Error>>()?;
	engine.set_inputs(positional.clone(), named);
	engine.parameters = inputs.parameters;
	engine.set_base_dir(base_dir);
	if let Some(path) = output.filter(|path| *path != "-") {
		// The script was read already, but writing over its file would still destroy it.
		let script = match source {
			Source::File(script) => Some(("script", script.as_str())),
			Source::Stdin | Source::Expression(_) => None,
		};
		let given = inputs.positional.iter().chain(inputs.named.values());
		let files = script
			.into_iter()
			.chain(given.map(|input| ("input", input.as_str())));
		engine.sink = create_output(path, files)?;
	}
	let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
		let bound = bind_declarations(&mut engine, &forms, &positional)?;
		engine.execute(&forms[bound..])
	}));
	match result {
		Ok(Ok(())) => {}
		Ok(Err(error)) => return Err(error),
		Err(_) => {
			return Err(Error::plain(
				"the Mata library aborted the operation".into(),
			));
		}
	}
	Ok(if engine.assertions_failed.on() {
		Exit::Assertion
	} else {
		Exit::Success
	})
}

/// A command-line path made absolute when it is relative and the script is in another directory.
fn anchored(path: &str, base_dir: Option<&std::path::Path>) -> Result<String, Error> {
	if base_dir.is_none() || path == "-" || std::path::Path::new(path).is_absolute() {
		return Ok(path.to_owned());
	}
	std::path::absolute(path)
		.map_err(|e| Error::plain(format!("cannot resolve '{path}': {e}")))?
		.into_os_string()
		.into_string()
		.map_err(|_| {
			Error::plain(format!(
				"cannot resolve '{path}': the current directory is not UTF-8"
			))
		})
}

pub(crate) fn check(source: &Source) -> Result<Exit, Error> {
	let (name, text, _) = source.read()?;
	let (forms, version) = parse(&text)?;
	let engine = Engine::for_language(version);
	let report = crate::check::check(&forms, &engine, None)?;
	println!("checked: {name}");
	eprintln!(
		"termination: {}",
		if report.termination {
			"guaranteed"
		} else {
			"not-guaranteed"
		}
	);
	for warning in &report.warnings {
		eprintln!("{warning}");
	}
	Ok(Exit::Success)
}

pub(crate) fn repl(output: Option<&str>) -> Result<Exit, Error> {
	let mut engine = Engine::new();
	if let Some(path) = output.filter(|path| *path != "-") {
		engine.sink = create_output(path, [])?;
	}
	let interactive = std::io::stdin().is_terminal();
	let mut failed = false;
	let mut buffer = String::new();
	let mut stdin = std::io::stdin().lock();
	loop {
		if interactive {
			print!("mata> ");
			flush();
		}
		let mut line = String::new();
		let read = stdin
			.read_line(&mut line)
			.map_err(|e| Error::plain(format!("cannot read standard input: {e}")))?;
		if read == 0 {
			if !buffer.trim().is_empty() {
				eprintln!("mata: the input ends inside a form");
				failed = true;
			}
			break;
		}
		buffer.push_str(&line);
		let forms = match ast::parse_complete(&buffer) {
			Ok(Some(forms)) => forms,
			Ok(None) => continue,
			Err(error) => {
				buffer.clear();
				eprintln!("mata: {}", error.render());
				failed = true;
				continue;
			}
		};
		buffer.clear();
		for form in forms {
			let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
				engine.execute_form(&form)
			}));
			match result {
				Ok(Ok(value)) => {
					if !matches!(value, Value::None) {
						let text = summary(&value, &engine);
						engine.emit(&text)?;
					}
				}
				Ok(Err(error)) => {
					eprintln!("mata: {}", error.render());
					failed = true;
				}
				Err(_) => {
					eprintln!("mata: the Mata library aborted the operation");
					failed = true;
				}
			}
			if engine.exit.on() {
				break;
			}
		}
		if engine.exit.on() {
			break;
		}
	}
	Ok(if failed {
		Exit::Usage
	} else if engine.assertions_failed.on() {
		Exit::Assertion
	} else {
		Exit::Success
	})
}

/// Opens the `--output` file at once, so every value is written as it is printed. The file must not
/// be one of `files`, the script and its inputs, each with what it is, which opening it would empty.
fn create_output<'a>(
	path: &str,
	files: impl IntoIterator<Item = (&'a str, &'a str)>,
) -> Result<Sink, Error> {
	if let Ok(output) = std::fs::canonicalize(path)
		&& let Some((what, file)) = files.into_iter().find(|(_, file)| {
			*file != "-" && std::fs::canonicalize(file).is_ok_and(|file| file == output)
		}) {
		return Err(Error::plain(format!(
			"the output '{path}' is the {what} '{file}'"
		)));
	}
	let file = std::fs::File::create(path)
		.map_err(|e| Error::plain(format!("cannot write '{path}': {e}")))?;
	Ok(Sink::File(std::io::BufWriter::new(file)))
}

/// The value of a REPL form: an automaton by its kind and size, so that a large one does not flood the
/// session; `print` shows it in full.
fn summary(value: &Value, engine: &Engine) -> String {
	let automaton = match value {
		Value::Nfa(nfa) => Some(("Nfa", nfa.num_states(), nfa.num_transitions())),
		Value::Nft(nft) => Some(("Nft", nft.num_states(), nft.num_edges())),
		Value::Automaton(automaton) => Some(match &**automaton {
			AutomatonKind::Nfa(nfa) => ("Nfa", nfa.num_states(), nfa.num_transitions()),
			AutomatonKind::Nft(nft) => ("Nft", nft.num_states(), nft.num_edges()),
		}),
		_ => None,
	};
	match automaton {
		Some((kind, states, transitions)) => {
			format!("#<{kind} with {states} states and {transitions} transitions>\n")
		}
		None => display_value(value, engine),
	}
}

fn flush() {
	use std::io::Write;
	let _ = std::io::stdout().flush();
}

/// Binds the declared inputs and parameters, which come first in a script, interleaved only with
/// `set-option` and `set-info`. Those forms run in order with the declarations, so an option such as
/// `:alphabet` applies to the automata the declarations load. A positional input file with several
/// automata gives one automaton to each declaration in turn, and every input given must be taken. The
/// inputs are read ahead at the first declaration, after the forms before it, so that bitvector
/// automata of different files share their minterms. Returns how many forms it ran.
fn bind_declarations(
	engine: &mut Engine,
	forms: &[Spanned<FormKind>],
	inputs: &[String],
) -> Result<usize, Error> {
	let named = declared_named_inputs(engine, forms)?;
	let declares = named.is_some();
	let mut unread = named;
	let mut files = inputs.iter();
	let mut pending: std::collections::VecDeque<Value> = std::collections::VecDeque::new();
	let mut bound = 0;
	for form in forms {
		match &form.node {
			FormKind::Declare {
				name,
				sort,
				variadic,
			} => {
				if let Some(named) = unread.take() {
					for path in inputs.iter().chain(&named) {
						engine.read_ahead(path)?;
					}
				}
				let at = |mut error: Error| {
					error.at.get_or_insert((form.line, form.column));
					error
				};
				let kind = kind_of(sort);
				let value = if *variadic {
					let mut values: Vec<Value> = pending.drain(..).collect();
					for path in files.by_ref() {
						values.extend(engine.load_inputs(path, kind).map_err(at)?);
					}
					Value::List(std::rc::Rc::new(values))
				} else if let Some(path) = engine.named_inputs.get(name).cloned() {
					engine.load_input(&path, kind).map_err(at)?
				} else {
					if pending.is_empty() {
						let path = files
							.next()
							.ok_or_else(|| form.at(format!("no input for '{name}'")))?;
						pending.extend(engine.load_inputs(path, kind).map_err(at)?);
					}
					pending
						.pop_front()
						.expect("a loaded input holds an automaton")
				};
				engine.scope.define(name.clone(), value);
			}
			FormKind::Header { .. }
			| FormKind::SetOption { .. }
			| FormKind::SetInfo { .. }
			| FormKind::Parameter { .. } => {
				engine.execute_form(form)?;
			}
			_ => break,
		}
		bound += 1;
	}
	let left = files.count();
	if declares && left > 0 {
		return Err(Error::plain(format!(
			"{left} positional inputs are left over; declare an input for each, or use declare-inputs"
		)));
	}
	if !pending.is_empty() {
		return Err(Error::plain(format!(
			"{} automata of the inputs are left over; declare an input for each, or use declare-inputs",
			pending.len()
		)));
	}
	Ok(bound)
}

/// Rejects a named input that no `declare-input` takes. Returns the paths of the named inputs in the
/// order of their declarations, or `None` when the script declares no input.
fn declared_named_inputs(
	engine: &Engine,
	forms: &[Spanned<FormKind>],
) -> Result<Option<Vec<String>>, Error> {
	let declared: Vec<(&str, bool)> = forms
		.iter()
		.take_while(|form| {
			matches!(
				form.node,
				FormKind::Declare { .. }
					| FormKind::Header { .. }
					| FormKind::SetOption { .. }
					| FormKind::SetInfo { .. }
					| FormKind::Parameter { .. }
			)
		})
		.filter_map(|form| match &form.node {
			FormKind::Declare { name, variadic, .. } => Some((name.as_str(), *variadic)),
			_ => None,
		})
		.collect();
	if declared.is_empty() {
		return Ok(None);
	}
	let mut named: Vec<&String> = engine.named_inputs.keys().collect();
	named.sort();
	if let Some(name) = named
		.iter()
		.find(|name| !declared.contains(&(name.as_str(), false)))
	{
		return Err(Error::plain(format!(
			"--input {name}: the script declares no input '{name}'"
		)));
	}
	Ok(Some(
		declared
			.iter()
			.filter_map(|(name, _)| engine.named_inputs.get(*name).cloned())
			.collect(),
	))
}
