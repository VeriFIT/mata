// Copyright (c) 2026 VeriFIT. GNU GPL v3. See LICENSE in the project root.

use std::collections::{HashMap, HashSet};

use crate::Error;
use crate::ast::{Expr, FormKind, Kind, Parameter, Spanned};
use crate::eval::{Engine, Switch, sort_expression};
use crate::scripts::Inputs;

/// What `mata script check` reports besides its errors.
pub(crate) struct Report {
	/// Whether the script is guaranteed to terminate, provided the library operations it calls do.
	pub(crate) termination: bool,
	/// Report lines: unused declarations and portability warnings, in the order they were found.
	pub(crate) warnings: Vec<String>,
}

/// Special forms a call may name, besides the operations of `ops::OPERATIONS`.
const SPECIAL_FORMS: &[&str] = &[
	"->", "and", "as", "cond", "do", "else", "if", "lambda", "let", "loop", "or", "recur", "time",
	"witness",
];

/// Names a script cannot define: the special forms, the directives, and the predefined names.
pub(crate) const RESERVED_NAMES: &[&str] = &[
	"->",
	"and",
	"as",
	"cond",
	"do",
	"else",
	"if",
	"lambda",
	"let",
	"loop",
	"or",
	"recur",
	"time",
	"witness",
	"get-option",
	"get-info",
	"mata-script",
	"define",
	"declare-input",
	"declare-inputs",
	"declare-param",
	"set-option",
	"set-info",
	"push",
	"pop",
	"reset",
	"exit",
	"inputs",
	"epsilon",
	"dont-care",
];

const OPTIONS: &[&str] = &[
	"alphabet",
	"default",
	"loops-and-recursion",
	"optimize",
	"produce-witnesses",
	"time-operations",
	"trace",
];

/// Why a `:loops-and-recursion` option comes too late; it holds for the whole file.
pub(crate) const LOOPS_OPTION_PLACE: &str = "the option :loops-and-recursion must be set before the first form that is not set-option or set-info";

/// Why `what` is an error without `:loops-and-recursion`.
pub(crate) fn needs_loops(what: &str) -> String {
	format!("{what} needs (set-option :loops-and-recursion true)")
}

/// Why a `recur` stands where it cannot restart a loop.
pub(crate) const RECUR_OUTSIDE: &str =
	"'recur' must be inside a loop, and not inside a function defined in it";

/// Why a `recur` is not the last thing its loop body evaluates.
pub(crate) const RECUR_NOT_LAST: &str = "'recur' must be the last thing a loop body evaluates";

/// Operations a portable core script may call; `script-language.md`, "Portable core".
const CORE_OPERATIONS: &[&str] = &[
	"nfa:load",
	"nfa:union-nondeterministic",
	"nfa:intersection",
	"nfa:complement",
	"nfa:concatenate",
	"nfa:determinize",
	"nfa:minimize",
	"nfa:language-empty?",
	"nfa:included?",
	"nfa:equivalent?",
	"nfa:universal?",
	"script-alphabet",
];

/// Sorts a portable core script may use; `script-language.md`, "Portable core".
const CORE_SORTS: &[&str] = &["Nfa", "Alphabet", "Bool", "Int", "String"];

pub(crate) const SIMPLE_SORTS: &[&str] = &[
	"Nfa",
	"Nft",
	"Automaton",
	"IntermediateAutomaton",
	"Alphabet",
	"Symbol",
	"Word",
	"Run",
	"Bool",
	"Int",
	"String",
	"Function",
];

/// Static check: names, shapes, arities, sorts where known, declarations, and the termination report.
/// It never loads an input and never calls a library operation. With `inputs`, the inputs a run is
/// given are matched against the declarations too.
pub(crate) fn check(
	forms: &[Spanned<FormKind>],
	engine: &Engine,
	inputs: Option<&Inputs>,
) -> Result<Report, Error> {
	let mut checker = Checker::new(engine);
	if !forms
		.first()
		.is_some_and(|form| matches!(form.node, FormKind::Header { .. }))
	{
		checker.warnings.push(format!(
			"no (mata-script \"{}\") header; the script is read as the version of this mata",
			crate::version::LANGUAGE
		));
	}
	for (index, form) in forms.iter().enumerate() {
		if index > 0 && matches!(form.node, FormKind::Header { .. }) {
			checker.reject_form(form, "mata-script must be the first form".into());
		}
		checker.form(form);
	}
	if let Some(inputs) = inputs {
		checker.match_inputs(forms, inputs);
	}
	checker.result()
}

/// One lexical scope, holding what is known statically about the names it binds.
#[derive(Default)]
struct Frame {
	/// Sort of each name bound here, `None` when the sort is not known.
	sorts: HashMap<String, Option<String>>,
	/// Parameter count of each function defined here, when it is known.
	arities: HashMap<String, usize>,
	/// The frame binds only the name of the function whose body is checked, under
	/// `:loops-and-recursion`; a use of that name is a self-call.
	own: bool,
}

/// The flags of the profile check: the portable core profile and its declaration rules.
struct Profile {
	/// The script turned on the portable core profile.
	core: bool,
	/// Declarations may still come; set by the first form that is not `set-option` or `set-info`.
	declarations_open: bool,
	/// A `declare-inputs` was seen, so no declaration may follow it.
	declared_variadic: bool,
}

/// What checking a special form concluded.
enum Special {
	/// `head` names no special form.
	NotSpecial,
	/// `head` names a special form; its sort, when the form knows one.
	Form(Option<String>),
}

struct Checker<'a> {
	engine: &'a Engine,
	frames: Vec<Frame>,
	/// Declared inputs that the script uses.
	used: HashSet<String>,
	/// Declared inputs in declaration order, for the unused report.
	declarations: Vec<String>,
	errors: Vec<Error>,
	warnings: Vec<String>,
	termination: Switch,
	/// The profile and declaration state of the script.
	profile: Profile,
	/// Self-call errors are reported once per function.
	recursion_reported: Switch,
	/// Open `push` forms, for a `pop` without a matching `push`.
	pushes: usize,
	/// `:loops-and-recursion` is on.
	loops: Switch,
	/// No form other than `set-option` and `set-info` has been seen yet.
	options_open: Switch,
	/// The names and sorts of the innermost loop, for `recur`; `None` inside a function, where `recur`
	/// cannot restart a loop around it.
	loop_bindings: LoopBindings,
}

/// The names and sorts that the loops around an expression bound, innermost last; `None` inside a
/// function, where a `recur` cannot restart a loop around it.
type LoopBindings = Vec<Option<Vec<(String, Option<String>)>>>;

