//! Plans and policies: random formulas under random plans against a brute-force evaluator, the plan view, the
//! annotations, per-node statistics and JSON plans.

use std::collections::{BTreeMap, BTreeSet};
use std::time::Duration;

use mata_lazy::{
	AllEager, AllLazy, Annotation, Automaton, Error, Eval, Formula, Heuristic, NodeKind, Options,
	Order, Outcome, Plan, PlanView, Policy, Symbol, Tape, Term,
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

/// `project(identity(u) ∩ (a × b), [0])`, with both products eligible for diagonal rewrites.
fn diagonal_projection() -> Result<(Formula, Term, Term, Term), Error> {
	let mut formula = Formula::new();
	let u = formula.leaf(&Tuples::bounded(1))?;
	let a = formula.leaf(&one_at(2, 3))?;
	let b = formula.leaf(&Tuples::bounded(1))?;
	let identity = formula.identity(u)?;
	let product = formula.sync_product(a, b, &[], &[Tape::Lhs(0), Tape::Rhs(0)])?;
	let intersection = formula.intersection(identity, product)?;
	let projection = formula.project(intersection, &[0])?;

	Ok((formula, projection, intersection, product))
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
	let fallback = formula.check(abc, &options.clone().plan(tight.clone()))?;
	assert!(
		fallback
			.stats
			.nodes
			.iter()
			.any(|node| node.term == ab && node.posts > 0)
	);
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

#[test]
fn a_fixed_intersection_order_survives_diagonal_rewriting() -> Result<(), Error> {
	let (formula, projection, intersection, _) = diagonal_projection()?;
	let check = |order| {
		formula.check(
			projection,
			&Options::new()
				.plan(Plan::new().annotate(intersection, Annotation::default().order(order))),
		)
	};
	let automatic = check(Order::Auto)?;
	let written = check(Order::AsWritten)?;
	let swapped = check(Order::Swapped)?;

	assert!(!automatic.is_empty());
	assert!(!written.is_empty());
	assert!(!swapped.is_empty());
	assert_eq!(written.stats, swapped.stats);
	assert_ne!(automatic.stats, written.stats);
	Ok(())
}

#[test]
fn a_fixed_sync_order_survives_diagonal_fusion() -> Result<(), Error> {
	let (formula, projection, _, product) = diagonal_projection()?;
	let check = |order| {
		formula.check(
			projection,
			&Options::new().plan(Plan::new().annotate(product, Annotation::default().order(order))),
		)
	};
	let automatic = check(Order::Auto)?;
	let written = check(Order::AsWritten)?;
	let swapped = check(Order::Swapped)?;

	assert!(!automatic.is_empty());
	assert!(!written.is_empty());
	assert!(!swapped.is_empty());
	assert_eq!(written.stats, swapped.stats);
	assert_ne!(automatic.stats, written.stats);
	Ok(())
}

#[test]
fn node_statistics_count_the_macrostates_and_posts_of_each_term() -> Result<(), Error> {
	let (formula, [a, _, c, ab, abc]) = two_intersections()?;
	let options = Options::new().subsumption(false);
	let lazy = formula.check(abc, &options)?;
	let node = |outcome: &Outcome, term| {
		outcome
			.stats
			.nodes
			.iter()
			.find(|node| node.term == term)
			.map(|node| (node.macrostates, node.posts))
	};

	// The root's macrostates are pairs of `a ∩ b` and `c`; its successors are computed once per explored one.
	let (root_states, root_posts) = node(&lazy, abc).expect("the root has a node");
	assert_eq!(root_posts, lazy.stats.explored);
	assert!(root_states >= root_posts);
	let (ab_states, ab_posts) = node(&lazy, ab).expect("a ∩ b has a node");
	assert!(ab_states > 0 && ab_posts > 0);
	// Leaves create no macrostates.
	assert_eq!(node(&lazy, a).map(|(states, _)| states), Some(0));
	assert_eq!(node(&lazy, c).map(|(states, _)| states), Some(0));
	let total: usize = lazy.stats.nodes.iter().map(|node| node.macrostates).sum();
	assert_eq!(total, lazy.stats.macrostates);

	// Built eagerly, `a ∩ b` is a leaf of the query.
	let eager = formula.check(
		abc,
		&options
			.clone()
			.plan(Plan::new().annotate(ab, Annotation::eager())),
	)?;
	assert_eq!(node(&eager, ab).map(|(states, _)| states), Some(0));

	// Without a cache, the successors of `a ∩ b` are computed again; the macrostates stay.
	let uncached = formula.check(abc, &options.cache_budget(0))?;
	let (uncached_states, uncached_posts) = node(&uncached, ab).expect("a ∩ b has a node");
	assert_eq!(uncached_states, ab_states);
	assert!(uncached_posts > ab_posts, "{uncached_posts} > {ab_posts}");

	Ok(())
}

/// The pairs over {0, 1} that differ in at most one position.
fn flip_at_most_one() -> Tuples {
	let mut moves = Vec::new();
	for a in SYMBOLS {
		for b in SYMBOLS {
			if a == b {
				moves.push((0, vec![a, b], 0));
				moves.push((1, vec![a, b], 1));
			} else {
				moves.push((0, vec![a, b], 1));
			}
		}
	}

	Tuples {
		arity: 2,
		initial: vec![0],
		finals: vec![true, true],
		moves,
	}
}

#[test]
fn eager_pre_images_stay_small_level_by_level() -> Result<(), Error> {
	// The pre-image k levels down holds the words within k flips of a 1 at position 6: at most a few states per
	// position. Built eagerly but unreduced, each level multiplied the size of the one below.
	const LEVELS: usize = 8;
	let mut formula = Formula::new();
	let relation = formula.leaf(&flip_at_most_one())?;
	let mut chain = formula.leaf(&one_at(6, 8))?;
	for _ in 0..LEVELS {
		let pre_image = formula.pre_image(chain, relation)?;
		chain = formula.explicit(pre_image)?;
	}
	let initial = formula.leaf(&one_at(0, 0))?;
	let query = formula.intersection(initial, chain)?;
	let outcome = formula.check(query, &Options::new())?;
	assert!(!outcome.is_empty());
	assert_eq!(outcome.stats.explicit.len(), LEVELS);
	for node in &outcome.stats.explicit {
		assert!(!node.lazy);
		assert!(node.states <= 12, "{} states", node.states);
	}

	Ok(())
}

/// The one word of `length` zeros, as a chain of states.
struct Zeros {
	length: u32,
}

impl Automaton for Zeros {
	type State = u32;

	fn arity(&self) -> usize {
		1
	}

	fn initial_states(&self) -> impl IntoIterator<Item = u32> {
		[0]
	}

	fn is_final(&self, state: u32) -> bool {
		state == self.length
	}

	fn for_each_move(&self, state: u32, mut visit: impl FnMut(&[Symbol], u32)) {
		if state < self.length {
			visit(&[SYMBOLS[0]], state + 1);
		}
	}
}

#[test]
fn a_long_explicit_chain_is_reduced_in_quasi_linear_time() -> Result<(), Error> {
	// Refinement that rescans every state each round separates one more state of the chain per round: about
	// 2.5 * 10^9 visits here, far past the limit. Subsumption and the antichain are off, as the search over a
	// leaf this long is quadratic with them; this test is about building the explicit node.
	const LENGTH: u32 = 50_000;
	let mut formula = Formula::new();
	let zeros = formula.leaf(&Zeros { length: LENGTH })?;
	let product = formula.intersection(zeros, zeros)?;
	let explicit = formula.explicit(product)?;
	let outcome = formula.check(
		explicit,
		&Options::new()
			.subsumption(false)
			.antichain(false)
			.time_limit(Duration::from_secs(20)),
	)?;
	assert!(!outcome.is_empty());
	assert_eq!(outcome.stats.explicit[0].states, LENGTH as usize + 1);

	Ok(())
}

/// The chain of [`Zeros`] with every state initial.
struct AllInitial {
	length: u32,
}

impl Automaton for AllInitial {
	type State = u32;

	fn arity(&self) -> usize {
		1
	}

	fn initial_states(&self) -> impl IntoIterator<Item = u32> {
		0..=self.length
	}

	fn is_final(&self, state: u32) -> bool {
		state == self.length
	}

	fn for_each_move(&self, state: u32, mut visit: impl FnMut(&[Symbol], u32)) {
		if state < self.length {
			visit(&[SYMBOLS[0]], state + 1);
		}
	}
}

#[test]
fn a_chain_whose_states_are_all_initial_keeps_its_quotient() -> Result<(), Error> {
	// The subset construction has as many subsets as states, the suffixes of the chain: about 1.25 * 10^9 members
	// in all, which a bound on the number of subsets alone lets through.
	const LENGTH: u32 = 50_000;
	let mut formula = Formula::new();
	let chain = formula.leaf(&AllInitial { length: LENGTH })?;
	let explicit = formula.explicit(chain)?;
	let outcome = formula.check(
		explicit,
		&Options::new()
			.subsumption(false)
			.antichain(false)
			.time_limit(Duration::from_secs(20)),
	)?;
	assert!(!outcome.is_empty());
	assert_eq!(outcome.stats.explicit[0].states, LENGTH as usize + 1);

	Ok(())
}

/// A chain of `length` zeros to its final state, and two initial states with a move into every state of the
/// chain, each with its own letter.
struct FanOut {
	length: u32,
}

impl Automaton for FanOut {
	type State = u32;

	fn arity(&self) -> usize {
		1
	}

	fn initial_states(&self) -> impl IntoIterator<Item = u32> {
		[self.length + 1, self.length + 2]
	}

	fn is_final(&self, state: u32) -> bool {
		state == self.length
	}

	fn for_each_move(&self, state: u32, mut visit: impl FnMut(&[Symbol], u32)) {
		if state < self.length {
			visit(&[SYMBOLS[0]], state + 1);
		} else if state > self.length {
			for target in 0..=self.length {
				visit(&[Symbol::new(target + 2)], target);
			}
		}
	}
}

#[test]
fn an_explicit_node_with_high_fan_out_is_reduced_in_quasi_linear_time() -> Result<(), Error> {
	// Every split of the chain touches the initial states; comparing their whole moves again on each split
	// visits about 10^9 moves here.
	const LENGTH: u32 = 20_000;
	let mut formula = Formula::new();
	let fan_out = formula.leaf(&FanOut { length: LENGTH })?;
	let product = formula.intersection(fan_out, fan_out)?;
	let explicit = formula.explicit(product)?;
	let outcome = formula.check(
		explicit,
		&Options::new()
			.subsumption(false)
			.antichain(false)
			.time_limit(Duration::from_secs(20)),
	)?;
	assert!(!outcome.is_empty());
	// The four initial pairs are one state.
	assert_eq!(outcome.stats.explicit[0].states, LENGTH as usize + 2);

	Ok(())
}

#[test]
fn a_query_stopped_by_its_limit_caches_no_explicit_node() -> Result<(), Error> {
	let mut formula = Formula::new();
	let zeros = formula.leaf(&Zeros { length: 50_000 })?;
	let product = formula.intersection(zeros, zeros)?;
	let explicit = formula.explicit(product)?;
	let options = Options::new().subsumption(false).antichain(false);
	let stopped = formula.check(
		explicit,
		&options.clone().time_limit(Duration::from_millis(1)),
	);
	assert!(
		matches!(stopped, Err(Error::TimeLimit { .. })),
		"{stopped:?}"
	);
	let outcome = formula.check(explicit, &options)?;
	assert!(!outcome.stats.explicit[0].cached);

	Ok(())
}

#[cfg(feature = "serde")]
#[test]
fn plans_and_views_are_json() -> Result<(), Error> {
	let (formula, [a, _, c, ab, abc]) = two_intersections()?;
	let plan = Plan::new()
		.annotate(ab, Annotation::eager_within(100))
		.annotate(abc, Annotation::lazy().order(Order::Swapped))
		.annotate(c, Annotation::lazy());
	assert_eq!(formula.plan_from_json(&plan.to_json())?, plan);

	let written = format!(
		r#"{{"annotations": [{{"term": {}, "eval": {{"eager": {{}}}}}}, {{"term": {}, "order": "as_written"}}]}}"#,
		ab.id(),
		abc.id()
	);
	let read = formula.plan_from_json(&written)?;
	assert_eq!(read.annotation(ab), Some(&Annotation::eager()));
	assert_eq!(
		read.annotation(abc),
		Some(&Annotation::default().order(Order::AsWritten))
	);
	let explicit = formula
		.check(abc, &Options::new().plan(read))?
		.stats
		.explicit;
	assert_eq!(
		explicit.iter().map(|node| node.term).collect::<Vec<_>>(),
		[ab]
	);

	for invalid in [
		r#"{"annotations": [{"term": 99}]}"#.to_owned(),
		format!(
			r#"{{"annotations": [{{"term": {}, "order": "swapped"}}]}}"#,
			a.id()
		),
		format!(
			r#"{{"annotations": [{{"term": {0}}}, {{"term": {0}}}]}}"#,
			ab.id()
		),
		format!(
			r#"{{"annotations": [{{"term": {}, "eval": "eagerly"}}]}}"#,
			ab.id()
		),
		r#"{"annotations": [], "extra": 1}"#.to_owned(),
	] {
		assert!(
			matches!(
				formula.plan_from_json(&invalid),
				Err(Error::InvalidPlan { .. })
			),
			"{invalid}"
		);
	}

	let view = formula.view(abc)?.to_json();
	assert!(view.contains(r#""kind": "intersection""#), "{view}");
	assert!(view.contains(r#""deterministic": true"#), "{view}");

	Ok(())
}
