//! Symbolic Cartesian labels against concrete-word oracles.

use mata_lazy::{Automaton, Formula, Options, Pad, Symbol, Tape, Term};

const A: Symbol = Symbol::new(0);
const B: Symbol = Symbol::new(1);

#[derive(Clone)]
struct Nfa {
	arity: usize,
	initial: Vec<usize>,
	finals: Vec<bool>,
	edges: Vec<(usize, Vec<Symbol>, usize)>,
}

impl Nfa {
	fn moveless(arity: usize) -> Nfa {
		Nfa {
			arity,
			initial: vec![0],
			finals: vec![true],
			edges: Vec::new(),
		}
	}

	fn word(steps: &[Vec<Symbol>]) -> Nfa {
		Nfa {
			arity: steps.first().map_or(1, Vec::len),
			initial: vec![0],
			finals: (0..=steps.len())
				.map(|state| state == steps.len())
				.collect(),
			edges: steps
				.iter()
				.enumerate()
				.map(|(state, step)| (state, step.clone(), state + 1))
				.collect(),
		}
	}

	fn accepts(&self, steps: &[Vec<Symbol>]) -> bool {
		let mut current = self.initial.clone();
		for step in steps {
			let mut next = Vec::new();
			for &(source, ref label, target) in &self.edges {
				let matches = label
					.iter()
					.zip(step)
					.all(|(&read, &symbol)| read == Symbol::ANY || read == symbol);
				if current.contains(&source) && matches && !next.contains(&target) {
					next.push(target);
				}
			}
			current = next;
		}

		current.iter().any(|&state| self.finals[state])
	}
}

impl Automaton for Nfa {
	type State = usize;

	fn arity(&self) -> usize {
		self.arity
	}

	fn initial_states(&self) -> impl IntoIterator<Item = usize> {
		self.initial.clone()
	}

	fn is_final(&self, state: usize) -> bool {
		self.finals[state]
	}

	fn for_each_move(&self, state: usize, mut visit: impl FnMut(&[Symbol], usize)) {
		for &(source, ref tuple, target) in &self.edges {
			if source == state {
				visit(tuple, target);
			}
		}
	}

	fn alphabet(&self, _tape: usize) -> impl IntoIterator<Item = Symbol> {
		[A, B]
	}
}

fn holds(formula: &mut Formula, term: Term, steps: &[Vec<Symbol>]) -> bool {
	let word = formula.leaf(&Nfa::word(steps)).expect("a word");
	let both = formula.intersection(term, word).expect("matching arity");

	!formula.is_empty(both).expect("no query limits")
}

#[test]
fn every_small_wildcard_complement_matches_concrete_nfa_membership() {
	let moves: Vec<(Vec<Symbol>, usize)> = [A, B, Symbol::ANY, Symbol::EPSILON]
		.into_iter()
		.flat_map(|symbol| [0, 1].map(move |target| (vec![symbol], target)))
		.collect();
	let mut words = vec![Vec::new()];
	for len in 1..=2 {
		for bits in 0..1_usize << len {
			words.push(
				(0..len)
					.map(|position| vec![[A, B][(bits >> position) & 1]])
					.collect(),
			);
		}
	}

	for final_state in [false, true] {
		for mask in 0..1_u16 << moves.len() {
			let nfa = Nfa {
				arity: 1,
				initial: vec![0],
				finals: vec![final_state, false],
				edges: moves
					.iter()
					.enumerate()
					.filter(|(index, _)| mask & (1 << index) != 0)
					.map(|(_, (label, target))| (0, label.clone(), *target))
					.collect(),
			};
			let mut formula = Formula::new();
			let leaf = formula.leaf(&nfa).expect("a leaf");
			let complement = formula.complement(leaf).expect("one tape");

			for word in &words {
				assert_eq!(
					holds(&mut formula, complement, word),
					!nfa.accepts(word),
					"final {final_state}, mask {mask:#x}, word {word:?}"
				);
			}
		}
	}
}

#[test]
fn overlapping_moves_on_three_tapes_match_concrete_membership() {
	let nfa = Nfa {
		arity: 3,
		initial: vec![0],
		finals: vec![false, true, true, true],
		edges: vec![
			(0, vec![A, Symbol::ANY, Symbol::ANY], 1),
			(0, vec![B, B, Symbol::ANY], 2),
			(0, vec![Symbol::ANY, A, B], 3),
		],
	};
	let mut formula = Formula::new();
	let leaf = formula.leaf(&nfa).expect("a three-tape leaf");
	let complement = formula.complement(leaf).expect("a symbolic complement");

	for bits in 0..64 {
		let step: Vec<Symbol> = (0..3).map(|tape| [A, B][(bits >> tape) & 1]).collect();
		assert_eq!(
			holds(&mut formula, complement, std::slice::from_ref(&step)),
			!nfa.accepts(&[step]),
			"mismatched one-step membership for {bits:#07b}"
		);
	}
}

