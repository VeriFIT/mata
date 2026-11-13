//! Random formulas against an eager evaluator written independently of the engine.
//!
//! Every tape has the alphabet {0, 1}; the oracle writes padding as 2 and expands wildcards at the leaves.

use std::collections::{BTreeSet, HashMap, VecDeque};
use std::hash::Hash;
use std::str::FromStr;

use lazymat::{Error, Formula, Options, Outcome, Symbol, Tape, Term, Witness};
use mata::State;
use mata::nft::{DONT_CARE, Nft};
use rand::rngs::StdRng;
use rand::{RngExt, SeedableRng};

const PADDING: u8 = 2;
const SIGMA: [u8; 2] = [0, 1];
/// Each complement is a subset construction in the oracle, so a formula gets few of them.
const MAX_COMPLEMENTS: usize = 2;
/// The language comparison of a materialized term is exponential, so it runs on small automata only.
const MAX_COMPARED_ORACLE_STATES: usize = 64;
const MAX_COMPARED_NFT_STATES: usize = 1024;

/// The symbols that one step reads, one per tape.
type Tuple = Vec<u8>;
type StateSet = BTreeSet<usize>;

/// An explicit automaton over tuples of 0, 1 and padding.
#[derive(Clone, Debug)]
struct Eager {
	arity: usize,
	initial: Vec<usize>,
	finals: Vec<bool>,
	moves: Vec<Vec<(Tuple, usize)>>,
}

impl Eager {
	fn new(arity: usize) -> Eager {
		Eager {
			arity,
			initial: Vec::new(),
			finals: Vec::new(),
			moves: Vec::new(),
		}
	}

	fn num_states(&self) -> usize {
		self.finals.len()
	}

	fn add_state(&mut self, is_final: bool) -> usize {
		self.finals.push(is_final);
		self.moves.push(Vec::new());

		self.num_states() - 1
	}

	fn initial_set(&self) -> StateSet {
		self.initial.iter().copied().collect()
	}

	fn has_final(&self, states: &StateSet) -> bool {
		states.iter().any(|&state| self.finals[state])
	}

	fn is_empty(&self) -> bool {
		let mut seen = vec![false; self.num_states()];
		let mut stack = self.initial.clone();

		while let Some(state) = stack.pop() {
			if seen[state] {
				continue;
			}

			seen[state] = true;

			if self.finals[state] {
				return false;
			}

			stack.extend(self.moves[state].iter().map(|(_, target)| *target));
		}

		true
	}

	fn step(&self, states: &StateSet, tuple: &[u8]) -> StateSet {
		states
			.iter()
			.flat_map(|&state| &self.moves[state])
			.filter(|(read, _)| read == tuple)
			.map(|(_, target)| *target)
			.collect()
	}

	fn accepts(&self, word: &[Tuple]) -> bool {
		let mut states = self.initial_set();
		for tuple in word {
			states = self.step(&states, tuple);
		}

		self.has_final(&states)
	}

	fn union(&self, other: &Eager) -> Eager {
		let offset = self.num_states();
		let mut result = self.clone();
		result
			.initial
			.extend(other.initial.iter().map(|state| state + offset));
		result.finals.extend(&other.finals);

		for moves in &other.moves {
			let shifted = moves
				.iter()
				.map(|(tuple, target)| (tuple.clone(), target + offset))
				.collect();
			result.moves.push(shifted);
		}

		result
	}

	/// The reachable pairs of states; `combine` gives the tuple that two moves read together, if any.
	fn product(
		&self,
		other: &Eager,
		arity: usize,
		combine: impl Fn(&[u8], &[u8]) -> Option<Tuple>,
	) -> Eager {
		let is_final = |(lhs, rhs): (usize, usize)| self.finals[lhs] && other.finals[rhs];
		let mut result = Eager::new(arity);
		let mut worklist = Worklist::new();

		for &lhs in &self.initial {
			for &rhs in &other.initial {
				let pair = (lhs, rhs);
				let state = worklist.find_or_add(&mut result, pair, is_final(pair));
				result.initial.push(state);
			}
		}

		while let Some(((lhs, rhs), source)) = worklist.queue.pop_front() {
			for (lhs_tuple, lhs_target) in &self.moves[lhs] {
				for (rhs_tuple, rhs_target) in &other.moves[rhs] {
					if let Some(tuple) = combine(lhs_tuple, rhs_tuple) {
						let pair = (*lhs_target, *rhs_target);
						let target = worklist.find_or_add(&mut result, pair, is_final(pair));
						result.moves[source].push((tuple, target));
					}
				}
			}
		}

		result
	}

