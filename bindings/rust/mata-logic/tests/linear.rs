//! Linear constraints over natural numbers and integers, against their values.

use mata_lazy::{Automaton, Options, Symbol};
use mata_logic::{Error, Logic, Model, ONE, Padding, Prop, Var, ZERO};
use rand::rngs::StdRng;
use rand::{RngExt, SeedableRng};

#[derive(Clone, Copy, Debug)]
enum Relation {
	Equal,
	AtMost,
}

/// `Σ coefficients[i]·x_i = constant` or `≤ constant`, least significant bit first: a state is the constant that
/// the bits not read yet must reach.
struct Linear {
	padding: Padding,
	coefficients: Vec<i64>,
	relation: Relation,
	constant: i64,
}

/// A state of [`Linear`]; `Done` follows the sign column of an integer.
#[derive(Clone, Copy, PartialEq, Eq, Hash)]
enum Carry {
	Rest(i64),
	Done,
}

impl Automaton for Linear {
	type State = Carry;

	fn arity(&self) -> usize {
		self.coefficients.len()
	}

	fn initial_states(&self) -> impl IntoIterator<Item = Carry> {
		[Carry::Rest(self.constant)]
	}

	fn is_final(&self, carry: Carry) -> bool {
		match (self.padding, carry) {
			(Padding::Sign, carry) => carry == Carry::Done,
			(Padding::Zero, Carry::Rest(rest)) => match self.relation {
				Relation::Equal => rest == 0,
				Relation::AtMost => rest >= 0,
			},
			(Padding::Zero, Carry::Done) => unreachable!("natural numbers have no sign"),
		}
	}

	fn for_each_move(&self, carry: Carry, mut visit: impl FnMut(&[Symbol], Carry)) {
		let Carry::Rest(rest) = carry else {
			return;
		};

		let tapes = self.coefficients.len();
		for column in 0..1_u32 << tapes {
			let bits: Vec<Symbol> = (0..tapes)
				.map(|tape| if column >> tape & 1 == 1 { ONE } else { ZERO })
				.collect();
			let sum: i64 = (0..tapes)
				.filter(|&tape| column >> tape & 1 == 1)
				.map(|tape| self.coefficients[tape])
				.sum();
			match self.relation {
				Relation::Equal if (rest - sum) % 2 == 0 => {
					visit(&bits, Carry::Rest((rest - sum) / 2));
				}
				Relation::Equal => {}
				Relation::AtMost => visit(&bits, Carry::Rest((rest - sum).div_euclid(2))),
			}

			// The sign bit weighs −2^(n−1): the sum of the rest is −sum.
			let last = match self.relation {
				Relation::Equal => rest + sum == 0,
				Relation::AtMost => rest + sum >= 0,
			};
			if self.padding == Padding::Sign && last {
				visit(&bits, Carry::Done);
			}
		}
	}
}

/// The tuple of words `words`, one per tape, of equal length.
struct Words(Vec<Vec<bool>>);

impl Automaton for Words {
	type State = usize;

	fn arity(&self) -> usize {
		self.0.len()
	}

	fn initial_states(&self) -> impl IntoIterator<Item = usize> {
		[0]
	}

	fn is_final(&self, read: usize) -> bool {
		self.0.iter().all(|word| word.len() == read)
	}

	fn for_each_move(&self, read: usize, mut visit: impl FnMut(&[Symbol], usize)) {
		let column: Option<Vec<Symbol>> = self
			.0
			.iter()
			.map(|word| word.get(read).map(|&bit| if bit { ONE } else { ZERO }))
			.collect();
		if let Some(column) = column {
			visit(&column, read + 1);
		}
	}
}

fn value(padding: Padding, bits: &[bool]) -> i64 {
	let magnitude: i64 = bits
		.iter()
		.enumerate()
		.filter(|&(_, &bit)| bit)
		.map(|(position, _)| 1 << position)
		.sum();
	match (padding, bits.last()) {
		(Padding::Sign, Some(true)) => magnitude - (1 << bits.len()),
		_ => magnitude,
	}
}

/// Builds `Σ terms = constant` (or `≤`), the terms as (coefficient, variable), a variable at most once.
fn linear(
	logic: &mut Logic,
	terms: &[(i64, Var)],
	relation: Relation,
	constant: i64,
) -> Result<Prop, Error> {
	if terms.is_empty() {
		let holds = match relation {
			Relation::Equal => constant == 0,
			Relation::AtMost => constant >= 0,
		};
		return logic.constant(holds);
	}

	let atom = Linear {
		padding: logic.padding(),
		coefficients: terms.iter().map(|&(coefficient, _)| coefficient).collect(),
		relation,
		constant,
	};
	let vars: Vec<Var> = terms.iter().map(|&(_, var)| var).collect();
	logic.atom(&atom, &vars)
}

