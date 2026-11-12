// Copyright (c) 2026 VeriFIT. GNU GPL v3. See LICENSE in the project root.

use std::cell::RefCell;
use std::collections::HashMap;
use std::fs;
use std::path::{Path, PathBuf};
use std::rc::Rc;
use std::time::Instant;

use mata::{Nfa, Symbol, Word, nft::Nft};

use crate::Error;
use crate::ast::{Expr, FormKind, Kind, Parameter, Spanned};

#[derive(Clone)]
pub(crate) enum Value {
	Bool(bool),
	Int(i64),
	String(String),
	Nfa(Rc<Nfa>),
	Nft(Rc<Nft>),
	Automaton(Rc<AutomatonKind>),
	List(Rc<Vec<Value>>),
	Alphabet(Rc<mata::Alphabet>),
	Word(Word),
	Symbol(Symbol),
	Run(Rc<mata::Run>),
	Function(Rc<Function>),
	None,
}

pub(crate) enum AutomatonKind {
	Nfa(Rc<Nfa>),
	Nft(Rc<Nft>),
}

pub(crate) enum Function {
	Native {
		name: String,
	},
	User {
		parameters: Vec<Parameter>,
		body: Vec<Expr>,
		environment: Scope,
	},
}

pub(crate) struct Scope {
	frames: RefCell<Vec<HashMap<String, Value>>>,
}

impl Scope {
	fn new() -> Scope {
		Scope {
			frames: RefCell::new(vec![HashMap::new()]),
		}
	}

	pub(crate) fn define(&self, name: String, value: Value) {
		self.frames
			.borrow_mut()
			.last_mut()
			.unwrap()
			.insert(name, value);
	}

	fn push(&self) {
		self.frames.borrow_mut().push(HashMap::new());
	}

	/// Closes the innermost `push` scope; `false` when only the top-level scope is left.
	fn pop(&self) -> bool {
		let mut frames = self.frames.borrow_mut();
		if frames.len() > 1 {
			frames.pop();
			true
		} else {
			false
		}
	}

	/// Whether the innermost scope already binds `name`; an inner scope may hide an outer name.
	pub(crate) fn defines(&self, name: &str) -> bool {
		self.frames
			.borrow()
			.last()
			.is_some_and(|frame| frame.contains_key(name))
	}

	pub(crate) fn lookup(&self, name: &str) -> Option<Value> {
		let frames = self.frames.borrow();
		frames
			.iter()
			.rev()
			.find_map(|frame| frame.get(name).cloned())
	}

	fn snapshot(&self) -> Scope {
		let frames = self.frames.borrow().clone();
		Scope {
			frames: RefCell::new(frames),
		}
	}
}

/// The script options that change how operations behave.
#[derive(Debug, Default)]
pub(crate) struct Options {
	pub(crate) bytes_alphabet: bool,
	pub(crate) produce_witnesses: bool,
	pub(crate) time_operations: bool,
}

/// Where the values a script prints go: written as they are printed, or kept for the caller.
pub(crate) enum Sink {
	/// Standard output, flushed after every value so a run that is killed keeps what it printed.
	Stdout,
	/// A file opened once, written after every value.
	File(std::io::BufWriter<fs::File>),
	/// Kept in memory, as the REPL does with `--output`.
	Buffer(String),
}

pub(crate) struct Engine {
	pub(crate) script_alphabet: mata::Alphabet,
	pub(crate) options: Options,
	pub(crate) last_witness: Option<Value>,
	pub(crate) assertions_failed: bool,
	pub(crate) exit: bool,
	pub(crate) sink: Sink,
	pub(crate) base_dir: Option<PathBuf>,
	pub(crate) info: HashMap<String, Value>,
	pub(crate) scope: Scope,
	pub(crate) inputs: Vec<String>,
	pub(crate) named_inputs: HashMap<String, String>,
	/// `--param NAME=VALUE` texts, read by `declare-param`.
	pub(crate) parameters: HashMap<String, String>,
	/// The language version the script declares, which selects the operations it sees.
	pub(crate) language: crate::version::Version,
	/// The hint defaults set with `(set-option :default …)`, per operation.
	pub(crate) defaults: HashMap<String, Vec<(String, String)>>,
	/// The hints of the operation being called.
	pub(crate) hints: Vec<(String, String)>,
	/// Labels of the records printed so far; a label is printed once.
	labels: std::collections::HashSet<String>,
	/// The form being evaluated, so that a failure at run time names its place.
	pub(crate) place: Option<(usize, usize)>,
}

/// A synthetic expression for calling a function value outside of syntax, e.g. from `list:map`.
pub(crate) fn dummy_expr() -> Expr {
	Expr {
		node: Kind::None,
		line: 0,
		column: 0,
	}
}

impl Engine {
	pub(crate) fn new() -> Engine {
		Engine::for_language(crate::version::LANGUAGE)
	}

	/// An engine for a script that declares `language`: it sees the operations of that version only.
	pub(crate) fn for_language(language: crate::version::Version) -> Engine {
		let scope = Scope::new();
		for operation in crate::ops::OPERATIONS {
			if operation.since.visible_in(language) {
				scope.define(
					operation.name.to_owned(),
					Value::Function(Rc::new(Function::Native {
						name: operation.name.to_owned(),
					})),
				);
			}
		}
		// Predefined symbols (script-language.md sorts table).
		scope.define("epsilon".to_string(), Value::Symbol(mata::Symbol::EPSILON));
		scope.define("dont-care".to_string(), Value::Symbol(mata::nft::DONT_CARE));
		Engine {
			script_alphabet: mata::Alphabet::new(),
			options: Options::default(),
			last_witness: None,
			assertions_failed: false,
			exit: false,
			sink: Sink::Stdout,
			base_dir: None,
			info: HashMap::new(),
			scope,
			inputs: Vec::new(),
			named_inputs: HashMap::new(),
			parameters: HashMap::new(),
			language,
			defaults: HashMap::new(),
			hints: Vec::new(),
			labels: std::collections::HashSet::new(),
			place: None,
		}
	}

