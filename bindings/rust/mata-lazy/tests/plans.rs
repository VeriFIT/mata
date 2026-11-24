//! Plans and policies: random formulas under random plans against a brute-force evaluator, the plan view and
//! the annotations.

use std::collections::{BTreeMap, BTreeSet};

use mata_lazy::{
	AllEager, AllLazy, Annotation, Automaton, Error, Eval, Formula, Heuristic, NodeKind, Options,
	Order, Outcome, Plan, PlanView, Policy, Symbol, Term,
};
use rand::rngs::StdRng;
use rand::{RngExt, SeedableRng};

const SYMBOLS: [Symbol; 2] = [Symbol::new(0), Symbol::new(1)];
/// The evaluator checks the words up to this length.
const MAX_LENGTH: usize = 4;
const SEEDS: u64 = 400;
const PLANS_PER_FORMULA: usize = 6;
const BUDGET: usize = 200_000;

/// A tuple automaton given by its moves.
#[derive(Clone, Debug)]
struct Tuples {
	arity: usize,
	initial: Vec<u32>,
	finals: Vec<bool>,
	moves: Vec<(u32, Vec<Symbol>, u32)>,
}

impl Automaton for Tuples {
	type State = u32;

	fn arity(&self) -> usize {
		self.arity
	}

	fn initial_states(&self) -> impl IntoIterator<Item = u32> {
		self.initial.iter().copied()
	}

	fn is_final(&self, state: u32) -> bool {
		self.finals[state as usize]
	}

	fn for_each_move(&self, state: u32, mut visit: impl FnMut(&[Symbol], u32)) {
		for (source, tuple, target) in &self.moves {
			if *source == state {
				visit(tuple, *target);
			}
		}
	}
}

impl Tuples {
	fn random(rng: &mut StdRng, arity: usize) -> Tuples {
		let num_states = rng.random_range(1..=3u32);
		let moves = (0..rng.random_range(0..=6))
			.map(|_| {
				let tuple = (0..arity)
					.map(|_| SYMBOLS[rng.random_range(0..2)])
					.collect();

				(
					rng.random_range(0..num_states),
					tuple,
					rng.random_range(0..num_states),
				)
			})
			.collect();

		Tuples {
			arity,
			initial: vec![0],
			finals: (0..num_states).map(|_| rng.random_bool(0.5)).collect(),
			moves,
		}
	}

	/// Every tuple of words of length at most `MAX_LENGTH`.
	fn bounded(arity: usize) -> Tuples {
		let max = u32::try_from(MAX_LENGTH).expect("a small bound");
		let moves = (0..max)
			.flat_map(|state| all_tuples(arity).map(move |tuple| (state, tuple, state + 1)))
			.collect();

		Tuples {
			arity,
			initial: vec![0],
			finals: vec![true; MAX_LENGTH + 1],
			moves,
		}
	}

	fn accepts(&self, words: &[Vec<Symbol>]) -> bool {
		let length = words.first().map_or(0, Vec::len);
		let mut current: BTreeSet<u32> = self.initial.iter().copied().collect();
		for position in 0..length {
			let step: Vec<Symbol> = words.iter().map(|word| word[position]).collect();
			current = self
				.moves
				.iter()
				.filter(|(source, tuple, _)| current.contains(source) && *tuple == step)
				.map(|&(_, _, target)| target)
				.collect();
		}

		current.iter().any(|&state| self.finals[state as usize])
	}
}

/// Every tuple of `arity` symbols.
fn all_tuples(arity: usize) -> impl Iterator<Item = Vec<Symbol>> {
	(0..1usize << arity)
		.map(move |bits| (0..arity).map(|tape| SYMBOLS[(bits >> tape) & 1]).collect())
}

/// A tuple of words of equal length.
type Words = Vec<Vec<Symbol>>;

/// The operation of a term, with operands as indices of earlier terms.
#[derive(Clone, Debug)]
enum Expr {
	Leaf(Tuples),
	Union(usize, usize),
	Intersection(usize, usize),
	Complement(usize),
	Identity(usize),
	Project(usize, Vec<usize>),
	Compose(usize, usize),
	PostImage(usize, usize),
	Explicit(usize),
}