impl<'a> Checker<'a> {
	fn new(engine: &'a Engine) -> Checker<'a> {
		Checker {
			engine,
			frames: vec![Frame::default()],
			used: HashSet::new(),
			declarations: Vec::new(),
			errors: Vec::new(),
			warnings: Vec::new(),
			termination: Switch::On,
			profile: Profile {
				core: false,
				declarations_open: true,
				declared_variadic: false,
			},
			recursion_reported: Switch::Off,
			pushes: 0,
			loops: Switch::Off,
			options_open: Switch::On,
			loop_bindings: Vec::new(),
		}
	}

	fn result(mut self) -> Result<Report, Error> {
		for name in &self.declarations {
			if !self.used.contains(name) {
				self.warnings.push(format!("unused declaration '{name}'"));
			}
		}
		if self.errors.is_empty() {
			Ok(Report {
				termination: self.termination.on(),
				warnings: self.warnings,
			})
		} else {
			Err(Error::plain(
				self.errors
					.iter()
					.map(Error::render)
					.collect::<Vec<_>>()
					.join("; "),
			))
		}
	}

	// Environment -----------------------------------------------------------------------------------

	/// Whether `name` is bound anywhere the script can see. `inputs` is bound by every run, after the
	/// check.
	fn known(&self, name: &str) -> bool {
		name == "inputs"
			|| self
				.frames
				.iter()
				.rev()
				.any(|frame| frame.sorts.contains_key(name))
			|| self.engine.scope.lookup(name).is_some()
	}

	/// The sort of `name` used as a value, when it is known: annotations, the predefined names, and
	/// operations, which are functions.
	fn sort_of(&self, name: &str) -> Option<String> {
		for frame in self.frames.iter().rev() {
			if let Some(sort) = frame.sorts.get(name) {
				return sort.clone();
			}
		}
		match name {
			"epsilon" | "dont-care" => Some("Symbol".to_owned()),
			"inputs" => Some("(List String)".to_owned()),
			_ => crate::ops::visible(name, self.engine.language).map(|_| "Function".to_owned()),
		}
	}

	fn defined_here(&self, name: &str) -> bool {
		self.frames
			.last()
			.is_some_and(|frame| frame.sorts.contains_key(name))
	}

	fn bind(&mut self, name: &str, sort: Option<String>, arity: Option<usize>) {
		let frame = self.frames.last_mut().expect("the checker has a frame");
		frame.sorts.insert(name.to_owned(), sort);
		if let Some(arity) = arity {
			frame.arities.insert(name.to_owned(), arity);
		}
	}

	fn arity_of(&self, name: &str) -> Option<usize> {
		for frame in self.frames.iter().rev() {
			if frame.sorts.contains_key(name) {
				return frame.arities.get(name).copied();
			}
		}
		None
	}

	/// Takes away the termination guarantee when `name` resolves to the function whose body binds it
	/// as its own name: a self-call.
	fn own_reference(&mut self, name: &str) {
		let own = self
			.frames
			.iter()
			.rev()
			.find(|frame| frame.sorts.contains_key(name))
			.is_some_and(|frame| frame.own);
		if own {
			self.termination = Switch::Off;
		}
	}

	/// A function that uses its own name in its body, without `:loops-and-recursion`; reported once.
	fn self_call(&mut self, name: &str, expression: &Expr) {
		if !self.recursion_reported.on() {
			self.recursion_reported = Switch::On;
			self.reject(
				expression,
				needs_loops(&format!("'{name}' calls itself; recursion")),
			);
		}
		self.termination = Switch::Off;
	}

	fn reject(&mut self, expression: &Expr, message: String) {
		self.errors.push(expression.at(message));
	}

	fn reject_form(&mut self, form: &Spanned<FormKind>, message: String) {
		self.reject_at(form.line, form.column, message);
	}

	fn reject_at(&mut self, line: usize, column: usize, message: String) {
		self.errors.push(Error::at(message, line, column));
	}

	/// A function value in a position that the termination guarantee does not allow; `to_apply` is the
	/// position of the function a `list:` function applies, the only one it allows.
	fn consumed(&mut self, sort: Option<&str>, to_apply: bool) {
		let function = sort.is_some_and(holds_function);
		if function && !to_apply {
			self.termination = Switch::Off;
		}
	}

	/// A special form outside of the portable core, in a script that promised to stay within it.
	fn core_form(&mut self, name: &str, expression: &Expr) {
		if self.profile.core {
			self.reject(expression, format!("'{name}' is not in the portable core"));
		}
	}

	// Forms -----------------------------------------------------------------------------------------

	fn form(&mut self, form: &Spanned<FormKind>) {
		if self.profile.core
			&& let Some(directive) = outside_core(&form.node)
		{
			self.reject_form(form, format!("'{directive}' is not in the portable core"));
		}
		if !matches!(
			form.node,
			FormKind::Header { .. } | FormKind::SetOption { .. } | FormKind::SetInfo { .. }
		) {
			self.options_open = Switch::Off;
		}
		if self.loops.on() {
			for expression in form.node.expressions() {
				misplaced_recur(expression, false, false, &mut self.errors);
			}
		}
		match &form.node {
			FormKind::Header { requires, profile } => {
				for feature in requires {
					if let Some(message) = crate::version::missing_feature(feature) {
						self.reject_form(form, message);
					}
				}
				match profile.as_deref() {
					None => {}
					Some("core") => self.profile.core = true,
					Some(other) => self.reject_form(form, format!("unknown profile '{other}'")),
				}
			}
			FormKind::Declare {
				name,
				sort,
				variadic,
			} => self.declaration(form, name, sort, *variadic),
			FormKind::Parameter {
				name,
				sort,
				default,
			} => self.parameter(form, name, sort, default.as_ref()),
			FormKind::SetOption { keyword, value } => self.option(form, keyword, value),
			FormKind::SetInfo { keyword, value } => self.info(form, keyword, value),
			FormKind::Push => {
				self.profile.declarations_open = false;
				self.pushes += 1;
				self.frames.push(Frame::default());
			}
			FormKind::Pop => {
				self.profile.declarations_open = false;
				if self.pushes == 0 {
					self.reject_form(form, "'pop' without a matching 'push'".into());
				} else {
					self.pushes -= 1;
					self.frames.pop();
				}
			}
			FormKind::Reset => {
				self.profile.declarations_open = false;
				self.pushes = 0;
				self.frames.truncate(1);
				self.frames[0] = Frame::default();
			}
			FormKind::Exit => self.profile.declarations_open = false,
			FormKind::Define {
				name,
				parameters,
				sort,
				body,
			} => {
				self.profile.declarations_open = false;
				self.definition(
					name,
					sort.as_deref(),
					parameters.as_deref(),
					body,
					form.line,
					form.column,
				);
			}
			FormKind::Expression(expression) => {
				self.profile.declarations_open = false;
				let _ = self.expression(expression, None);
			}
		}
	}

	/// Checks one `set-info`: a key of the format or an `x-` key, with a value of the sort it names.
	fn info(&mut self, form: &Spanned<FormKind>, keyword: &str, value: &Expr) {
		if keyword == "profile" {
			self.reject_form(
				form,
				"the profile is set in the header: (mata-script \"…\" :profile core)".into(),
			);
		} else if !keyword.starts_with("x-")
			&& !crate::eval::INFO_KEYS
				.iter()
				.any(|(key, _)| *key == keyword)
		{
			self.reject_form(form, crate::eval::info_key_message(keyword));
		} else if let Some((_, expected)) = crate::eval::INFO_KEYS
			.iter()
			.find(|(key, _)| *key == keyword)
		{
			if let Some(sort) = self.expression(value, None)
				&& !accepts(expected, &sort)
			{
				self.reject_form(
					form,
					format!("info :{keyword} must be a {expected}, found {sort}"),
				);
			}
		} else {
			let _ = self.expression(value, None);
		}
	}

	/// Matches the inputs a run is given against the declarations: every declaration needs an input,
	/// every named input a declaration when the script declares inputs, every positional input a
	/// declaration that takes it, and every parameter a value of its sort.
	fn match_inputs(&mut self, forms: &[Spanned<FormKind>], inputs: &Inputs) {
		let mut takers = 0;
		let mut variadic = false;
		let mut declared = HashSet::new();
		let mut parameters = HashSet::new();
		for form in forms {
			match &form.node {
				// `declare-inputs` takes only the positional inputs that are left, never a named one.
				FormKind::Declare { variadic: true, .. } => variadic = true,
				FormKind::Declare { name, .. } => {
					declared.insert(name.as_str());
					if !inputs.named.contains_key(name) {
						takers += 1;
					}
				}
				FormKind::Parameter {
					name,
					sort,
					default,
				} => {
					parameters.insert(name.as_str());
					// The run binds parameters after it loads the inputs; a bad value fails here first.
					match inputs.parameters.get(name) {
						Some(text) if matches!(sort.as_str(), "String" | "Int" | "Bool") => {
							if let Err(message) = crate::eval::parameter_value(text, sort) {
								self.reject_form(form, format!("--param {name}: {message}"));
							}
						}
						None if default.is_none() => self.reject_form(
							form,
							format!(
								"no value for parameter '{name}'; give it with --param {name}=VALUE"
							),
						),
						_ => {}
					}
				}
				_ => {}
			}
		}
		// A script without declarations reads a named input as $name.
		let mut named: Vec<&String> = inputs.named.keys().collect();
		named.sort();
		for name in named {
			if (variadic || !declared.is_empty()) && !declared.contains(name.as_str()) {
				self.errors.push(Error::plain(format!(
					"--input {name}: the script declares no input '{name}'"
				)));
			}
		}
		let mut given: Vec<&String> = inputs.parameters.keys().collect();
		given.sort();
		for name in given {
			if !parameters.contains(name.as_str()) {
				self.errors.push(Error::plain(format!(
					"--param {name}: the script declares no parameter '{name}'"
				)));
			}
		}
		// A file can hold several automata, so fewer files than declarations can be enough; the
		// run reports a declaration that is still missing its automaton. A script without
		// declarations reads its inputs as $1, $2, … or `inputs`.
		if !variadic && !declared.is_empty() && inputs.positional.len() > takers {
			self.errors.push(Error::plain(format!(
				"{} positional inputs given, but the script takes {takers}",
				inputs.positional.len()
			)));
		}
	}

	fn parameter(
		&mut self,
		form: &Spanned<FormKind>,
		name: &str,
		sort: &str,
		default: Option<&Expr>,
	) {
		if !self.profile.declarations_open {
			self.reject_form(
				form,
				"declarations must come before every other form except set-option and set-info"
					.into(),
			);
		}
		if let Some(message) = crate::eval::reserved(name) {
			self.reject_form(form, message);
		} else if self.known(name) {
			self.reject_form(form, format!("declaration '{name}' is already defined"));
		}
		if !matches!(sort, "String" | "Int" | "Bool") {
			self.reject_form(
				form,
				format!("a parameter must be String, Int, or Bool; '{sort}' is not"),
			);
		}
		if let Some(default) = default
			&& let Some(actual) = self.expression(default, None)
			&& !accepts(sort, &actual)
		{
			self.reject(default, format!("expected {sort}, found {actual}"));
		}
		self.bind(name, Some(sort.to_owned()), None);
		self.declarations.push(name.to_owned());
	}

	fn check_sort_name(&mut self, line: usize, column: usize, sort: &str) {
		if !valid_sort(sort) {
			self.reject_at(line, column, format!("unknown sort '{sort}'"));
			return;
		}
		if self.profile.core && !portable_sort(sort) {
			self.reject_at(
				line,
				column,
				format!("sort '{sort}' is not in the portable core"),
			);
		}
	}

	fn declaration(&mut self, form: &Spanned<FormKind>, name: &str, sort: &str, variadic: bool) {
		if !self.profile.declarations_open {
			self.reject_form(
				form,
				"declarations must come before every other form except set-option and set-info"
					.into(),
			);
		}
		if let Some(message) = crate::eval::reserved(name) {
			self.reject_form(form, message);
		} else if self.known(name) {
			self.reject_form(form, format!("declaration '{name}' is already defined"));
		}
		if self.profile.declared_variadic {
			self.reject_form(form, "'declare-inputs' must come last".into());
		}
		let element = match compound(sort) {
			Some(("List", element)) => Some(element),
			_ => None,
		};
		if variadic {
			self.profile.declared_variadic = true;
			match element {
				Some("Nfa" | "Nft" | "Automaton") => {}
				_ => self.reject_form(
					form,
					"declare-inputs needs a list of Nfa, Nft, or Automaton".into(),
				),
			}
		} else if !matches!(sort, "Nfa" | "Nft" | "Automaton") {
			self.reject_form(
				form,
				format!("a declared input must be Nfa, Nft, or Automaton; '{sort}' is not"),
			);
		}
		self.check_sort_name(form.line, form.column, sort);
		self.bind(name, Some(sort.to_owned()), None);
		self.declarations.push(name.to_owned());
	}

	fn option(&mut self, form: &Spanned<FormKind>, keyword: &str, value: &Expr) {
		if !OPTIONS.contains(&keyword) {
			self.reject_form(form, format!("unknown option ':{keyword}'"));
			return;
		}
		match keyword {
			"alphabet" => {
				self.expect_sort(value, "String", form);
				if let Kind::String(name) = &value.node
					&& !matches!(name.as_str(), "on-the-fly" | "bytes")
				{
					self.reject_form(form, format!("unknown alphabet '{name}'"));
				}
			}
			"produce-witnesses" | "time-operations" | "optimize" | "trace" => {
				self.expect_sort(value, "Bool", form);
			}
			"loops-and-recursion" => {
				if !self.options_open.on() {
					self.reject_form(form, LOOPS_OPTION_PLACE.into());
				}
				match value.node {
					Kind::Bool(enabled) => self.loops = Switch::from(enabled),
					_ => self.reject_form(
						form,
						"the value of :loops-and-recursion must be written as true or false".into(),
					),
				}
				if self.loops.on() && self.profile.core {
					self.reject_form(
						form,
						"a portable core script cannot turn on :loops-and-recursion".into(),
					);
				}
			}
			_ => self.default_hints(form, value),
		}
	}

	/// `(set-option :default (operation :hint "value" …))`.
	fn default_hints(&mut self, form: &Spanned<FormKind>, value: &Expr) {
		let Kind::Call {
			function,
			arguments,
			keywords,
		} = &value.node
		else {
			self.reject(
				value,
				":default needs an operation with only hints, as (nfa:included? :algorithm \"naive\")"
					.into(),
			);
			return;
		};
		let Kind::Name(operation) = &function.node else {
			self.reject(function, ":default needs an operation name".into());
			return;
		};
		if crate::ops::visible(operation, self.engine.language).is_none() {
			self.reject(function, format!("unknown operation '{operation}'"));
			return;
		}
		if !arguments.is_empty() || keywords.is_empty() {
			self.reject_form(form, ":default needs an operation with only hints".into());
		}
		self.hints(operation, keywords);
	}

	/// Checks the hints of one call of `operation`.
	fn hints(&mut self, operation: &str, keywords: &[(String, Expr)]) {
		for (name, value) in keywords {
			if let Some(parameter) = crate::ops::optional_parameter(operation, name) {
				let expected = parameter.script_sort();
				if let Some(sort) = self.expression(value, None)
					&& !accepts(expected, &sort)
				{
					self.reject(
						value,
						format!("the value of :{name} must be of sort {expected}, found {sort}"),
					);
				}
				continue;
			}
			if let Kind::String(text) = &value.node {
				if let Err(message) = crate::ops::check_hint(operation, name, text) {
					self.reject(value, message);
				}
				continue;
			}
			if crate::ops::hint(operation, name).is_none() {
				self.reject(
					value,
					crate::ops::check_hint(operation, name, "")
						.expect_err("an operation without the hint rejects it"),
				);
			}
			if let Some(sort) = self.expression(value, None)
				&& !accepts("String", &sort)
			{
				self.reject(
					value,
					format!("the value of hint :{name} must be a String, found {sort}"),
				);
			}
		}
	}

	fn expect_sort(&mut self, value: &Expr, expected: &str, form: &Spanned<FormKind>) {
		if let Some(sort) = self.expression(value, None)
			&& !accepts(expected, &sort)
		{
			self.reject_form(form, format!("expected {expected}, found {sort}"));
		}
	}

	/// Checks `(define name [sort] value)` or `(define (name parameters…) body…)` and binds `name`.
	fn definition(
		&mut self,
		name: &str,
		annotation: Option<&str>,
		parameters: Option<&[Parameter]>,
		body: &[Expr],
		line: usize,
		column: usize,
	) {
		if let Some(message) = crate::eval::reserved(name) {
			self.reject_at(line, column, message);
		} else if self.defined_here(name) {
			self.reject_at(line, column, format!("'{name}' is already defined"));
		}
		if let Some(annotation) = annotation {
			self.check_sort_name(line, column, annotation);
		}
		let sort = match parameters {
			None => self.body(body, None),
			Some(parameters) => {
				if self.profile.core {
					self.reject_at(
						line,
						column,
						"a portable core script cannot define a function".into(),
					);
				}
				// With the option, the function sees its own name; without it, a use of the name is
				// reported as a self-call.
				if self.loops.on() {
					self.frames.push(Frame {
						own: true,
						..Frame::default()
					});
					self.bind(name, Some("Function".to_owned()), Some(parameters.len()));
				}
				self.frames.push(Frame::default());
				for parameter in parameters {
					if let Some(sort) = &parameter.sort {
						self.check_sort_name(line, column, sort);
					}
					if let Some(message) = crate::eval::reserved(&parameter.name) {
						self.reject_at(line, column, message);
					}
					self.bind(&parameter.name, parameter.sort.clone(), None);
				}
				self.loop_bindings.push(None);
				let result = self.body(body, (!self.loops.on()).then_some(name));
				self.loop_bindings.pop();
				self.frames.pop();
				if self.loops.on() {
					self.frames.pop();
				}
				if result.as_deref() == Some("Function") {
					self.termination = Switch::Off;
				}
				Some("Function".to_owned())
			}
		};
		// The annotation narrows like `as`: only a sort that cannot hold the value is an error.
		let sort = match (annotation, sort) {
			(Some(expected), Some(actual))
				if actual == "Function" && !accepts(expected, "Function") =>
			{
				self.reject_at(
					line,
					column,
					format!("'{name}' is a function, not a {expected}"),
				);
				Some(expected.to_owned())
			}
			(Some(expected), Some(actual)) if accepts(expected, &actual) => Some(actual),
			(Some(expected), Some(actual)) => {
				if !accepts(&actual, expected) {
					self.reject_at(line, column, format!("expected {expected}, found {actual}"));
				}
				Some(expected.to_owned())
			}
			(Some(expected), None) => Some(expected.to_owned()),
			(None, sort) => sort,
		};
		let arity = parameters.map(<[Parameter]>::len);
		self.bind(name, sort, arity);
	}

	/// Checks a body in a fresh scope and returns the sort of its last expression.
	fn body(&mut self, items: &[Expr], self_name: Option<&str>) -> Option<String> {
		self.frames.push(Frame::default());
		let result = self.sequence(items, self_name);
		self.frames.pop();
		result
	}

	/// Checks each expression in the current scope and returns the sort of the last one.
	fn sequence(&mut self, items: &[Expr], self_name: Option<&str>) -> Option<String> {
		let mut result = None;
		for item in items {
			result = Some(self.expression(item, self_name));
		}
		result.flatten()
	}

	// Expressions -----------------------------------------------------------------------------------

	fn expression(&mut self, expression: &Expr, self_name: Option<&str>) -> Option<String> {
		match &expression.node {
			Kind::Bool(_) => Some("Bool".into()),
			Kind::Int(_) => Some("Int".into()),
			Kind::String(_) | Kind::Input(_) => Some("String".into()),
			Kind::None => Some("none".into()),
			Kind::Name(name) => {
				if Some(name.as_str()) == self_name {
					self.self_call(name, expression);
					return Some("Function".into());
				}
				if !self.known(name) {
					self.reject(expression, format!("unknown name '{name}'"));
					return None;
				}
				self.used.insert(name.clone());
				self.own_reference(name);
				self.sort_of(name)
			}
			Kind::List(items) => self.list(items, expression, self_name),
			Kind::Lambda { parameters, body } => {
				if self.profile.core {
					self.reject(
						expression,
						"a portable core script cannot use a lambda".into(),
					);
				}
				self.frames.push(Frame::default());
				for parameter in parameters {
					if let Some(sort) = &parameter.sort {
						self.check_sort_expression(expression, sort);
					}
					if let Some(message) = crate::eval::reserved(&parameter.name) {
						self.reject(expression, message);
					}
					self.bind(&parameter.name, parameter.sort.clone(), None);
				}
				self.loop_bindings.push(None);
				let result = self.body(body, self_name);
				self.loop_bindings.pop();
				self.frames.pop();
				if result.as_deref() == Some("Function") {
					self.termination = Switch::Off;
				}
				Some("Function".into())
			}
			Kind::Definition {
				name,
				parameters,
				sort,
				body,
			} => {
				self.definition(
					name,
					sort.as_deref(),
					parameters.as_deref(),
					body,
					expression.line,
					expression.column,
				);
				None
			}
			Kind::Call { .. } => self.call(expression, self_name),
			Kind::Literal { name, arguments } => Some(self.literal(name, arguments, expression)),
			Kind::GetOption(keyword) => {
				self.core_form("get-option", expression);
				if OPTIONS.contains(&keyword.as_str()) && keyword != "default" {
					Some(
						if keyword == "alphabet" {
							"String"
						} else {
							"Bool"
						}
						.into(),
					)
				} else {
					self.reject(expression, format!("unknown option ':{keyword}'"));
					None
				}
			}
			Kind::GetInfo(keyword) => {
				self.core_form("get-info", expression);
				if !keyword.starts_with("x-")
					&& !crate::eval::INFO_KEYS.iter().any(|(key, _)| key == keyword)
				{
					self.reject(expression, crate::eval::info_key_message(keyword));
				}
				None
			}
		}
	}

	fn check_sort_expression(&mut self, expression: &Expr, sort: &str) {
		if !valid_sort(sort) {
			self.reject(expression, format!("unknown sort '{sort}'"));
			return;
		}
		if self.profile.core && !portable_sort(sort) {
			self.reject(
				expression,
				format!("sort '{sort}' is not in the portable core"),
			);
		}
	}

	fn literal(&mut self, name: &str, arguments: &[(String, Expr)], expression: &Expr) -> String {
		if self.profile.core {
			self.reject(expression, format!("'{name}' is not in the portable core"));
		}
		let allowed: &[&str] = if name == "nfa:literal" {
			&["initial", "final", "transitions"]
		} else {
			&["initial", "final", "levels", "states", "transitions"]
		};
		let mut seen = HashSet::new();
		for (keyword, _) in arguments {
			if !allowed.contains(&keyword.as_str()) {
				self.reject(expression, format!("'{name}' has no keyword ':{keyword}'"));
			} else if !seen.insert(keyword.as_str()) {
				self.reject(expression, format!("duplicate keyword ':{keyword}'"));
			}
		}
		let mut required: Vec<&str> = vec!["initial", "final"];
		if name == "nft:literal" {
			required.push("levels");
		}
		for keyword in required {
			if !seen.contains(keyword) {
				self.reject(expression, format!("{name} needs :{keyword}"));
			}
		}
		if name == "nfa:literal" {
			"Nfa".to_owned()
		} else {
			"Nft".to_owned()
		}
	}

	fn list(
		&mut self,
		items: &[Expr],
		expression: &Expr,
		self_name: Option<&str>,
	) -> Option<String> {
		let Some(Kind::Name(head)) = items.first().map(|item| &item.node) else {
			self.reject(expression, "expected a function or special form".into());
			return None;
		};
		match head.as_str() {
			"let" | "loop" => {
				if head == "loop" {
					self.termination = Switch::Off;
					if !self.loops.on() {
						self.reject(expression, needs_loops("'loop'"));
						return None;
					}
				}
				self.list_let(head, &items[1..], expression, self_name)
			}
			"cond" => {
				self.core_form("cond", expression);
				self.list_cond(&items[1..], expression, self_name)
			}
			_ => {
				let arguments = items[1..].to_vec();
				self.call_head(head, &arguments, &[], expression, self_name)
			}
		}
	}

	/// Checks `let` and `loop`, which bind names in turn and then evaluate a body; the body of a `loop`
	/// can `recur` with new values of its names.
	fn list_let(
		&mut self,
		head: &str,
		parts: &[Expr],
		expression: &Expr,
		self_name: Option<&str>,
	) -> Option<String> {
		let Some(Kind::List(bindings)) = parts.first().map(|part| &part.node) else {
			self.reject(expression, format!("{head} needs a binding list"));
			return None;
		};
		if parts.len() < 2 {
			self.reject(expression, format!("{head} needs a body"));
			return None;
		}
		self.frames.push(Frame::default());
		let mut bound = Vec::new();
		for binding in bindings {
			let Kind::List(pair) = &binding.node else {
				self.reject(binding, format!("a {head} binding must be (name value)"));
				continue;
			};
			if pair.len() != 2 {
				self.reject(binding, format!("a {head} binding must be (name value)"));
				continue;
			}
			let Kind::Name(name) = &pair[0].node else {
				self.reject(binding, format!("a {head} binding needs a name"));
				continue;
			};
			if let Some(message) = crate::eval::reserved(name) {
				self.reject(binding, message);
			}
			let sort = self.expression(&pair[1], self_name);
			let arity = match &pair[1].node {
				Kind::Lambda { parameters, .. }
				| Kind::Definition {
					parameters: Some(parameters),
					..
				} => Some(parameters.len()),
				_ => None,
			};
			bound.push((name.clone(), sort.clone()));
			self.bind(name, sort, arity);
		}
		let looping = head == "loop";
		if looping {
			self.loop_bindings.push(Some(bound));
		}
		let result = self.body(&parts[1..], self_name);
		if looping {
			self.loop_bindings.pop();
		}
		self.frames.pop();
		result
	}

	fn list_cond(
		&mut self,
		clauses: &[Expr],
		expression: &Expr,
		self_name: Option<&str>,
	) -> Option<String> {
		if clauses.is_empty() {
			self.reject(expression, "cond needs at least one clause".into());
			return None;
		}
		let mut sorts = Vec::new();
		let last = clauses.len() - 1;
		for (index, clause) in clauses.iter().enumerate() {
			let Kind::List(pair) = &clause.node else {
				self.reject(clause, "a cond clause must be a list".into());
				sorts.push(None);
				continue;
			};
			if pair.len() != 2 {
				self.reject(clause, "a cond clause needs a condition and a value".into());
				sorts.push(None);
				continue;
			}
			let is_else = matches!(&pair[0].node, Kind::Name(name) if name == "else");
			if is_else {
				if index != last {
					self.reject(clause, "'else' must be the last cond clause".into());
				}
			} else if let Some(sort) = self.expression(&pair[0], self_name)
				&& !accepts("Bool", &sort)
			{
				self.reject(
					&pair[0],
					format!("cond needs Bool conditions, found {sort}"),
				);
			}
			sorts.push(self.expression(&pair[1], self_name));
		}
		let mut sorts = sorts.into_iter();
		let first = sorts.next().flatten()?;
		if sorts.all(|sort| sort.as_deref() == Some(first.as_str())) {
			Some(first)
		} else {
			None
		}
	}

	fn call(&mut self, expression: &Expr, self_name: Option<&str>) -> Option<String> {
		let Kind::Call {
			function,
			arguments,
			keywords,
		} = &expression.node
		else {
			unreachable!()
		};
		if let Kind::Name(head) = &function.node {
			return self.call_head(head, arguments, keywords, expression, self_name);
		}
		if let Some(sort) = self.expression(function, self_name)
			&& !may_be_function(&sort)
		{
			self.reject(
				function,
				"the first element of an application must be a function".into(),
			);
		}
		for argument in arguments {
			let sort = self.expression(argument, self_name);
			self.consumed(sort.as_deref(), false);
		}
		for (_, value) in keywords {
			let _ = self.expression(value, self_name);
		}
		None
	}

	fn call_head(
		&mut self,
		head: &str,
		arguments: &[Expr],
		keywords: &[(String, Expr)],
		expression: &Expr,
		self_name: Option<&str>,
	) -> Option<String> {
		if Some(head) == self_name {
			self.self_call(head, expression);
			for argument in arguments {
				let _ = self.expression(argument, self_name);
			}
			return None;
		}
		let special = self.special(head, arguments, keywords, expression, self_name);
		if let Special::Form(result) = special {
			return result;
		}
		if let Some(operation) = crate::ops::visible(head, self.engine.language) {
			return self.operation_call(operation, arguments, keywords, expression, self_name);
		}
		if SPECIAL_FORMS.contains(&head) {
			// A special form is handled above; reaching here means it is not allowed in this position.
			self.reject(expression, format!("'{head}' is not allowed here"));
			return None;
		}
		if !self.known(head) {
			self.reject(expression, format!("unknown name '{head}'"));
			return None;
		}
		self.own_reference(head);
		let sort = self.sort_of(head);
		if let Some(sort) = &sort
			&& !may_be_function(sort)
		{
			self.reject(
				expression,
				"the first element of an application must be a function".into(),
			);
			return None;
		}
		if !keywords.is_empty() {
			self.reject(
				expression,
				"a defined function takes no keyword arguments".to_owned(),
			);
		}
		if let Some(expected) = self.arity_of(head)
			&& expected != arguments.len()
		{
			self.reject(
				expression,
				format!(
					"the function expects {expected} arguments, {} given",
					arguments.len()
				),
			);
		}
		for argument in arguments {
			let sort = self.expression(argument, self_name);
			self.consumed(sort.as_deref(), false);
		}
		for (_, value) in keywords {
			let _ = self.expression(value, self_name);
		}
		None
	}

	/// Checks a call of a catalogue operation: its hints, the portable core, its arity, and the sorts of
	/// its operands.
	fn operation_call(
		&mut self,
		operation: &crate::ops::Operation,
		arguments: &[Expr],
		keywords: &[(String, Expr)],
		expression: &Expr,
		self_name: Option<&str>,
	) -> Option<String> {
		self.hints(operation.name, keywords);
		self.operation_shape(operation, arguments.len(), expression);
		let mut first = None;
		for (index, argument) in arguments.iter().enumerate() {
			let sort = self.expression(argument, self_name);
			self.operand(operation, index, arguments.len(), sort.as_deref(), argument);
			if index == 0 {
				first = sort;
			}
		}
		result_sort(operation, first.as_deref())
	}

	/// Checks that a portable core script may call `operation` and that it takes `given` operands.
	fn operation_shape(&mut self, operation: &crate::ops::Operation, given: usize, at: &Expr) {
		let head = operation.name;
		if self.profile.core && !core_call(head) {
			self.reject(at, format!("'{head}' is not in the portable core"));
		}
		if !crate::ops::accepts(operation, given) {
			self.reject(at, crate::ops::arity_message(operation, given));
		}
	}

	/// Checks operand `index` of the `given` operands of `operation`, of sort `sort` when known.
	fn operand(
		&mut self,
		operation: &crate::ops::Operation,
		index: usize,
		given: usize,
		sort: Option<&str>,
		at: &Expr,
	) {
		let head = operation.name;
		let expected = if head == "check" {
			// `(check condition)` or `(check "label" condition)`.
			Some(if index + 1 == given { "Bool" } else { "String" })
		} else {
			crate::ops::operand_sort(operation, index)
		};
		if let Some(expected) = expected
			&& let Some(actual) = sort
			&& !accepts(expected, actual)
		{
			self.reject(
				at,
				format!(
					"argument {} of '{head}' must be {expected}, found {actual}",
					index + 1
				),
			);
		}
		self.consumed(
			sort,
			head.starts_with("list:") && expected == Some("Function"),
		);
	}

	/// Checks a special form named by `head`; `NotSpecial` when `head` names no special form.
	fn special(
		&mut self,
		head: &str,
		arguments: &[Expr],
		keywords: &[(String, Expr)],
		expression: &Expr,
		self_name: Option<&str>,
	) -> Special {
		if !keywords.is_empty() && SPECIAL_FORMS.contains(&head) {
			self.reject(expression, format!("'{head}' takes no keyword arguments"));
		}
		if matches!(head, "if" | "do" | "and" | "or" | "as" | "witness") {
			self.core_form(head, expression);
		}
		match head {
			"if" => self.special_if(arguments, expression, self_name),
			"do" => Special::Form(self.sequence(arguments, self_name)),
			"and" | "or" => self.special_logic(head, arguments, self_name),
			"->" => Special::Form(self.pipeline(arguments, expression, self_name)),
			"as" => self.special_as(arguments, expression, self_name),
			"time" => self.special_time(arguments, expression, self_name),
			"witness" => self.special_witness(arguments, expression, self_name),
			"recur" => {
				self.special_recur(arguments, expression, self_name);
				Special::Form(None)
			}
			"else" => {
				self.reject(
					expression,
					"'else' is only allowed as the last cond clause".into(),
				);
				Special::Form(None)
			}
			_ => Special::NotSpecial,
		}
	}

	/// Checks `recur`: one value for each name of the innermost loop, of the sort the name started
	/// with. Where it stands is checked by [`misplaced_recur`].
	fn special_recur(&mut self, arguments: &[Expr], expression: &Expr, self_name: Option<&str>) {
		self.termination = Switch::Off;
		if !self.loops.on() {
			self.reject(expression, needs_loops("'recur'"));
		}
		let sorts: Vec<Option<String>> = arguments
			.iter()
			.map(|argument| self.expression(argument, self_name))
			.collect();
		let Some(Some(bindings)) = self.loop_bindings.last() else {
			return;
		};
		if bindings.len() != sorts.len() {
			let message = format!(
				"recur takes {} values, one for each name of the loop; {} given",
				bindings.len(),
				sorts.len()
			);
			self.reject(expression, message);
			return;
		}
		let mismatches: Vec<(usize, String)> = bindings
			.iter()
			.zip(&sorts)
			.enumerate()
			.filter_map(|(index, ((name, expected), actual))| {
				let (Some(expected), Some(actual)) = (expected, actual) else {
					return None;
				};
				(expected != "none" && !accepts(expected, actual)).then(|| {
					(
						index,
						format!(
							"recur gives '{name}' a {actual}, but the loop started it as {expected}"
						),
					)
				})
			})
			.collect();
		for (index, message) in mismatches {
			self.reject(&arguments[index], message);
		}
	}

	/// Checks `if`: a Bool condition and two branches of one sort.
	fn special_if(
		&mut self,
		arguments: &[Expr],
		expression: &Expr,
		self_name: Option<&str>,
	) -> Special {
		if arguments.len() != 3 {
			self.reject(expression, "if needs a condition and two branches".into());
			return Special::Form(None);
		}
		if let Some(sort) = self.expression(&arguments[0], self_name)
			&& !accepts("Bool", &sort)
		{
			self.reject(
				&arguments[0],
				format!("if needs a Bool condition, found {sort}"),
			);
		}
		let then = self.expression(&arguments[1], self_name);
		let otherwise = self.expression(&arguments[2], self_name);
		if then.is_some() && then == otherwise {
			Special::Form(then)
		} else {
			Special::Form(None)
		}
	}

	/// Checks `and` or `or`: Bool operands, Bool result.
	fn special_logic(
		&mut self,
		head: &str,
		arguments: &[Expr],
		self_name: Option<&str>,
	) -> Special {
		for argument in arguments {
			if let Some(sort) = self.expression(argument, self_name)
				&& !accepts("Bool", &sort)
			{
				self.reject(
					argument,
					format!("{head} needs Bool operands, found {sort}"),
				);
			}
		}
		Special::Form(Some("Bool".into()))
	}

	/// Checks `as`: a value and the sort it must conform to.
	fn special_as(
		&mut self,
		arguments: &[Expr],
		expression: &Expr,
		self_name: Option<&str>,
	) -> Special {
		if arguments.len() != 2 {
			self.reject(expression, "as needs a value and a sort".into());
			return Special::Form(None);
		}
		let Some(sort) = sort_expression(&arguments[1]) else {
			self.reject(&arguments[1], "expected a sort".into());
			return Special::Form(None);
		};
		self.check_sort_expression(&arguments[1], &sort);
		let actual = self.expression(&arguments[0], self_name);
		// `as` narrows: an `Automaton` may be checked as an `Nfa`.
		if let Some(actual) = &actual
			&& !accepts(&sort, actual)
			&& !accepts(actual, &sort)
		{
			self.reject(&arguments[0], format!("expected {sort}, found {actual}"));
		}
		Special::Form(Some(sort))
	}

	/// Checks `time`: a String label followed by the expressions it measures.
	fn special_time(
		&mut self,
		arguments: &[Expr],
		expression: &Expr,
		self_name: Option<&str>,
	) -> Special {
		if arguments.len() < 2 {
			self.reject(expression, "time needs a label and an expression".into());
			return Special::Form(None);
		}
		if let Some(sort) = self.expression(&arguments[0], self_name)
			&& !accepts("String", &sort)
		{
			self.reject(
				&arguments[0],
				format!("the time label must be a String, found {sort}"),
			);
		}
		Special::Form(self.sequence(&arguments[1..], self_name))
	}

	/// Checks `witness`: an application of an operation that stores a witness, a counterexample word.
	fn special_witness(
		&mut self,
		arguments: &[Expr],
		expression: &Expr,
		self_name: Option<&str>,
	) -> Special {
		if arguments.len() != 1 {
			self.reject(expression, "witness takes one application".into());
			return Special::Form(None);
		}
		self.expression(&arguments[0], self_name);
		if !crate::eval::applies_witness_operation(&arguments[0]) {
			self.reject(&arguments[0], crate::eval::witness_message());
		}
		Special::Form(Some("(Option Word)".into()))
	}

	fn pipeline(
		&mut self,
		steps: &[Expr],
		expression: &Expr,
		self_name: Option<&str>,
	) -> Option<String> {
		let Some(first) = steps.first() else {
			self.reject(expression, "a pipeline needs a value".into());
			return None;
		};
		let mut sort = self.expression(first, self_name);
		for step in &steps[1..] {
			sort = match &step.node {
				Kind::Name(name) => {
					self.pipeline_step(name, sort.as_deref(), &[], &[], step, self_name)
				}
				Kind::Call {
					function,
					arguments,
					keywords,
				} => {
					if let Kind::Name(head) = &function.node {
						self.pipeline_step(
							head,
							sort.as_deref(),
							arguments,
							keywords,
							step,
							self_name,
						)
					} else {
						// A function value, such as a lambda, is checked as any other application.
						self.consumed(sort.as_deref(), false);
						self.call(step, self_name)
					}
				}
				_ => {
					self.reject(
						step,
						"a pipeline step must be a name or an application".into(),
					);
					None
				}
			};
		}
		sort
	}

	/// Checks one pipeline step that calls `name` with the piped value of sort `piped` followed by
	/// `arguments`, and returns the sort of its result when known.
	fn pipeline_step(
		&mut self,
		name: &str,
		piped: Option<&str>,
		arguments: &[Expr],
		keywords: &[(String, Expr)],
		step: &Expr,
		self_name: Option<&str>,
	) -> Option<String> {
		let given = arguments.len() + 1;
		let operation = if self.known(name) {
			self.used.insert(name.to_owned());
			self.own_reference(name);
			crate::ops::visible(name, self.engine.language)
		} else {
			self.reject(step, format!("unknown name '{name}'"));
			None
		};
		if let Some(operation) = operation {
			self.operation_shape(operation, given, step);
			self.operand(operation, 0, given, piped, step);
		} else {
			if let Some(sort) = self.sort_of(name)
				&& !may_be_function(&sort)
			{
				self.reject(step, "a pipeline step must be a function".into());
			} else if let Some(expected) = self.arity_of(name)
				&& expected != given
			{
				self.reject(
					step,
					format!("the function expects {expected} arguments, {given} given"),
				);
			}
			self.consumed(piped, false);
		}
		// The written arguments follow the piped value.
		for (index, argument) in arguments.iter().enumerate() {
			let sort = self.expression(argument, self_name);
			if let Some(operation) = operation {
				self.operand(operation, index + 1, given, sort.as_deref(), argument);
			} else {
				self.consumed(sort.as_deref(), false);
			}
		}
		if let Some(operation) = operation {
			self.hints(operation.name, keywords);
			result_sort(operation, piped)
		} else {
			for (_, value) in keywords {
				let _ = self.expression(value, self_name);
			}
			None
		}
	}
}

/// Whether a value of `sort` may be a function: an optional function is one unless it is `none`, which
/// the evaluator rejects when it is applied.
fn may_be_function(sort: &str) -> bool {
	accepts("Function", sort) || holds_function(sort)
}

/// Whether `sort` is `Function`, possibly inside any number of `Option`s.
fn holds_function(sort: &str) -> bool {
	sort == "Function"
		|| compound(sort).is_some_and(|(name, inner)| name == "Option" && holds_function(inner))
}

/// Splits `(List Nfa)` into `("List", "Nfa")`; `None` for a simple sort.
fn compound(sort: &str) -> Option<(&str, &str)> {
	let inner = sort.strip_prefix('(')?.strip_suffix(')')?.trim();
	let mut parts = inner.splitn(2, char::is_whitespace);
	let name = parts.next()?;
	let argument = parts.next().unwrap_or("").trim();
	(!argument.is_empty()).then_some((name, argument))
}

fn valid_sort(sort: &str) -> bool {
	if SIMPLE_SORTS.contains(&sort) {
		return true;
	}
	match compound(sort) {
		Some(("List" | "Option", argument)) => valid_sort(argument),
		_ => false,
	}
}

/// Whether a portable core script may call `name`: the core operations, the `list:` functions, and the
/// reporting forms that the core allows.
fn core_call(name: &str) -> bool {
	CORE_OPERATIONS.contains(&name)
		|| name.starts_with("list:")
		|| matches!(name, "assert" | "check" | "echo" | "print")
}

/// The name of a directive outside of the portable core.
fn outside_core(form: &FormKind) -> Option<&'static str> {
	match form {
		FormKind::Parameter { .. } => Some("declare-param"),
		FormKind::Push => Some("push"),
		FormKind::Pop => Some("pop"),
		FormKind::Reset => Some("reset"),
		FormKind::Exit => Some("exit"),
		_ => None,
	}
}