	/// Writes printed values to the sink at once.
	///
	/// # Errors
	///
	/// The output file cannot be written.
	pub(crate) fn emit(&mut self, text: &str) -> Result<(), Error> {
		use std::io::Write as _;
		match &mut self.sink {
			Sink::Stdout => {
				let mut stdout = std::io::stdout().lock();
				stdout
					.write_all(text.as_bytes())
					.and_then(|()| stdout.flush())
					.map_err(|e| Error::plain(format!("cannot write standard output: {e}")))
			}
			Sink::File(file) => file
				.write_all(text.as_bytes())
				.and_then(|()| file.flush())
				.map_err(|e| Error::plain(format!("cannot write the output file: {e}"))),
			Sink::Buffer(buffer) => {
				buffer.push_str(text);
				Ok(())
			}
		}
	}

	/// A `label: value` record line; a label already printed is an error, because a reader of the
	/// records keys them by label.
	///
	/// # Errors
	///
	/// The label was printed before, or it holds a line break.
	pub(crate) fn record(&mut self, label: &str, value: &str) -> Result<String, Error> {
		if label.contains('\n') {
			return Err(Error::plain(format!(
				"the record label {label:?} spans lines"
			)));
		}
		if !self.labels.insert(label.to_owned()) {
			return Err(Error::plain(format!(
				"the record label '{label}' is already printed; give each record its own label"
			)));
		}
		Ok(format!("{label}: {value}\n"))
	}

	/// Records a failed assertion and reports its place on standard error.
	pub(crate) fn fail_assertion(&mut self) {
		self.assertions_failed = true;
		match self.place {
			Some((line, column)) => eprintln!("mata: assertion failed at {line}:{column}"),
			None => eprintln!("mata: assertion failed"),
		}
	}

	/// The hint defaults of `operation` set by `(set-option :default …)`.
	pub(crate) fn defaults_of(&self, operation: &str) -> Vec<(String, String)> {
		self.defaults.get(operation).cloned().unwrap_or_default()
	}

	/// Times `work` as a record labelled `label` when `:time-operations` is on.
	///
	/// # Errors
	///
	/// The error of `work`, or of printing the record.
	pub(crate) fn timed<T>(
		&mut self,
		label: &str,
		work: impl FnOnce(&mut Engine) -> Result<T, Error>,
	) -> Result<T, Error> {
		if !self.options.time_operations {
			return work(self);
		}
		let started = Instant::now();
		let value = work(self)?;
		let seconds = format!("{:.6}", started.elapsed().as_secs_f64());
		let line = self.record_repeatable(label, &seconds)?;
		self.emit(&line)?;
		Ok(value)
	}

	/// A record whose label may repeat: every repetition after the first gets a `#2`, `#3`, … suffix.
	fn record_repeatable(&mut self, label: &str, value: &str) -> Result<String, Error> {
		let mut unique = label.to_owned();
		let mut count = 1;
		while self.labels.contains(&unique) {
			count += 1;
			unique = format!("{label}#{count}");
		}
		self.record(&unique, value)
	}

	pub(crate) fn set_inputs(&mut self, positional: Vec<String>, named: HashMap<String, String>) {
		self.scope.define(
			"inputs".to_owned(),
			Value::List(Rc::new(
				positional.iter().cloned().map(Value::String).collect(),
			)),
		);
		self.inputs = positional;
		self.named_inputs = named;
	}

	pub(crate) fn set_base_dir<P: Into<PathBuf>>(&mut self, path: Option<P>) {
		self.base_dir = path.map(Into::into);
	}

	/// A path written in the script: relative to the script's directory.
	fn resolve_path(&self, path: &str) -> PathBuf {
		let path = Path::new(path);
		if path.is_absolute() {
			path.to_path_buf()
		} else if let Some(base) = &self.base_dir {
			base.join(path)
		} else {
			path.to_path_buf()
		}
	}

	/// Evaluates forms until end or `(exit)`.
	pub(crate) fn execute(&mut self, forms: &[Spanned<FormKind>]) -> Result<(), Error> {
		for form in forms {
			self.execute_form(form)?;
			if self.exit {
				return Ok(());
			}
		}
		Ok(())
	}

	/// Evaluates one top-level form; a failure without a place of its own gets the form's place.
	pub(crate) fn execute_form(&mut self, form: &Spanned<FormKind>) -> Result<Value, Error> {
		self.place = Some((form.line, form.column));
		self.execute_form_at(form).map_err(|mut error| {
			if error.at.is_none() {
				error.at = Some((form.line, form.column));
			}
			error
		})
	}