/// A formula over leaves of {0, 1} without padding, and the operations that built its terms.
struct Random {
	formula: Formula,
	terms: Vec<(Term, Expr)>,
}

impl Random {
	fn new(rng: &mut StdRng) -> Random {
		let mut random = Random {
			formula: Formula::new(),
			terms: Vec::new(),
		};
		let mut complements = 0;
		for _ in 0..rng.random_range(2..=9) {
			let pick = |rng: &mut StdRng, count: usize| rng.random_range(0..count);
			let count = random.terms.len();
			let operation = if count == 0 {
				0
			} else {
				rng.random_range(0..=9)
			};
			let expr = match operation {
				0 | 1 => {
					let arity = rng.random_range(1..=2);

					Expr::Leaf(Tuples::random(rng, arity))
				}
				2 => Expr::Union(pick(rng, count), pick(rng, count)),
				3 => Expr::Intersection(pick(rng, count), pick(rng, count)),
				4 if complements < 2 => {
					complements += 1;

					Expr::Complement(pick(rng, count))
				}
				5 => Expr::Identity(pick(rng, count)),
				6 => {
					let tapes = [vec![0], vec![1], vec![1, 0]][rng.random_range(0..3)].clone();

					Expr::Project(pick(rng, count), tapes)
				}
				7 => Expr::Compose(pick(rng, count), pick(rng, count)),
				8 => Expr::PostImage(pick(rng, count), pick(rng, count)),
				_ => Expr::Explicit(pick(rng, count)),
			};

			random.push(expr);
		}

		random
	}

	/// Adds the term of `expr`, unless a builder rejects it (such as for a wrong arity).
	fn push(&mut self, expr: Expr) {
		let term = |index: usize| self.terms[index].0;
		let formula = &mut self.formula;
		let built = match &expr {
			Expr::Leaf(leaf) => formula.leaf_over(leaf, [SYMBOLS]),
			Expr::Union(a, b) => formula.union(term(*a), term(*b)),
			Expr::Intersection(a, b) => formula.intersection(term(*a), term(*b)),
			Expr::Complement(a) => formula.complement(term(*a)),
			Expr::Identity(a) => formula.identity(term(*a)),
			Expr::Project(a, tapes) => formula.project(term(*a), tapes),
			Expr::Compose(a, b) => formula.compose(term(*a), term(*b)),
			Expr::PostImage(a, b) => formula.post_image(term(*a), term(*b)),
			Expr::Explicit(a) => formula.explicit(term(*a)),
		};

		if let Ok(term) = built {
			self.terms.push((term, expr));
		}
	}

	/// The tuples of each term of length `length`, by brute force.
	fn members(&self, length: usize) -> Vec<BTreeSet<Words>> {
		let mut members: Vec<BTreeSet<Words>> = Vec::new();
		for (term, expr) in &self.terms {
			let arity = self.formula.arity(*term).expect("a term of the formula");
			let set = match expr {
				Expr::Leaf(leaf) => universe(arity, length)
					.filter(|words| leaf.accepts(words))
					.collect(),
				Expr::Union(a, b) => members[*a].union(&members[*b]).cloned().collect(),
				Expr::Intersection(a, b) => {
					members[*a].intersection(&members[*b]).cloned().collect()
				}
				Expr::Complement(a) => universe(arity, length)
					.filter(|words| !members[*a].contains(words))
					.collect(),
				Expr::Identity(a) => members[*a]
					.iter()
					.map(|words| vec![words[0].clone(), words[0].clone()])
					.collect(),
				Expr::Project(a, tapes) => members[*a]
					.iter()
					.map(|words| tapes.iter().map(|&tape| words[tape].clone()).collect())
					.collect(),
				Expr::Compose(a, b) => join(&members[*a], &members[*b], |u, w| {
					vec![u.clone(), w.clone()]
				}),
				Expr::PostImage(a, b) => {
					let language: BTreeSet<Words> = members[*a]
						.iter()
						.map(|words| vec![words[0].clone(), words[0].clone()])
						.collect();

					join(&language, &members[*b], |_, w| vec![w.clone()])
				}
				Expr::Explicit(a) => members[*a].clone(),
			};

			members.push(set);
		}

		members
	}
}

