//! [rustfst](https://docs.rs/rustfst) transducers as leaves.
//!
//! A [`VectorFst`] or a [`ConstFst`] is a leaf of arity 2, where tape 0 reads the input labels and tape 1 the
//! output labels. Label 0 ([`EPS_LABEL`]) is padding, and label `n > 0` is the symbol `n`, so the largest
//! label is [`EPSILON`](Symbol::EPSILON) and the one below it [`ANY`](Symbol::ANY). An acceptor is the identity
//! relation of its language; `formula.project(leaf, &[0])` gives the language.
//!
//! Weights only decide whether a transition or a final weight exists. Zero ones are dropped, so the leaf
//! is the support of the weighted relation (for semirings without zero divisors). Like a Mata transducer,
//! an FST is read in lock step, while rustfst aligns epsilon asynchronously.
//!
//! ```
//! use lazymat::Formula;
//! use rustfst::fst_impls::VectorFst;
//! use rustfst::fst_traits::MutableFst;
//! use rustfst::semirings::{Semiring, TropicalWeight};
//! use rustfst::Tr;
//!
//! // a ↦ b, any number of times.
//! let mut fst = VectorFst::<TropicalWeight>::new();
//! let q = fst.add_state();
//! fst.set_start(q)?;
//! fst.set_final(q, TropicalWeight::one())?;
//! fst.add_tr(q, Tr::new(1, 2, TropicalWeight::new(0.5), q))?;
//!
//! let mut formula = Formula::new();
//! let relation = formula.leaf(&fst)?;
//! let outputs = formula.project(relation, &[1])?;
//! assert!(!formula.is_empty(outputs)?);
//! # Ok::<(), Box<dyn std::error::Error>>(())
//! ```

use rustfst::fst_impls::{ConstFst, VectorFst};
use rustfst::fst_traits::CoreFst;
use rustfst::semirings::Semiring;
use rustfst::{EPS_LABEL, Label, StateId};

use crate::{Automaton, Symbol};

fn symbol(label: Label) -> Symbol {
	if label == EPS_LABEL {
		Symbol::EPSILON
	} else {
		Symbol::new(label)
	}
}

macro_rules! fst_leaf {
	($($fst:ident),*) => {$(
		/// Arity 2, with input labels on tape 0 and output labels on tape 1.
		impl<W: Semiring> Automaton for $fst<W> {
			type State = StateId;

			fn arity(&self) -> usize {
				2
			}

			fn initial_states(&self) -> impl IntoIterator<Item = StateId> {
				self.start()
			}

			fn is_final(&self, state: StateId) -> bool {
				matches!(self.final_weight(state), Ok(Some(weight)) if !weight.is_zero())
			}

			fn for_each_move(&self, state: StateId, mut visit: impl FnMut(&[Symbol], StateId)) {
				let Ok(trs) = self.get_trs(state) else {
					return;
				};

				for tr in trs.iter() {
					if !tr.weight.is_zero() {
						visit(&[symbol(tr.ilabel), symbol(tr.olabel)], tr.nextstate);
					}
				}
			}
		}
	)*};
}

fst_leaf!(VectorFst, ConstFst);
