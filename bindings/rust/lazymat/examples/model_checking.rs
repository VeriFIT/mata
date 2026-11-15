//! Regular model checking with an inductive invariant.
//!
//! The inputs are initial and unsafe configurations over Σ, an abstract transition relation δ over Γ, and an
//! interpretation v ⊆ Γ* × Σ*. The proof rule is as follows.
//!
//! - ind = ¬π₀(id(Γ*) ∩ (δ ∘ ¬v ∘ v⁻¹))  (abstract states whose successors stay interpretable);
//! - preach = ¬(v⁻¹ ∘ id(ind) ∘ ¬v)      (concrete states that all interpretations keep in ind);
//! - post(initial, preach) ∩ unsafe = ∅ => safe, unknown otherwise.

use std::error::Error;
use std::time::Instant;

use lazymat::{Formula, Options, Term};
use mata::nft::Nft;
use mata::{Alphabet, Nfa, NftError, NoSuchState, State, Symbol};

fn word(symbol: Symbol) -> Result<Nfa, NoSuchState> {
	let mut nfa = Nfa::with_states(2);
	nfa.add_initial(State::new(0))?;
	nfa.add_final(State::new(1))?;
	nfa.add_transition(State::new(0), symbol, State::new(1))?;

	Ok(nfa)
}

fn star(symbols: &[Symbol]) -> Result<Nfa, NoSuchState> {
	let mut nfa = Nfa::with_states(1);
	nfa.add_initial(State::new(0))?;
	nfa.add_final(State::new(0))?;
	for &symbol in symbols {
		nfa.add_transition(State::new(0), symbol, State::new(0))?;
	}

	Ok(nfa)
}

/// The relation `{(x₁…xₙ, y₁…yₙ) | every (xᵢ, yᵢ) is in pairs}`.
fn letterwise(pairs: &[(Symbol, Symbol)]) -> Result<Nft, NftError> {
	let mut nft = Nft::new(2)?;
	let state = nft.add_state();
	nft.add_initial(state)?;
	nft.add_final(state)?;
	for &(input, output) in pairs {
		nft.add_transition(state, &[input, output], state)?;
	}

	Ok(nft)
}

/// The formula and its root, the unsafe configurations that may be reachable.
///
/// Σ = {0, 1} and Γ = {A, B} share `names`, so A and B get the symbols after 0 and 1. The symbols 0 and 1
/// have no names and print as numbers.
fn build(names: &mut Alphabet) -> Result<(Formula, Term), Box<dyn Error>> {
	let [zero, one] = [0, 1].map(Symbol::new);
	names.insert(zero)?;
	names.insert(one)?;
	let [a, b] = [names.insert_name("A")?, names.insert_name("B")?];

	// δ takes A to B and B to B, and v reads A as 0 and B as 1.
	let delta = letterwise(&[(a, b), (b, b)])?;
	let v = letterwise(&[(a, zero), (b, one)])?;

	let mut formula = Formula::new();
	let initial = formula.leaf(&word(one)?)?;
	let unsafe_configurations = formula.leaf(&word(zero)?)?;
	let gamma_star = formula.leaf(&star(&[a, b])?)?;
	let delta = formula.leaf(&delta)?;
	let v = formula.leaf(&v)?;

	let not_v = formula.complement(v)?;
	let delta_not_v = formula.compose(delta, not_v)?;
	let conflicts = formula.compose_on(delta_not_v, v, &[(1, 1)])?;
	let id_gamma = formula.identity(gamma_star)?;
	let bad_pairs = formula.intersection(id_gamma, conflicts)?;
	let bad_sources = formula.project(bad_pairs, &[0])?;

	// Built explicitly, as it is small and saves work later. Without this line, the query is fully lazy.
	let bad_sources = formula.explicit(bad_sources)?;
	let ind = formula.complement(bad_sources)?;

	let id_ind = formula.identity(ind)?;
	let v_inverse_ind = formula.compose_on(v, id_ind, &[(0, 0)])?;
	let preach_bad = formula.compose(v_inverse_ind, not_v)?;
	let preach = formula.complement(preach_bad)?;

	let reachable = formula.post_image(initial, preach)?;
	let unsafe_reachable = formula.intersection(reachable, unsafe_configurations)?;

	Ok((formula, unsafe_reachable))
}

fn main() -> Result<(), Box<dyn Error>> {
	let mut names = Alphabet::new();
	let (formula, unsafe_reachable) = build(&mut names)?;

	// Up to here no automata work was done; the query explores what it needs.
	let start = Instant::now();
	let outcome = formula.check(unsafe_reachable, &Options::new())?;
	println!(
		"safe = {}, {} macrostates, {} explored, {} pruned, {:?}",
		outcome.is_empty(),
		outcome.stats.macrostates,
		outcome.stats.explored,
		outcome.stats.pruned,
		start.elapsed()
	);

	for node in &outcome.stats.explicit {
		println!(
			"  explicit node: {} states, {} transitions",
			node.states, node.transitions
		);
	}

	if let Some(witness) = outcome.witness {
		println!(
			"  reachable unsafe configuration: {}",
			witness.display_with(&names)
		);
	}

	Ok(())
}