/// `{result(u, w) | (u, v) ∈ lhs, (v, w) ∈ rhs}`.
fn join(
	lhs: &BTreeSet<Words>,
	rhs: &BTreeSet<Words>,
	result: impl Fn(&Vec<Symbol>, &Vec<Symbol>) -> Words,
) -> BTreeSet<Words> {
	let mut by_first: BTreeMap<&Vec<Symbol>, Vec<&Vec<Symbol>>> = BTreeMap::new();
	for words in rhs {
		by_first.entry(&words[0]).or_default().push(&words[1]);
	}

	lhs.iter()
		.flat_map(|words| {
			by_first
				.get(&words[1])
				.into_iter()
				.flatten()
				.map(|w| result(&words[0], w))
				.collect::<Vec<_>>()
		})
		.collect()
}

/// Every tuple of `arity` words of `length` over [`SYMBOLS`].
fn universe(arity: usize, length: usize) -> impl Iterator<Item = Words> {
	let count = 1usize << (arity * length);

	(0..count).map(move |bits| {
		(0..arity)
			.map(|tape| {
				(0..length)
					.map(|position| SYMBOLS[(bits >> (tape * length + position)) & 1])
					.collect()
			})
			.collect()
	})
}

/// A random plan over the terms of `view`.
fn random_plan(rng: &mut StdRng, view: &PlanView) -> Plan {
	let mut plan = Plan::new();
	for node in &view.nodes {
		if rng.random_bool(0.4) {
			continue;
		}

		let mut annotation = match rng.random_range(0..4) {
			0 => Annotation::default(),
			1 => Annotation::lazy(),
			2 => Annotation::eager(),
			_ => Annotation::eager_within(rng.random_range(0..20)),
		};
		if matches!(node.kind, NodeKind::Intersection | NodeKind::Sync { .. }) {
			annotation = annotation
				.order([Order::Auto, Order::AsWritten, Order::Swapped][rng.random_range(0..3)]);
		}

		plan = plan.annotate(node.term, annotation);
	}

	plan
}

/// The outcome of a query that may fail only at the state limit.
fn within_limit(result: Result<Outcome, Error>) -> Option<Outcome> {
	match result {
		Ok(outcome) => Some(outcome),
		Err(Error::StateLimit { .. }) => None,
		Err(error) => panic!("{error}"),
	}
}

fn witness_words(outcome: &Outcome) -> Option<Words> {
	let witness = outcome.witness.as_ref()?;

	Some(
		(0..witness.arity())
			.map(|tape| witness.tape(tape).expect("a tape of the witness"))
			.collect(),
	)
}

