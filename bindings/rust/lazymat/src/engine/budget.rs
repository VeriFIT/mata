//! The limits of one query, and the macrostates and steps counted against them.

use std::time::{Duration, Instant};

use crate::Options;

/// Steps between two reads of the clock.
const CLOCK_STEPS: u32 = 256;

/// A limit of the query that it passed.
#[derive(Clone, Copy, Debug)]
pub(crate) enum Limit {
	/// [`max_states`](Options::max_states).
	States,
	/// [`time_limit`](Options::time_limit).
	Time,
}

/// The limits of one query, and what it has spent of them.
#[derive(Debug)]
pub(crate) struct Budget {
	pub(crate) max_states: Option<usize>,
	deadline: Option<(Instant, Duration)>,
	/// The macrostates of all nodes, as of the last post or initial macrostates computed.
	macrostates: usize,
	steps_to_clock: u32,
	/// The first limit passed.
	passed: Option<Limit>,
}

impl Budget {
	pub(crate) fn new(options: &Options) -> Budget {
		let deadline = options
			.time_limit
			.and_then(|limit| Some((Instant::now().checked_add(limit)?, limit)));

		Budget {
			max_states: options.max_states,
			deadline,
			macrostates: 0,
			steps_to_clock: CLOCK_STEPS,
			passed: None,
		}
	}

	pub(crate) fn deadline(&self) -> Option<Instant> {
		self.deadline.map(|(deadline, _)| deadline)
	}

	pub(crate) fn time_limit(&self) -> Option<Duration> {
		self.deadline.map(|(_, limit)| limit)
	}

	/// Adds the macrostates that one post, or the initial macrostates of one node, created.
	pub(crate) fn add(&mut self, created: usize) {
		self.macrostates += created;
		self.check_states(0);
	}

	/// Counts a step of a loop that has created `growing` macrostates not added yet, reading the clock every
	/// [`CLOCK_STEPS`] steps.
	pub(crate) fn step(&mut self, growing: usize) {
		self.check_states(growing);

		self.steps_to_clock -= 1;
		if self.steps_to_clock == 0 {
			self.steps_to_clock = CLOCK_STEPS;
			if self
				.deadline
				.is_some_and(|(deadline, _)| Instant::now() > deadline)
			{
				self.passed.get_or_insert(Limit::Time);
			}
		}
	}

	fn check_states(&mut self, growing: usize) {
		if self
			.max_states
			.is_some_and(|max_states| self.macrostates + growing > max_states)
		{
			self.passed.get_or_insert(Limit::States);
		}
	}

	/// Whether a limit passed, in which case the posts and initial macrostates computed since may be partial.
	pub(crate) fn passed(&self) -> bool {
		self.passed.is_some()
	}

	pub(crate) fn check(&self) -> Result<(), Limit> {
		self.passed.map_or(Ok(()), Err)
	}
}
