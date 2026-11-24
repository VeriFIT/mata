//! Query options, limits and results.

use std::time::Duration;

use crate::{Symbol, Term, Witness};

/// Options of one query ([`check`](crate::Formula::check), `materialize`).
///
/// ```
/// use mata_lazy::Options;
/// use std::time::Duration;
///
/// let options = Options::new()
///     .alphabet(*b"abc")
///     .max_states(100_000)
///     .time_limit(Duration::from_secs(10))
///     .cache_budget(256 << 20);
/// ```
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Options {
	pub(crate) alphabets: Vec<ExtraAlphabet>,
	pub(crate) explicit: Vec<Term>,
	pub(crate) max_states: Option<usize>,
	pub(crate) time_limit: Option<Duration>,
	pub(crate) subsumption: bool,
	pub(crate) antichain: bool,
	pub(crate) cache_budget: Option<usize>,
	pub(crate) max_length: Option<usize>,
	pub(crate) shortest: bool,
}

/// Symbols that a query adds to the alphabets of the checked term.
#[derive(Clone, Debug, PartialEq, Eq)]
pub(crate) struct ExtraAlphabet {
	pub(crate) tapes: Tapes,
	pub(crate) symbols: Vec<Symbol>,
}

/// The tapes of an [`ExtraAlphabet`].
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) enum Tapes {
	Every,
	One(usize),
}

impl Default for Options {
	fn default() -> Options {
		Options::new()
	}
}

impl Options {
	/// No extra alphabets, no explicit terms, no limits, subsumption and the antichain on, depth first.
	#[must_use]
	pub fn new() -> Options {
		Options {
			alphabets: Vec::new(),
			explicit: Vec::new(),
			max_states: None,
			time_limit: None,
			subsumption: true,
			antichain: true,
			cache_budget: None,
			max_length: None,
			shortest: false,
		}
	}

	/// Adds `symbols` to the alphabet of every tape of the checked term.
	#[must_use]
	pub fn alphabet(mut self, symbols: impl IntoIterator<Item: Into<Symbol>>) -> Options {
		self.alphabets.push(ExtraAlphabet {
			tapes: Tapes::Every,
			symbols: symbols.into_iter().map(Into::into).collect(),
		});

		self
	}

	/// Adds `symbols` to the alphabet of `tape` of the checked term.
	#[must_use]
	pub fn tape_alphabet(
		mut self,
		tape: usize,
		symbols: impl IntoIterator<Item: Into<Symbol>>,
	) -> Options {
		self.alphabets.push(ExtraAlphabet {
			tapes: Tapes::One(tape),
			symbols: symbols.into_iter().map(Into::into).collect(),
		});

		self
	}

	/// Builds `term` explicitly in this query, as if it were wrapped in
	/// [`explicit`](crate::Formula::explicit).
	#[must_use]
	pub fn explicit(mut self, term: Term) -> Options {
		self.explicit.push(term);

		self
	}

	/// Fails with [`StateLimit`](crate::Error::StateLimit) once the query holds more than `max_states` macrostates,
	/// so a query that answers holds at most `max_states`. The successors of a macrostate are counted step by step, a
	/// step being a move of a product's left operand or a tuple of a complement. A query stops after the step that
	/// passes the limit, and a step of a product can create one macrostate per matching move of the right operand.
	#[must_use]
	pub fn max_states(mut self, max_states: usize) -> Options {
		self.max_states = Some(max_states);

		self
	}

	/// Fails with [`TimeLimit`](crate::Error::TimeLimit) once the query runs longer than `limit`. The
	/// clock is read every 256 steps of the search and of the loops that create macrostates, during a leaf's
	/// simulation, and every few thousand comparisons of a minimization, so a zero limit still lets a small
	/// query answer. A step runs to its end first, such as a move of a product's left operand joined with many
	/// moves of the right one.
	///
	/// A simulation cut short by the limit is not kept, so a query whose limit is shorter than a leaf's simulation
	/// fails every time. Give one query enough time (or no limit), and later queries of the formula reuse its
	/// simulation. Queries on other threads wait, up to their own limit, for a simulation that one of them
	/// computes.
	#[must_use]
	pub fn time_limit(mut self, limit: Duration) -> Options {
		self.time_limit = Some(limit);

		self
	}

	/// Prunes with simulation-based subsumption (on by default). Complement sets drop the members below another
	/// member, and the search keeps the antichain of [`antichain`](Options::antichain). Off, the search is a plain
	/// reachability check, which helps to cross-check results.
	#[must_use]
	pub fn subsumption(mut self, enabled: bool) -> Options {
		self.subsumption = enabled;

		self
	}