#[test]
fn random_plans_keep_the_answers_of_brute_force() -> Result<(), Error> {
	let mut compared: u64 = 0;
	for seed in 0..SEEDS {
		let mut rng = StdRng::seed_from_u64(seed);
		let mut random = Random::new(&mut rng);
		let Some(&(root, _)) = random.terms.last() else {
			continue;
		};

		let arity = random.formula.arity(root)?;
		let members: Vec<Vec<BTreeSet<Words>>> = (0..=MAX_LENGTH)
			.map(|length| random.members(length))
			.collect();
		let root_index = random.terms.len() - 1;
		let expected: BTreeSet<&Words> =
			members.iter().flat_map(|sets| &sets[root_index]).collect();

		let bound = random
			.formula
			.leaf_over(&Tuples::bounded(arity), [SYMBOLS])?;
		let bounded = random.formula.intersection(root, bound)?;
		let view = random.formula.view(bounded)?;

		let mut plans: Vec<(String, Plan)> = vec![
			("no plan".into(), Plan::new()),
			("all lazy".into(), AllLazy.plan(&view)),
			("all eager".into(), AllEager.plan(&view)),
			(
				"heuristic".into(),
				Heuristic::new()
					.max_states(rng.random_range(1..64))
					.plan(&view),
			),
		];
		plans.extend(
			(0..PLANS_PER_FORMULA)
				.map(|index| (format!("random {index}"), random_plan(&mut rng, &view))),
		);

		let lazy = within_limit(
			random.formula.check(
				root,
				&Options::new()
					.max_states(BUDGET)
					.plan(AllLazy.plan(&random.formula.view(root)?)),
			),
		);
		for (name, plan) in plans {
			let options = Options::new().max_states(BUDGET).plan(plan);
			let Some(outcome) = within_limit(random.formula.check(bounded, &options)) else {
				continue;
			};

			compared += 1;
			assert_eq!(
				outcome.is_empty(),
				expected.is_empty(),
				"seed {seed}, {name}: {outcome:?}"
			);
			if let Some(words) = witness_words(&outcome) {
				assert!(
					expected.contains(&words),
					"seed {seed}, {name}: witness {words:?} not in the relation"
				);
			}

			// Unbounded, against the purely lazy query.
			let unbounded = options.clone();
			if let (Some(lazy), Some(other)) =
				(&lazy, within_limit(random.formula.check(root, &unbounded)))
			{
				assert_eq!(
					other.is_empty(),
					lazy.is_empty(),
					"seed {seed}, {name}, unbounded"
				);
			}
		}
	}

	assert!(
		compared > SEEDS * 8,
		"only {compared} queries answered within the budget"
	);

	Ok(())
}

/// The words over {0, 1} of length at least `length` that have 1 at position `index`.
fn one_at(index: u32, length: u32) -> Tuples {
	let last = length.max(index + 1);
	let mut moves = Vec::new();
	for state in 0..last {
		for symbol in SYMBOLS {
			if state != index || symbol == SYMBOLS[1] {
				moves.push((state, vec![symbol], state + 1));
			}
		}
	}
	moves.extend(SYMBOLS.map(|symbol| (last, vec![symbol], last)));
	let mut finals = vec![false; last as usize + 1];
	finals[last as usize] = true;

	Tuples {
		arity: 1,
		initial: vec![0],
		finals,
		moves,
	}
}

/// `(a ∩ b) ∩ c` with small `a` and `b` and a larger `c`.
fn two_intersections() -> Result<(Formula, [Term; 5]), Error> {
	let mut formula = Formula::new();
	let a = formula.leaf(&one_at(0, 2))?;
	let b = formula.leaf(&one_at(1, 2))?;
	let c = formula.leaf(&one_at(40, 0))?;
	let ab = formula.intersection(a, b)?;
	let abc = formula.intersection(ab, c)?;

	Ok((formula, [a, b, c, ab, abc]))
}

#[test]
fn the_view_lists_the_dependencies_children_first() -> Result<(), Error> {
	let (mut formula, [a, b, c, ab, abc]) = two_intersections()?;
	let unrelated = formula.complement(c)?;
	let shared = formula.union(abc, ab)?;

	let view = formula.view(shared)?;
	let terms: Vec<Term> = view.nodes.iter().map(|node| node.term).collect();
	assert_eq!(terms, [a, b, c, ab, abc, shared]);
	assert!(view.node(unrelated).is_none());
	assert_eq!(view.root, shared);

	let node = view.node(ab).expect("ab is below the root");
	assert_eq!(node.kind, NodeKind::Intersection);
	assert_eq!(node.children, [a, b]);
	// Used by `abc` and by the union.
	assert_eq!(node.fan_out, 2);
	assert_eq!(node.arity, 1);
	assert!(node.leaf.is_none());

	let leaf = view
		.node(c)
		.and_then(|node| node.leaf.as_ref())
		.expect("c is a leaf");
	// Two moves from each of the 41 states before the last but state 40, which reads only 1, and two loops.
	assert_eq!((leaf.states, leaf.transitions), (42, 83));
	assert!(leaf.deterministic);
	assert_eq!(leaf.tape_alphabet_sizes, [2]);

	let prefixes = formula.prefixes(abc)?;
	let view = formula.view(prefixes)?;
	let node = view
		.node(prefixes)
		.expect("the prefix closure is in its view");
	assert_eq!(node.kind, NodeKind::Prefixes);
	assert_eq!(node.children, [abc]);

	assert_eq!(formula.view(unrelated)?.nodes.len(), 2);
	assert_eq!(Formula::new().view(a), Err(Error::ForeignTerm));

	Ok(())
}

