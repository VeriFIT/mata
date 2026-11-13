//! Concurrency: unrelated automata on several threads, and one automaton shared by several threads.
//!
//! The first test fails with a Mata whose automata share static buffers (before upstream #868).
//! Rust's standard library for Emscripten cannot spawn threads, so these tests do not run there.
#![cfg(not(target_os = "emscripten"))]

use std::sync::{Arc, Barrier};
use std::thread;

use mata::{Alphabet, Error, Nfa, State, Symbol};

const THREADS: usize = 8;
const ROUNDS: usize = 300;

fn workload(seed: usize) -> (usize, bool) {
	let pattern = format!("(a|b)*a(a|b){{{}}}", 2 + seed % 4);
	let nfa = Nfa::from_regex(&pattern).unwrap();
	let dfa = nfa.determinize();
	let minimal = dfa.minimize();
	let sigma = Alphabet::from_symbols([Symbol::from(b'a'), Symbol::from(b'b')]).unwrap();
	let complement = minimal.complement(&sigma);
	let consistent =
		nfa.is_equivalent_to(&minimal) && complement.intersection(&nfa).is_lang_empty();
	(minimal.num_states(), consistent)
}

#[test]
fn independent_automata_on_many_threads() {
	let expected: Vec<usize> = (0..THREADS).map(|seed| workload(seed).0).collect();
	let start = Arc::new(Barrier::new(THREADS));
	let handles: Vec<_> = (0..THREADS)
		.map(|seed| {
			let start = Arc::clone(&start);
			thread::spawn(move || {
				start.wait();
				(0..ROUNDS / 10).all(|_| {
					let (states, consistent) = workload(seed);
					consistent && states == 1 << (3 + seed % 4)
				})
			})
		})
		.collect();
	for (seed, handle) in handles.into_iter().enumerate() {
		assert!(handle.join().unwrap(), "thread {seed} saw a wrong result");
	}
	assert_eq!(
		expected,
		(0..THREADS)
			.map(|seed| 1 << (3 + seed % 4))
			.collect::<Vec<_>>()
	);
}

#[test]
fn one_automaton_read_by_many_threads() -> Result<(), Error> {
	let nfa = Arc::new(Nfa::from_regex("(a|b)*a(a|b){4}")?);
	let word: Vec<Symbol> = b"bbaabab".iter().copied().map(Symbol::from).collect();
	let dfa_states = nfa.determinize().num_states();
	let successors = nfa.successors(State::new(0), Symbol::from(b'a')).to_vec();
	let start = Arc::new(Barrier::new(THREADS));
	let handles: Vec<_> = (0..THREADS)
		.map(|_| {
			let (nfa, start) = (Arc::clone(&nfa), Arc::clone(&start));
			let (word, successors) = (word.clone(), successors.clone());
			thread::spawn(move || {
				start.wait();
				(0..ROUNDS).all(|_| {
					nfa.accepts(&word)
						&& nfa.successors(State::new(0), Symbol::from(b'a')) == successors
				}) && nfa.determinize().num_states() == dfa_states
			})
		})
		.collect();
	for handle in handles {
		assert!(handle.join().unwrap());
	}
	Ok(())
}

#[test]
fn automata_move_between_threads() -> Result<(), Error> {
	let nfa = Nfa::from_regex("abc")?;
	let moved = thread::spawn(move || {
		let mut nfa = nfa;
		nfa.trim();
		nfa
	})
	.join()
	.unwrap();
	assert!(moved.accepts(&b"abc".map(Symbol::from)));
	Ok(())
}

#[test]
fn transducers_on_many_threads() {
	use mata::nft::Nft;
	fn make() -> Nft {
		let (a, b) = (Symbol::from(b'a'), Symbol::from(b'b'));
		let mut t = Nft::new(2).unwrap();
		let q = t.add_state();
		t.add_initial(q).unwrap();
		t.add_final(q).unwrap();
		t.add_transition(q, &[a, b], q).unwrap();
		t.add_transition(q, &[b, b], q).unwrap();
		t
	}
	let (a, b) = (Symbol::from(b'a'), Symbol::from(b'b'));
	let shared = Arc::new(make());
	let start = Arc::new(Barrier::new(THREADS));
	let handles: Vec<_> = (0..THREADS)
		.map(|_| {
			let (shared, start) = (Arc::clone(&shared), Arc::clone(&start));
			thread::spawn(move || {
				start.wait();
				(0..ROUNDS / 10).all(|_| {
					let own = make();
					let composed = own.compose(&shared).unwrap();
					let intersected = own.intersection(&shared).unwrap();
					let image = own.apply(&Nfa::from_regex("a*").unwrap(), 0).unwrap();
					composed.accepts(&[[a, b, a], [b, b, b]])
						&& intersected.is_equivalent_to(&own).unwrap()
						&& image
							.to_nfa()
							.is_equivalent_to(&Nfa::from_regex("b*").unwrap())
				})
			})
		})
		.collect();
	for handle in handles {
		assert!(handle.join().unwrap());
	}
}