	fn intersection(&self, other: &Eager) -> Eager {
		self.product(other, self.arity, |lhs, rhs| {
			(lhs == rhs).then(|| lhs.to_vec())
		})
	}

	fn sync(&self, other: &Eager, pairs: &[(usize, usize)], layout: &[Tape]) -> Eager {
		self.product(other, layout.len(), |lhs, rhs| {
			let synchronized = pairs
				.iter()
				.all(|&(lhs_tape, rhs_tape)| lhs[lhs_tape] == rhs[rhs_tape]);

			synchronized.then(|| {
				layout
					.iter()
					.map(|&tape| match tape {
						Tape::Lhs(index) => lhs[index],
						Tape::Rhs(index) => rhs[index],
					})
					.collect()
			})
		})
	}

	/// The subset construction over the tuples of `SIGMA`, which never read padding.
	fn complement(&self) -> Eager {
		let universe = cartesian(&vec![SIGMA.to_vec(); self.arity]);
		let mut result = Eager::new(self.arity);
		let mut worklist = Worklist::new();
		let start = self.initial_set();
		let rejects = !self.has_final(&start);
		let initial = worklist.find_or_add(&mut result, start, rejects);
		result.initial.push(initial);

		while let Some((states, source)) = worklist.queue.pop_front() {
			for tuple in &universe {
				let next = self.step(&states, tuple);
				let rejects = !self.has_final(&next);
				let target = worklist.find_or_add(&mut result, next, rejects);
				result.moves[source].push((tuple.clone(), target));
			}
		}

		result
	}

	/// The same automaton over `arity` tapes, each tuple replaced by `map(tuple)`.
	fn map_tuples(&self, arity: usize, map: impl Fn(&[u8]) -> Tuple) -> Eager {
		let mut result = self.clone();
		result.arity = arity;
		for (tuple, _) in result.moves.iter_mut().flatten() {
			*tuple = map(tuple);
		}

		result
	}

	fn identity(&self) -> Eager {
		self.map_tuples(2, |tuple| vec![tuple[0], tuple[0]])
	}

	fn project(&self, tapes: &[usize]) -> Eager {
		self.map_tuples(tapes.len(), |tuple| {
			tapes.iter().map(|&tape| tuple[tape]).collect()
		})
	}

	/// A shortest word in exactly one of the languages, over all tuples (padding included).
	fn distinguishing(&self, other: &Eager) -> Option<Vec<Tuple>> {
		let tuples = cartesian(&vec![vec![0, 1, PADDING]; self.arity]);
		let mut seen = BTreeSet::new();
		let mut queue = VecDeque::from([(self.initial_set(), other.initial_set(), Vec::new())]);

		while let Some((ours, theirs, word)) = queue.pop_front() {
			if !seen.insert((ours.clone(), theirs.clone())) {
				continue;
			}

			if self.has_final(&ours) != other.has_final(&theirs) {
				return Some(word);
			}

			for tuple in &tuples {
				let mut longer = word.clone();
				longer.push(tuple.clone());
				queue.push_back((self.step(&ours, tuple), other.step(&theirs, tuple), longer));
			}
		}

		None
	}

	/// The relation of a transducer whose transitions have one edge per level.
	fn from_nft(nft: &Nft) -> Eager {
		/// Collects the transitions that continue `tuple` from `state` into `moves`.
		fn walk(
			nft: &Nft,
			state: State,
			tuple: &mut Vec<mata::Symbol>,
			moves: &mut Vec<(Tuple, State)>,
		) {
			for edge in nft.edges_from(state) {
				tuple.push(edge.symbol);
				if nft.level(edge.target) == Some(0) {
					moves.extend(
						expansions(tuple)
							.into_iter()
							.map(|expanded| (expanded, edge.target)),
					);
				} else {
					walk(nft, edge.target, tuple, moves);
				}

				tuple.pop();
			}
		}

		let mut result = Eager::new(nft.num_levels());
		let level_0: Vec<State> = nft
			.states()
			.filter(|&state| nft.level(state) == Some(0))
			.collect();
		let mut ids = HashMap::new();
		for &state in &level_0 {
			ids.insert(state, result.add_state(nft.is_final(state)));
		}

		result.initial = nft
			.initial_states()
			.iter()
			.map(|state| ids[state])
			.collect();

		for &state in &level_0 {
			let mut moves = Vec::new();
			walk(nft, state, &mut Vec::new(), &mut moves);
			result.moves[ids[&state]] = moves
				.into_iter()
				.map(|(tuple, target)| (tuple, ids[&target]))
				.collect();
		}

		result
	}
}

