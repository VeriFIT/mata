// Copyright (c) 2026 VeriFIT. GNU GPL v3. See LICENSE in the project root.

use std::cell::RefCell;
use std::collections::HashMap;
use std::fmt::Write as _;
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

pub(crate) struct Engine {
	pub(crate) script_alphabet: mata::Alphabet,
	pub(crate) options: Options,
	pub(crate) last_witness: Option<Value>,
	pub(crate) assertions_failed: bool,
	pub(crate) exit: bool,
	pub(crate) output: String,
	pub(crate) base_dir: Option<PathBuf>,
	pub(crate) info: HashMap<String, Value>,
	pub(crate) scope: Scope,
	pub(crate) inputs: Vec<String>,
	pub(crate) named_inputs: HashMap<String, String>,
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
		let scope = Scope::new();
		for operation in crate::ops::OPERATIONS {
			scope.define(
				operation.name.to_owned(),
				Value::Function(Rc::new(Function::Native {
					name: operation.name.to_owned(),
				})),
			);
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
			output: String::new(),
			base_dir: None,
			info: HashMap::new(),
			scope,
			inputs: Vec::new(),
			named_inputs: HashMap::new(),
		}
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

	/// One input by name for a declaration reached outside the pre-bound script path.
	fn load_input_for(&mut self, name: &str) -> Option<Value> {
		let path = self.named_inputs.get(name)?.clone();
		self.load_file(&path, "automaton").ok()
	}
	pub(crate) fn set_base_dir<P: Into<PathBuf>>(&mut self, path: Option<P>) {
		self.base_dir = path.map(Into::into);
	}

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

	pub(crate) fn execute_form(&mut self, form: &Spanned<FormKind>) -> Result<Value, Error> {
		match &form.node {
			FormKind::Define {
				name,
				parameters,
				body,
				sort,
			} => {
				if self.scope.defines(name) {
					return Err(form.at(format!("'{name}' is already defined")));
				}
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
			FormKind::Declare { name, .. } => {
				// scripts.rs binds declared inputs before execution; a declaration reached at
				// run time (REPL, nested) rebinds the same name from the stored inputs.
				if self.scope.lookup(name).is_some() {
					return Ok(Value::None);
				}
				let value = self
					.load_input_for(name)
					.ok_or_else(|| Error::plain(format!("no input for '{name}'")))?;
				self.scope.define(name.clone(), value);
				Ok(Value::None)
			}
			FormKind::SetOption { keyword, value } => {
				let value = self.evaluate(value)?;
				self.apply_option(keyword, &value, form)
			}
			FormKind::GetOption { keyword } => self.get_option(keyword),
			FormKind::SetInfo { keyword, value } => {
				// Information that does not change evaluation; kept for get-info.
				let value = self.evaluate(value)?;
				self.info.insert(keyword.clone(), value);
				Ok(Value::None)
			}
			FormKind::GetInfo { keyword } => {
				Ok(self.info.get(keyword).cloned().unwrap_or(Value::None))
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
				let mut fresh = Engine::new();
				fresh.set_inputs(
					std::mem::take(&mut self.inputs),
					std::mem::take(&mut self.named_inputs),
				);
				fresh.set_base_dir(self.base_dir.clone());
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

	fn apply_option(
		&mut self,
		keyword: &str,
		value: &Value,
		form: &Spanned<FormKind>,
	) -> Result<Value, Error> {
		let keyword = keyword.trim_start_matches(':');
		match keyword {
			"alphabet" => {
				let name = as_string(value).map_err(|e| form.at(e.clone()))?;
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
				self.options.produce_witnesses = as_bool(value).map_err(|e| form.at(e.clone()))?;
			}
			"time-operations" => {
				self.options.time_operations = as_bool(value).map_err(|e| form.at(e.clone()))?;
			}
			"optimize" | "trace" => {
				let enabled = as_bool(value).map_err(|e| form.at(e.clone()))?;
				if !enabled {
					return Ok(Value::None);
				}
				return Err(form.at(format!("option :{keyword} is not implemented")));
			}
			"loops-and-recursion" => {
				return Err(form.at("option :loops-and-recursion is not implemented".into()));
			}
			"default" => return Err(form.at("option :default is not implemented".into())),
			_ => return Err(form.at(format!("unknown option '{keyword}'"))),
		}
		Ok(Value::None)
	}

	fn get_option(&self, keyword: &str) -> Result<Value, Error> {
		let keyword = keyword.trim_start_matches(':');
		Ok(match keyword {
			"alphabet" => Value::String(if self.options.bytes_alphabet {
				"bytes".into()
			} else {
				"on-the-fly".into()
			}),
			"produce-witnesses" => Value::Bool(self.options.produce_witnesses),
			"time-operations" => Value::Bool(self.options.time_operations),
			"optimize" | "trace" | "loops-and-recursion" => Value::Bool(false),
			"default" => Value::None,
			_ => return Err(Error::plain(format!("unknown option '{keyword}'"))),
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
		writeln!(
			self.output,
			"{label}: {:.6}",
			started.elapsed().as_secs_f64()
		)
		.expect("writing to a String cannot fail");
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
				if !keywords.is_empty() {
					return Err(
						expression.at(format!("operation {name} takes no keyword arguments here"))
					);
				}
				crate::ops::call(name, arguments, self)
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

	pub(crate) fn load_file(&mut self, path: &str, kind: &str) -> Result<Value, Error> {
		let resolved = self.resolve_path(path);
		let text = fs::read_to_string(&resolved)
			.map_err(|e| Error::plain(format!("cannot read '{}': {}", resolved.display(), e)))?;
		self.parse_text(&text, kind)
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
				let trimmed = text.trim_start();
				if trimmed.starts_with("@NFT") || trimmed.contains("%LevelsNum") {
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

	// Automata are normalized to Value::Automaton at parse time, so no conversion helper is needed.

	pub(crate) fn path_for_write(&self, path: &str) -> PathBuf {
		self.resolve_path(path)
	}
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
