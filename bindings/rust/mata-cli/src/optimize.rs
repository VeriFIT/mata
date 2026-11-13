// Copyright (c) 2026 VeriFIT. GNU GPL v3. See LICENSE in the project root.

//! What `(set-option :optimize true)` changes; `script-language.md`, "Optimizer". Every rewrite keeps
//! the structure of the automata a script computes, so the operand of a form that shows structure is
//! computed as written without exceptions.

use std::collections::{HashMap, HashSet};
use std::rc::{Rc, Weak};

use mata::{Nfa, Symbol, Word, nft::Nft};

use crate::ast::{Expr, FormKind, Kind, Spanned};
use crate::eval::{AutomatonKind, Function, Value};

/// The operations a dead definition may consist of: each builds an `Nfa` from `Nfa` operands and
/// cannot fail on them, so leaving it out changes nothing but the time a script takes.
const PURE_OPERATIONS: &[&str] = &[
	"nfa:union-nondeterministic",
	"nfa:intersection",
	"nfa:language-difference",
	"nfa:concatenate",
	"nfa:complement",
	"nfa:determinize",
	"nfa:minimize",
	"nfa:reduce",
	"nfa:revert",
	"nfa:remove-epsilon",
	"nfa:trim",
	"nfa:make-complete",
];

/// The places a whole script can be optimized at, found before it runs: definitions whose value is
/// never used, and the last use of a named value, after which the name is dropped so that the
/// operation that receives the value may change it in place instead of copying it.
#[derive(Default)]
pub(crate) struct Plan {
	dead: HashSet<(usize, usize)>,
	last_uses: HashSet<(usize, usize)>,
}

impl Plan {
	pub(crate) fn of(forms: &[Spanned<FormKind>]) -> Plan {
		let mut plan = Plan::default();
		let items: Vec<Item> = forms.iter().map(Item::Form).collect();
		plan.find_dead(&items);
		for form in forms {
			if let FormKind::Define {
				parameters: Some(_),
				body,
				..
			} = &form.node
			{
				plan.find_dead(&body.iter().map(Item::Expression).collect::<Vec<_>>());
			}
			for expression in form.node.expressions() {
				plan.walk(expression);
			}
		}
		let mut last: HashMap<&str, (usize, usize)> = HashMap::new();
		let mut candidates = Vec::new();
		for form in forms {
			let place = (form.line, form.column);
			if let Some(name) = bound_name(&form.node) {
				note(&mut last, name, place);
			}
			// The body of a function runs once per call.
			let mut once = true;
			if let FormKind::Define {
				parameters: Some(parameters),
				..
			} = &form.node
			{
				once = false;
				for parameter in parameters {
					note(&mut last, &parameter.name, place);
				}
			}
			for expression in form.node.expressions() {
				occurrences(expression, once, &mut last, &mut candidates);
			}
		}
		plan.last_uses = candidates
			.into_iter()
			.filter(|(name, place)| last.get(name) == Some(place))
			.map(|(_, place)| place)
			.collect();
		plan
	}

	/// Whether the definition at `line`:`column` binds a value that nothing uses.
	pub(crate) fn is_dead(&self, line: usize, column: usize) -> bool {
		self.dead.contains(&(line, column))
	}

	/// Whether `expression`, a name, is the last use of the value it names.
	pub(crate) fn is_last_use(&self, expression: &Expr) -> bool {
		self.last_uses
			.contains(&(expression.line, expression.column))
	}

	/// Finds the dead definitions of every body inside `expression`.
	fn walk(&mut self, expression: &Expr) {
		match &expression.node {
			Kind::Call {
				function,
				arguments,
				keywords,
			} => {
				self.walk(function);
				for argument in arguments
					.iter()
					.chain(keywords.iter().map(|(_, value)| value))
				{
					self.walk(argument);
				}
			}
			Kind::List(items) => {
				let body = match items.first().map(|item| &item.node) {
					Some(Kind::Name(head)) if head == "let" || head == "loop" => {
						items.get(2..).unwrap_or_default()
					}
					_ => &[],
				};
				if !body.is_empty() {
					self.find_dead(&body.iter().map(Item::Expression).collect::<Vec<_>>());
				}
				for item in items {
					self.walk(item);
				}
			}
			Kind::Lambda { body, .. }
			| Kind::Definition {
				parameters: Some(_),
				body,
				..
			} => {
				self.find_dead(&body.iter().map(Item::Expression).collect::<Vec<_>>());
				for item in body {
					self.walk(item);
				}
			}
			Kind::Definition { body, .. } => {
				for item in body {
					self.walk(item);
				}
			}
			_ => {}
		}
	}

