//! Leaves from your own automata. Implement [`Automaton`] and pass the automaton to the formula's `leaf`.
//!
//! An automaton counts letters modulo a number, a transducer swaps `a` and `b`, and the queries ask what
//! swapping does to the counts.

use lazymat::{Automaton, Error, Formula, Symbol, Term, Witness};

/// Words over {a, b} in which `letter` occurs a multiple of `modulus` times.
struct CountModulo {
	letter: u8,
	modulus: u32,
}

impl Automaton for CountModulo {
	/// The number of `letter`s read so far, modulo `modulus`.
	type State = u32;

	fn arity(&self) -> usize {
		1
	}

	fn initial_states(&self) -> impl IntoIterator<Item = u32> {
		[0]
	}

	fn is_final(&self, state: u32) -> bool {
		state == 0
	}

	fn for_each_move(&self, state: u32, mut visit: impl FnMut(&[Symbol], u32)) {
		for letter in *b"ab" {
			let next = if letter == self.letter {
				(state + 1) % self.modulus
			} else {
				state
			};

			visit(&[Symbol::from(letter)], next);
		}
	}
}

/// The relation that swaps `a` and `b`, with the input word on tape 0 and the output word on tape 1.
struct Swap;

impl Automaton for Swap {
	type State = ();

	fn arity(&self) -> usize {
		2
	}

	fn initial_states(&self) -> impl IntoIterator<Item = ()> {
		[()]
	}

	fn is_final(&self, (): ()) -> bool {
		true
	}

	fn for_each_move(&self, (): (), mut visit: impl FnMut(&[Symbol], ())) {
		let (a, b) = (Symbol::from(b'a'), Symbol::from(b'b'));
		visit(&[a, b], ());
		visit(&[b, a], ());
	}
}

/// A word of `language` outside `property`, if there is one.
fn outside(
	formula: &mut Formula,
	language: Term,
	property: Term,
) -> Result<Option<Witness>, Error> {
	let not_property = formula.complement(property)?;
	let difference = formula.intersection(language, not_property)?;

	formula.find_word(difference)
}

/// The word on tape 0, with bytes shown as characters.
fn text(witness: &Witness) -> String {
	witness
		.tape(0)
		.unwrap_or_default()
		.into_iter()
		.map(|symbol| char::from_u32(symbol.value()).unwrap_or('?'))
		.collect()
}

fn main() -> Result<(), Error> {
	let mut formula = Formula::new();
	let a_count = formula.leaf(&CountModulo {
		letter: b'a',
		modulus: 3,
	})?;
	let b_count = formula.leaf(&CountModulo {
		letter: b'b',
		modulus: 3,
	})?;
	let swap = formula.leaf(&Swap)?;

	// The words with a multiple of 3 `b`s, swapped.
	let swapped = formula.post_image(b_count, swap)?;

	for (letter, count) in [('a', a_count), ('b', b_count)] {
		match outside(&mut formula, swapped, count)? {
			None => println!("{letter}: always a multiple of 3 after the swap"),
			Some(witness) => println!("{letter}: not a multiple of 3 in {:?}", text(&witness)),
		}
	}

	Ok(())
}
