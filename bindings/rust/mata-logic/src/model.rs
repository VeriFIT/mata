//! Satisfying assignments.

use mata_lazy::Witness;

use crate::Var;
use crate::logic::ONE;

/// An assignment, as the bits of one encoding of each variable's value, in the order they were read.
#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub struct Model {
	vars: Box<[Var]>,
	words: Vec<Vec<bool>>,
}

impl Model {
	pub(crate) fn new(vars: Box<[Var]>, witness: &Witness) -> Model {
		let words = (0..vars.len())
			.map(|tape| {
				witness
					.tape(tape)
					.expect("a tape per variable")
					.into_iter()
					.map(|symbol| symbol == ONE)
					.collect()
			})
			.collect();

		Model { vars, words }
	}

	/// The variables of the model, ascending: the free variables of the prop.
	#[must_use]
	pub fn vars(&self) -> &[Var] {
		&self.vars
	}

	/// The bits that encode the value of `var`, or `None` if `var` is not a variable of the model. All variables
	/// have the same number of bits.
	#[must_use]
	pub fn bits(&self, var: Var) -> Option<&[bool]> {
		let tape = self.vars.binary_search(&var).ok()?;
		Some(&self.words[tape])
	}
}
