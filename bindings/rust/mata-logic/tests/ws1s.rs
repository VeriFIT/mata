//! WS1S formulas, parsed and decided, against their meaning.

use std::fmt::Write as _;

use mata_lazy::{Automaton, Options, Symbol};
use mata_logic::ws1s::{Assignment, Value, Ws1s};
use mata_logic::{Error, ONE, Prop, ZERO};
use rand::rngs::StdRng;
use rand::{RngExt, SeedableRng};

const WRITE: &str = "a String takes any text";

fn parse(ws1s: &mut Ws1s, text: &str) -> Prop {
	ws1s.parse(text)
		.unwrap_or_else(|error| panic!("`{text}`: {error}"))
}

fn valid(text: &str) -> Result<bool, Error> {
	let mut ws1s = Ws1s::new();
	let prop = parse(&mut ws1s, text);
	ws1s.valid(&prop, &Options::new())
}

fn satisfiable(text: &str) -> Result<bool, Error> {
	let mut ws1s = Ws1s::new();
	let prop = parse(&mut ws1s, text);
	ws1s.satisfiable(&prop, &Options::new())
}

#[test]
fn the_notebook_formula_is_valid() -> Result<(), Error> {
	assert!(valid("(x = y + 1 & y = z + 1) => x = z + 2")?);
	assert!(!satisfiable("~((x = y + 1 & y = z + 1) => x = z + 2)")?);
	assert!(valid("ex1 y: (x = y + 1 & y = z + 1) => x = z + 2")?);
	assert!(!valid("(x = y + 1 & y = z + 1) => x = z + 1")?);
	Ok(())
}

#[test]
fn sentences_have_the_ws1s_meaning() -> Result<(), Error> {
	// Positions are unbounded: there is no last one, and every one has a successor.
	assert!(valid("all1 x: ex1 y: x < y")?);
	assert!(!valid("ex1 x: all1 y: y <= x")?);
	assert!(valid("all1 x: x = 0 | ex1 y: x = y + 1")?);
	assert!(valid("ex1 x: x = 1000")?);
	assert!(!satisfiable("~ex1 x: x = 1000")?);
	// Sets are finite: each has a bound, and each nonempty one a least and a greatest element.
	assert!(valid("all2 X: ex1 x: all1 y: y in X => y < x")?);
	assert!(valid(
		"all2 X: X = empty | ex1 x, y: x in X & y in X & all1 z: z in X => x <= z & z <= y"
	)?);
	assert!(!satisfiable(
		"ex2 X: 0 in X & all1 x: x in X => x + 1 in X"
	)?);
	// Every set of positions below 3 is one of 2^3, and so is its complement up to 3.
	assert!(valid(
		"all2 X: (all1 x: x in X => x < 3) => ex2 Y: all1 x: x < 3 => (x in X <=> ~x in Y)"
	)?);
	Ok(())
}

#[test]
fn a_sentence_is_decided_without_an_assignment() -> Result<(), Error> {
	let mut ws1s = Ws1s::new();
	let prop = parse(&mut ws1s, "ex1 x: all1 y: y <= x");
	assert_eq!(prop.vars(), []);
	let counterexample = ws1s.counterexample(&prop, &Options::new())?;
	assert_eq!(
		counterexample.map(|assignment| assignment.to_string()),
		Some(String::new())
	);
	Ok(())
}

#[test]
fn a_model_decodes_to_positions_and_sets() -> Result<(), Error> {
	let mut ws1s = Ws1s::new();
	let prop = parse(
		&mut ws1s,
		"x = 3 & X = Y & 1 in X & 4 in Y & all1 z: z in X => z = 1 | z = 4",
	);
	let model = ws1s.model(&prop, &Options::new())?.expect("satisfiable");
	assert_eq!(model.get("x"), Some(&Value::Position(3)));
	assert_eq!(model.get("X"), Some(&Value::Set(vec![1, 4])));
	assert_eq!(model.get("z"), None);
	assert_eq!(model.to_string(), "x = 3, X = {1, 4}, Y = {1, 4}");
	Ok(())
}