	/// Marks the value definitions of one body that no other live item of it mentions, the last ones
	/// first, so a definition used only by dead ones is dead too.
	fn find_dead(&mut self, items: &[Item<'_>]) {
		for (index, item) in items.iter().enumerate().rev() {
			let Some((name, place)) = item.value_definition() else {
				continue;
			};
			let used = items.iter().enumerate().any(|(other, item)| {
				other != index && !self.dead.contains(&item.place()) && item.mentions(name)
			});
			if !used {
				self.dead.insert(place);
			}
		}
	}
}

/// One element of a body: a top-level form or an expression.
enum Item<'a> {
	Form(&'a Spanned<FormKind>),
	Expression(&'a Expr),
}

impl Item<'_> {
	fn place(&self) -> (usize, usize) {
		match self {
			Item::Form(form) => (form.line, form.column),
			Item::Expression(expression) => (expression.line, expression.column),
		}
	}

	/// The name and place of a definition of a value, not of a function.
	fn value_definition(&self) -> Option<(&str, (usize, usize))> {
		match self {
			Item::Form(Spanned {
				node: FormKind::Define {
					name,
					parameters: None,
					..
				},
				..
			})
			| Item::Expression(Spanned {
				node: Kind::Definition {
					name,
					parameters: None,
					..
				},
				..
			}) => Some((name.as_str(), self.place())),
			_ => None,
		}
	}

	fn mentions(&self, name: &str) -> bool {
		match self {
			Item::Form(form) => {
				bound_name(&form.node) == Some(name)
					|| matches!(&form.node, FormKind::Define { parameters: Some(parameters), .. }
						if parameters.iter().any(|parameter| parameter.name == name))
					|| form
						.node
						.expressions()
						.into_iter()
						.any(|expression| mentions(expression, name))
			}
			Item::Expression(expression) => mentions(expression, name),
		}
	}
}

/// The name a top-level form binds.
fn bound_name(form: &FormKind) -> Option<&str> {
	match form {
		FormKind::Define { name, .. }
		| FormKind::Declare { name, .. }
		| FormKind::Parameter { name, .. } => Some(name),
		_ => None,
	}
}

/// Whether `name` occurs in `expression` in any role: used, bound, or a parameter.
fn mentions(expression: &Expr, name: &str) -> bool {
	match &expression.node {
		Kind::Name(found) => found == name,
		Kind::Call {
			function,
			arguments,
			keywords,
		} => {
			mentions(function, name)
				|| arguments.iter().any(|argument| mentions(argument, name))
				|| keywords.iter().any(|(_, value)| mentions(value, name))
		}
		Kind::List(items) => items.iter().any(|item| mentions(item, name)),
		Kind::Lambda { parameters, body } => {
			parameters.iter().any(|parameter| parameter.name == name)
				|| body.iter().any(|item| mentions(item, name))
		}
		Kind::Definition {
			name: defined,
			parameters,
			body,
			..
		} => {
			defined == name
				|| parameters
					.iter()
					.flatten()
					.any(|parameter| parameter.name == name)
				|| body.iter().any(|item| mentions(item, name))
		}
		_ => false,
	}
}

fn note<'a>(last: &mut HashMap<&'a str, (usize, usize)>, name: &'a str, place: (usize, usize)) {
	let latest = last.entry(name).or_insert(place);
	*latest = (*latest).max(place);
}

