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
	Intermediate(Rc<mata::IntermediateAutomaton>),
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
		/// The name of a function defined with `define`, which its body sees under
		/// `:loops-and-recursion`.
		name: Option<String>,
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

	/// Removes the innermost binding of `name`, for the optimizer at the last use of the value.
	fn take(&self, name: &str) -> Option<Value> {
		self.frames
			.borrow_mut()
			.iter_mut()
			.rev()
			.find_map(|frame| frame.remove(name))
	}

	fn snapshot(&self) -> Scope {
		let frames = self.frames.borrow().clone();
		Scope {
			frames: RefCell::new(frames),
		}
	}
}

/// An option, flag or status that is off or on: a named `Switch` rather than a `bool`, so that a
/// struct of many of them keeps a name for each.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub(crate) enum Switch {
	#[default]
	Off,
	On,
}

impl Switch {
	pub(crate) fn on(self) -> bool {
		matches!(self, Switch::On)
	}
}

impl From<bool> for Switch {
	fn from(on: bool) -> Switch {
		if on { Switch::On } else { Switch::Off }
	}
}

/// The script options that change how operations behave.
#[derive(Debug, Default, Clone)]
pub(crate) struct Options {
	pub(crate) bytes_alphabet: Switch,
	pub(crate) produce_witnesses: Switch,
	pub(crate) time_operations: Switch,
	pub(crate) optimize: Switch,
	pub(crate) trace: Switch,
}

/// The options and hint defaults when a `push` ran, which its `pop` restores.
struct Pushed {
	options: Options,
	defaults: HashMap<String, Vec<(String, String)>>,
}

/// Where the values a script prints go, written as they are printed.
pub(crate) enum Sink {
	/// Standard output, flushed after every value so a run that is killed keeps what it printed.
	Stdout,
	/// A file opened once, written after every value.
	File(std::io::BufWriter<fs::File>),
}

/// A command-line input read before it is loaded.
enum Ahead {
	/// Its text, parsed when it is loaded.
	Text(String),
	/// Its automata, parsed with the bitvector automata of the other inputs so that they share minterms.
	Bits(Vec<Rc<Nfa>>),
}

pub(crate) struct Engine {
	pub(crate) script_alphabet: mata::Alphabet,
	pub(crate) options: Options,
	pub(crate) last_witness: Option<Value>,
	/// Set while `witness` evaluates its application, which then searches a counterexample.
	pub(crate) witness_requested: Switch,
	pub(crate) assertions_failed: Switch,
	pub(crate) exit: Switch,
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
	/// What each open `push` saved for its `pop` to restore.
	pushed: Vec<Pushed>,
	/// Command-line inputs read before they are loaded, so that bitvector automata are parsed together.
	ahead: Vec<(String, Ahead)>,
	/// The hints of the operation being called.
	pub(crate) hints: Vec<(String, String)>,
	/// The optional parameters given to the operation being called, of their sorts.
	pub(crate) call_parameters: Vec<(String, Value)>,
	/// Labels of the records printed so far; a label is printed once.
	labels: std::collections::HashSet<String>,
	/// The form being evaluated, so that a failure at run time names its place.
	pub(crate) place: Option<(usize, usize)>,
	/// `:loops-and-recursion` is on, for the whole file: `pop` and `reset` keep it.
	loops_and_recursion: Switch,
	/// No form other than `set-option` and `set-info` has run yet.
	options_open: Switch,
	/// The number of names of each loop around the expression being evaluated, innermost last; a
	/// function call starts with none.
	loops: Vec<usize>,
	/// The values of a `recur` on their way to the loop it restarts.
	recur: Option<Vec<Value>>,
	/// How deeply defined functions and lambdas are calling each other.
	depth: usize,
	/// The optimizer's findings about the script being executed.
	plan: crate::optimize::Plan,
	/// The results of library applications, for the optimizer.
	memo: crate::optimize::Memo,
}