#[test]
fn a_counterexample_falsifies_the_formula() -> Result<(), Error> {
	let mut ws1s = Ws1s::new();
	let prop = parse(&mut ws1s, "x < y | y + 2 < x");
	let counterexample = ws1s
		.counterexample(&prop, &Options::new())?
		.expect("not valid");
	let (Some(&Value::Position(x)), Some(&Value::Position(y))) =
		(counterexample.get("x"), counterexample.get("y"))
	else {
		panic!("positions: {counterexample}");
	};
	assert!(y <= x && x <= y + 2, "{counterexample}");
	Ok(())
}

#[test]
fn parses_share_free_variables() -> Result<(), Error> {
	let mut ws1s = Ws1s::new();
	let three = parse(&mut ws1s, "x = 3");
	let small = parse(&mut ws1s, "x < 2");
	let both = ws1s.logic_mut().and(&three, &small)?;
	assert!(!ws1s.satisfiable(&both, &Options::new())?);
	Ok(())
}

#[test]
fn a_binding_hides_the_free_variable() -> Result<(), Error> {
	let mut ws1s = Ws1s::new();
	let prop = parse(&mut ws1s, "x = 5 & ex1 x: x = 1 & ex1 x: x = 2");
	let model = ws1s.model(&prop, &Options::new())?.expect("satisfiable");
	assert_eq!(model.to_string(), "x = 5");
	Ok(())
}

#[test]
fn operators_bind_as_in_mona() -> Result<(), Error> {
	// `&` before `|`.
	assert!(valid("(x = 0 | x = 1 & x = 2) <=> x = 0")?);
	// `~` before `&`.
	assert!(satisfiable("~x = 0 & x = 1")?);
	// `=>` is right associative: `x = 0 => (x = 1 => x = 2)` holds, `(x = 0 => x = 1) => x = 2` does not.
	assert!(valid("x = 0 => x = 1 => x = 2")?);
	// `<=>` is the loosest, and left associative.
	assert!(valid("x = 0 <=> x = 0 & true")?);
	assert!(valid(
		"(x = 0 <=> x = 1 <=> x = 2) <=> ((x = 0 <=> x = 1) <=> x = 2)"
	)?);
	// A quantifier reaches to the end.
	assert!(valid("(x = 1 & ex1 y: y = x | x = 0) <=> x = 1")?);
	assert!(valid("(~ex1 y: y < x & y = 0) <=> x = 0")?);
	Ok(())
}

#[test]
fn relations_take_constants_on_both_sides() -> Result<(), Error> {
	assert!(valid("(3 = x + 1) <=> x = 2")?);
	assert!(valid("(2 < x + 1) <=> 1 < x")?);
	assert!(valid("(x + 2 <= 3) <=> x <= 1")?);
	assert!(valid("(x + 3 < y + 1) <=> x + 2 < y")?);
	assert!(valid("(2 in X) <=> ex1 x: x = 2 & x in X")?);
	assert!(valid("x + 0 = x & ~x < x & x <= x & 1 < 2 & ~3 <= 2")?);
	assert!(valid(
		"~x in empty & empty sub X & X sub X & X = X & (X sub empty <=> X = empty)"
	)?);
	assert!(!satisfiable("x + 1 = 0")?);
	Ok(())
}