/// Records the place of every occurrence of a name in `expression`, and as `candidates` the uses of a
/// name that run at most once: outside of every function and loop body.
fn occurrences<'a>(
	expression: &'a Expr,
	once: bool,
	last: &mut HashMap<&'a str, (usize, usize)>,
	candidates: &mut Vec<(&'a str, (usize, usize))>,
) {
	let place = (expression.line, expression.column);
	match &expression.node {
		Kind::Name(name) => {
			note(last, name, place);
			if once {
				candidates.push((name, place));
			}
		}
		Kind::Call {
			function,
			arguments,
			keywords,
		} => {
			occurrences(function, once, last, candidates);
			for argument in arguments
				.iter()
				.chain(keywords.iter().map(|(_, value)| value))
			{
				occurrences(argument, once, last, candidates);
			}
		}
		Kind::List(items) => {
			let looping = matches!(items.first().map(|item| &item.node), Some(Kind::Name(head)) if head == "loop");
			for (index, item) in items.iter().enumerate() {
				occurrences(item, once && !(looping && index > 1), last, candidates);
			}
		}
		Kind::Lambda { parameters, body } => {
			for parameter in parameters {
				note(last, &parameter.name, place);
			}
			for item in body {
				occurrences(item, false, last, candidates);
			}
		}
		Kind::Definition {
			name,
			parameters,
			body,
			..
		} => {
			note(last, name, place);
			for parameter in parameters.iter().flatten() {
				note(last, &parameter.name, place);
			}
			for item in body {
				occurrences(item, once && parameters.is_none(), last, candidates);
			}
		}
		_ => {}
	}
}

/// Whether `expression` computes an `Nfa` from `Nfa` operands with [`PURE_OPERATIONS`] only, so that
/// leaving it out cannot hide an error. `is_nfa` tells whether a name holds an `Nfa` now.
pub(crate) fn computes_nfa(expression: &Expr, is_nfa: &dyn Fn(&str) -> bool) -> bool {
	match &expression.node {
		Kind::Name(name) => is_nfa(name),
		Kind::Call {
			function,
			arguments,
			keywords,
		} => {
			let Kind::Name(name) = &function.node else {
				return false;
			};
			let hints_are_fixed = keywords.iter().all(|(key, value)| {
				matches!(&value.node, Kind::String(text)
					if crate::ops::check_hint(name, key, text).is_ok())
			});
			PURE_OPERATIONS.contains(&name.as_str())
				&& crate::ops::operation(name)
					.is_some_and(|operation| crate::ops::accepts(operation, arguments.len()))
				&& hints_are_fixed
				&& arguments
					.iter()
					.all(|argument| computes_nfa(argument, is_nfa))
		}
		_ => false,
	}
}

/// The two operands of `(nfa:language-empty? (nfa:intersection lhs rhs))`, which runs as
/// `(nfa:intersection-empty? lhs rhs)`: the library decides the emptiness of the product without
/// building it.
pub(crate) fn disjointness<'a>(name: &str, parts: &'a [Expr]) -> Option<(&'a Expr, &'a Expr)> {
	if name != "nfa:language-empty?" {
		return None;
	}
	let [operand] = parts else {
		return None;
	};
	match &operand.node {
		Kind::Call {
			function,
			arguments,
			keywords,
		} if keywords.is_empty()
			&& matches!(&function.node, Kind::Name(inner) if inner == "nfa:intersection") =>
		{
			match arguments.as_slice() {
				[lhs, rhs] => Some((lhs, rhs)),
				_ => None,
			}
		}
		_ => None,
	}
}

/// One operand of a remembered application, compared by value or, for a value behind an `Rc`, by
/// identity: values are immutable, so the same allocation holds the same value.
#[derive(PartialEq, Eq, Hash)]
enum Part {
	Shared(usize),
	Bool(bool),
	Int(i64),
	String(String),
	Word(Word),
	Symbol(Symbol),
	None,
}

/// An application: the operation, its operands, the hints written and set as defaults, and the size
/// of the script alphabet, which only grows and which some operations read.
#[derive(PartialEq, Eq, Hash)]
pub(crate) struct Key {
	operation: String,
	operands: Vec<Part>,
	hints: Vec<(String, String)>,
	defaults: Vec<(String, String)>,
	alphabet: usize,
}

/// A value behind an `Rc`, held without keeping it alive.
enum Held {
	Nfa(Weak<Nfa>),
	Nft(Weak<Nft>),
	Automaton(Weak<AutomatonKind>),
	List(Weak<Vec<Value>>),
	Alphabet(Weak<mata::Alphabet>),
	Run(Weak<mata::Run>),
	Function(Weak<Function>),
}