	fn execute_form_at(&mut self, form: &Spanned<FormKind>) -> Result<Value, Error> {
		match &form.node {
			FormKind::Define {
				name,
				parameters,
				body,
				sort,
			} => {
				self.check_definable(name, form)?;
				let value = match parameters {
					None => self.evaluate_sequence(body)?,
					Some(parameters) => Value::Function(Rc::new(Function::User {
						parameters: parameters.clone(),
						body: body.clone(),
						environment: self.scope.snapshot(),
					})),
				};
				if let Some(sort) = sort {
					let actual = value_sort(&value);
					if matches!(&value, Value::Function(_)) {
						return Err(form.at(format!("'{name}' is a function, not a {sort}")));
					}
					if !sort_matches(sort, &actual) {
						return Err(form.at(format!("expected {sort}, found {actual}")));
					}
				}
				self.scope.define(name.clone(), value.clone());
				Ok(Value::None)
			}
			FormKind::Declare {
				name,
				sort,
				variadic,
			} => {
				// A script binds its declared inputs before it runs; a declaration typed into a REPL
				// binds the input given under its name.
				if self.scope.lookup(name).is_some() {
					return Ok(Value::None);
				}
				if *variadic {
					return Err(form.at("declare-inputs needs inputs from the command line".into()));
				}
				let path = self
					.named_inputs
					.get(name)
					.cloned()
					.ok_or_else(|| form.at(format!("no input for '{name}'")))?;
				let value = self.load_input(&path, kind_of(sort))?;
				self.scope.define(name.clone(), value);
				Ok(Value::None)
			}
			FormKind::Parameter {
				name,
				sort,
				default,
			} => self.bind_parameter(name, sort, default.as_ref(), form),
			FormKind::Header { requires, profile } => {
				check_header(requires, profile.as_deref()).map_err(|message| form.at(message))?;
				Ok(Value::None)
			}
			FormKind::SetOption { keyword, value } => self.apply_option(keyword, value, form),
			FormKind::SetInfo { keyword, value } => {
				let value = self.evaluate(value)?;
				check_info(keyword, &value).map_err(|message| form.at(message))?;
				self.info.insert(keyword.clone(), value);
				Ok(Value::None)
			}
			FormKind::Push => {
				self.scope.push();
				Ok(Value::None)
			}
			FormKind::Pop => {
				if !self.scope.pop() {
					return Err(form.at("'pop' without a matching 'push'".into()));
				}
				Ok(Value::None)
			}
			FormKind::Reset => {
				let mut fresh = Engine::for_language(self.language);
				fresh.set_inputs(
					std::mem::take(&mut self.inputs),
					std::mem::take(&mut self.named_inputs),
				);
				fresh.parameters = std::mem::take(&mut self.parameters);
				fresh.set_base_dir(self.base_dir.clone());
				fresh.sink = std::mem::replace(&mut self.sink, Sink::Stdout);
				fresh.assertions_failed = self.assertions_failed;
				fresh.labels = std::mem::take(&mut self.labels);
				*self = fresh;
				Ok(Value::None)
			}
			FormKind::Exit => {
				self.exit = true;
				Ok(Value::None)
			}
			FormKind::Expression(expression) => self.evaluate(expression),
		}
	}

	/// A name an operation of the catalogue owns cannot be defined, whether or not the script's
	/// language version sees it, so that a later version adding an operation breaks no script.
	fn check_definable(&self, name: &str, form: &Spanned<FormKind>) -> Result<(), Error> {
		if let Some(message) = reserved(name) {
			return Err(form.at(message));
		}
		if self.scope.defines(name) {
			return Err(form.at(format!("'{name}' is already defined")));
		}
		Ok(())
	}

	/// Binds a `declare-param` to its `--param` value, or to its default.
	fn bind_parameter(
		&mut self,
		name: &str,
		sort: &str,
		default: Option<&Expr>,
		form: &Spanned<FormKind>,
	) -> Result<Value, Error> {
		if self.scope.lookup(name).is_some() {
			return Ok(Value::None);
		}
		let value = match (self.parameters.get(name).cloned(), default) {
			(Some(text), _) => parameter_value(&text, sort)
				.map_err(|message| form.at(format!("--param {name}: {message}")))?,
			(None, Some(default)) => {
				let value = self.evaluate(default)?;
				check_sort(&value, sort, default)?;
				value
			}
			(None, None) => {
				return Err(form.at(format!(
					"no value for parameter '{name}'; give it with --param {name}=VALUE"
				)));
			}
		};
		self.scope.define(name.to_owned(), value);
		Ok(Value::None)
	}

	fn apply_option(
		&mut self,
		keyword: &str,
		expression: &Expr,
		form: &Spanned<FormKind>,
	) -> Result<Value, Error> {
		let keyword = keyword.trim_start_matches(':');
		if keyword == "default" {
			return self.set_default(expression);
		}
		let value = self.evaluate(expression)?;
		match keyword {
			"alphabet" => {
				let name = as_string(&value).map_err(|e| form.at(e.clone()))?;
				match name.as_str() {
					"on-the-fly" => self.options.bytes_alphabet = false,
					"bytes" => {
						if !self.script_alphabet.is_empty() {
							return Err(form.at("the script alphabet is already in use".into()));
						}
						self.options.bytes_alphabet = true;
						for byte in u8::MIN..=u8::MAX {
							let name = byte_name(byte);
							self.script_alphabet
								.insert_named(&name, Symbol::new(u32::from(byte)))
								.map_err(|e| form.at(e.to_string()))?;
						}
					}
					_ => return Err(form.at(format!("unknown alphabet '{name}'"))),
				}
			}
			"produce-witnesses" => {
				self.options.produce_witnesses = as_bool(&value).map_err(|e| form.at(e.clone()))?;
			}
			"time-operations" => {
				self.options.time_operations = as_bool(&value).map_err(|e| form.at(e.clone()))?;
			}
			"optimize" | "trace" => {
				let enabled = as_bool(&value).map_err(|e| form.at(e.clone()))?;
				if !enabled {
					return Ok(Value::None);
				}
				return Err(form.at(format!("option :{keyword} is not implemented")));
			}
			"loops-and-recursion" => {
				return Err(form.at("option :loops-and-recursion is not implemented".into()));
			}
			_ => return Err(form.at(format!("unknown option ':{keyword}'"))),
		}
		Ok(Value::None)
	}

