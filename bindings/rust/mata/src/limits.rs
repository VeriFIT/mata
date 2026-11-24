//! Limits on the constructions that can blow up exponentially.

use mata_sys::ffi;

/// Limits that stop a construction before it grows too big, for example on untrusted input.
///
/// The `_within` methods of [`Nfa`](crate::Nfa) take them: [`determinize_within`], [`minimize_within`],
/// [`complement_within`] and [`difference_within`]. Each fails with
/// [`LimitExceeded`](crate::LimitExceeded) instead of building a deterministic automaton with more states
/// than [`Limits::max_states`] allows. The limit applies to every determinization that the operation runs
/// and to its result. Mata stops a determinization at the first state over the limit.
///
/// Other operations take no limits because Mata cannot stop them: the language checks, residual
/// reductions and the determinization of transducers.
///
/// [`determinize_within`]: crate::Nfa::determinize_within
/// [`minimize_within`]: crate::Nfa::minimize_within
/// [`complement_within`]: crate::Nfa::complement_within
/// [`difference_within`]: crate::Nfa::difference_within
///
/// # Examples
///
/// ```
/// use mata::{LimitExceeded, Limits, Nfa};
///
/// // The DFA must remember the last four letters: 16 states.
/// let nfa = Nfa::from_regex("(a|b)*a(a|b)(a|b)(a|b)")?;
/// let dfa = nfa.determinize_within(&Limits::new().max_states(16))?;
/// assert_eq!(dfa.num_states(), 16);
/// assert_eq!(
///     nfa.determinize_within(&Limits::new().max_states(15)).err(),
///     Some(LimitExceeded::States { max_states: 15 }),
/// );
/// # Ok::<(), mata::Error>(())
/// ```
#[derive(Clone, Debug, Default)]
pub struct Limits {
	max_states: Option<usize>,
}

impl Limits {
	/// No limits.
	#[must_use]
	pub const fn new() -> Limits {
		Limits { max_states: None }
	}

	/// At most `max_states` states in each deterministic automaton that the operation builds.
	#[must_use]
	pub const fn max_states(mut self, max_states: usize) -> Limits {
		self.max_states = Some(max_states);
		self
	}
}

impl From<&Limits> for ffi::Limits {
	fn from(limits: &Limits) -> ffi::Limits {
		ffi::Limits {
			max_states: limits.max_states.unwrap_or(usize::MAX),
		}
	}
}