/// The states of a construction on the fly, numbered when found and queued until their moves are added.
struct Worklist<K> {
	ids: HashMap<K, usize>,
	queue: VecDeque<(K, usize)>,
}

impl<K: Clone + Eq + Hash> Worklist<K> {
	fn new() -> Worklist<K> {
		Worklist {
			ids: HashMap::new(),
			queue: VecDeque::new(),
		}
	}

	/// The state of `key` in `automaton`; a new key gets a new state, which is queued.
	fn find_or_add(&mut self, automaton: &mut Eager, key: K, is_final: bool) -> usize {
		if let Some(&state) = self.ids.get(&key) {
			return state;
		}

		let state = automaton.add_state(is_final);
		self.ids.insert(key.clone(), state);
		self.queue.push_back((key, state));

		state
	}
}

/// The oracle's symbols for a Mata symbol.
fn expand(symbol: mata::Symbol) -> Vec<u8> {
	match symbol {
		mata::Symbol::EPSILON => vec![PADDING],
		DONT_CARE => SIGMA.to_vec(),
		symbol => vec![u8::try_from(symbol.value()).expect("a symbol of SIGMA")],
	}
}

/// The oracle tuples of a Mata tuple.
fn expansions(tuple: &[mata::Symbol]) -> Vec<Tuple> {
	let choices: Vec<Vec<u8>> = tuple.iter().copied().map(expand).collect();

	cartesian(&choices)
}

/// Every tuple that takes one of `choices[tape]` on each tape.
fn cartesian(choices: &[Vec<u8>]) -> Vec<Tuple> {
	choices
		.iter()
		.fold(vec![Vec::new()], |prefixes, tape_choices| {
			prefixes
				.iter()
				.flat_map(|prefix| {
					tape_choices.iter().map(move |&choice| {
						let mut tuple = prefix.clone();
						tuple.push(choice);

						tuple
					})
				})
				.collect()
		})
}

/// A wildcard, padding (for a transducer), or a symbol of `SIGMA`.
fn random_symbol(rng: &mut StdRng, arity: usize) -> mata::Symbol {
	match rng.random_range(0..10) {
		0 => DONT_CARE,
		1 if arity > 1 => mata::Symbol::EPSILON,
		roll => mata::Symbol::new(u32::from(roll % 2 == 0)),
	}
}

/// A random transducer, and the same relation for the oracle.
fn random_leaf(rng: &mut StdRng, arity: usize) -> (Nft, Eager) {
	let mut nft = Nft::new(arity).expect("1 to MAX_LEVELS levels");
	let mut eager = Eager::new(arity);
	let num_states: usize = rng.random_range(1..=3);
	let states: Vec<State> = (0..num_states).map(|_| nft.add_state()).collect();

	for (index, &state) in states.iter().enumerate() {
		let is_final = rng.random_bool(0.4);
		if is_final {
			nft.add_final(state).expect("a level-0 state");
		}

		eager.add_state(is_final);
		let is_initial = match index {
			0 => true,
			1 => rng.random_bool(0.3),
			_ => false,
		};

		if is_initial {
			nft.add_initial(state).expect("a level-0 state");
			eager.initial.push(index);
		}
	}

	for _ in 0..rng.random_range(0..=5) {
		let source = rng.random_range(0..num_states);
		let target = rng.random_range(0..num_states);
		let mut tuple: Vec<mata::Symbol> = (0..arity).map(|_| random_symbol(rng, arity)).collect();
		// A move that pads every tape would be silent.
		if tuple.iter().all(|symbol| symbol.is_epsilon()) {
			tuple[0] = mata::Symbol::new(0);
		}

		nft.add_transition(states[source], &tuple, states[target])
			.expect("a transition of the NFT");
		eager.moves[source].extend(
			expansions(&tuple)
				.into_iter()
				.map(|expanded| (expanded, target)),
		);
	}

	(nft, eager)
}

/// Random formulas, built in a [`Formula`] and in the oracle side by side.
struct Generator {
	rng: StdRng,
	formula: Formula,
	/// The terms that one query marks explicit.
	terms: Vec<Term>,
	complements: usize,
}

impl Generator {
	fn new(seed: u64) -> Generator {
		Generator {
			rng: StdRng::seed_from_u64(seed),
			formula: Formula::new(),
			terms: Vec::new(),
			complements: 0,
		}
	}

	fn leaf(&mut self, arity: usize) -> Result<(Term, Eager), Error> {
		let (nft, eager) = random_leaf(&mut self.rng, arity);

		Ok((self.formula.leaf_over(&nft, [SIGMA])?, eager))
	}