	/// `(set-option :default (operation :hint value …))`: the hints of later calls of the operation.
	fn set_default(&mut self, expression: &Expr) -> Result<Value, Error> {
		let Kind::Call {
			function,
			arguments,
			keywords,
		} = &expression.node
		else {
			return Err(expression.at(
				":default needs an operation with only hints, as (nfa:included? :algorithm \"naive\")"
					.into(),
			));
		};
		let Kind::Name(operation) = &function.node else {
			return Err(function.at(":default needs an operation name".into()));
		};
		if !arguments.is_empty() || keywords.is_empty() {
			return Err(expression.at(":default needs an operation with only hints".into()));
		}
		if crate::ops::visible(operation, self.language).is_none() {
			return Err(function.at(format!("unknown operation '{operation}'")));
		}
		let mut chosen = self.defaults_of(operation);
		for (name, value) in keywords {
			let text = as_string(&self.evaluate(value)?)
				.map_err(|_| value.at(format!("the value of hint :{name} must be a String")))?;
			crate::ops::check_hint(operation, name, &text).map_err(|message| value.at(message))?;
			chosen.retain(|(known, _)| known != name);
			chosen.push((name.clone(), text));
		}
		self.defaults.insert(operation.clone(), chosen);
		Ok(Value::None)
	}

	fn get_option(&self, keyword: &str) -> Result<Value, String> {
		Ok(match keyword {
			"alphabet" => Value::String(if self.options.bytes_alphabet {
				"bytes".into()
			} else {
				"on-the-fly".into()
			}),
			"produce-witnesses" => Value::Bool(self.options.produce_witnesses),
			"time-operations" => Value::Bool(self.options.time_operations),
			"optimize" | "trace" | "loops-and-recursion" => Value::Bool(false),
			_ => return Err(format!("unknown option ':{keyword}'")),
		})
	}

	pub(crate) fn evaluate(&mut self, expression: &Expr) -> Result<Value, Error> {
		match &expression.node {
			Kind::Bool(value) => Ok(Value::Bool(*value)),
			Kind::Int(value) => Ok(Value::Int(*value)),
			Kind::String(value) => Ok(Value::String(value.clone())),
			Kind::None => Ok(Value::None),
			Kind::Name(name) => self
				.scope
				.lookup(name)
				.ok_or_else(|| expression.at(format!("unknown name '{name}'"))),
			Kind::Input(name) => {
				if name.chars().all(|c| c.is_ascii_digit()) {
					let index: usize = name
						.parse()
						.map_err(|_| expression.at("bad input reference".into()))?;
					self.inputs
						.get(index - 1)
						.cloned()
						.map(Value::String)
						.ok_or_else(|| expression.at(format!("no positional input {index}")))
				} else {
					self.named_inputs
						.get(name)
						.cloned()
						.map(Value::String)
						.ok_or_else(|| expression.at(format!("no input named '{name}'")))
				}
			}
			Kind::List(items) => self.evaluate_list(items, expression),
			Kind::Lambda { parameters, body } => Ok(Value::Function(Rc::new(Function::User {
				parameters: parameters.clone(),
				body: body.clone(),
				environment: self.scope.snapshot(),
			}))),
			Kind::Definition { .. } => Err(expression.at("define is not allowed here".into())),
			Kind::Call { .. } => self.evaluate_call(expression),
			Kind::Literal { .. } => self.evaluate_literal(expression),
			Kind::GetOption(keyword) => self
				.get_option(keyword)
				.map_err(|message| expression.at(message)),
			Kind::GetInfo(keyword) => Ok(self.info.get(keyword).cloned().unwrap_or(Value::None)),
		}
	}

	fn evaluate_list(&mut self, items: &[Expr], expression: &Expr) -> Result<Value, Error> {
		let Some(Kind::Name(head)) = items.first().map(|e| &e.node) else {
			return Err(expression.at("expected a function or special form".into()));
		};
		match head.as_str() {
			"let" => self.evaluate_let(&items[1..], expression),
			"if" => self.evaluate_if(&items[1..], expression),
			"cond" => self.evaluate_cond(&items[1..], expression),
			"do" => self.evaluate_sequence(&items[1..]),
			"and" | "or" => self.evaluate_connective(&items[1..], head),
			"->" => self.evaluate_pipeline(&items[1..], expression),
			"as" => self.evaluate_as(&items[1..], expression),
			"time" => self.evaluate_time(&items[1..], expression),
			"witness" => self.evaluate_witness(&items[1..], expression),
			_ => {
				let function = self.evaluate(&items[0])?;
				let arguments = items[1..]
					.iter()
					.map(|item| self.evaluate(item))
					.collect::<Result<Vec<_>, _>>()?;
				self.call_function(function, arguments, &[], expression)
			}
		}
	}

