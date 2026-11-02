//! `Serialize` and `Deserialize` for [`Nfa`].

use serde::de::Error as _;
use serde::ser::SerializeStruct as _;
use serde::{Deserialize, Deserializer, Serialize, Serializer};

use crate::types::sorted;
use crate::{Nfa, NoSuchState, State, Transition};

/// The serialized form: the number of states, the initial and final states (ascending) and the
/// transitions (ordered by source, symbol and target).
#[derive(Deserialize)]
#[serde(rename = "Nfa")]
struct Repr {
	num_states: usize,
	initial: Vec<State>,
	#[serde(rename = "final")]
	final_states: Vec<State>,
	transitions: Vec<Transition>,
}

/// Serializes the transitions as a sequence, without collecting them first.
struct TransitionsOf<'a>(&'a Nfa);

impl Serialize for TransitionsOf<'_> {
	fn serialize<S: Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
		serializer.collect_seq(self.0.transitions())
	}
}

impl Serialize for Nfa {
	fn serialize<S: Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
		let mut repr = serializer.serialize_struct("Nfa", 4)?;
		repr.serialize_field("num_states", &self.num_states())?;
		repr.serialize_field("initial", &sorted(self.initial_states()))?;
		repr.serialize_field("final", &sorted(self.final_states()))?;
		repr.serialize_field("transitions", &TransitionsOf(self))?;
		repr.end()
	}
}

/// A number of states that cannot be allocated is an error, not a panic.
impl<'de> Deserialize<'de> for Nfa {
	fn deserialize<D: Deserializer<'de>>(deserializer: D) -> Result<Nfa, D::Error> {
		let repr = Repr::deserialize(deserializer)?;
		let mut nfa = Nfa::try_with_states(repr.num_states).map_err(D::Error::custom)?;
		let invalid = |error: NoSuchState| {
			D::Error::custom(format!(
				"state {} is not below num_states = {}",
				error.state, error.num_states
			))
		};
		for &initial in &repr.initial {
			nfa.add_initial(initial).map_err(invalid)?;
		}
		for &accepting in &repr.final_states {
			nfa.add_final(accepting).map_err(invalid)?;
		}
		nfa.add_transitions(repr.transitions).map_err(invalid)?;
		Ok(nfa)
	}
}