fn portable_sort(sort: &str) -> bool {
	if CORE_SORTS.contains(&sort) {
		return true;
	}
	matches!(compound(sort), Some(("List", argument)) if portable_sort(argument))
}

/// The sort of the result of `operation` when known, from the sort `first` of its first operand where
/// it depends on it: a load gives one automaton for a path and a list of them for a list of paths.
fn result_sort(operation: &crate::ops::Operation, first: Option<&str>) -> Option<String> {
	let kind = match operation.name {
		"nfa:load" => "Nfa",
		"nft:load" => "Nft",
		"automaton:load" => "Automaton",
		_ => return (!operation.result.is_empty()).then(|| operation.result.to_owned()),
	};
	match first? {
		"String" => Some(kind.to_owned()),
		sort if compound(sort).is_some_and(|(name, _)| name == "List") => {
			Some(format!("(List {kind})"))
		}
		_ => None,
	}
}

/// Whether a value of sort `actual` fits where `expected` is required. An empty or `Any` expected sort,
/// and an unknown actual sort, fit everywhere; an `Any` actual sort, as the elements of the list a
/// generic `list:` function returns, is unknown.
fn accepts(expected: &str, actual: &str) -> bool {
	if expected.is_empty()
		|| expected == "Any"
		|| actual.is_empty()
		|| actual == "unknown"
		|| actual == "Any"
	{
		return true;
	}
	if expected.contains('|') {
		return expected
			.split('|')
			.any(|alternative| accepts(alternative.trim(), actual));
	}
	if expected == actual {
		return true;
	}
	if expected == "Automaton" && matches!(actual, "Nfa" | "Nft") {
		return true;
	}
	match (compound(expected), compound(actual)) {
		(Some(("Option", _)), _) if actual == "none" => true,
		(Some(("Option", expected)), Some(("Option", actual)))
		| (Some(("List", expected)), Some(("List", actual))) => accepts(expected, actual),
		// A `T` is an `(Option T)`, as the run has it.
		(Some(("Option", expected)), _) => accepts(expected, actual),
		_ => false,
	}
}