	fn evaluate_if(&mut self, parts: &[Expr], expression: &Expr) -> Result<Value, Error> {
		if parts.len() != 3 {
			return Err(expression.at("if needs a condition and two branches".into()));
		}
		let condition = self.evaluate(&parts[0])?;
		if as_bool(&condition).map_err(|_| parts[0].at("if needs a Bool".into()))? {
			self.evaluate(&parts[1])
		} else {
			self.evaluate(&parts[2])
		}
	}

	fn evaluate_cond(&mut self, clauses: &[Expr], expression: &Expr) -> Result<Value, Error> {
		for clause in clauses {
			let Kind::List(pair) = &clause.node else {
				return Err(clause.at("a cond clause must be a list".into()));
			};
			if pair.len() != 2 {
				return Err(clause.at("a cond clause needs a condition and a value".into()));
			}
			if matches!(&pair[0].node, Kind::Name(name) if name == "else") {
				return self.evaluate(&pair[1]);
			}
			let condition = self.evaluate(&pair[0])?;
			if as_bool(&condition).map_err(|_| pair[0].at("cond needs Bool conditions".into()))? {
				return self.evaluate(&pair[1]);
			}
		}
		Err(expression.at("no cond clause matched and no else".into()))
	}

	/// `and` and `or`, which differ only in the value they start from and the one that ends the run.
	fn evaluate_connective(&mut self, operands: &[Expr], name: &str) -> Result<Value, Error> {
		let (start, stop) = if name == "and" {
			(true, false)
		} else {
			(false, true)
		};
		let mut result = start;
		for operand in operands {
			result = as_bool(&self.evaluate(operand)?)
				.map_err(|_| operand.at(format!("{name} needs Bool operands")))?;
			if result == stop {
				break;
			}
		}
		Ok(Value::Bool(result))
	}

	fn evaluate_as(&mut self, parts: &[Expr], expression: &Expr) -> Result<Value, Error> {
		if parts.len() != 2 {
			return Err(expression.at("as needs a value and a sort".into()));
		}
		let value = self.evaluate(&parts[0])?;
		let sort = parse_sort(&parts[1])?;
		check_sort(&value, &sort, &parts[0])?;
		Ok(value)
	}

	fn evaluate_time(&mut self, parts: &[Expr], expression: &Expr) -> Result<Value, Error> {
		if parts.len() < 2 {
			return Err(expression.at("time needs a label and an expression".into()));
		}
		let Value::String(label) = self.evaluate(&parts[0])? else {
			return Err(parts[0].at("the time label must be a String".into()));
		};
		let started = Instant::now();
		let value = self.evaluate_sequence(&parts[1..])?;
		let seconds = format!("{:.6}", started.elapsed().as_secs_f64());
		let line = self
			.record(&label, &seconds)
			.map_err(|error| parts[0].at(error.message))?;
		self.emit(&line)?;
		Ok(value)
	}

	fn evaluate_witness(&mut self, parts: &[Expr], expression: &Expr) -> Result<Value, Error> {
		if parts.len() != 1 {
			return Err(expression.at("witness takes one application".into()));
		}
		let value = self.evaluate(&parts[0])?;
		if !matches!(value, Value::Bool(_)) {
			return Err(parts[0].at("witness applies to a decision operation".into()));
		}
		Ok(self.last_witness.clone().unwrap_or(Value::None))
	}

	fn evaluate_let(&mut self, parts: &[Expr], expression: &Expr) -> Result<Value, Error> {
		let Kind::List(bindings) = &parts[0].node else {
			return Err(expression.at("let needs a binding list".into()));
		};
		if parts.len() < 2 {
			return Err(expression.at("let needs a body".into()));
		}
		self.scope.push();
		let result = (|| {
			for binding in bindings {
				let Kind::List(pair) = &binding.node else {
					return Err(binding.at("a let binding must be (name value)".into()));
				};
				if pair.len() != 2 {
					return Err(binding.at("a let binding must be (name value)".into()));
				}
				let Kind::Name(name) = &pair[0].node else {
					return Err(binding.at("a let binding needs a name".into()));
				};
				let value = self.evaluate(&pair[1])?;
				self.scope.define(name.clone(), value);
			}
			self.evaluate_definitions(&parts[1..])?;
			let expressions = parts[1..]
				.iter()
				.filter(|e| !matches!(e.node, Kind::Definition { .. }))
				.cloned()
				.collect::<Vec<_>>();
			self.evaluate_sequence(&expressions)
		})();
		let _ = self.scope.pop();
		result
	}

	fn evaluate_pipeline(&mut self, steps: &[Expr], expression: &Expr) -> Result<Value, Error> {
		if steps.is_empty() {
			return Err(expression.at("a pipeline needs a value".into()));
		}
		let mut value = self.evaluate(&steps[0])?;
		for step in &steps[1..] {
			value = match &step.node {
				Kind::Name(name) => {
					let function = self
						.scope
						.lookup(name)
						.ok_or_else(|| step.at(format!("unknown name '{name}'")))?;
					self.call_function(function, vec![value.clone()], &[], step)?
				}
				Kind::Call {
					function,
					arguments,
					keywords,
				} => {
					let head = self.evaluate(function)?;
					let mut arguments = arguments
						.iter()
						.map(|a| self.evaluate(a))
						.collect::<Result<Vec<_>, _>>()?;
					arguments.insert(0, value.clone());
					self.call_function(head, arguments, keywords, step)?
				}
				_ => return Err(step.at("a pipeline step must be a name or an application".into())),
			};
		}
		Ok(value)
	}