	/// A random term of `arity` (1 or 2), at most `depth` operations deep, and its oracle.
	fn generate(&mut self, arity: usize, depth: usize) -> Result<(Term, Eager), Error> {
		if depth == 0 || self.rng.random_bool(0.2) {
			return self.leaf(arity);
		}

		let (term, eager) = match self.rng.random_range(0..8) {
			0 => {
				let (lhs, lhs_eager) = self.generate(arity, depth - 1)?;
				let (rhs, rhs_eager) = self.generate(arity, depth - 1)?;

				(self.formula.union(lhs, rhs)?, lhs_eager.union(&rhs_eager))
			}
			1 | 2 => {
				let (lhs, lhs_eager) = self.generate(arity, depth - 1)?;
				let (rhs, rhs_eager) = self.generate(arity, depth - 1)?;

				(
					self.formula.intersection(lhs, rhs)?,
					lhs_eager.intersection(&rhs_eager),
				)
			}
			3 if self.complements < MAX_COMPLEMENTS => {
				self.complements += 1;
				let (operand, operand_eager) = self.generate(arity, depth - 1)?;

				(
					self.formula.complement(operand)?,
					operand_eager.complement(),
				)
			}
			4 if arity == 2 => {
				let (language, language_eager) = self.generate(1, depth - 1)?;

				(self.formula.identity(language)?, language_eager.identity())
			}
			5 => {
				// One tape of a binary relation, or both in either order.
				let first = self.rng.random_range(0..2);
				let tapes = if arity == 1 {
					vec![first]
				} else {
					vec![first, 1 - first]
				};

				let (relation, relation_eager) = self.generate(2, depth - 1)?;

				(
					self.formula.project(relation, &tapes)?,
					relation_eager.project(&tapes),
				)
			}
			6 => self.sync(arity, depth)?,
			7 => {
				let (operand, operand_eager) = self.generate(arity, depth - 1)?;

				(self.formula.explicit(operand)?, operand_eager)
			}
			_ => self.leaf(arity)?,
		};

		self.terms.push(term);

		Ok((term, eager))
	}

	/// A synchronized product of random operands, with a random layout of `arity` tapes.
	fn sync(&mut self, arity: usize, depth: usize) -> Result<(Term, Eager), Error> {
		let lhs_arity = self.rng.random_range(1..=2);
		let rhs_arity = self.rng.random_range(1..=2);
		let pairs: Vec<(usize, usize)> = if self.rng.random_bool(0.8) {
			let lhs_tape = self.rng.random_range(0..lhs_arity);
			let rhs_tape = self.rng.random_range(0..rhs_arity);

			vec![(lhs_tape, rhs_tape)]
		} else {
			Vec::new()
		};

		// A synchronized pair gives one result tape, so the right tape of a pair is left out.
		let unsynchronized_rhs = (0..rhs_arity)
			.filter(|&tape| !pairs.iter().any(|&(_, rhs_tape)| rhs_tape == tape))
			.map(Tape::Rhs);
		let mut candidates: Vec<Tape> = (0..lhs_arity)
			.map(Tape::Lhs)
			.chain(unsynchronized_rhs)
			.collect();
		if candidates.len() < arity {
			return self.leaf(arity);
		}

		let mut layout = Vec::new();
		for _ in 0..arity {
			let index = self.rng.random_range(0..candidates.len());
			layout.push(candidates.remove(index));
		}

		let (lhs, lhs_eager) = self.generate(lhs_arity, depth - 1)?;
		let (rhs, rhs_eager) = self.generate(rhs_arity, depth - 1)?;

		Ok((
			self.formula.sync_product(lhs, rhs, &pairs, &layout)?,
			lhs_eager.sync(&rhs_eager, &pairs, &layout),
		))
	}
}

/// The steps of `witness` as oracle tuples.
fn word(witness: &Witness) -> Vec<Tuple> {
	witness
		.steps()
		.map(|step| {
			step.iter()
				.map(|&symbol| match symbol {
					Symbol::EPSILON => PADDING,
					Symbol::ANY => panic!("witnesses are concrete"),
					symbol => u8::try_from(symbol.value()).expect("a symbol of SIGMA"),
				})
				.collect()
		})
		.collect()
}

/// The value of the environment variable `name`, or `default` if it is unset.
fn setting<T: FromStr>(name: &str, default: T) -> T {
	std::env::var(name).map_or(default, |value| {
		value
			.parse()
			.unwrap_or_else(|_| panic!("{name} is not a number: {value}"))
	})
}

/// The number of random formulas per test; `LAZYMAT_ORACLE_SEEDS` scales the test up.
fn seeds() -> u64 {
	setting("LAZYMAT_ORACLE_SEEDS", 1500)
}