/// Reports every `recur` in `expression` that does not restart a loop as the last thing its body
/// evaluates. `tail`: `expression` is in that place; `in_loop`: a loop of the same function is around.
fn misplaced_recur(expression: &Expr, tail: bool, in_loop: bool, errors: &mut Vec<Error>) {
	match &expression.node {
		Kind::Call {
			function,
			arguments,
			keywords,
		} => {
			let head = match &function.node {
				Kind::Name(head) => head.as_str(),
				_ => "",
			};
			match head {
				"recur" => {
					if !in_loop {
						errors.push(expression.at(RECUR_OUTSIDE.into()));
					} else if !tail {
						errors.push(expression.at(RECUR_NOT_LAST.into()));
					}
				}
				"if" => {
					for (index, argument) in arguments.iter().enumerate() {
						misplaced_recur(argument, tail && index > 0, in_loop, errors);
					}
					return;
				}
				"do" => {
					misplaced_in_body(arguments, tail, in_loop, errors);
					return;
				}
				_ => misplaced_recur(function, false, in_loop, errors),
			}
			for argument in arguments
				.iter()
				.chain(keywords.iter().map(|(_, value)| value))
			{
				misplaced_recur(argument, false, in_loop, errors);
			}
		}
		Kind::List(items) => {
			let head = match items.first().map(|item| &item.node) {
				Some(Kind::Name(head)) => head.as_str(),
				_ => "",
			};
			match head {
				"let" | "loop" => {
					if let Some(Kind::List(bindings)) = items.get(1).map(|item| &item.node) {
						for binding in bindings {
							misplaced_recur(binding, false, in_loop, errors);
						}
					}
					let looping = head == "loop";
					let body = items.get(2..).unwrap_or_default();
					misplaced_in_body(body, tail || looping, in_loop || looping, errors);
				}
				"cond" => {
					for clause in &items[1..] {
						if let Kind::List(pair) = &clause.node {
							for (index, part) in pair.iter().enumerate() {
								misplaced_recur(part, tail && index == 1, in_loop, errors);
							}
						}
					}
				}
				_ => {
					for item in items {
						misplaced_recur(item, false, in_loop, errors);
					}
				}
			}
		}
		Kind::Lambda { body, .. }
		| Kind::Definition {
			parameters: Some(_),
			body,
			..
		} => misplaced_in_body(body, false, false, errors),
		Kind::Definition {
			parameters: None,
			body,
			..
		} => {
			for item in body {
				misplaced_recur(item, false, in_loop, errors);
			}
		}
		_ => {}
	}
}

/// [`misplaced_recur`] over a body, whose last expression after its definitions is in the place of
/// the body itself.
fn misplaced_in_body(body: &[Expr], tail: bool, in_loop: bool, errors: &mut Vec<Error>) {
	let last = body
		.iter()
		.rposition(|item| !matches!(item.node, Kind::Definition { .. }));
	for (index, item) in body.iter().enumerate() {
		misplaced_recur(item, tail && Some(index) == last, in_loop, errors);
	}
}