	/// Skips the macrostates of the checked term that are below one found before (on by default; only with
	/// [`subsumption`](Options::subsumption)). Each new macrostate is compared with those found, which can cost more
	/// than it saves, for example on a product of deterministic automata. Off, complement sets are still minimized.
	#[must_use]
	pub fn antichain(mut self, enabled: bool) -> Options {
		self.antichain = enabled;

		self
	}

	/// Keeps the successors that the query caches within `bytes` (by default it keeps all). Over the budget, it
	/// drops those that took the least time to compute per byte, weighted by their reads, and were not read again
	/// since (GreedyDual-Size with frequency). It computes a dropped successor again when it needs it, so only the
	/// time and [`dropped`](Stats::dropped) change, not the answer or the other statistics.
	#[must_use]
	pub fn cache_budget(mut self, bytes: usize) -> Options {
		self.cache_budget = Some(bytes);

		self
	}

	/// Searches only the words of at most `steps` steps (lock-step tuples, padding included). The search then does not
	/// expand a macrostate reached in `steps` steps, and when it leaves one unexpanded the [`Outcome`] without a witness
	/// is [`bounded`](Outcome::bounded): no word of at most `steps` steps is in the relation, but longer ones may be.
	/// [`materialize`](crate::Formula::materialize) ignores it.
	///
	/// The antichain then skips a macrostate only for one found in at most as many steps.
	///
	/// ```
	/// # #[cfg(feature = "regex")] {
	/// use mata_lazy::{Formula, Options};
	/// use mata_lazy::regex::Regex;
	///
	/// let mut formula = Formula::new();
	/// let long = formula.leaf(&Regex::new("aaaa")?)?;
	/// let outcome = formula.check(long, &Options::new().max_length(3))?;
	/// assert!(outcome.witness.is_none() && outcome.bounded && !outcome.is_empty());
	/// assert!(formula.check(long, &Options::new().max_length(4))?.witness.is_some());
	/// # }
	/// # Ok::<(), Box<dyn std::error::Error>>(())
	/// ```
	#[must_use]
	pub fn max_length(mut self, steps: usize) -> Options {
		self.max_length = Some(steps);

		self
	}

	/// Searches breadth first (off by default), so that the witness has the fewest steps of any word in the
	/// relation. The search then numbers every initial macrostate of the checked term before it expands one, and
	/// keeps the whole frontier, which can take more memory than the default depth-first search. The antichain skips
	/// a macrostate only for one found in at most as many steps. [`materialize`](crate::Formula::materialize)
	/// ignores it.
	#[must_use]
	pub fn shortest(mut self, enabled: bool) -> Options {
		self.shortest = enabled;

		self
	}
}

/// Options of one explicit node.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash)]
pub struct ExplicitOptions {
	pub(crate) max_states: Option<usize>,
}

impl ExplicitOptions {
	/// No budget.
	#[must_use]
	pub fn new() -> ExplicitOptions {
		ExplicitOptions::default()
	}

	/// Keeps the node lazy when its exploration reaches more than `max_states` states.
	#[must_use]
	pub fn max_states(mut self, max_states: usize) -> ExplicitOptions {
		self.max_states = Some(max_states);

		self
	}
}

/// The result of [`check`](crate::Formula::check).
#[derive(Clone, Debug, PartialEq, Eq)]
#[non_exhaustive]
pub struct Outcome {
	/// A tuple of words in the relation, or `None` if there is none (of at most
	/// [`max_length`](Options::max_length) steps).
	pub witness: Option<Witness>,
	/// Whether the search found no witness and left a macrostate unexpanded at
	/// [`max_length`](Options::max_length), so that longer words may be in the relation. Never with a witness.
	pub bounded: bool,
	/// What the query did.
	pub stats: Stats,
}

impl Outcome {
	/// Whether the relation is empty: no witness, and the search was not [`bounded`](Outcome::bounded).
	#[must_use]
	pub fn is_empty(&self) -> bool {
		self.witness.is_none() && !self.bounded
	}
}

/// Counters of one query.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
#[non_exhaustive]
pub struct Stats {
	/// Macrostates of the checked term whose successors were computed.
	pub explored: usize,
	/// Macrostates of the checked term skipped because a found one subsumes them.
	pub pruned: usize,
	/// Macrostates created in all nodes.
	pub macrostates: usize,
	/// The explicit nodes of the query.
	pub explicit: Vec<Materialized>,
	/// Cached successors dropped to stay within [`cache_budget`](Options::cache_budget).
	pub dropped: usize,
}

/// One explicit node of a query.
#[derive(Clone, Debug, PartialEq, Eq)]
#[non_exhaustive]
pub struct Materialized {
	/// The term marked explicit.
	pub term: Term,
	/// States of its automaton (after trimming), or the states explored before it fell back.
	pub states: usize,
	/// Transitions of its automaton.
	pub transitions: usize,
	/// Whether an earlier query built it.
	pub cached: bool,
	/// Whether it passed its budget and stayed lazy.
	pub lazy: bool,
}