	fn evaluate_call(&mut self, expression: &Expr) -> Result<Value, Error> {
		let Kind::Call {
			function,
			arguments,
			keywords,
		} = &expression.node
		else {
			unreachable!()
		};
		if let Kind::Name(name) = &function.node {
			return self.evaluate_list_name(name, arguments, keywords, expression);
		}
		let function = self.evaluate(function)?;
		let arguments = arguments
			.iter()
			.map(|a| self.evaluate(a))
			.collect::<Result<Vec<_>, _>>()?;
		self.call_function(function, arguments, keywords, expression)
	}

	fn evaluate_list_name(
		&mut self,
		name: &str,
		arguments: &[Expr],
		keywords: &[(String, Expr)],
		expression: &Expr,
	) -> Result<Value, Error> {
		let items = std::iter::once(Kind::Name(name.to_owned()))
			.chain(arguments.iter().map(|a| a.node.clone()))
			.collect::<Vec<_>>();
		if !keywords.is_empty() {
			let function = self
				.scope
				.lookup(name)
				.ok_or_else(|| expression.at(format!("unknown name '{name}'")))?;
			let arguments = arguments
				.iter()
				.map(|a| self.evaluate(a))
				.collect::<Result<Vec<_>, _>>()?;
			return self.call_function(function, arguments, keywords, expression);
		}
		let wrapped: Vec<Expr> = items
			.into_iter()
			.zip(std::iter::repeat(expression.line))
			.enumerate()
			.map(|(i, (node, line))| Expr {
				node,
				line,
				column: expression.column + i,
			})
			.collect();
		self.evaluate_list(&wrapped, expression)
	}

	pub(crate) fn call_function(
		&mut self,
		function: Value,
		arguments: Vec<Value>,
		keywords: &[(String, Expr)],
		expression: &Expr,
	) -> Result<Value, Error> {
		let Value::Function(function) = function else {
			return Err(
				expression.at("the first element of an application must be a function".into())
			);
		};
		match &*function {
			Function::Native { name } => {
				let mut hints = Vec::with_capacity(keywords.len());
				for (key, value) in keywords {
					let text = as_string(&self.evaluate(value)?).map_err(|_| {
						value.at(format!("the value of hint :{key} must be a String"))
					})?;
					hints.push((key.clone(), text));
				}
				let located = |error: Error| {
					if error.at.is_some() || expression.line == 0 {
						error
					} else {
						expression.at(error.message)
					}
				};
				if is_timed(name) {
					let label = name.clone();
					self.timed(&label, |engine| {
						crate::ops::call(&label, arguments, &hints, engine)
					})
					.map_err(located)
				} else {
					crate::ops::call(name, arguments, &hints, self).map_err(located)
				}
			}
			Function::User {
				parameters,
				body,
				environment,
			} => {
				if !keywords.is_empty() {
					return Err(
						expression.at("a defined function takes no keyword arguments".into())
					);
				}
				if parameters.len() != arguments.len() {
					return Err(expression.at(format!(
						"the function expects {} arguments, {} given",
						parameters.len(),
						arguments.len()
					)));
				}
				let saved = std::mem::replace(&mut self.scope, environment.snapshot());
				self.scope.push();
				let result = (|| {
					for (parameter, value) in parameters.iter().zip(arguments) {
						if let Some(sort) = &parameter.sort {
							check_sort(&value, sort, expression)?;
						}
						self.scope.define(parameter.name.clone(), value);
					}
					self.evaluate_definitions(body)?;
					self.evaluate_sequence(
						&body
							.iter()
							.filter(|e| !matches!(e.node, Kind::Definition { .. }))
							.cloned()
							.collect::<Vec<_>>(),
					)
				})();
				let _ = self.scope.pop();
				self.scope = saved;
				result
			}
		}
	}

	/// Binds the local definitions of a body in order, so each one sees the previous ones and not itself.
	fn evaluate_definitions(&mut self, body: &[Expr]) -> Result<(), Error> {
		for item in body {
			if let Kind::Definition {
				name,
				parameters,
				body,
			} = &item.node
			{
				if self.scope.defines(name) {
					return Err(item.at(format!("'{name}' is already defined")));
				}
				let value = match parameters {
					None => self.evaluate_sequence(body)?,
					Some(parameters) => Value::Function(Rc::new(Function::User {
						parameters: parameters.clone(),
						body: body.clone(),
						environment: self.scope.snapshot(),
					})),
				};
				self.scope.define(name.clone(), value);
			}
		}
		Ok(())
	}

	pub(crate) fn evaluate_sequence(&mut self, body: &[Expr]) -> Result<Value, Error> {
		let mut result = Value::None;
		for item in body {
			result = self.evaluate(item)?;
		}
		Ok(result)
	}

	fn evaluate_literal(&mut self, expression: &Expr) -> Result<Value, Error> {
		let Kind::Literal { name, arguments } = &expression.node else {
			unreachable!()
		};
		crate::literal::build(name, arguments, self).map_err(|message| expression.at(message))
	}

	pub(crate) fn intern_name(&mut self, name: &str) -> Result<Symbol, String> {
		self.script_alphabet
			.insert_name(name)
			.map_err(|e| e.to_string())
	}

	/// Loads one automaton from a path written in the script, relative to the script's directory.
	pub(crate) fn load_file(&mut self, path: &str, kind: &str) -> Result<Value, Error> {
		let resolved = self.resolve_path(path);
		let text = read_text(&resolved.to_string_lossy())?;
		self.timed("parsing", |engine| engine.parse_text(&text, kind))
	}