#[test]
fn malformed_text_is_rejected_where_it_goes_wrong() {
	let cases = [
		("x <", 3, "expected a formula"),
		("(x = 0", 0, "unclosed `(`"),
		("x = 0)", 5, "unmatched `)`"),
		("()", 1, "expected a formula"),
		(
			"x = 0 x",
			6,
			"expected `&`, `|`, `=>`, `<=>`, `)` or the end",
		),
		("x # y", 2, "unexpected character `#`"),
		("x = 99999999999", 4, "the number does not fit 32 bits"),
		(
			"ex1 X: true",
			4,
			"`ex1` binds positions, whose names start with a lowercase letter",
		),
		(
			"all2 x: true",
			5,
			"`all2` binds sets, whose names start with an uppercase letter",
		),
		("ex1 x, x: true", 7, "`x` is bound twice"),
		("ex1 in: true", 4, "`in` is a keyword, not a variable"),
		("ex1 x true", 6, "expected `,` or `:`"),
		("x in y", 5, "expected a set"),
		("X < Y", 0, "expected a position"),
		("x sub X", 0, "expected a set"),
		("x = X", 4, "expected a position"),
		("x + y = 0", 4, "expected a number"),
		("x X", 2, "expected `=`, `<`, `<=`, `in` or `sub`"),
		("", 0, "expected a formula"),
	];
	for (text, offset, message) in cases {
		let mut ws1s = Ws1s::new();
		assert_eq!(
			ws1s.parse(text),
			Err(Error::Parse {
				offset,
				message: message.into()
			}),
			"`{text}`"
		);
	}
}

#[test]
fn nesting_is_limited_but_chains_are_balanced() -> Result<(), Error> {
	let mut ws1s = Ws1s::new();
	let deepest = format!("{}x = 0", "~".repeat(999));
	let prop = parse(&mut ws1s, &deepest);
	assert!(ws1s.satisfiable(&prop, &Options::new())?);

	let deeper = format!("{}x = 0", "~".repeat(1000));
	assert!(matches!(
		ws1s.parse(&deeper),
		Err(Error::Parse { offset: 0, .. })
	));

	let parentheses = format!("{}x = 0{}", "(~".repeat(1000), ")".repeat(1000));
	assert!(matches!(ws1s.parse(&parentheses), Err(Error::Parse { .. })));

	let chain = vec!["x <= 4"; 4000].join(" & ");
	let prop = parse(&mut ws1s, &chain);
	assert!(ws1s.satisfiable(&prop, &Options::new())?);

	// Near the limit, with connectives that the engine cannot fold: 4 levels per round.
	let mut alternating = String::from("x = 0");
	for round in 0..240 {
		alternating = format!(
			"x = {} | ~(x = {} & ~({alternating}))",
			round % 5,
			(round + 1) % 5
		);
	}
	let prop = parse(&mut ws1s, &alternating);
	assert!(ws1s.satisfiable(&prop, &Options::new())?);

	// 2 levels per quantifier.
	let mut quantified = String::from("y < x");
	for _ in 0..450 {
		quantified = format!("all1 y: y = x + 1 => ({quantified})");
	}
	let prop = parse(&mut ws1s, &quantified);
	assert!(!ws1s.satisfiable(&prop, &Options::new())?);
	Ok(())
}

// Differential tests: random formulas with bounded quantifiers, whose meaning is computed by enumeration.

/// The free first-order variables, the free second-order one, and the names that quantifiers bind.
const FREE_POSITIONS: [&str; 2] = ["x", "y"];
const FREE_SET: &str = "X";
const BOUND_POSITIONS: [&str; 2] = ["u", "v"];
const BOUND_SET: &str = "U";
/// The positions of the free variables in the enumeration, and the longest word compared.
const LENGTH: usize = 4;

#[derive(Clone, Copy, Debug)]
enum Rel {
	Eq,
	Less,
	LessEq,
}