/// How deeply function calls may nest; deeper recursion is an error rather than an overflow of the
/// stack.
const MAX_DEPTH: usize = 1000;

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
			witness_requested: Switch::Off,
			assertions_failed: Switch::Off,
			exit: Switch::Off,
			sink: Sink::Stdout,
			base_dir: None,
			info: HashMap::new(),
			scope,
			inputs: Vec::new(),
			named_inputs: HashMap::new(),
			parameters: HashMap::new(),
			language,
			defaults: HashMap::new(),
			pushed: Vec::new(),
			ahead: Vec::new(),
			hints: Vec::new(),
			call_parameters: Vec::new(),
			labels: std::collections::HashSet::new(),
			place: None,
			loops_and_recursion: Switch::Off,
			options_open: Switch::On,
			loops: Vec::new(),
			recur: None,
			depth: 0,
			plan: crate::optimize::Plan::default(),
			memo: crate::optimize::Memo::default(),
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
			Sink::Stdout => crate::write_stdout(text),
			Sink::File(file) => file
				.write_all(text.as_bytes())
				.and_then(|()| file.flush())
				.map_err(|e| Error::plain(format!("cannot write the output file: {e}"))),
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
		self.assertions_failed = Switch::On;
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
		if !self.options.time_operations.on() {
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

	/// Evaluates forms until end or `(exit)`. The forms are the whole script, so the optimizer may
	/// plan for them.
	pub(crate) fn execute(&mut self, forms: &[Spanned<FormKind>]) -> Result<(), Error> {
		self.plan = crate::optimize::Plan::of(forms);
		for form in forms {
			self.execute_form(form)?;
			if self.exit.on() {
				return Ok(());
			}
		}
		Ok(())
	}

	/// Evaluates one top-level form; a failure without a place of its own gets the form's place.
	pub(crate) fn execute_form(&mut self, form: &Spanned<FormKind>) -> Result<Value, Error> {
		self.place = Some((form.line, form.column));
		if !matches!(
			form.node,
			FormKind::Header { .. } | FormKind::SetOption { .. } | FormKind::SetInfo { .. }
		) {
			self.options_open = Switch::Off;
		}
		self.execute_form_at(form).map_err(|mut error| {
			// A `recur` that failed on its way to its loop does not restart the next form.
			self.recur = None;
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
			} => self.define(name, parameters.as_deref(), sort.as_deref(), body, form),
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
				self.pushed.push(Pushed {
					options: self.options.clone(),
					defaults: self.defaults.clone(),
				});
				Ok(Value::None)
			}
			FormKind::Pop => self.pop(form),
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
				fresh.loops_and_recursion = self.loops_and_recursion;
				fresh.options_open = Switch::Off;
				fresh.plan = std::mem::take(&mut self.plan);
				*self = fresh;
				Ok(Value::None)
			}
			FormKind::Exit => {
				self.exit = Switch::On;
				Ok(Value::None)
			}
			FormKind::Expression(expression) => self.evaluate(expression),
		}
	}

	/// Binds a definition's value, which a function definition makes a closure over the current scope.
	fn define(
		&mut self,
		name: &str,
		parameters: Option<&[Parameter]>,
		sort: Option<&str>,
		body: &[Expr],
		form: &Spanned<FormKind>,
	) -> Result<Value, Error> {
		self.check_definable(name, form)?;
		if parameters.is_none() && self.dead_definition(form.line, form.column, sort, body) {
			return Ok(Value::None);
		}
		let value = match parameters {
			None => self.evaluate_sequence(body)?,
			Some(parameters) => Value::Function(Rc::new(Function::User {
				name: Some(name.to_owned()),
				parameters: parameters.to_vec(),
				body: body.to_vec(),
				environment: self.scope.snapshot(),
			})),
		};
		if let Some(sort) = sort {
			let actual = value_sort(&value);
			if matches!(&value, Value::Function(_)) && !fits(&value, sort) {
				return Err(form.at(format!("'{name}' is a function, not a {sort}")));
			}
			if !fits(&value, sort) {
				return Err(form.at(format!("expected {sort}, found {actual}")));
			}
		}
		self.scope.define(name.to_owned(), value.clone());
		Ok(Value::None)
	}

	/// Whether a definition can be left out: nothing reads its value, and its value is an `Nfa`
	/// computed with pure library operations, so that leaving it out cannot hide an error.
	fn dead_definition(
		&self,
		line: usize,
		column: usize,
		sort: Option<&str>,
		body: &[Expr],
	) -> bool {
		if !self.options.optimize.on()
			|| !self.plan.is_dead(line, column)
			|| !matches!(sort, None | Some("Nfa"))
		{
			return false;
		}
		let [expression] = body else {
			return false;
		};
		crate::optimize::computes_nfa(expression, &|name| {
			matches!(self.scope.lookup(name), Some(Value::Nfa(_)))
		})
	}

	/// Closes the innermost `push`: drops its definitions and restores the options and hint defaults,
	/// except the alphabet, which is fixed once set.
	fn pop(&mut self, form: &Spanned<FormKind>) -> Result<Value, Error> {
		if !self.scope.pop() {
			return Err(form.at("'pop' without a matching 'push'".into()));
		}
		if let Some(Pushed { options, defaults }) = self.pushed.pop() {
			self.options = Options {
				bytes_alphabet: self.options.bytes_alphabet,
				..options
			};
			self.defaults = defaults;
		}
		Ok(Value::None)
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
					"on-the-fly" => self.options.bytes_alphabet = Switch::Off,
					"bytes" => {
						if !self.script_alphabet.is_empty() {
							return Err(form.at("the script alphabet is already in use".into()));
						}
						self.options.bytes_alphabet = Switch::On;
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
				self.options.produce_witnesses =
					Switch::from(as_bool(&value).map_err(|e| form.at(e.clone()))?);
			}
			"time-operations" => {
				self.options.time_operations =
					Switch::from(as_bool(&value).map_err(|e| form.at(e.clone()))?);
			}
			"optimize" => {
				self.options.optimize =
					Switch::from(as_bool(&value).map_err(|e| form.at(e.clone()))?);
			}
			"trace" => {
				self.options.trace = Switch::from(as_bool(&value).map_err(|e| form.at(e.clone()))?);
			}
			"loops-and-recursion" => {
				if !self.options_open.on() {
					return Err(form.at(crate::check::LOOPS_OPTION_PLACE.into()));
				}
				self.loops_and_recursion =
					Switch::from(as_bool(&value).map_err(|e| form.at(e.clone()))?);
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
			let text = crate::ops::keyword_text(operation, name, &self.evaluate(value)?)
				.map_err(|message| value.at(message))?;
			crate::ops::check_keyword(operation, name, &text)
				.map_err(|message| value.at(message))?;
			chosen.retain(|(known, _)| known != name);
			chosen.push((name.clone(), text));
		}
		self.defaults.insert(operation.clone(), chosen);
		Ok(Value::None)
	}

	fn get_option(&self, keyword: &str) -> Result<Value, String> {
		Ok(match keyword {
			"alphabet" => Value::String(if self.options.bytes_alphabet.on() {
				"bytes".into()
			} else {
				"on-the-fly".into()
			}),
			"produce-witnesses" => Value::Bool(self.options.produce_witnesses.on()),
			"time-operations" => Value::Bool(self.options.time_operations.on()),
			"optimize" => Value::Bool(self.options.optimize.on()),
			"trace" => Value::Bool(self.options.trace.on()),
			"loops-and-recursion" => Value::Bool(self.loops_and_recursion.on()),
			_ => return Err(format!("unknown option ':{keyword}'")),
		})
	}

	pub(crate) fn evaluate(&mut self, expression: &Expr) -> Result<Value, Error> {
		if self.recur.is_some() {
			return Err(expression.at(crate::check::RECUR_NOT_LAST.into()));
		}
		match &expression.node {
			Kind::Bool(value) => Ok(Value::Bool(*value)),
			Kind::Int(value) => Ok(Value::Int(*value)),
			Kind::String(value) => Ok(Value::String(value.clone())),
			Kind::None => Ok(Value::None),
			Kind::Name(name) => {
				// Dropping the name at the last use of its value lets an operation change the value in
				// place. An operation stays bound, as the optimizer calls some by name.
				let last = self.options.optimize.on()
					&& self.plan.is_last_use(expression)
					&& crate::ops::operation(name).is_none();
				if last {
					self.scope.take(name)
				} else {
					self.scope.lookup(name)
				}
				.ok_or_else(|| expression.at(format!("unknown name '{name}'")))
			}
			Kind::Input(name) => {
				if name.chars().all(|c| c.is_ascii_digit()) {
					let index: usize = name
						.parse()
						.map_err(|_| expression.at("bad input reference".into()))?;
					index
						.checked_sub(1)
						.and_then(|index| self.inputs.get(index))
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
				name: None,
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
		let Some(Kind::Name(name)) = items.first().map(|e| &e.node) else {
			return Err(expression.at("expected a function or special form".into()));
		};
		self.evaluate_application(name, &items[1..], expression)
	}

	/// An application whose head is the name `name`: a special form, or a call of what the name binds.
	fn evaluate_application(
		&mut self,
		name: &str,
		parts: &[Expr],
		expression: &Expr,
	) -> Result<Value, Error> {
		match name {
			"let" => self.evaluate_let(parts, expression),
			"loop" => self.evaluate_loop(parts, expression),
			"recur" => self.evaluate_recur(parts, expression),
			"if" => self.evaluate_if(parts, expression),
			"cond" => self.evaluate_cond(parts, expression),
			"do" => self.evaluate_sequence(parts),
			"and" | "or" => self.evaluate_connective(parts, name),
			"->" => self.evaluate_pipeline(parts, expression),
			"as" => self.evaluate_as(parts, expression),
			"time" => self.evaluate_time(parts, expression),
			"witness" => self.evaluate_witness(parts, expression),
			_ => {
				if self.options.optimize.on()
					&& let Some(value) = self.evaluate_disjointness(name, parts, expression)?
				{
					return Ok(value);
				}
				let function = self
					.scope
					.lookup(name)
					.ok_or_else(|| expression.at(format!("unknown name '{name}'")))?;
				let arguments = parts
					.iter()
					.map(|item| self.evaluate(item))
					.collect::<Result<Vec<_>, _>>()?;
				self.call_function(function, arguments, &[], expression)
			}
		}
	}

	/// `(nfa:language-empty? (nfa:intersection lhs rhs))` runs as `(nfa:intersection-empty? lhs rhs)`,
	/// which the library decides without building the product.
	fn evaluate_disjointness(
		&mut self,
		name: &str,
		parts: &[Expr],
		expression: &Expr,
	) -> Result<Option<Value>, Error> {
		let Some((lhs, rhs)) = crate::optimize::disjointness(name, parts) else {
			return Ok(None);
		};
		let arguments = vec![self.evaluate(lhs)?, self.evaluate(rhs)?];
		let function = Value::Function(Rc::new(Function::Native {
			name: "nfa:intersection-empty?".to_owned(),
		}));
		self.call_function(function, arguments, &[], expression)
			.map(Some)
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
		Ok(narrow(value, &sort))
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
		if !applies_witness_operation(&parts[0]) {
			return Err(parts[0].at(witness_message()));
		}
		// The operation is the last call made, so the stored witness is its own.
		let requested = std::mem::replace(&mut self.witness_requested, Switch::On);
		let value = self.evaluate(&parts[0]);
		self.witness_requested = requested;
		value?;
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
			self.bind_names(bindings, "let")?;
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

	/// Binds the `(name value)` pairs of a `let` or a `loop` in order, so that a later value sees the
	/// names bound before it.
	fn bind_names(&mut self, bindings: &[Expr], head: &str) -> Result<Vec<(String, Value)>, Error> {
		let mut bound = Vec::with_capacity(bindings.len());
		for binding in bindings {
			let Kind::List(pair) = &binding.node else {
				return Err(binding.at(format!("a {head} binding must be (name value)")));
			};
			if pair.len() != 2 {
				return Err(binding.at(format!("a {head} binding must be (name value)")));
			}
			let Kind::Name(name) = &pair[0].node else {
				return Err(binding.at(format!("a {head} binding needs a name")));
			};
			let value = self.evaluate(&pair[1])?;
			self.scope.define(name.clone(), value.clone());
			bound.push((name.clone(), value));
		}
		Ok(bound)
	}

	/// `(loop ((name value) …) body …)`: the body runs in a fresh scope until it ends without a
	/// `recur`, whose values then bind the names again.
	fn evaluate_loop(&mut self, parts: &[Expr], expression: &Expr) -> Result<Value, Error> {
		if !self.loops_and_recursion.on() {
			return Err(expression.at(crate::check::needs_loops("'loop'")));
		}
		let Kind::List(bindings) = &parts[0].node else {
			return Err(expression.at("loop needs a binding list".into()));
		};
		if parts.len() < 2 {
			return Err(expression.at("loop needs a body".into()));
		}
		self.scope.push();
		let bound = self.bind_names(bindings, "loop");
		let _ = self.scope.pop();
		let bound = bound?;
		let names = bound
			.iter()
			.map(|(name, _)| name.clone())
			.collect::<Vec<_>>();
		let mut values = bound
			.into_iter()
			.map(|(_, value)| value)
			.collect::<Vec<_>>();
		let expressions = parts[1..]
			.iter()
			.filter(|e| !matches!(e.node, Kind::Definition { .. }))
			.cloned()
			.collect::<Vec<_>>();
		self.loops.push(names.len());
		let result = (|| {
			loop {
				// A fresh scope per round: what the body defines belongs to that round.
				self.scope.push();
				let round = (|| {
					for (name, value) in names.iter().zip(&values) {
						self.scope.define(name.clone(), value.clone());
					}
					self.evaluate_definitions(&parts[1..])?;
					self.evaluate_sequence(&expressions)
				})();
				let _ = self.scope.pop();
				let value = round?;
				match self.recur.take() {
					Some(next) => values = next,
					None => break Ok(value),
				}
			}
		})();
		self.loops.pop();
		result
	}

	/// `(recur value …)`: hands the values to the innermost `loop`, which runs its body again.
	fn evaluate_recur(&mut self, parts: &[Expr], expression: &Expr) -> Result<Value, Error> {
		if !self.loops_and_recursion.on() {
			return Err(expression.at(crate::check::needs_loops("'recur'")));
		}
		let Some(count) = self.loops.last().copied() else {
			return Err(expression.at(crate::check::RECUR_OUTSIDE.into()));
		};
		if parts.len() != count {
			return Err(expression.at(format!(
				"recur takes {count} values, one for each name of the loop; {} given",
				parts.len()
			)));
		}
		let values = parts
			.iter()
			.map(|part| self.evaluate(part))
			.collect::<Result<Vec<_>, _>>()?;
		self.recur = Some(values);
		Ok(Value::None)
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
		self.evaluate_application(name, arguments, expression)
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
			Function::Native { name } => self.call_native(name, arguments, keywords, expression),
			Function::User { .. } => self.call_user(&function, arguments, keywords, expression),
		}
	}

	/// Calls a library operation: the optimizer may answer from the memo instead, and `:trace` reports
	/// the call that is made.
	fn call_native(
		&mut self,
		name: &str,
		arguments: Vec<Value>,
		keywords: &[(String, Expr)],
		expression: &Expr,
	) -> Result<Value, Error> {
		let mut hints = Vec::with_capacity(keywords.len());
		for (key, value) in keywords {
			let text = crate::ops::keyword_text(name, key, &self.evaluate(value)?)
				.map_err(|message| value.at(message))?;
			hints.push((key.clone(), text));
		}
		let located = |error: Error| {
			if error.at.is_some() || expression.line == 0 {
				error
			} else {
				expression.at(error.message)
			}
		};
		// A decision stores its own witness, or none: `get-witness` never answers an older call.
		if is_timed(name) && name.ends_with('?') {
			self.last_witness = None;
		}
		// A decision whose witness is wanted makes a real call, so that the library stores a witness.
		let wants_witness = self.witness_requested.on() || self.options.produce_witnesses.on();
		let remembered = self.options.optimize.on() && !(wants_witness && name.ends_with('?'));
		let key = if remembered {
			crate::optimize::Memo::key(
				name,
				&arguments,
				hints.clone(),
				self.defaults_of(name),
				self.script_alphabet.len(),
			)
		} else {
			None
		};
		if let Some(key) = &key
			&& let Some(value) = self.memo.get(key)
		{
			return Ok(value);
		}
		if self.options.trace.on() && is_timed(name) {
			self.trace(name, &hints);
		}
		// The memo keeps the identity of the operands, which the call itself consumes.
		let operands = key.as_ref().map(|_| arguments.clone());
		let value = if is_timed(name) {
			let label = name.to_owned();
			self.timed(&label, |engine| {
				crate::ops::call(&label, arguments, &hints, engine)
			})
		} else {
			crate::ops::call(name, arguments, &hints, self)
		}
		.map_err(located)?;
		if let Some((key, operands)) = key.zip(operands) {
			self.memo.insert(key, &operands, &value);
		}
		Ok(value)
	}

	/// Reports one library call and its effective hints on standard error, as `:trace` asks.
	fn trace(&self, name: &str, hints: &[(String, String)]) {
		use std::fmt::Write as _;

		let mut line = format!("mata: trace: {name}");
		for (key, value) in self.defaults_of(name) {
			if !hints.iter().any(|(written, _)| *written == key) {
				let _ = write!(line, " :{key} {value:?}");
			}
		}
		for (key, value) in hints {
			let _ = write!(line, " :{key} {value:?}");
		}
		eprintln!("{line}");
	}

	/// Calls a function a script defined, or a lambda: its body runs in the scope it closed over.
	fn call_user(
		&mut self,
		function: &Rc<Function>,
		arguments: Vec<Value>,
		keywords: &[(String, Expr)],
		expression: &Expr,
	) -> Result<Value, Error> {
		let Function::User {
			name,
			parameters,
			body,
			environment,
		} = &**function
		else {
			unreachable!("a native function is not called as a user function")
		};
		if !keywords.is_empty() {
			return Err(expression.at("a defined function takes no keyword arguments".into()));
		}
		if parameters.len() != arguments.len() {
			return Err(expression.at(format!(
				"the function expects {} arguments, {} given",
				parameters.len(),
				arguments.len()
			)));
		}
		if self.depth >= MAX_DEPTH {
			return Err(expression.at(format!("function calls nest deeper than {MAX_DEPTH}")));
		}
		// A `recur` cannot reach a loop that the call is made in, only one in the body.
		let loops = std::mem::take(&mut self.loops);
		self.depth += 1;
		let saved = std::mem::replace(&mut self.scope, environment.snapshot());
		self.scope.push();
		let result = (|| {
			// With the option on, a function sees itself, so that it can call itself.
			if let Some(name) = name
				&& self.loops_and_recursion.on()
			{
				self.scope
					.define(name.clone(), Value::Function(Rc::clone(function)));
			}
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
		self.depth -= 1;
		self.loops = loops;
		result
	}

	/// Binds the local definitions of a body in order, so each one sees the previous ones and not itself.
	fn evaluate_definitions(&mut self, body: &[Expr]) -> Result<(), Error> {
		for item in body {
			if let Kind::Definition {
				name,
				parameters,
				sort,
				body,
			} = &item.node
			{
				if self.scope.defines(name) {
					return Err(item.at(format!("'{name}' is already defined")));
				}
				let value = match parameters {
					None => self.evaluate_sequence(body)?,
					Some(parameters) => Value::Function(Rc::new(Function::User {
						name: Some(name.clone()),
						parameters: parameters.clone(),
						body: body.clone(),
						environment: self.scope.snapshot(),
					})),
				};
				if let Some(sort) = sort {
					check_sort(&value, sort, item)?;
				}
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

	/// Loads one automaton from a path written in the script, relative to the script's directory, or
	/// a list of them from a list of paths.
	pub(crate) fn load(&mut self, paths: &Value, kind: &str) -> Result<Value, Error> {
		let Value::List(paths) = paths else {
			let path = as_string(paths).map_err(Error::plain)?;
			let text = self.read_script_path(&path)?;
			return self.timed("parsing", |engine| engine.parse_text(&text, kind));
		};
		let mut texts = Vec::with_capacity(paths.len());
		for path in paths.iter() {
			let path = as_string(path).map_err(Error::plain)?;
			let text = self.read_script_path(&path)?;
			let count = sections(&text);
			if count != 1 {
				return Err(Error::plain(format!(
					"'{path}' holds {count} automata; load takes one from each path"
				)));
			}
			if kind == "nft" && holds_bits(&text) {
				return Err(Error::plain(format!(
					"{path}: a transducer cannot have a bitvector alphabet"
				)));
			}
			texts.push(text);
		}
		// The bitvector automata are parsed in one call, which mintermizes them together.
		let bits: Vec<&String> = texts.iter().filter(|text| holds_bits(text)).collect();
		let mut shared = if bits.is_empty() {
			Vec::new()
		} else {
			let joined = bits.iter().fold(String::new(), |mut joined, text| {
				joined.push_str(text);
				joined.push('\n');
				joined
			});
			self.timed("parsing", |engine| {
				Nfa::parse_mata_all(&joined, &mut engine.script_alphabet)
					.map_err(|e| Error::plain(e.to_string()))
			})?
		}
		.into_iter();
		if shared.len() != bits.len() {
			return Err(Error::plain(
				"cannot tell which file each automaton comes from".into(),
			));
		}
		let mut automata = Vec::with_capacity(texts.len());
		for text in &texts {
			automata.push(if holds_bits(text) {
				let nfa = shared
					.next()
					.expect("one parsed automaton per bitvector file");
				of_kind(kind, Value::Nfa(Rc::new(nfa)))
			} else {
				self.timed("parsing", |engine| engine.parse_text(text, kind))?
			});
		}
		Ok(Value::List(Rc::new(automata)))
	}

	/// Reads a path written in the script, relative to the script's directory; `-` is standard
	/// input, as on the command line.
	fn read_script_path(&self, path: &str) -> Result<String, Error> {
		if path == "-" {
			return read_text(path);
		}
		read_text(&self.resolve_path(path).to_string_lossy())
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
		let position = self.ahead.iter().position(|(known, _)| known == path);
		if position.is_some_and(
			|index| matches!(&self.ahead[index].1, Ahead::Text(text) if holds_bits(text)),
		) {
			self.parse_bits_ahead()?;
		}
		let text = match position.map(|index| &self.ahead[index].1) {
			Some(Ahead::Bits(automata)) => {
				if kind == "nft" {
					return Err(Error::plain(format!(
						"{path}: a transducer cannot have a bitvector alphabet"
					)));
				}
				return Ok(automata
					.iter()
					.map(|nfa| of_kind(kind, Value::Nfa(Rc::clone(nfa))))
					.collect());
			}
			Some(Ahead::Text(text)) => text.clone(),
			None => read_text(path)?,
		};
		self.timed("parsing", |engine| engine.parse_all(&text, kind))
			.map_err(|error| Error::plain(format!("{path}: {}", error.message)))
	}

	/// Reads a command-line input before it is loaded, so that [`Engine::load_inputs`] parses the
	/// bitvector automata of all inputs read ahead together and they share their minterms. Returns how
	/// many automata the input holds.
	pub(crate) fn read_ahead(&mut self, path: &str) -> Result<usize, Error> {
		if let Some((_, ahead)) = self.ahead.iter().find(|(known, _)| known == path) {
			return Ok(match ahead {
				Ahead::Text(text) => sections(text),
				Ahead::Bits(automata) => automata.len(),
			});
		}
		let text = read_text(path)?;
		let count = sections(&text);
		self.ahead.push((path.to_owned(), Ahead::Text(text)));
		Ok(count)
	}

	/// Parses the bitvector inputs read ahead in one call, which mintermizes them together.
	fn parse_bits_ahead(&mut self) -> Result<(), Error> {
		let mut shared = Vec::new();
		let mut paths = Vec::new();
		let mut text = String::new();
		for (index, (path, ahead)) in self.ahead.iter().enumerate() {
			if let Ahead::Text(input) = ahead
				&& holds_bits(input)
			{
				shared.push((index, sections(input)));
				paths.push(path.as_str());
				text.push_str(input);
				text.push('\n');
			}
		}
		let paths = paths.join(", ");
		let parsed = self
			.timed("parsing", |engine| {
				Nfa::parse_mata_all(&text, &mut engine.script_alphabet)
					.map_err(|e| Error::plain(e.to_string()))
			})
			.map_err(|error| Error::plain(format!("{paths}: {}", error.message)))?;
		if parsed.len() != shared.iter().map(|&(_, count)| count).sum::<usize>() {
			return Err(Error::plain(format!(
				"{paths}: cannot tell which file each automaton comes from"
			)));
		}
		let mut automata = parsed.into_iter().map(Rc::new);
		for (index, count) in shared {
			self.ahead[index].1 = Ahead::Bits(automata.by_ref().take(count).collect());
		}
		Ok(())
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

	/// Every automaton section of `text`; bitvector sections are mintermized together. For an
	/// `automaton` each other section is parsed as the kind it is, so NFAs and NFTs may mix.
	fn parse_all(&mut self, text: &str, kind: &str) -> Result<Vec<Value>, Error> {
		let transducer = match kind {
			"nfa" => false,
			"nft" => true,
			_ if holds_bits(text) => false,
			_ => {
				let values = split_sections(text)
					.into_iter()
					.map(|section| self.parse_text(section, kind))
					.collect::<Result<Vec<_>, _>>()?;
				if values.is_empty() {
					return Err(Error::plain("no automaton in the input".into()));
				}
				return Ok(values);
			}
		};
		let wrap = |value: Value| of_kind(kind, value);
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

/// Whether a `.mata` text holds a bitvector NFA, whose symbols are the minterms of its parse.
fn holds_bits(text: &str) -> bool {
	text.lines()
		.any(|line| line.trim_start().starts_with("@NFA-bits"))
}

/// The number of automaton sections of a `.mata` text: each starts with an `@TYPE` line.
fn sections(text: &str) -> usize {
	text.lines()
		.filter(|line| line.trim_start().starts_with('@'))
		.count()
}

/// The automaton sections of a `.mata` text, each from its `@TYPE` line; the lines before the first
/// one, such as comments, go with it.
fn split_sections(text: &str) -> Vec<&str> {
	let mut starts: Vec<usize> = Vec::new();
	let mut offset = 0;
	for line in text.split_inclusive('\n') {
		if line.trim_start().starts_with('@') {
			starts.push(offset);
		}
		offset += line.len();
	}
	if let Some(first) = starts.first_mut() {
		*first = 0;
	}
	starts
		.iter()
		.enumerate()
		.map(|(index, &start)| &text[start..starts.get(index + 1).copied().unwrap_or(text.len())])
		.collect()
}

/// An input loaded as an `automaton` narrowed to the kind a declaration takes; the other kind is an
/// error.
pub(crate) fn narrow_input(kind: &str, value: Value) -> Result<Value, String> {
	let Value::Automaton(automaton) = &value else {
		return Ok(value);
	};
	match (kind, &**automaton) {
		("nfa", AutomatonKind::Nfa(nfa)) => Ok(Value::Nfa(Rc::clone(nfa))),
		("nft", AutomatonKind::Nft(nft)) => Ok(Value::Nft(Rc::clone(nft))),
		("nfa", AutomatonKind::Nft(_)) => Err("expected an Nfa, but the input holds an Nft".into()),
		("nft", AutomatonKind::Nfa(_)) => Err("expected an Nft, but the input holds an Nfa".into()),
		_ => Ok(value),
	}
}

/// A parsed automaton as the value of `kind`: an `Automaton` wraps either kind.
fn of_kind(kind: &str, value: Value) -> Value {
	match (kind, value) {
		("automaton", Value::Nfa(nfa)) => Value::Automaton(Rc::new(AutomatonKind::Nfa(nfa))),
		("automaton", Value::Nft(nft)) => Value::Automaton(Rc::new(AutomatonKind::Nft(nft))),
		(_, value) => value,
	}
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

/// The decision operations that store a witness, the ones `witness` applies to.
pub(crate) const WITNESS_OPERATIONS: &[&str] = &[
	"nfa:language-empty?",
	"nfa:intersection-empty?",
	"nfa:included?",
	"nfa:equivalent?",
	"nfa:universal?",
	"automaton:language-empty?",
	"automaton:included?",
	"automaton:equivalent?",
];

/// Whether `expression` is an application of a witness operation.
pub(crate) fn applies_witness_operation(expression: &Expr) -> bool {
	matches!(&expression.node, Kind::Call { function, .. }
		if matches!(&function.node, Kind::Name(name) if WITNESS_OPERATIONS.contains(&name.as_str())))
}

/// The error for a `witness` whose operand is not an application of a witness operation.
pub(crate) fn witness_message() -> String {
	format!(
		"witness applies to an application of {}",
		WITNESS_OPERATIONS.join(", ")
	)
}

/// Whether a call of `name` is a library operation that `:time-operations` measures.
fn is_timed(name: &str) -> bool {
	name.starts_with("nfa:")
		|| name.starts_with("nft:")
		|| name.starts_with("alphabet:")
		|| name.starts_with("strings:")
		|| name.starts_with("automaton:")
		|| name.starts_with("intermediate:")
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
pub(crate) fn parameter_value(text: &str, sort: &str) -> Result<Value, String> {
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
	sort_expression(expression).ok_or_else(|| expression.at("expected a sort".into()))
}

/// The sort written as an expression, as in `(as value (List Nfa))`, spelled as a declaration spells
/// it: `(List (Option Int))`.
pub(crate) fn sort_expression(expression: &Expr) -> Option<String> {
	match &expression.node {
		Kind::Name(name) => Some(name.clone()),
		Kind::Call {
			function,
			arguments,
			keywords,
		} if keywords.is_empty() && !arguments.is_empty() => {
			let Kind::Name(name) = &function.node else {
				return None;
			};
			let arguments = arguments
				.iter()
				.map(sort_expression)
				.collect::<Option<Vec<_>>>()?;
			Some(format!("({name} {})", arguments.join(" ")))
		}
		_ => None,
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
	if fits(value, sort) {
		Ok(())
	} else {
		Err(expression.at(format!("expected {sort}, found {}", value_sort(value))))
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
		Value::Intermediate(_) => "IntermediateAutomaton".into(),
		Value::List(list) => {
			let mut sorts = list.iter().map(value_sort);
			let element = match sorts.next() {
				None => "unknown".into(),
				Some(first) if sorts.all(|sort| sort == first) => first,
				Some(_) => "Any".into(),
			};
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

/// Whether `value` is of sort `sort`: `none` is of every `(Option T)`, a list is a `(List T)` when every
/// element is a `T`, and an `Automaton` is also of the kind it holds.
fn fits(value: &Value, sort: &str) -> bool {
	if let Some(element) = compound(sort, "List") {
		return matches!(value, Value::List(items) if items.iter().all(|item| fits(item, element)));
	}
	if let Some(inner) = compound(sort, "Option") {
		return matches!(value, Value::None) || fits(value, inner);
	}
	match (sort, value) {
		("Any", _) | ("Automaton", Value::Nfa(_) | Value::Nft(_)) => true,
		("Nfa", Value::Automaton(automaton)) => matches!(**automaton, AutomatonKind::Nfa(_)),
		("Nft", Value::Automaton(automaton)) => matches!(**automaton, AutomatonKind::Nft(_)),
		(_, Value::List(_)) => false,
		_ => value_sort(value) == sort,
	}
}

/// The argument of `sort` when it is `(name argument)`.
fn compound<'s>(sort: &'s str, name: &str) -> Option<&'s str> {
	sort.strip_prefix('(')?
		.strip_suffix(')')?
		.strip_prefix(name)?
		.strip_prefix(' ')
}

/// `value` narrowed to `sort`: an `Automaton` narrowed to `Nfa` or `Nft` is the automaton it holds.
fn narrow(value: Value, sort: &str) -> Value {
	match (sort, &value) {
		("Nfa" | "Nft", Value::Automaton(automaton)) => match &**automaton {
			AutomatonKind::Nfa(nfa) => Value::Nfa(Rc::clone(nfa)),
			AutomatonKind::Nft(nft) => Value::Nft(Rc::clone(nft)),
		},
		_ => value,
	}
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