/// The depth of the random formulas; `LAZYMAT_ORACLE_DEPTH` scales the test up.
fn depth() -> usize {
	setting("LAZYMAT_ORACLE_DEPTH", 3)
}

/// Checks the query of `root` against `eager` in every mode, and returns whether `root` is empty.
fn assert_agrees(
	formula: &Formula,
	terms: &[Term],
	root: Term,
	eager: &Eager,
	seed: u64,
) -> Result<bool, Error> {
	let empty = eager.is_empty();

	let outcome = formula.check(root, &Options::new())?;
	assert_eq!(outcome.is_empty(), empty, "seed {seed}");
	if let Some(witness) = &outcome.witness {
		assert!(
			eager.accepts(&word(witness)),
			"seed {seed}: witness {witness}"
		);
	}

	let plain = formula.check(root, &Options::new().subsumption(false))?;
	assert_eq!(plain.is_empty(), empty, "seed {seed}, no subsumption");

	let no_antichain = formula.check(root, &Options::new().antichain(false))?;
	assert_eq!(no_antichain.is_empty(), empty, "seed {seed}, no antichain");
	if let Some(witness) = &no_antichain.witness {
		assert!(
			eager.accepts(&word(witness)),
			"seed {seed}, no antichain: witness {witness}"
		);
	}

	// After the first query built the explicit nodes, a query without a cache computes every post again.
	let cached = formula.check(root, &Options::new())?;
	let uncached = formula.check(root, &Options::new().cache_budget(0))?;
	let counts = |outcome: &Outcome| {
		(
			outcome.stats.macrostates,
			outcome.stats.explored,
			outcome.stats.pruned,
		)
	};
	assert_eq!(uncached.witness, cached.witness, "seed {seed}, no cache");
	assert_eq!(counts(&uncached), counts(&cached), "seed {seed}, no cache");

	let all_explicit = terms
		.iter()
		.fold(Options::new(), |options, &term| options.explicit(term));
	assert_eq!(
		formula.check(root, &all_explicit)?.is_empty(),
		empty,
		"seed {seed}, all explicit"
	);

	let materialized = formula.materialize(root, &Options::new())?;
	if eager.num_states() <= MAX_COMPARED_ORACLE_STATES
		&& materialized.num_states() <= MAX_COMPARED_NFT_STATES
	{
		let difference = Eager::from_nft(&materialized).distinguishing(eager);
		assert!(
			difference.is_none(),
			"seed {seed}, materialized language differs on {difference:?}"
		);
	}

	let mut reimported = Formula::new();
	let leaf = reimported.leaf_over(&materialized, [SIGMA])?;
	assert_eq!(
		reimported.is_empty(leaf)?,
		empty,
		"seed {seed}, re-imported"
	);

	Ok(empty)
}

#[test]
fn random_formulas_agree_with_the_oracle() -> Result<(), Error> {
	let (seeds, depth) = (seeds(), depth());
	let mut non_empty = 0;

	for seed in 0..seeds {
		let mut generator = Generator::new(seed);
		let arity = generator.rng.random_range(1..=2);
		let (root, eager) = generator.generate(arity, depth)?;
		if !assert_agrees(&generator.formula, &generator.terms, root, &eager, seed)? {
			non_empty += 1;
		}
	}

	assert!(
		non_empty > seeds / 5 && non_empty < seeds * 4 / 5,
		"{non_empty} of {seeds} non-empty"
	);

	Ok(())
}

/// `project(id(L) ∩ X, [i])` over a sync product `X`, with the operands either way, the shape of a diagonal node.
#[test]
fn diagonals_of_products_agree_with_the_oracle() -> Result<(), Error> {
	let depth = depth().max(2);

	for seed in 0..seeds() {
		let mut generator = Generator::new(seed);
		let (language, language_eager) = generator.generate(1, depth - 1)?;
		let (relation, relation_eager) = generator.sync(2, depth - 1)?;
		let tape = generator.rng.random_range(0..2);
		let swap = generator.rng.random_bool(0.5);

		let identity = generator.formula.identity(language)?;
		let (lhs, rhs) = if swap {
			(relation, identity)
		} else {
			(identity, relation)
		};

		let both = generator.formula.intersection(lhs, rhs)?;
		let root = generator.formula.project(both, &[tape])?;

		let eager = language_eager
			.identity()
			.intersection(&relation_eager)
			.project(&[tape]);
		assert_agrees(&generator.formula, &generator.terms, root, &eager, seed)?;
	}

	Ok(())
}