#[derive(Clone, Copy, Debug)]
enum PosTerm {
	Var(&'static str, u32),
	Number(u32),
}

#[derive(Clone, Copy, Debug)]
enum SetTerm {
	Var(&'static str),
	Empty,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum Op {
	Iff,
	Implies,
	Or,
	And,
}

/// The values that a quantifier tries: positions up to a bound, or the subsets of a set.
#[derive(Clone, Copy, Debug)]
enum Range {
	Upto(PosTerm),
	Subsets(&'static str),
}

#[derive(Clone, Debug)]
enum Tree {
	Bool(bool),
	Compare(Rel, PosTerm, PosTerm),
	In(PosTerm, SetTerm),
	Sub(SetTerm, SetTerm),
	SetEq(SetTerm, SetTerm),
	Not(Box<Tree>),
	Binary(Op, Box<Tree>, Box<Tree>),
	/// `body` is `guard & φ`, or `guard => φ` if universal, where `guard` limits the variable to `range`.
	Quantifier {
		universal: bool,
		var: &'static str,
		range: Range,
		body: Box<Tree>,
	},
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum Val {
	Position(u32),
	Set(u32),
}

/// The names in scope, innermost last.
#[derive(Clone, Default)]
struct Scope {
	positions: Vec<&'static str>,
	sets: Vec<&'static str>,
}

impl Tree {
	fn random(rng: &mut StdRng, depth: u32, scope: &Scope) -> Tree {
		let pos_term = |rng: &mut StdRng| {
			if rng.random_bool(0.2) {
				PosTerm::Number(rng.random_range(0..=4))
			} else {
				let var = scope.positions[rng.random_range(0..scope.positions.len())];
				let k = if rng.random_bool(0.6) {
					0
				} else {
					rng.random_range(1..=2)
				};
				PosTerm::Var(var, k)
			}
		};
		let set_term = |rng: &mut StdRng| {
			if rng.random_bool(0.1) {
				SetTerm::Empty
			} else {
				SetTerm::Var(scope.sets[rng.random_range(0..scope.sets.len())])
			}
		};

		let kinds = if depth == 0 { 5 } else { 9 };
		match rng.random_range(0..kinds) {
			0 if rng.random_bool(0.3) => Tree::Bool(rng.random_bool(0.5)),
			0 | 1 => {
				let rel = [Rel::Eq, Rel::Less, Rel::LessEq][rng.random_range(0..3)];
				Tree::Compare(rel, pos_term(rng), pos_term(rng))
			}
			2 => Tree::In(pos_term(rng), set_term(rng)),
			3 => Tree::Sub(set_term(rng), set_term(rng)),
			4 => Tree::SetEq(set_term(rng), set_term(rng)),
			5 => Tree::Not(Box::new(Tree::random(rng, depth - 1, scope))),
			6 | 7 => {
				let op = [Op::Iff, Op::Implies, Op::Or, Op::And][rng.random_range(0..4)];
				Tree::Binary(
					op,
					Box::new(Tree::random(rng, depth - 1, scope)),
					Box::new(Tree::random(rng, depth - 1, scope)),
				)
			}
			_ => {
				let universal = rng.random_bool(0.5);
				let mut inner = scope.clone();
				let (var, range, guard) = if rng.random_bool(0.7) {
					let var = BOUND_POSITIONS[rng.random_range(0..BOUND_POSITIONS.len())];
					let bound = if rng.random_bool(0.2) {
						PosTerm::Number(rng.random_range(0..=5))
					} else {
						let base = scope.positions[rng.random_range(0..scope.positions.len())];
						PosTerm::Var(base, rng.random_range(0..=2))
					};
					inner.positions.push(var);
					let guard = Tree::Compare(Rel::LessEq, PosTerm::Var(var, 0), bound);
					(var, Range::Upto(bound), guard)
				} else {
					let outer = scope.sets[rng.random_range(0..scope.sets.len())];
					inner.sets.push(BOUND_SET);
					let guard = Tree::Sub(SetTerm::Var(BOUND_SET), SetTerm::Var(outer));
					(BOUND_SET, Range::Subsets(outer), guard)
				};
				let op = if universal { Op::Implies } else { Op::And };
				let body = Tree::random(rng, depth - 1, &inner);
				Tree::Quantifier {
					universal,
					var,
					range,
					body: Box::new(Tree::Binary(op, Box::new(guard), Box::new(body))),
				}
			}
		}
	}

	fn precedence(&self) -> u8 {
		match self {
			Tree::Quantifier { .. } => 0,
			Tree::Binary(Op::Iff, ..) => 1,
			Tree::Binary(Op::Implies, ..) => 2,
			Tree::Binary(Op::Or, ..) => 3,
			Tree::Binary(Op::And, ..) => 4,
			Tree::Not(_) => 5,
			_ => 6,
		}
	}

	/// The text of the formula, with parentheses around every connective if `full`, else only where needed.
	fn text(&self, full: bool) -> String {
		let mut out = String::new();
		self.write(&mut out, full, 0, true);
		out
	}

	/// Writes the formula as an operand that binds at least as tight as `context`; `last` if no text follows.
	fn write(&self, out: &mut String, full: bool, context: u8, last: bool) {
		let precedence = self.precedence();
		let parenthesize = precedence < context
			|| (precedence == 0 && !last)
			|| (full && precedence < 6 && context > 0);
		if parenthesize {
			out.push('(');
			self.write(out, full, 0, true);
			out.push(')');
			return;
		}

		match self {
			Tree::Bool(value) => out.push_str(if *value { "true" } else { "false" }),
			Tree::Compare(rel, lhs, rhs) => {
				let rel = match rel {
					Rel::Eq => "=",
					Rel::Less => "<",
					Rel::LessEq => "<=",
				};
				write!(out, "{} {rel} {}", pos_text(*lhs), pos_text(*rhs)).expect(WRITE);
			}
			Tree::In(lhs, rhs) => {
				write!(out, "{} in {}", pos_text(*lhs), set_text(*rhs)).expect(WRITE);
			}
			Tree::Sub(lhs, rhs) => {
				write!(out, "{} sub {}", set_text(*lhs), set_text(*rhs)).expect(WRITE);
			}
			Tree::SetEq(lhs, rhs) => {
				write!(out, "{} = {}", set_text(*lhs), set_text(*rhs)).expect(WRITE);
			}
			Tree::Not(operand) => {
				out.push('~');
				operand.write(out, full, 5, last);
			}
			Tree::Binary(op, lhs, rhs) => {
				let (symbol, left, right) = match op {
					Op::Iff => ("<=>", 1, 2),
					Op::Implies => ("=>", 3, 2),
					Op::Or => ("|", 3, 3),
					Op::And => ("&", 4, 4),
				};
				lhs.write(out, full, left, false);
				write!(out, " {symbol} ").expect(WRITE);
				rhs.write(out, full, right, last);
			}
			Tree::Quantifier {
				universal,
				var,
				body,
				..
			} => {
				let order = if *var == BOUND_SET { 2 } else { 1 };
				let keyword = if *universal { "all" } else { "ex" };
				write!(out, "{keyword}{order} {var}: ").expect(WRITE);
				body.write(out, full, 0, last);
			}
		}
	}

	/// The value under `env`, the innermost binding of a name last.
	fn eval(&self, env: &mut Vec<(&'static str, Val)>) -> bool {
		let lookup = |env: &[(&str, Val)], name: &str| {
			env.iter()
				.rev()
				.find(|(bound, _)| *bound == name)
				.map(|&(_, value)| value)
				.expect("a variable in scope")
		};
		let position = |env: &[(&str, Val)], term: PosTerm| match term {
			PosTerm::Var(name, k) => match lookup(env, name) {
				Val::Position(position) => position + k,
				Val::Set(_) => unreachable!("a position"),
			},
			PosTerm::Number(k) => k,
		};
		let set = |env: &[(&str, Val)], term: SetTerm| match term {
			SetTerm::Var(name) => match lookup(env, name) {
				Val::Set(set) => set,
				Val::Position(_) => unreachable!("a set"),
			},
			SetTerm::Empty => 0,
		};

		match self {
			Tree::Bool(value) => *value,
			Tree::Compare(rel, lhs, rhs) => {
				let (lhs, rhs) = (position(env, *lhs), position(env, *rhs));
				match rel {
					Rel::Eq => lhs == rhs,
					Rel::Less => lhs < rhs,
					Rel::LessEq => lhs <= rhs,
				}
			}
			Tree::In(lhs, rhs) => {
				let position = position(env, *lhs);
				set(env, *rhs).checked_shr(position).unwrap_or(0) & 1 == 1
			}
			Tree::Sub(lhs, rhs) => set(env, *lhs) & !set(env, *rhs) == 0,
			Tree::SetEq(lhs, rhs) => set(env, *lhs) == set(env, *rhs),
			Tree::Not(operand) => !operand.eval(env),
			Tree::Binary(op, lhs, rhs) => {
				let (lhs, rhs) = (lhs.eval(env), rhs.eval(env));
				match op {
					Op::Iff => lhs == rhs,
					Op::Implies => !lhs || rhs,
					Op::Or => lhs || rhs,
					Op::And => lhs && rhs,
				}
			}
			Tree::Quantifier {
				universal,
				var,
				range,
				body,
			} => {
				let values: Vec<Val> = match *range {
					Range::Upto(bound) => (0..=position(env, bound)).map(Val::Position).collect(),
					Range::Subsets(outer) => {
						let outer = set(env, SetTerm::Var(outer));
						(0..=outer)
							.filter(|subset| subset & !outer == 0)
							.map(Val::Set)
							.collect()
					}
				};
				let mut holds = |value| {
					env.push((var, value));
					let holds = body.eval(env);
					env.pop();
					holds
				};
				if *universal {
					values.into_iter().all(&mut holds)
				} else {
					values.into_iter().any(&mut holds)
				}
			}
		}
	}
}

fn pos_text(term: PosTerm) -> String {
	match term {
		PosTerm::Var(name, 0) => name.to_owned(),
		PosTerm::Var(name, k) => format!("{name} + {k}"),
		PosTerm::Number(k) => k.to_string(),
	}
}

fn set_text(term: SetTerm) -> String {
	match term {
		SetTerm::Var(name) => name.to_owned(),
		SetTerm::Empty => "empty".to_owned(),
	}
}

fn is_position(name: &str) -> bool {
	name.starts_with(|c: char| c.is_ascii_lowercase())
}

/// The words of at most [`LENGTH`] columns over the tapes `names` whose first-order tapes read one 1 each,
/// those whose assignment satisfies `tree` if `tree` is given. A state is the length and the bits read, the bit
/// of tape `t` at step `i` at `i·tapes + t`.
struct Bounded<'a> {
	names: Vec<&'static str>,
	tree: Option<&'a Tree>,
}

impl Bounded<'_> {
	fn assignment(&self, (length, bits): (usize, u32)) -> Option<Vec<(&'static str, Val)>> {
		let tapes = self.names.len();
		let mut env = Vec::new();
		// Free variables the prop dropped are unconstrained: their value must not matter.
		env.extend(FREE_POSITIONS.map(|name| (name, Val::Position(0))));
		env.push((FREE_SET, Val::Set(0)));
		for (tape, &name) in self.names.iter().enumerate() {
			let set = (0..length)
				.filter(|step| bits >> (step * tapes + tape) & 1 == 1)
				.fold(0_u32, |set, step| set | 1 << step);
			let value = if is_position(name) {
				if set.count_ones() != 1 {
					return None;
				}
				Val::Position(set.trailing_zeros())
			} else {
				Val::Set(set)
			};
			env.push((name, value));
		}
		Some(env)
	}
}

impl Automaton for Bounded<'_> {
	type State = (usize, u32);

	fn arity(&self) -> usize {
		self.names.len()
	}

	fn initial_states(&self) -> impl IntoIterator<Item = (usize, u32)> {
		[(0, 0)]
	}

	fn is_final(&self, state: (usize, u32)) -> bool {
		self.assignment(state)
			.is_some_and(|mut env| self.tree.is_none_or(|tree| tree.eval(&mut env)))
	}

	fn for_each_move(
		&self,
		(length, bits): (usize, u32),
		mut visit: impl FnMut(&[Symbol], (usize, u32)),
	) {
		if length == LENGTH {
			return;
		}

		let tapes = self.names.len();
		for column in 0_u32..1 << tapes {
			let symbols: Vec<Symbol> = (0..tapes)
				.map(|tape| if column >> tape & 1 == 1 { ONE } else { ZERO })
				.collect();
			visit(&symbols, (length + 1, bits | column << (length * tapes)));
		}
	}
}

fn env_of(assignment: &Assignment) -> Vec<(&'static str, Val)> {
	let mut env: Vec<(&'static str, Val)> = FREE_POSITIONS
		.map(|name| (name, Val::Position(0)))
		.into_iter()
		.chain([(FREE_SET, Val::Set(0))])
		.collect();
	for (name, value) in assignment.iter() {
		let name = *FREE_POSITIONS
			.iter()
			.chain([&FREE_SET])
			.find(|&&free| free == name)
			.unwrap_or_else(|| panic!("{name} is not free"));
		let value = match value {
			Value::Position(position) => Val::Position(u32::try_from(*position).expect("small")),
			Value::Set(positions) => Val::Set(positions.iter().fold(0, |set, &position| {
				let position = u32::try_from(position).expect("small");
				set | 1_u32
					.checked_shl(position)
					.expect("a set of small positions")
			})),
		};
		env.push((name, value));
	}
	env
}

/// Every random formula holds exactly the encodings of its models of at most [`LENGTH`] columns, and its
/// models and counterexamples are ones.
#[test]
fn random_formulas_hold_the_encodings_of_their_models() -> Result<(), Error> {
	let mut rng = StdRng::seed_from_u64(1);
	let scope = Scope {
		positions: FREE_POSITIONS.to_vec(),
		sets: vec![FREE_SET],
	};
	let rounds = if cfg!(target_os = "emscripten") {
		60
	} else {
		300
	};
	for round in 0..rounds {
		let tree = Tree::random(&mut rng, 3, &scope);
		let text = tree.text(round % 2 == 0);
		let mut ws1s = Ws1s::new();
		let prop = parse(&mut ws1s, &text);
		let names: Vec<&'static str> = prop
			.vars()
			.iter()
			.map(|&var| {
				let name = ws1s.logic().name(var).expect("a variable");
				*FREE_POSITIONS
					.iter()
					.chain([&FREE_SET])
					.find(|&&free| free == name)
					.expect("a free name")
			})
			.collect();

		// The encodings of the models, and the words that encode an assignment.
		let models = Bounded {
			names: names.clone(),
			tree: Some(&tree),
		};
		let assignments = Bounded { names, tree: None };
		let logic = ws1s.logic_mut();
		let models = logic.atom(&models, prop.vars())?;
		let assignments = logic.atom(&assignments, prop.vars())?;
		let not_models = logic.not(&models)?;
		let not_prop = logic.not(&prop)?;
		let missed = logic.and(&models, &not_prop)?;
		let extra = logic.and(&prop, &not_models)?;
		let extra = logic.and(&extra, &assignments)?;
		for (what, difference) in [("misses", missed), ("holds more than", extra)] {
			let word = logic.model(&difference, &Options::new())?;
			assert!(word.is_none(), "`{text}` {what} its models: {word:?}");
		}

		let some_model = logic.model(&models, &Options::new())?.is_some();
		match ws1s.model(&prop, &Options::new())? {
			Some(model) => assert!(tree.eval(&mut env_of(&model)), "`{text}`: {model}"),
			None => assert!(!some_model, "`{text}` has a model"),
		}
		if let Some(counterexample) = ws1s.counterexample(&prop, &Options::new())? {
			assert!(
				!tree.eval(&mut env_of(&counterexample)),
				"`{text}`: {counterexample}"
			);
		}
	}
	Ok(())
}