/// Whether `prop` holds the words `words` of its free variables.
fn holds(logic: &mut Logic, prop: &Prop, words: &[Vec<bool>]) -> Result<bool, Error> {
	let word = logic.atom(&Words(words.to_vec()), prop.vars())?;
	let both = logic.and(prop, &word)?;
	Ok(logic.model(&both, &Options::new())?.is_some())
}

/// Every tuple of `tapes` words of `length` bits.
fn tuples(tapes: usize, length: usize) -> impl Iterator<Item = Vec<Vec<bool>>> {
	(0..1_u64 << (tapes * length)).map(move |code| {
		(0..tapes)
			.map(|tape| {
				(0..length)
					.map(|bit| code >> (tape * length + bit) & 1 == 1)
					.collect()
			})
			.collect()
	})
}

fn lengths(padding: Padding, max: usize) -> std::ops::RangeInclusive<usize> {
	match padding {
		Padding::Zero => 0..=max,
		Padding::Sign => 1..=max,
	}
}

/// A random quantifier-free formula and its value under an assignment of `x`, `y`, `z`.
enum Tree {
	Atom(Vec<i64>, Relation, i64),
	Not(Box<Tree>),
	And(Box<Tree>, Box<Tree>),
	Or(Box<Tree>, Box<Tree>),
}

impl Tree {
	fn random(rng: &mut StdRng, depth: u32) -> Tree {
		match rng.random_range(0..if depth == 0 { 1 } else { 4 }) {
			0 => {
				let coefficients = (0..3).map(|_| rng.random_range(-3..=3)).collect();
				let relation = if rng.random_bool(0.5) {
					Relation::Equal
				} else {
					Relation::AtMost
				};
				Tree::Atom(coefficients, relation, rng.random_range(-4..=4))
			}
			1 => Tree::Not(Box::new(Tree::random(rng, depth - 1))),
			2 => Tree::And(
				Box::new(Tree::random(rng, depth - 1)),
				Box::new(Tree::random(rng, depth - 1)),
			),
			_ => Tree::Or(
				Box::new(Tree::random(rng, depth - 1)),
				Box::new(Tree::random(rng, depth - 1)),
			),
		}
	}

	fn build(&self, logic: &mut Logic, vars: &[Var]) -> Result<Prop, Error> {
		match self {
			Tree::Atom(coefficients, relation, constant) => {
				let terms: Vec<(i64, Var)> = coefficients
					.iter()
					.zip(vars)
					.filter(|&(&coefficient, _)| coefficient != 0)
					.map(|(&coefficient, &var)| (coefficient, var))
					.collect();
				linear(logic, &terms, *relation, *constant)
			}
			Tree::Not(inner) => {
				let inner = inner.build(logic, vars)?;
				logic.not(&inner)
			}
			Tree::And(lhs, rhs) => {
				let (lhs, rhs) = (lhs.build(logic, vars)?, rhs.build(logic, vars)?);
				logic.and(&lhs, &rhs)
			}
			Tree::Or(lhs, rhs) => {
				let (lhs, rhs) = (lhs.build(logic, vars)?, rhs.build(logic, vars)?);
				logic.or(&lhs, &rhs)
			}
		}
	}

	fn eval(&self, values: &[i64]) -> bool {
		match self {
			Tree::Atom(coefficients, relation, constant) => {
				let sum: i64 = coefficients.iter().zip(values).map(|(a, x)| a * x).sum();
				match relation {
					Relation::Equal => sum == *constant,
					Relation::AtMost => sum <= *constant,
				}
			}
			Tree::Not(inner) => !inner.eval(values),
			Tree::And(lhs, rhs) => lhs.eval(values) && rhs.eval(values),
			Tree::Or(lhs, rhs) => lhs.eval(values) || rhs.eval(values),
		}
	}
}

/// Every random quantifier-free formula holds exactly the encodings of its models, up to 3 bits.
#[test]
fn boolean_combinations_hold_the_encodings_of_their_models() -> Result<(), Error> {
	let mut rng = StdRng::seed_from_u64(1);
	for padding in [Padding::Zero, Padding::Sign] {
		for _ in 0..40 {
			let mut logic = Logic::new(padding);
			let vars = [logic.var("x"), logic.var("y"), logic.var("z")];
			let tree = Tree::random(&mut rng, 3);
			let prop = tree.build(&mut logic, &vars)?;
			for length in lengths(padding, 3) {
				for words in tuples(prop.vars().len(), length) {
					// The variables that the formula does not mention are 0.
					let values: Vec<i64> = vars
						.iter()
						.map(|var| {
							prop.vars()
								.iter()
								.position(|free| free == var)
								.map_or(0, |tape| value(padding, &words[tape]))
						})
						.collect();
					assert_eq!(
						holds(&mut logic, &prop, &words)?,
						tree.eval(&values),
						"{padding:?} {values:?}"
					);
				}
			}
		}
	}

	Ok(())
}

