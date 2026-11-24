// Copyright (c) 2026 VeriFIT. GNU GPL v3. See LICENSE in the project root.

//! The `lazy:` namespace: terms of the one [`Formula`] of a run, which builders extend without automata
//! work, and queries that each run one lazy check of a term.

use std::collections::{HashMap, HashSet};
use std::rc::{Rc, Weak};
use std::time::Duration;

use mata::lazy::{
	self, ExplicitOptions, Formula, Options, Outcome, Plan, Policy, Stats, Term, Witness,
};
use mata::{Nfa, Word, nft::Nft};

use crate::Error;
use crate::ast::{Expr, Kind};
use crate::eval::{AutomatonKind, Engine, Value, as_int, as_list, value_sort};
use crate::ops::{expect_nfa, optional, take};
use crate::optimize::{Decision, Tree};

/// The operations that run one check of a term, with the statistics `lazy:stats` reports.
pub(crate) const CHECKS: &[&str] = &[
	"lazy:empty?",
	"lazy:empty-within?",
	"lazy:witness",
	"lazy:included?",
	"lazy:equivalent?",
];

/// Whether `name` is a query, which a run measures and traces as a library call; a builder only adds a node.
pub(crate) fn is_query(name: &str) -> bool {
	CHECKS.contains(&name) || name == "lazy:materialize"
}

/// Whether `expression` is an application of a check, the operand of `lazy:stats`.
pub(crate) fn applies_check(expression: &Expr) -> bool {
	matches!(&expression.node, Kind::Call { function, .. }
		if matches!(&function.node, Kind::Name(name) if CHECKS.contains(&name.as_str())))
}

/// The error for a `lazy:stats` whose operand is not an application of a check.
pub(crate) fn stats_message() -> String {
	format!(
		"lazy:stats applies to an application of {}",
		CHECKS.join(", ")
	)
}