	/// Loads one automaton from a path given on the command line, relative to the current directory,
	/// or from standard input for `-`.
	pub(crate) fn load_input(&mut self, path: &str, kind: &str) -> Result<Value, Error> {
		let mut automata = self.load_inputs(path, kind)?;
		if automata.len() == 1 {
			Ok(automata.remove(0))
		} else {
			Err(Error::plain(format!(
				"'{path}' holds {} automata; a declare-input takes one",
				automata.len()
			)))
		}
	}

	/// Loads every automaton section of a command-line input; standard input for `-`.
	pub(crate) fn load_inputs(&mut self, path: &str, kind: &str) -> Result<Vec<Value>, Error> {
		let text = read_text(path)?;
		self.timed("parsing", |engine| engine.parse_all(&text, kind))
			.map_err(|error| Error::plain(format!("{path}: {}", error.message)))
	}

	pub(crate) fn parse_text(&mut self, text: &str, kind: &str) -> Result<Value, Error> {
		match kind {
			"nfa" => {
				let nfa = Nfa::parse_mata(text, &mut self.script_alphabet)
					.map_err(|e| Error::plain(e.to_string()))?;
				Ok(Value::Nfa(Rc::new(nfa)))
			}
			"nft" => {
				let nft = Nft::parse_mata(text, &mut self.script_alphabet)
					.map_err(|e| Error::plain(e.to_string()))?;
				Ok(Value::Nft(Rc::new(nft)))
			}
			"automaton" => {
				if holds_transducer(text) {
					let nft = Nft::parse_mata(text, &mut self.script_alphabet)
						.map_err(|e| Error::plain(e.to_string()))?;
					Ok(Value::Automaton(Rc::new(AutomatonKind::Nft(Rc::new(nft)))))
				} else {
					let nfa = Nfa::parse_mata(text, &mut self.script_alphabet)
						.map_err(|e| Error::plain(e.to_string()))?;
					Ok(Value::Automaton(Rc::new(AutomatonKind::Nfa(Rc::new(nfa)))))
				}
			}
			_ => Err(Error::plain(format!("unknown automaton kind '{kind}'"))),
		}
	}

	/// Every automaton section of `text`; bitvector sections are mintermized together.
	fn parse_all(&mut self, text: &str, kind: &str) -> Result<Vec<Value>, Error> {
		let transducer = match kind {
			"nfa" => false,
			"nft" => true,
			_ => holds_transducer(text),
		};
		let wrap = |value: Value| match (kind, value) {
			("automaton", Value::Nfa(nfa)) => Value::Automaton(Rc::new(AutomatonKind::Nfa(nfa))),
			("automaton", Value::Nft(nft)) => Value::Automaton(Rc::new(AutomatonKind::Nft(nft))),
			(_, value) => value,
		};
		let values: Vec<Value> = if transducer {
			Nft::parse_mata_all(text, &mut self.script_alphabet)
				.map_err(|e| Error::plain(e.to_string()))?
				.into_iter()
				.map(|nft| wrap(Value::Nft(Rc::new(nft))))
				.collect()
		} else {
			Nfa::parse_mata_all(text, &mut self.script_alphabet)
				.map_err(|e| Error::plain(e.to_string()))?
				.into_iter()
				.map(|nfa| wrap(Value::Nfa(Rc::new(nfa))))
				.collect()
		};
		if values.is_empty() {
			return Err(Error::plain("no automaton in the input".into()));
		}
		Ok(values)
	}

	pub(crate) fn path_for_write(&self, path: &str) -> PathBuf {
		self.resolve_path(path)
	}
}

/// Whether a `.mata` text holds a transducer: an `@NFT` section, or levels.
pub(crate) fn holds_transducer(text: &str) -> bool {
	text.trim_start().starts_with("@NFT") || text.contains("%LevelsNum")
}

/// The text of a file, or of standard input for `-`.
///
/// # Errors
///
/// The file or standard input cannot be read.
pub(crate) fn read_text(path: &str) -> Result<String, Error> {
	if path == "-" {
		let mut text = String::new();
		std::io::Read::read_to_string(&mut std::io::stdin(), &mut text)
			.map_err(|e| Error::plain(format!("cannot read standard input: {e}")))?;
		return Ok(text);
	}
	fs::read_to_string(path).map_err(|e| Error::plain(format!("cannot read '{path}': {e}")))
}

/// The kind a declared sort loads as.
pub(crate) fn kind_of(sort: &str) -> &'static str {
	if sort.contains("Nft") {
		"nft"
	} else if sort.contains("Automaton") {
		"automaton"
	} else {
		"nfa"
	}
}

/// Whether a call of `name` is a library operation that `:time-operations` measures.
fn is_timed(name: &str) -> bool {
	name.starts_with("nfa:") || name.starts_with("nft:") || name.starts_with("alphabet:")
}

/// Why `name` cannot be defined by a script, or `None` when it can. Every namespaced name is reserved,
/// as are the names of the core functions and of the special forms.
pub(crate) fn reserved(name: &str) -> Option<String> {
	if name.contains(':') {
		return Some(format!(
			"'{name}' is reserved: a namespaced name belongs to an operation"
		));
	}
	if crate::ops::operation(name).is_some() || crate::check::RESERVED_NAMES.contains(&name) {
		return Some(format!("'{name}' is reserved for the language"));
	}
	None
}