/// `∃x. x = 4y ∧ x ≥ 0` holds every encoding of a natural `y`, also those too short for `x`: the projection is closed
/// under padding. Its negation holds the negative ones.
#[test]
fn a_projection_holds_the_words_too_short_for_the_dropped_variable() -> Result<(), Error> {
	let mut logic = Logic::new(Padding::Sign);
	let (x, y) = (logic.var("x"), logic.var("y"));
	let times_four = linear(&mut logic, &[(1, x), (-4, y)], Relation::Equal, 0)?;
	let natural = linear(&mut logic, &[(-1, x)], Relation::AtMost, 0)?;
	let both = logic.and(&times_four, &natural)?;
	let exists = logic.exists(&both, &[x])?;
	let not_exists = logic.not(&exists)?;
	assert_eq!(exists.vars(), [y]);

	for length in 1..=4 {
		for words in tuples(1, length) {
			let y = value(Padding::Sign, &words[0]);
			assert_eq!(holds(&mut logic, &exists, &words)?, y >= 0, "y = {y}");
			assert_eq!(holds(&mut logic, &not_exists, &words)?, y < 0, "y = {y}");
		}
	}

	Ok(())
}

/// As [`a_projection_holds_the_words_too_short_for_the_dropped_variable`], over natural numbers: `∃x. x = 4y` holds
/// every word, and `¬∃x. y = 2x` the odd numbers.
#[test]
fn a_projection_of_naturals_is_closed_under_zeros() -> Result<(), Error> {
	let mut logic = Logic::new(Padding::Zero);
	let (x, y) = (logic.var("x"), logic.var("y"));
	let times_four = linear(&mut logic, &[(1, x), (-4, y)], Relation::Equal, 0)?;
	let every = logic.exists(&times_four, &[x])?;
	let half = linear(&mut logic, &[(1, y), (-2, x)], Relation::Equal, 0)?;
	let even = logic.exists(&half, &[x])?;
	let odd = logic.not(&even)?;

	for length in 0..=4 {
		for words in tuples(1, length) {
			let y = value(Padding::Zero, &words[0]);
			assert!(holds(&mut logic, &every, &words)?, "y = {y}");
			assert_eq!(holds(&mut logic, &odd, &words)?, y % 2 == 1, "y = {y}");
		}
	}

	Ok(())
}

/// `∀x ∃y. x = 2y ∨ x = 2y + 1`.
#[test]
fn every_integer_is_even_or_odd() -> Result<(), Error> {
	let mut logic = Logic::new(Padding::Sign);
	let (x, y) = (logic.var("x"), logic.var("y"));
	let even = linear(&mut logic, &[(1, x), (-2, y)], Relation::Equal, 0)?;
	let odd = linear(&mut logic, &[(1, x), (-2, y)], Relation::Equal, 1)?;
	let either = logic.or(&even, &odd)?;
	let exists = logic.exists(&either, &[y])?;
	let every = logic.forall(&exists, &[x])?;
	assert_eq!(every.vars(), []);
	assert_eq!(logic.counterexample(&every, &Options::new())?, None);

	let both = logic.and(&even, &odd)?;
	assert_eq!(logic.model(&both, &Options::new())?, None);

	Ok(())
}

/// `∀n. n ≥ from → ∃a, b ≥ 0. 3a + 5b = n`, over `padding`.
fn coins(padding: Padding, from: i64) -> Result<(Logic, Var, Prop), Error> {
	let mut logic = Logic::new(padding);
	let (n, a, b) = (logic.var("n"), logic.var("a"), logic.var("b"));
	let sum = linear(&mut logic, &[(3, a), (5, b), (-1, n)], Relation::Equal, 0)?;
	let a_natural = linear(&mut logic, &[(-1, a)], Relation::AtMost, 0)?;
	let b_natural = linear(&mut logic, &[(-1, b)], Relation::AtMost, 0)?;
	let naturals = logic.and(&a_natural, &b_natural)?;
	let paid = logic.and(&sum, &naturals)?;
	let paid = logic.exists(&paid, &[a, b])?;
	let large = linear(&mut logic, &[(-1, n)], Relation::AtMost, -from)?;
	let claim = logic.implies(&large, &paid)?;

	Ok((logic, n, claim))
}

/// The Frobenius number of 3 and 5 is 7: every amount from 8 on can be paid, 7 cannot.
#[test]
fn every_amount_from_eight_on_can_be_paid_with_threes_and_fives() -> Result<(), Error> {
	for padding in [Padding::Zero, Padding::Sign] {
		let (mut logic, _, claim) = coins(padding, 8)?;
		let every = logic.forall(&claim, claim.vars())?;
		assert_eq!(
			logic.counterexample(&every, &Options::new())?,
			None,
			"{padding:?}"
		);

		let (mut logic, n, claim) = coins(padding, 7)?;
		let model: Model = logic
			.counterexample(&claim, &Options::new())?
			.expect("7 cannot be paid");
		let bits = model.bits(n).expect("n is free");
		assert_eq!(value(padding, bits), 7, "{padding:?}");
	}

	Ok(())
}