/// The statistics of a query as `lazy:stats` prints them: a record name under the label and its value.
pub(crate) fn stats_records(stats: &Stats) -> [(&'static str, usize); 7] {
	let (mut nodes, mut node_states, mut node_transitions) = (0, 0, 0);
	for node in stats.explicit.iter().filter(|node| !node.lazy) {
		nodes += 1;
		node_states += node.states;
		node_transitions += node.transitions;
	}
	[
		("explored", stats.explored),
		("pruned", stats.pruned),
		("macrostates", stats.macrostates),
		("dropped", stats.dropped),
		("explicit", nodes),
		("explicit-states", node_states),
		("explicit-transitions", node_transitions),
	]
}

/// An automaton imported as a leaf, held weakly: the entry keeps the address of the value from being reused
/// without keeping the automaton alive.
enum Imported {
	Nfa(#[expect(dead_code, reason = "held only to keep the address")] Weak<Nfa>),
	Nft(#[expect(dead_code, reason = "held only to keep the address")] Weak<Nft>),
}

/// The leaf of a value is found by the address of the value and the symbols `:alphabet` adds to it.
type LeafKey = (usize, Option<Vec<lazy::Symbol>>);

/// The formula of a run. Every `Term` of the run is a node of it, so terms of different queries combine; it
/// only grows while the run lasts.
pub(crate) struct Lazy {
	formula: Formula,
	/// The leaf of every automaton value imported so far, so that a value used in many terms is copied once.
	leaves: HashMap<LeafKey, (Imported, Term)>,
	/// The annotations used by every query that has no `:plan` or `:policy`.
	annotations: lazy::Plan,
	/// The statistics of the last query, for `lazy:stats`.
	pub(crate) stats: Option<Stats>,
}

impl Lazy {
	pub(crate) fn new() -> Lazy {
		Lazy {
			formula: Formula::new(),
			leaves: HashMap::new(),
			annotations: lazy::Plan::new(),
			stats: None,
		}
	}

	/// The number of tapes of `term`.
	pub(crate) fn arity(&self, term: Term) -> usize {
		self.formula
			.arity(term)
			.expect("every term of a run belongs to its formula")
	}

	/// The leaf of an automaton value, imported at its first use with `alphabet`.
	fn leaf(&mut self, value: &Value, alphabet: Option<Vec<lazy::Symbol>>) -> Result<Term, Error> {
		enum Automaton<'a> {
			Nfa(&'a Rc<Nfa>),
			Nft(&'a Rc<Nft>),
		}
		let automaton = match value {
			Value::Nfa(nfa) => Automaton::Nfa(nfa),
			Value::Nft(nft) => Automaton::Nft(nft),
			Value::Automaton(kind) => match &**kind {
				AutomatonKind::Nfa(nfa) => Automaton::Nfa(nfa),
				AutomatonKind::Nft(nft) => Automaton::Nft(nft),
			},
			other => {
				return Err(Error::plain(format!(
					"expected an Nfa or an Nft, found {}",
					value_sort(other)
				)));
			}
		};
		let address = match automaton {
			Automaton::Nfa(nfa) => Rc::as_ptr(nfa).addr(),
			Automaton::Nft(nft) => Rc::as_ptr(nft).addr(),
		};
		let key = (address, alphabet);
		if let Some((_, term)) = self.leaves.get(&key) {
			return Ok(*term);
		}
		let formula = &mut self.formula;
		let (imported, term) = match (automaton, &key.1) {
			(Automaton::Nfa(nfa), None) => {
				(Imported::Nfa(Rc::downgrade(nfa)), formula.leaf(&**nfa))
			}
			(Automaton::Nfa(nfa), Some(symbols)) => (
				Imported::Nfa(Rc::downgrade(nfa)),
				formula.leaf_over(&**nfa, [symbols.iter().copied()]),
			),
			(Automaton::Nft(nft), None) => {
				(Imported::Nft(Rc::downgrade(nft)), formula.leaf(&**nft))
			}
			(Automaton::Nft(nft), Some(symbols)) => (
				Imported::Nft(Rc::downgrade(nft)),
				formula.leaf_over(&**nft, [symbols.iter().copied()]),
			),
		};
		let term = term?;
		self.leaves.insert(key, (imported, term));
		Ok(term)
	}

	/// Runs one check of `term`, or a portfolio of its plans, and keeps its statistics for `lazy:stats`.
	fn check(&mut self, term: Term, query: &Query) -> Result<Outcome, Error> {
		self.stats = None;
		let outcome = if query.portfolio.is_empty() {
			self.formula.check(term, &query.options)?
		} else {
			self.formula
				.check_portfolio(term, &query.options, &query.portfolio)?
				.1
		};
		self.stats = Some(outcome.stats.clone());
		Ok(outcome)
	}

	/// `lhs ∩ ¬rhs`, the words of `lhs` that `rhs` lacks.
	fn difference(&mut self, lhs: Term, rhs: Term) -> Result<Term, Error> {
		let complement = self.formula.complement(rhs)?;
		Ok(self.formula.intersection(lhs, complement)?)
	}

	/// The term of an operand of the optimizer, whose leaves are automata.
	fn term_of(&mut self, tree: &Tree<Value>) -> Result<Term, Error> {
		type Join = fn(&mut Formula, Term, Term) -> Result<Term, lazy::Error>;
		let fold = |lazy: &mut Lazy, trees: &[Tree<Value>], join: Join| -> Result<Term, Error> {
			let mut joined = None;
			for tree in trees {
				let term = lazy.term_of(tree)?;
				joined = Some(match joined {
					None => term,
					Some(joined) => join(&mut lazy.formula, joined, term)?,
				});
			}
			Ok(joined.expect("an operation has operands"))
		};
		match tree {
			Tree::Leaf(value) => self.leaf(value, None),
			Tree::Union(trees) => fold(self, trees, Formula::union),
			Tree::Intersection(trees) => fold(self, trees, Formula::intersection),
			Tree::Difference(lhs, rhs) => {
				let (lhs, rhs) = (self.term_of(lhs)?, self.term_of(rhs)?);
				self.difference(lhs, rhs)
			}
			Tree::Complement(tree) => {
				let term = self.term_of(tree)?;
				Ok(self.formula.complement(term)?)
			}
		}
	}
}

/// Serves plan inspection and `lazy:annotate`; these do no automata search.
fn dispatch_planning(
	handler: &str,
	arguments: Vec<Value>,
	engine: &mut Engine,
) -> Result<Value, Error> {
	match handler {
		"lazy_plan" => {
			let [] = take(arguments, "lazy:plan")?;
			Ok(Value::String(engine.lazy.annotations.to_json()))
		}
		"lazy_view" => {
			let [value] = take(arguments, "lazy:view")?;
			let view = engine
				.lazy
				.formula
				.view(term(&value)?)
				.map_err(Error::from)?;
			Ok(Value::String(view.to_json()))
		}
		"lazy_annotate" => {
			let [value] = take(arguments, "lazy:annotate")?;
			annotate(engine, &value)
		}
		_ => unreachable!("the caller names only planning operations"),
	}
}

/// Records one operation's evaluation and operand order for later queries.
fn annotate(engine: &mut Engine, value: &Value) -> Result<Value, Error> {
	let operand = term(value)?;
	let previous = engine
		.lazy
		.annotations
		.annotation(operand)
		.copied()
		.unwrap_or_default();
	let eval = optional::<String>(engine, "eval");
	let max_states = optional::<i64>(engine, "max-states")
		.map(|limit| count(limit, ":max-states"))
		.transpose()?;
	let mut annotation = previous;
	annotation.eval = match eval.as_deref() {
		Some("keep") if max_states.is_none() => lazy::Eval::Keep,
		None if max_states.is_none() => previous.eval,
		Some("lazy") if max_states.is_none() => lazy::Eval::Lazy,
		Some("eager") | None => lazy::Eval::Eager { max_states },
		Some("keep" | "lazy") => {
			return Err(Error::plain(
				":max-states of lazy:annotate requires :eval \"eager\"".into(),
			));
		}
		Some(other) => {
			return Err(Error::plain(format!(
				":eval of lazy:annotate must be keep, lazy or eager, not {other}"
			)));
		}
	};
	if let Some(order) = optional::<String>(engine, "order") {
		annotation.order = match order.as_str() {
			"auto" => lazy::Order::Auto,
			"as-written" => lazy::Order::AsWritten,
			"swapped" => lazy::Order::Swapped,
			_ => {
				return Err(Error::plain(format!(
					":order of lazy:annotate must be auto, as-written or swapped, not {order}"
				)));
			}
		};
	}
	engine.lazy.annotations = engine
		.lazy
		.annotations
		.clone()
		.annotate(operand, annotation);
	Ok(Value::Term(operand))
}

/// Serves the `lazy:` operations.
pub(crate) fn dispatch(
	handler: &str,
	name: &str,
	arguments: Vec<Value>,
	engine: &mut Engine,
) -> Result<Value, Error> {
	match handler {
		"lazy_plan" | "lazy_view" | "lazy_annotate" => {
			dispatch_planning(handler, arguments, engine)
		}
		"lazy_leaf" => {
			let [value] = take(arguments, name)?;
			let alphabet = alphabet(engine)?;
			engine.lazy.leaf(&value, alphabet).map(Value::Term)
		}
		"lazy_union" | "lazy_intersection" | "lazy_compose" | "lazy_post_image"
		| "lazy_pre_image" => {
			let [lhs, rhs] = take(arguments, name)?;
			let (lhs, rhs) = (term(&lhs)?, term(&rhs)?);
			let formula = &mut engine.lazy.formula;
			match handler {
				"lazy_union" => formula.union(lhs, rhs),
				"lazy_intersection" => formula.intersection(lhs, rhs),
				"lazy_compose" => formula.compose(lhs, rhs),
				"lazy_post_image" => formula.post_image(lhs, rhs),
				_ => formula.pre_image(lhs, rhs),
			}
			.map(Value::Term)
			.map_err(Error::from)
		}
		"lazy_complement" | "lazy_identity" | "lazy_prefixes" => {
			let [value] = take(arguments, name)?;
			let operand = term(&value)?;
			let formula = &mut engine.lazy.formula;
			match handler {
				"lazy_complement" => formula.complement(operand),
				"lazy_identity" => formula.identity(operand),
				_ => formula.prefixes(operand),
			}
			.map(Value::Term)
			.map_err(Error::from)
		}
		"lazy_pad_closure" => {
			let mut arguments = arguments.into_iter();
			let operand = term(&arguments.next().expect("at least one argument"))?;
			let pad = arguments
				.map(|value| match value {
					Value::Symbol(symbol) => Ok(lazy::Symbol::from(symbol)),
					other => Err(Error::plain(format!(
						"expected a Symbol, found {}",
						value_sort(&other)
					))),
				})
				.collect::<Result<Vec<_>, _>>()?;
			let pad = if pad.is_empty() {
				lazy::Pad::Last
			} else {
				lazy::Pad::Step(pad.into_boxed_slice())
			};
			engine
				.lazy
				.formula
				.pad_closure(operand, pad)
				.map(Value::Term)
				.map_err(Error::from)
		}
		"lazy_project" => {
			let [value, tapes] = take(arguments, name)?;
			let operand = term(&value)?;
			let tapes = as_list(&tapes)
				.map_err(Error::plain)?
				.iter()
				.map(|tape| count(as_int(tape).map_err(Error::plain)?, "a tape"))
				.collect::<Result<Vec<_>, _>>()?;
			engine
				.lazy
				.formula
				.project(operand, &tapes)
				.map(Value::Term)
				.map_err(Error::from)
		}
		"lazy_explicit" => {
			let [value] = take(arguments, name)?;
			let operand = term(&value)?;
			let max_states = optional::<i64>(engine, "max-states")
				.map(|limit| count(limit, ":max-states"))
				.transpose()?;
			let formula = &mut engine.lazy.formula;
			match max_states {
				Some(limit) => {
					formula.explicit_with(operand, ExplicitOptions::new().max_states(limit))
				}
				None => formula.explicit(operand),
			}
			.map(Value::Term)
			.map_err(Error::from)
		}
		_ => dispatch_query(handler, name, arguments, engine),
	}
}

/// Serves the `lazy:` queries, each one check of the formula.
fn dispatch_query(
	handler: &str,
	name: &str,
	arguments: Vec<Value>,
	engine: &mut Engine,
) -> Result<Value, Error> {
	match handler {
		"lazy_empty" => {
			let [value] = take(arguments, name)?;
			let operand = term(&value)?;
			decide(engine, operand)
		}
		"lazy_empty_within" => {
			let [value] = take(arguments, name)?;
			let operand = term(&value)?;
			let query = query_options(engine, operand)?;
			let outcome = engine.lazy.check(operand, &query)?;
			Ok(Value::Bool(outcome.witness.is_none()))
		}
		"lazy_included" => {
			let [lhs, rhs] = take(arguments, name)?;
			let (lhs, rhs) = (term(&lhs)?, term(&rhs)?);
			let difference = engine.lazy.difference(lhs, rhs)?;
			decide(engine, difference)
		}
		"lazy_equivalent" => {
			let [lhs, rhs] = take(arguments, name)?;
			let (lhs, rhs) = (term(&lhs)?, term(&rhs)?);
			// One check of the symmetric difference, which holds a word of either direction.
			let missing = engine.lazy.difference(lhs, rhs)?;
			let extra = engine.lazy.difference(rhs, lhs)?;
			let either = engine.lazy.formula.union(missing, extra)?;
			decide(engine, either)
		}
		"lazy_witness" => {
			let [value] = take(arguments, name)?;
			let operand = term(&value)?;
			let query = query_options(engine, operand)?;
			let outcome = engine.lazy.check(operand, &query)?;
			Ok(outcome.witness.as_ref().map_or(Value::None, witness_value))
		}
		"lazy_materialize" => {
			let [value] = take(arguments, name)?;
			let operand = term(&value)?;
			let query = query_options(engine, operand)?;
			if !query.portfolio.is_empty() {
				return Err(Error::plain(
					"lazy:materialize builds the whole term, so a portfolio gains nothing".into(),
				));
			}
			let explicit = engine.lazy.formula.materialize(operand, &query.options)?;
			let nft = Nft::try_from(&explicit).map_err(|e| Error::plain(e.to_string()))?;
			Ok(if explicit.arity() == 1 {
				Value::Nfa(Rc::new(nft.to_nfa()))
			} else {
				Value::Nft(Rc::new(nft))
			})
		}
		_ => Err(Error::plain(format!(
			"operation '{name}' is not available in the Rust bindings"
		))),
	}
}

/// Checks `term` for emptiness: `true` when it is empty. The witness, a word of the term, is stored when one is
/// wanted, as the `nfa:` decisions do.
fn decide(engine: &mut Engine, term: Term) -> Result<Value, Error> {
	let query = query_options(engine, term)?;
	let outcome = engine.lazy.check(term, &query)?;
	let wanted = engine.options.produce_witnesses.on() || engine.witness_requested.on();
	engine.last_witness = if wanted {
		outcome.witness.as_ref().map(witness_value)
	} else {
		None
	};
	Ok(Value::Bool(outcome.is_empty()))
}

/// The options of the lazy query of the optimizer that decides `decision` of `operands`. `None` when its
/// answer could differ from the written one: a complement over the script alphabet, which a query cannot
/// restrict itself to, of automata that read symbols outside of it.
pub(crate) fn tree_options(
	engine: &Engine,
	decision: Decision,
	operands: &[Tree<Value>],
) -> Result<Option<Options>, Error> {
	let over_script_alphabet =
		decision == Decision::Universal || operands.iter().any(Tree::has_complement);
	let script: HashSet<mata::Symbol> = engine.script_alphabet.symbols().collect();
	for leaf in operands.iter().flat_map(Tree::leaves) {
		let nfa = expect_nfa(leaf).map_err(Error::plain)?;
		if over_script_alphabet
			&& nfa
				.used_symbols()
				.into_iter()
				.any(|symbol| symbol != mata::Symbol::EPSILON && !script.contains(&symbol))
		{
			return Ok(None);
		}
	}
	let mut options = Options::new();
	if over_script_alphabet {
		options = options.alphabet(script.into_iter().map(lazy::Symbol::from));
	}
	Ok(Some(options))
}

/// Decides `decision` of `operands` as one lazy query with `options`, for the optimizer; the witness is
/// stored as by the written decision.
pub(crate) fn decide_tree(
	engine: &mut Engine,
	decision: Decision,
	operands: &[Tree<Value>],
	options: &Options,
) -> Result<Value, Error> {
	let mut terms = Vec::with_capacity(operands.len());
	for operand in operands {
		terms.push(engine.lazy.term_of(operand)?);
	}
	let lazy = &mut engine.lazy;
	let term = match (decision, terms.as_slice()) {
		(Decision::Empty, &[term]) => term,
		(Decision::Universal, &[term]) => lazy.formula.complement(term)?,
		(Decision::Included, &[lhs, rhs]) => lazy.difference(lhs, rhs)?,
		(Decision::Equivalent, &[lhs, rhs]) => {
			let missing = lazy.difference(lhs, rhs)?;
			let extra = lazy.difference(rhs, lhs)?;
			lazy.formula.union(missing, extra)?
		}
		_ => unreachable!("a decision has as many operands as its operation"),
	};
	let query = Query {
		options: options.clone(),
		portfolio: Vec::new(),
	};
	let outcome = engine.lazy.check(term, &query)?;
	let wanted = engine.options.produce_witnesses.on() || engine.witness_requested.on();
	engine.last_witness = if wanted {
		outcome.witness.as_ref().map(witness_value)
	} else {
		None
	};
	Ok(Value::Bool(outcome.is_empty()))
}

/// A witness as a script value: the `Word` of a language, and a list of one `Word` per tape otherwise.
fn witness_value(witness: &Witness) -> Value {
	let tape = |index| {
		let symbols = witness
			.tape(index)
			.expect("a witness has a word on each of its tapes");
		Value::Word(Word::from(
			symbols
				.into_iter()
				.map(mata::Symbol::from)
				.collect::<Vec<_>>(),
		))
	};
	if witness.arity() == 1 {
		tape(0)
	} else {
		Value::List(Rc::new((0..witness.arity()).map(tape).collect()))
	}
}

/// The options of one lazy query, and the plans of a portfolio (none for a single query).
struct Query {
	options: Options,
	portfolio: Vec<Plan>,
}

/// The options of one lazy query, including the default annotations or an overriding policy or external plan.
fn query_options(engine: &mut Engine, term: Term) -> Result<Query, Error> {
	let options = options(engine)?;
	let external = optional::<String>(engine, "plan");
	let policy = optional::<String>(engine, "policy");
	if external.is_some() && policy.is_some() {
		return Err(Error::plain(":plan and :policy cannot be combined".into()));
	}
	let single = |plan: Plan| Query {
		options: options.clone().plan(plan),
		portfolio: Vec::new(),
	};
	if let Some(json) = external {
		return Ok(single(engine.lazy.formula.plan_from_json(&json)?));
	}
	let Some(policy) = policy else {
		return Ok(single(engine.lazy.annotations.clone()));
	};
	if !matches!(
		policy.as_str(),
		"lazy" | "eager" | "heuristic" | "portfolio"
	) {
		return Err(Error::plain(format!(
			":policy must be lazy, eager, heuristic or portfolio, not {policy}"
		)));
	}
	let view = engine.lazy.formula.view(term)?;
	let heuristic = match optional::<i64>(engine, "heuristic-max-states") {
		Some(limit) => lazy::Heuristic::new().max_states(count(limit, ":heuristic-max-states")?),
		None => lazy::Heuristic::new(),
	};
	Ok(match policy.as_str() {
		"lazy" => single(lazy::AllLazy.plan(&view)),
		"eager" => single(lazy::AllEager.plan(&view)),
		"heuristic" => single(heuristic.plan(&view)),
		_ => Query {
			options,
			portfolio: vec![
				lazy::AllLazy.plan(&view),
				lazy::AllEager.plan(&view),
				heuristic.plan(&view),
			],
		},
	})
}

/// The options of a query: its limits and the symbols it adds to the alphabet of the checked term.
pub(crate) fn options(engine: &mut Engine) -> Result<Options, Error> {
	let mut options = Options::new();
	if let Some(limit) = optional::<i64>(engine, "time-limit") {
		let milliseconds = u64::try_from(limit)
			.map_err(|_| Error::plain(format!(":time-limit must not be negative, not {limit}")))?;
		options = options.time_limit(Duration::from_millis(milliseconds));
	}
	if let Some(limit) = optional::<i64>(engine, "max-states") {
		options = options.max_states(count(limit, ":max-states")?);
	}
	if let Some(limit) = optional::<i64>(engine, "max-length") {
		options = options.max_length(count(limit, ":max-length")?);
	}
	if let Some(shortest) = optional::<bool>(engine, "shortest") {
		options = options.shortest(shortest);
	}
	if let Some(symbols) = alphabet(engine)? {
		options = options.alphabet(symbols);
	}
	Ok(options)
}

/// The symbols of `:alphabet`, names separated by spaces, translated through the script alphabet.
fn alphabet(engine: &mut Engine) -> Result<Option<Vec<lazy::Symbol>>, Error> {
	let Some(names) = optional::<String>(engine, "alphabet") else {
		return Ok(None);
	};
	names
		.split_whitespace()
		.map(|name| engine.intern_name(name).map(lazy::Symbol::from))
		.collect::<Result<Vec<_>, _>>()
		.map(Some)
		.map_err(Error::plain)
}

fn term(value: &Value) -> Result<Term, Error> {
	match value {
		Value::Term(term) => Ok(*term),
		other => Err(Error::plain(format!(
			"expected a Term, found {}",
			value_sort(other)
		))),
	}
}

/// `value` as a count, which must not be negative.
fn count(value: i64, what: &str) -> Result<usize, Error> {
	usize::try_from(value)
		.map_err(|_| Error::plain(format!("{what} must not be negative, not {value}")))
}

impl From<lazy::Error> for Error {
	fn from(error: lazy::Error) -> Error {
		Error::plain(error.to_string())
	}
}

#[cfg(test)]
mod tests {
	use crate::eval::{Engine, Value};

	/// Runs `script` in a fresh engine, which these tests read the values of; unlike the end-to-end tests of the
	/// binary, this runs on every target, WebAssembly included.
	fn run(script: &str) -> Engine {
		let mut engine = Engine::new();
		let forms = crate::ast::parse(script).expect("a script that parses");
		engine.execute(&forms).expect("a script that runs");
		engine
	}

	fn boolean(engine: &Engine, name: &str) -> bool {
		match engine.scope.lookup(name) {
			Some(Value::Bool(value)) => value,
			_ => panic!("'{name}' is not a Bool"),
		}
	}

	/// Regular expressions over `a` and `b` of up to `depth` nested operators, drawn by a fixed xorshift.
	fn regexes(count: usize, depth: u32) -> Vec<String> {
		fn draw(state: &mut u64, depth: u32) -> String {
			*state ^= *state << 13;
			*state ^= *state >> 7;
			*state ^= *state << 17;
			let choice = if depth == 0 { *state % 2 } else { *state % 5 };
			match choice {
				0 => "a".to_owned(),
				1 => "b".to_owned(),
				2 => format!("({})*", draw(state, depth - 1)),
				3 => format!("({}|{})", draw(state, depth - 1), draw(state, depth - 1)),
				_ => format!("{}{}", draw(state, depth - 1), draw(state, depth - 1)),
			}
		}
		let mut state = 0x9e37_79b9_7f4a_7c15;
		(0..count).map(|_| draw(&mut state, depth)).collect()
	}

	#[test]
	fn lazy_decisions_agree_with_the_eager_ones_on_regex_pairs() {
		use std::fmt::Write as _;

		let regexes = regexes(14, 3);
		let mut script = String::from("(set-option :alphabet \"bytes\")\n");
		for (index, regex) in regexes.iter().enumerate() {
			writeln!(
				script,
				"(define r{index} (nfa:create-from-regex \"{regex}\"))\n(define t{index} (lazy:leaf r{index}))"
			)
			.expect("writing to a String cannot fail");
		}
		let mut pairs = Vec::new();
		for lhs in 0..regexes.len() {
			for rhs in 0..regexes.len() {
				write!(
					script,
					"(define included-{lhs}-{rhs} (= (lazy:included? t{lhs} t{rhs}) (nfa:included? r{lhs} r{rhs})))\n\
					 (define equivalent-{lhs}-{rhs} (= (lazy:equivalent? t{lhs} t{rhs}) (nfa:equivalent? r{lhs} r{rhs})))\n\
					 (define disjoint-{lhs}-{rhs} (= (lazy:empty? (lazy:intersection t{lhs} t{rhs})) (nfa:intersection-empty? r{lhs} r{rhs})))\n\
					 (define witness-{lhs}-{rhs} (lazy:witness (lazy:intersection t{lhs} (lazy:complement t{rhs}))))\n\
					 (define explained-{lhs}-{rhs} (if (none? witness-{lhs}-{rhs}) (nfa:included? r{lhs} r{rhs}) \
					 (and (nfa:in-language? r{lhs} witness-{lhs}-{rhs}) (not (nfa:in-language? r{rhs} witness-{lhs}-{rhs})))))\n\
					 (define inclusion-{lhs}-{rhs} (nfa:included? r{lhs} r{rhs}))\n"
				)
				.expect("writing to a String cannot fail");
				pairs.push((lhs, rhs));
			}
		}
		let engine = run(&script);
		let mut verdicts = [0, 0];
		for (lhs, rhs) in pairs {
			for check in ["included", "equivalent", "disjoint", "explained"] {
				let name = format!("{check}-{lhs}-{rhs}");
				assert!(
					boolean(&engine, &name),
					"{name}: {} vs {}",
					regexes[lhs],
					regexes[rhs]
				);
			}
			verdicts[usize::from(boolean(&engine, &format!("inclusion-{lhs}-{rhs}")))] += 1;
		}
		assert!(
			verdicts.iter().all(|&count| count > 0),
			"both verdicts occur: {verdicts:?}"
		);
	}

	/// One step of token passing on a line of processes: the token moves one place to the right.
	const STEP: &str = "@NFT-explicit\n%Alphabet-auto\n%Initial s0\n%Final s2\n\
		%Levels s0:0 s1:0 s2:0 m0:1 m1:1 m2:1 m3:1\n%LevelsNum 2\n\
		s0 n m0\nm0 n s0\ns0 t m1\nm1 n s1\ns1 n m2\nm2 t s2\ns2 n m3\nm3 n s2\n";

	#[test]
	fn images_of_a_transducer_agree_with_the_eager_application() {
		let engine = run(&format!(
			"(set-option :alphabet \"bytes\")\n\
			 (define step (nft:parse \"{STEP}\"))\n\
			 (define one-token (nfa:create-from-regex \"n*tn*\"))\n\
			 (define post (lazy:post-image (lazy:leaf one-token) (lazy:leaf step)))\n\
			 (define eager-post (nft:to-nfa (nft:apply step one-token)))\n\
			 (define post-agrees (nfa:equivalent? (lazy:materialize post) eager-post))\n\
			 (define pre-agrees (lazy:equivalent? (lazy:pre-image (lazy:leaf one-token) (lazy:leaf step)) \
			 (lazy:leaf (nft:to-nfa (nft:apply (nft:invert-levels step) one-token)))))\n\
			 (define inductive (lazy:included? post (lazy:leaf one-token)))\n\
			 (define pair (lazy:witness (lazy:leaf step)))"
		));
		assert!(boolean(&engine, "post-agrees"));
		assert!(boolean(&engine, "pre-agrees"));
		assert!(boolean(&engine, "inductive"));
		// A witness of a relation is one word per tape, here a pair of the relation: the token moves right.
		let Some(Value::List(pair)) = engine.scope.lookup("pair") else {
			panic!("a witness of a relation is a list");
		};
		let words: Vec<Vec<String>> = pair
			.iter()
			.map(|word| match word {
				Value::Word(word) => engine
					.script_alphabet
					.display_word(word)
					.to_string()
					.split(' ')
					.map(str::to_owned)
					.collect(),
				_ => panic!("a witness tape is a Word"),
			})
			.collect();
		let token = |word: &[String]| word.iter().position(|symbol| symbol == "t");
		assert_eq!(words.len(), 2);
		assert_eq!(words[0].len(), words[1].len(), "{words:?}");
		let place = token(&words[0]).expect("tape 0 holds the token");
		assert_eq!(token(&words[1]), Some(place + 1), "{words:?}");
	}

	#[test]
	fn a_value_is_imported_as_one_leaf() {
		let engine = run("(define a (nfa:create-from-regex \"a*\"))\n\
			 (define b (nfa:create-from-regex \"a*\"))\n\
			 (define same-value (= (lazy:leaf a) (lazy:leaf a)))\n\
			 (define same-language (= (lazy:leaf a) (lazy:leaf b)))\n\
			 (define other-alphabet (= (lazy:leaf a) (lazy:leaf a :alphabet \"b\")))");
		assert!(boolean(&engine, "same-value"));
		assert!(!boolean(&engine, "same-language"));
		assert!(!boolean(&engine, "other-alphabet"));
	}
}
