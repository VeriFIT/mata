// Copyright (c) 2026 VeriFIT. GNU GPL v3. See LICENSE in the project root.

use std::collections::HashMap;
use std::io::IsTerminal;
use std::io::Read;

use crate::ast::{self, FormKind};
use crate::cli::Exit;
use crate::eval::{Engine, Value};
use crate::{Error, display_value};

pub(crate) fn run_scripts_run(
	script: Option<&str>,
	script_path: Option<&str>,
	expression: Option<&str>,
	inputs: &[String],
	named: HashMap<String, String>,
	output: Option<&str>,
) -> Result<Exit, Error> {
	let (source, base_dir) = if let Some(text) = expression {
		(text.to_owned(), None)
	} else if script == Some("-") {
		let mut text = String::new();
		std::io::stdin()
			.read_to_string(&mut text)
			.map_err(|e| Error::plain(format!("cannot read standard input: {e}")))?;
		(text, None)
	} else if let Some(path) = script.or(script_path) {
		let path = std::path::Path::new(path);
		let text = std::fs::read_to_string(path)
			.map_err(|e| Error::plain(format!("cannot read '{}': {e}", path.display())))?;
		let base = path
			.parent()
			.map(std::path::Path::to_path_buf)
			.filter(|p| !p.as_os_str().is_empty());
		(text, base)
	} else {
		return Err(Error::plain("no script given".into()));
	};

	let forms = ast::parse(&source)?;
	let mut engine = Engine::new();
	engine.set_inputs(inputs.to_vec(), named);
	engine.set_base_dir(base_dir);
	let declarations = collect_declarations(&forms);
	load_declarations(&mut engine, &declarations, inputs)?;
	let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| engine.execute(&forms)));
	let outcome = match result {
		Ok(Ok(())) => Ok(()),
		Ok(Err(error)) => Err(error),
		Err(_) => Err(Error::plain(
			"the Mata library aborted the operation".into(),
		)),
	};
	// Values printed before a failure still belong on the output.
	write_output(output, &engine.output)?;
	outcome?;
	Ok(if engine.assertions_failed {
		Exit::Assertion
	} else {
		Exit::Success
	})
}

pub(crate) fn run_scripts_check(script: &str, source: &str) -> Result<Exit, Error> {
	let forms = ast::parse(source)?;
	let engine = Engine::new();
	let report = crate::check::check(&forms, &engine)?;
	println!("checked: {script}");
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

pub(crate) fn run_scripts_repl(output: Option<&str>) -> Result<Exit, Error> {
	let mut engine = Engine::new();
	let interactive = std::io::stdin().is_terminal();
	let mut buffer = String::new();
	let mut pending = String::new();
	let mut stdin = std::io::stdin().lock();
	let mut byte = [0u8; 1];
	loop {
		if interactive {
			print!("mata> ");
			flush();
		}
		let mut line = String::new();
		loop {
			match stdin.read(&mut byte) {
				Ok(0) => break,
				Ok(_) if byte[0] == b'\n' => break,
				Ok(_) => line.push(byte[0] as char),
				Err(e) => return Err(Error::plain(format!("cannot read standard input: {e}"))),
			}
		}
		if line.is_empty() && buffer.is_empty() && stdin_is_closed(&mut stdin) {
			break;
		}
		buffer.push_str(&line);
		buffer.push('\n');
		match ast::parse_complete(&buffer) {
			Ok(Some(forms)) => {
				buffer.clear();
				for form in forms {
					let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
						engine.execute_form(&form)
					}));
					emit(output, &mut pending, &std::mem::take(&mut engine.output));
					match result {
						Ok(Ok(value)) => {
							if !matches!(value, Value::None) {
								emit(output, &mut pending, &display_value(&value, &engine));
							}
						}
						Ok(Err(error)) => eprintln!("mata: {}", error.render()),
						Err(_) => eprintln!("mata: the Mata library aborted the operation"),
					}
				}
				flush();
			}
			Ok(None) => {}
			Err(error) => {
				buffer.clear();
				eprintln!("mata: {}", error.render());
			}
		}
	}
	write_output(output, &pending)?;
	Ok(if engine.assertions_failed {
		Exit::Assertion
	} else {
		Exit::Success
	})
}

/// Sends the values of a REPL chunk to standard output, or accumulates them for `--output`.
fn emit(output: Option<&str>, pending: &mut String, text: &str) {
	if text.is_empty() {
		return;
	}
	if output.is_some() {
		pending.push_str(text);
	} else {
		print!("{text}");
	}
}

fn flush() {
	use std::io::Write;
	let _ = std::io::stdout().flush();
}

fn stdin_is_closed(stdin: &mut std::io::StdinLock<'_>) -> bool {
	let mut probe = [0u8; 1];
	matches!(stdin.read(&mut probe), Ok(0))
}

fn collect_declarations(forms: &[ast::Spanned<FormKind>]) -> Vec<Declaration> {
	let mut declarations = Vec::new();
	for form in forms {
		match &form.node {
			FormKind::Declare {
				name,
				sort,
				variadic,
			} => declarations.push(Declaration {
				name: name.clone(),
				sort: sort.clone(),
				variadic: *variadic,
			}),
			FormKind::SetOption { .. } | FormKind::SetInfo { .. } => {}
			_ => break,
		}
	}
	declarations
}

struct Declaration {
	name: String,
	sort: String,
	variadic: bool,
}

fn load_declarations(
	engine: &mut Engine,
	declarations: &[Declaration],
	inputs: &[String],
) -> Result<(), Error> {
	if declarations.is_empty() {
		return Ok(());
	}
	let mut position = 0;
	for declaration in declarations {
		let path = if declaration.variadic {
			let values = inputs[position..]
				.iter()
				.map(|path| engine.load_file(path, kind_of(&declaration.sort)))
				.collect::<Result<Vec<_>, _>>()?;
			engine.scope.define(
				declaration.name.clone(),
				Value::List(std::rc::Rc::new(values)),
			);
			position = inputs.len();
			continue;
		} else {
			engine
				.named_inputs
				.get(&declaration.name)
				.cloned()
				.or_else(|| inputs.get(position).cloned())
		};
		let Some(path) = path else {
			continue;
		};
		position += 1;
		let value = engine.load_file(&path, kind_of(&declaration.sort))?;
		engine.scope.define(declaration.name.clone(), value);
	}
	Ok(())
}

fn kind_of(sort: &str) -> &'static str {
	if sort.contains("Nft") {
		"nft"
	} else if sort.contains("Automaton") {
		"automaton"
	} else {
		"nfa"
	}
}

fn write_output(output: Option<&str>, text: &str) -> Result<(), Error> {
	if let Some(path) = output {
		return std::fs::write(path, text)
			.map_err(|e| Error::plain(format!("cannot write '{path}': {e}")));
	}
	print!("{text}");
	flush();
	Ok(())
}