/// A model satisfies the formula: `x + y = 5 ∧ x − y = 1` has only `x = 3, y = 2`.
#[test]
fn a_model_decodes_to_the_solution() -> Result<(), Error> {
	for padding in [Padding::Zero, Padding::Sign] {
		let mut logic = Logic::new(padding);
		let (x, y) = (logic.var("x"), logic.var("y"));
		let sum = linear(&mut logic, &[(1, x), (1, y)], Relation::Equal, 5)?;
		let difference = linear(&mut logic, &[(1, x), (-1, y)], Relation::Equal, 1)?;
		let both = logic.and(&sum, &difference)?;
		let model = logic.model(&both, &Options::new())?.expect("satisfiable");
		assert_eq!(model.vars(), [x, y]);
		let values = [x, y].map(|var| value(padding, model.bits(var).expect("free")));
		assert_eq!(values, [3, 2], "{padding:?}");
	}

	Ok(())
}

/// Atoms take their variables in any order; constants combine with everything.
#[test]
fn atoms_take_variables_in_any_order_and_constants_combine() -> Result<(), Error> {
	let mut logic = Logic::new(Padding::Sign);
	let (x, y) = (logic.var("x"), logic.var("y"));
	// y − x = 1, given as (y, x).
	let next = linear(&mut logic, &[(1, y), (-1, x)], Relation::Equal, 1)?;
	assert_eq!(next.vars(), [x, y]);
	let model = logic.model(&next, &Options::new())?.expect("satisfiable");
	let [x_value, y_value] = [x, y].map(|var| value(Padding::Sign, model.bits(var).expect("free")));
	assert_eq!(y_value - x_value, 1);

	let truth = logic.constant(true)?;
	let falsity = logic.constant(false)?;
	let with_truth = logic.and(&next, &truth)?;
	let with_falsity = logic.and(&next, &falsity)?;
	let or_falsity = logic.or(&falsity, &next)?;
	assert!(logic.model(&with_truth, &Options::new())?.is_some());
	assert_eq!(logic.model(&with_falsity, &Options::new())?, None);
	let same = logic.iff(&or_falsity, &next)?;
	let same = logic.forall(&same, &[x, y])?;
	assert_eq!(logic.counterexample(&same, &Options::new())?, None);

	Ok(())
}

#[test]
fn malformed_atoms_are_rejected() {
	let mut logic = Logic::new(Padding::Sign);
	let x = logic.var("x");
	let mut other = Logic::new(Padding::Sign);
	let (_, foreign) = (other.var("a"), other.var("b"));
	let atom = |coefficients: Vec<i64>| Linear {
		padding: Padding::Sign,
		coefficients,
		relation: Relation::Equal,
		constant: 0,
	};

	assert_eq!(
		logic.atom(&atom(vec![1]), &[x, x]),
		Err(Error::AtomArity { arity: 1, vars: 2 })
	);
	assert_eq!(
		logic.atom(&atom(vec![1, 1]), &[x, x]),
		Err(Error::RepeatedVar { var: x })
	);
	assert_eq!(
		logic.atom(&atom(vec![1, 1]), &[x, foreign]),
		Err(Error::ForeignVar { var: foreign })
	);
}

/// A sentence is true or false whatever the length of the words: `∃x. x = 1000` needs 11 bits, and its negation is
/// still unsatisfiable, also next to a formula with short models.
#[test]
fn sentences_hold_at_every_length() -> Result<(), Error> {
	for padding in [Padding::Zero, Padding::Sign] {
		let mut logic = Logic::new(padding);
		let (x, y) = (logic.var("x"), logic.var("y"));
		let thousand = linear(&mut logic, &[(1, x)], Relation::Equal, 1000)?;
		let exists = logic.exists(&thousand, &[x])?;
		let not_exists = logic.not(&exists)?;
		assert_eq!(
			logic.model(&not_exists, &Options::new())?,
			None,
			"{padding:?}"
		);

		let one = linear(&mut logic, &[(1, y)], Relation::Equal, 1)?;
		let both = logic.and(&one, &exists)?;
		let model = logic
			.model(&both, &Options::new())?
			.expect("y = 1 and a sentence that holds");
		assert_eq!(
			value(padding, model.bits(y).expect("free")),
			1,
			"{padding:?}"
		);
		let neither = logic.and(&one, &not_exists)?;
		assert_eq!(logic.model(&neither, &Options::new())?, None, "{padding:?}");
	}

	Ok(())
}