#[test]
fn the_heuristic_builds_the_small_inner_intersection_eagerly() -> Result<(), Error> {
	let (formula, [_, _, _, ab, abc]) = two_intersections()?;

	let plan = formula.plan_with(abc, &Heuristic::new().max_states(16))?;
	let annotated: Vec<(Term, Eval)> = plan
		.iter()
		.map(|(term, annotation)| (term, annotation.eval))
		.collect();
	assert_eq!(
		annotated,
		[(
			ab,
			Eval::Eager {
				max_states: Some(16)
			}
		)]
	);

	let eager = formula.check(abc, &Options::new().plan(plan))?;
	let explicit: Vec<(Term, bool)> = eager
		.stats
		.explicit
		.iter()
		.map(|node| (node.term, node.lazy))
		.collect();
	assert_eq!(explicit, [(ab, false)]);

	let lazy = formula.check(abc, &Options::new())?;
	assert!(!lazy.is_empty());
	assert_eq!(eager.is_empty(), lazy.is_empty());

	// A bound below the leaves keeps everything lazy.
	assert!(
		formula
			.plan_with(abc, &Heuristic::new().max_states(2))?
			.is_empty()
	);

	Ok(())
}

#[test]
fn plans_override_explicit_nodes_and_options() -> Result<(), Error> {
	let (mut formula, [_, _, c, ab, abc]) = two_intersections()?;
	let explicit_ab = formula.explicit(ab)?;
	let root = formula.intersection(explicit_ab, c)?;

	// The explicit nodes of a query, and whether each stayed lazy.
	let built = |options: &Options, term| -> Result<Vec<(Term, bool)>, Error> {
		let outcome = formula.check(term, options)?;
		assert!(!outcome.is_empty());

		Ok(outcome
			.stats
			.explicit
			.iter()
			.map(|node| (node.term, node.lazy))
			.collect())
	};
	let none: Vec<(Term, bool)> = Vec::new();

	assert_eq!(built(&Options::new(), root)?, [(explicit_ab, false)]);
	let lazy = Plan::new().annotate(explicit_ab, Annotation::lazy());
	assert_eq!(built(&Options::new().plan(lazy), root)?, none);
	let all_lazy = AllLazy.plan(&formula.view(root)?);
	assert_eq!(built(&Options::new().plan(all_lazy), root)?, none);

	// A plan takes precedence over `Options::explicit`.
	let options = Options::new()
		.explicit(ab)
		.plan(Plan::new().annotate(ab, Annotation::lazy()));
	assert_eq!(built(&options, abc)?, none);

	// A budget below the size of `a ∩ b` keeps it lazy.
	let tight = Plan::new().annotate(ab, Annotation::eager_within(1));
	assert_eq!(built(&Options::new().plan(tight), abc)?, [(ab, true)]);

	Ok(())
}

#[test]
fn an_operand_order_needs_a_product() -> Result<(), Error> {
	let (mut formula, [a, b, _, ab, abc]) = two_intersections()?;
	let union = formula.union(a, b)?;
	let swapped = |term| {
		Options::new().plan(Plan::new().annotate(term, Annotation::default().order(Order::Swapped)))
	};

	assert!(matches!(
		formula.check(union, &swapped(union)),
		Err(Error::InvalidPlan { .. })
	));
	assert!(matches!(
		formula.check(abc, &swapped(a)),
		Err(Error::InvalidPlan { .. })
	));
	assert_eq!(
		formula.check(abc, &swapped(ab))?.is_empty(),
		formula.check(abc, &Options::new())?.is_empty()
	);

	let other = Formula::new().leaf(&one_at(0, 0))?;
	let foreign = Options::new().plan(Plan::new().annotate(other, Annotation::lazy()));
	assert_eq!(formula.check(abc, &foreign), Err(Error::ForeignTerm));

	Ok(())
}