/// The value of `--param NAME=TEXT` for a parameter of sort `sort`.
fn parameter_value(text: &str, sort: &str) -> Result<Value, String> {
	match sort {
		"String" => Ok(Value::String(text.to_owned())),
		"Int" => text
			.parse::<i64>()
			.map(Value::Int)
			.map_err(|_| format!("'{text}' is not an Int")),
		"Bool" => match text {
			"true" => Ok(Value::Bool(true)),
			"false" => Ok(Value::Bool(false)),
			_ => Err(format!("'{text}' is not a Bool; use true or false")),
		},
		_ => Err(format!(
			"a parameter must be String, Int, or Bool; '{sort}' is not"
		)),
	}
}

/// The keys `set-info` knows, with the sort of their value. Other keys are an error unless they start
/// with `x-`, which is free for any use.
pub(crate) const INFO_KEYS: &[(&str, &str)] = &[
	("name", "String"),
	("description", "String"),
	("source", "String"),
	("license", "String"),
	("authors", "String"),
	("expects", "Bool"),
];

/// Checks a `set-info` key and the sort of its value.
pub(crate) fn check_info(keyword: &str, value: &Value) -> Result<(), String> {
	if keyword.starts_with("x-") {
		return Ok(());
	}
	let Some((_, sort)) = INFO_KEYS.iter().find(|(key, _)| *key == keyword) else {
		return Err(info_key_message(keyword));
	};
	let actual = value_sort(value);
	if actual == *sort {
		Ok(())
	} else {
		Err(format!("info :{keyword} must be a {sort}, found {actual}"))
	}
}

/// The error for an unknown `set-info` key.
pub(crate) fn info_key_message(keyword: &str) -> String {
	format!(
		"unknown info key ':{keyword}'; the known keys are {}, and keys starting with x- are free",
		INFO_KEYS
			.iter()
			.map(|(key, _)| format!(":{key}"))
			.collect::<Vec<_>>()
			.join(", ")
	)
}

fn parse_sort(expression: &Expr) -> Result<String, Error> {
	match &expression.node {
		Kind::Name(name) => Ok(name.clone()),
		Kind::List(_) => Ok(render_list_sort(expression)),
		_ => Err(expression.at("expected a sort".into())),
	}
}

fn render_list_sort(expression: &Expr) -> String {
	let mut out = String::from("(");
	render(expression, &mut out);
	out.push(')');
	out
}

fn render(expression: &Expr, out: &mut String) {
	match &expression.node {
		Kind::Name(name) => {
			if !out.is_empty() && !out.ends_with('(') {
				out.push(' ');
			}
			out.push_str(name);
		}
		Kind::List(items) => {
			out.push('(');
			for item in items {
				render(item, out);
			}
			out.push(')');
		}
		_ => {}
	}
}

pub(crate) fn as_bool(value: &Value) -> Result<bool, String> {
	match value {
		Value::Bool(value) => Ok(*value),
		_ => Err("expected a Bool".into()),
	}
}

pub(crate) fn as_string(value: &Value) -> Result<String, String> {
	match value {
		Value::String(value) => Ok(value.clone()),
		_ => Err("expected a String".into()),
	}
}

pub(crate) fn as_int(value: &Value) -> Result<i64, String> {
	match value {
		Value::Int(value) => Ok(*value),
		_ => Err("expected an Int".into()),
	}
}

pub(crate) fn as_list(value: &Value) -> Result<Vec<Value>, String> {
	match value {
		Value::List(list) => Ok(list.as_ref().clone()),
		_ => Err("expected a list".into()),
	}
}

fn check_sort(value: &Value, sort: &str, expression: &Expr) -> Result<(), Error> {
	let actual = value_sort(value);
	if sort_matches(sort, &actual) {
		Ok(())
	} else {
		Err(expression.at(format!("expected {sort}, found {actual}")))
	}
}

pub(crate) fn value_sort(value: &Value) -> String {
	match value {
		Value::Bool(_) => "Bool".into(),
		Value::Int(_) => "Int".into(),
		Value::String(_) => "String".into(),
		Value::Nfa(_) => "Nfa".into(),
		Value::Nft(_) => "Nft".into(),
		Value::Automaton(_) => "Automaton".into(),
		Value::List(list) => {
			let element = list.first().map_or_else(|| "unknown".into(), value_sort);
			format!("(List {element})")
		}
		Value::Word(_) => "Word".into(),
		Value::Alphabet(_) => "Alphabet".into(),
		Value::Symbol(_) => "Symbol".into(),
		Value::Run(_) => "Run".into(),
		Value::Function(_) => "Function".into(),
		Value::None => "none".into(),
	}
}

fn sort_matches(expected: &str, actual: &str) -> bool {
	expected == actual
		|| (expected == "Automaton" && (actual == "Nfa" || actual == "Nft"))
		|| (expected == "(List Automaton)" && actual.starts_with("(List "))
}

fn byte_name(byte: u8) -> String {
	if (0x21..=0x7e).contains(&byte) && byte != b' ' && byte != b'\\' {
		(byte as char).to_string()
	} else {
		format!("\\x{byte:02X}")
	}
}

/// Rejects a header that requires a feature this build lacks or names an unknown profile. The
/// checker reports these first; repeated here for `script run --no-check`.
fn check_header(requires: &[String], profile: Option<&str>) -> Result<(), String> {
	if let Some(message) = requires
		.iter()
		.find_map(|feature| crate::version::missing_feature(feature))
	{
		return Err(message);
	}
	match profile {
		None | Some("core") => Ok(()),
		Some(other) => Err(format!("unknown profile '{other}'")),
	}
}