impl Held {
	/// The address and a weak reference of a shared value; `None` for a value held by value.
	fn of(value: &Value) -> Option<(usize, Held)> {
		fn address<T>(rc: &Rc<T>) -> usize {
			Rc::as_ptr(rc).addr()
		}
		Some(match value {
			Value::Nfa(rc) => (address(rc), Held::Nfa(Rc::downgrade(rc))),
			Value::Nft(rc) => (address(rc), Held::Nft(Rc::downgrade(rc))),
			Value::Automaton(rc) => (address(rc), Held::Automaton(Rc::downgrade(rc))),
			Value::List(rc) => (address(rc), Held::List(Rc::downgrade(rc))),
			Value::Alphabet(rc) => (address(rc), Held::Alphabet(Rc::downgrade(rc))),
			Value::Run(rc) => (address(rc), Held::Run(Rc::downgrade(rc))),
			Value::Function(rc) => (address(rc), Held::Function(Rc::downgrade(rc))),
			_ => return None,
		})
	}

	fn value(&self) -> Option<Value> {
		Some(match self {
			Held::Nfa(weak) => Value::Nfa(weak.upgrade()?),
			Held::Nft(weak) => Value::Nft(weak.upgrade()?),
			Held::Automaton(weak) => Value::Automaton(weak.upgrade()?),
			Held::List(weak) => Value::List(weak.upgrade()?),
			Held::Alphabet(weak) => Value::Alphabet(weak.upgrade()?),
			Held::Run(weak) => Value::Run(weak.upgrade()?),
			Held::Function(weak) => Value::Function(weak.upgrade()?),
		})
	}

	fn alive(&self) -> bool {
		self.value().is_some()
	}
}

/// A remembered result: a shared one held weakly, so that the memo never keeps an automaton alive,
/// or a plain one by value.
enum Cached {
	Shared(Held),
	Plain(Value),
}

/// A remembered application. Its operands are held weakly too, which keeps their addresses from
/// being reused while the entry exists.
struct Entry {
	operands: Vec<Held>,
	result: Cached,
}

impl Entry {
	fn alive(&self) -> bool {
		self.operands.iter().all(Held::alive)
			&& match &self.result {
				Cached::Shared(held) => held.alive(),
				Cached::Plain(_) => true,
			}
	}
}

/// The results of the library applications made so far, so that the same application of the same
/// values is one call.
#[derive(Default)]
pub(crate) struct Memo {
	entries: HashMap<Key, Entry>,
	/// The number of entries after the last sweep of dead ones.
	swept: usize,
}

impl Memo {
	/// The key of an application whose result depends on its operands, hints and the script alphabet
	/// only; `None` for one that reads a file or draws random numbers, or an operation of no library
	/// namespace.
	pub(crate) fn key(
		operation: &str,
		arguments: &[Value],
		hints: Vec<(String, String)>,
		defaults: Vec<(String, String)>,
		alphabet: usize,
	) -> Option<Key> {
		let library = ["nfa:", "nft:", "alphabet:", "strings:", "intermediate:"]
			.iter()
			.any(|namespace| operation.starts_with(namespace));
		if !library || operation.contains("load") || operation.contains("random") {
			return None;
		}
		let operands = arguments
			.iter()
			.map(|value| {
				Some(match value {
					Value::Bool(value) => Part::Bool(*value),
					Value::Int(value) => Part::Int(*value),
					Value::String(value) => Part::String(value.clone()),
					Value::Word(word) => Part::Word(word.clone()),
					Value::Symbol(symbol) => Part::Symbol(*symbol),
					Value::None => Part::None,
					shared => Part::Shared(Held::of(shared)?.0),
				})
			})
			.collect::<Option<Vec<_>>>()?;
		Some(Key {
			operation: operation.to_owned(),
			operands,
			hints,
			defaults,
			alphabet,
		})
	}

	pub(crate) fn get(&self, key: &Key) -> Option<Value> {
		match &self.entries.get(key)?.result {
			Cached::Shared(held) => held.value(),
			Cached::Plain(value) => Some(value.clone()),
		}
	}

	pub(crate) fn insert(&mut self, key: Key, arguments: &[Value], result: &Value) {
		let operands = arguments
			.iter()
			.filter_map(|value| Held::of(value).map(|(_, held)| held))
			.collect();
		let result = match Held::of(result) {
			Some((_, held)) => Cached::Shared(held),
			None => Cached::Plain(result.clone()),
		};
		self.entries.insert(key, Entry { operands, result });
		if self.entries.len() > 2 * self.swept + 64 {
			self.entries.retain(|_, entry| entry.alive());
			self.swept = self.entries.len();
		}
	}
}