#[test]
fn symbolic_sets_survive_relational_operations_and_materialize_concretely() {
	let mut formula = Formula::new();
	let empty = formula.leaf(&Nfa::moveless(1)).expect("a leaf");
	let one = formula.complement(empty).expect("one tape");
	let identity = formula.identity(one).expect("a binary relation");
	let composed = formula.compose(identity, identity).expect("two relations");
	let intersected = formula
		.intersection(identity, composed)
		.expect("matching arity");
	let synchronized = formula
		.sync_product(
			identity,
			composed,
			&[(0, 0), (1, 1)],
			&[Tape::Lhs(0), Tape::Lhs(1)],
		)
		.expect("matching layout");

	for term in [identity, composed, intersected, synchronized] {
		assert!(holds(&mut formula, term, &[vec![A, A]]));
		assert!(holds(&mut formula, term, &[vec![B, B]]));
		assert!(!holds(&mut formula, term, &[vec![A, B]]));
	}

	let binary = formula.leaf(&Nfa::moveless(2)).expect("a leaf");
	let binary = formula.complement(binary).expect("two tapes");
	let set_composition = formula.compose(binary, binary).expect("two relations");
	let set_intersection = formula
		.intersection(set_composition, binary)
		.expect("matching arity");
	for term in [set_composition, set_intersection] {
		for step in [vec![A, A], vec![A, B], vec![B, A], vec![B, B]] {
			assert!(holds(&mut formula, term, &[step]));
		}
	}

	let projected = formula.project(binary, &[1]).expect("a projection");
	assert!(holds(&mut formula, projected, &[vec![A]]));
	assert!(holds(&mut formula, projected, &[vec![B]]));

	let explicit = formula
		.materialize(identity, &Options::new())
		.expect("finite materialization");
	assert_eq!(explicit.num_moves(), 4);
	for initial in explicit.initial_states() {
		for (tuple, target) in explicit.moves(initial) {
			assert!(tuple.is_empty() || tuple[0] == tuple[1]);
			assert!(explicit.is_final(target));
		}
	}
}

#[test]
fn last_padding_witnesses_concrete_steps_from_symbolic_labels() {
	let mut formula = Formula::new();
	let empty = formula.leaf(&Nfa::moveless(1)).expect("a leaf");
	let one = formula.complement(empty).expect("one tape");
	let closed = formula.pad_closure(one, Pad::Last).expect("one tape");
	assert!(!holds(&mut formula, one, &[]));
	assert!(!holds(&mut formula, closed, &[]));

	for length in 1..=3 {
		for bits in 0..1_usize << length {
			let word: Vec<Vec<Symbol>> = (0..length)
				.map(|position| vec![[A, B][(bits >> position) & 1]])
				.collect();
			assert!(
				holds(&mut formula, closed, &word),
				"a nonempty word reaches itself with zero last-step repetitions: {word:?}"
			);
		}
	}
	let witness = formula
		.find_word(closed)
		.expect("no limits")
		.expect("one concrete step is accepted");
	assert!(
		witness
			.tape(0)
			.unwrap()
			.iter()
			.all(|symbol| symbol.is_concrete())
	);
}

#[test]
fn zero_arity_symbolic_labels_materialize_step_and_empty_word() {
	let mut formula = Formula::new();
	let empty = formula.leaf(&Nfa::moveless(1)).expect("a leaf");
	let one = formula.complement(empty).expect("one tape");
	let zero = formula.project(one, &[]).expect("zero tapes");

	let explicit = formula
		.materialize(zero, &Options::new())
		.expect("finite materialization");
	assert_eq!(explicit.num_moves(), 2);
	let initial = explicit.initial_states().next().expect("an initial state");
	let (tuple, target) = explicit.moves(initial).next().expect("the empty step");
	assert_eq!(tuple, []);
	assert!(explicit.is_final(target));
}

#[test]
fn symbolic_posts_can_be_evicted_without_changing_the_witness() {
	let mut formula = Formula::new();
	let empty = formula.leaf(&Nfa::moveless(1)).expect("a leaf");
	let one = formula.complement(empty).expect("one tape");
	let both = formula.intersection(one, one).expect("matching arity");

	let outcome = formula
		.check(both, &Options::new().cache_budget(0))
		.expect("cache eviction is not a semantic error");
	assert!(outcome.stats.dropped > 0);
	let witness = outcome.witness.expect("a one-step witness");
	assert_eq!(witness.tape(0), Some(vec![A]));
}

#[test]
fn wide_active_complements_grow_their_recursion_stack() -> Result<(), Box<dyn std::error::Error>> {
	// The covered part recurses once per tape before the first uncovered part splits off.
	let arity = 1 << 15;
	std::thread::Builder::new()
		.stack_size(64 << 10)
		.spawn(move || -> Result<(), mata_lazy::Error> {
			let mut formula = Formula::new();
			let leaf = formula.leaf(&Nfa::word(&[vec![A; arity]]))?;
			let complement = formula.complement(leaf)?;
			let explicit = formula.explicit(complement)?;
			let error = formula
				.check(explicit, &Options::new().max_states(2))
				.expect_err("the state budget stops after the deep partition");
			assert!(matches!(
				error,
				mata_lazy::Error::StateLimit { max_states: 2, .. }
			));

			Ok(())
		})?
		.join()
		.expect("the complement does not overflow its stack")?;

	Ok(())
}
