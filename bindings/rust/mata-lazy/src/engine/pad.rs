//! Pad closures: the words of the child that reach a final state when padding steps follow.
//!
//! A macrostate of a pad closure is a [`Tagged`] state of the child, whose branch numbers the step that padding
//! repeats: always 0 for [`Pad::Step`]; for [`Pad::Last`], 0 before the first step and `i + 1` for the `i`-th
//! step interned in [`PadSteps`]. A target accepts if the child's accepts, or if a final state is reachable from
//! it on the padding step alone.

use indexmap::IndexSet;
use rustc_hash::{FxBuildHasher, FxHashMap, FxHashSet};

use super::Engine;
use super::budget::Limit;
use super::post::{Post, TapeLabel, Target, for_each_concrete};
use super::store::Tagged;
use crate::Symbol;
use crate::formula::Pad;
use crate::ids::{ExecId, StateId};

/// The last steps that the macrostates of a [`Pad::Last`] closure remember, and what padding reaches.
#[derive(Debug, Default)]
pub(crate) struct PadSteps {
	steps: IndexSet<Box<[Symbol]>, FxBuildHasher>,
	/// Whether a final state of the child is reachable from a state on the padding step of a branch.
	reaches: FxHashMap<(StateId, u32), bool>,
}

/// Whether a symbolic move label includes a tape reading `step`.
fn reads(tuple: &[TapeLabel], step: &[Symbol]) -> bool {
	tuple
		.iter()
		.zip(step)
		.all(|(read, &padded)| match (read, padded) {
			(TapeLabel::Simple(Symbol::EPSILON), Symbol::ANY)
			| (TapeLabel::Set(_), Symbol::EPSILON) => false,
			(_, Symbol::ANY) => true,
			(TapeLabel::Simple(read), padded) => *read == padded || *read == Symbol::ANY,
			(TapeLabel::Set(symbols), padded) => symbols.binary_search(&padded).is_ok(),
		})
}

impl Engine<'_> {
	/// The padding step of `branch` of the pad closure `node`, if it has one.
	fn pad_step(&self, node: ExecId, pad: &Pad, branch: u32) -> Option<Box<[Symbol]>> {
		match pad {
			Pad::Step(step) => Some(step.clone()),
			Pad::Last => {
				let index = usize::try_from(branch.checked_sub(1)?).expect("a u32 fits in usize");

				Some(self.pad_steps[node].steps[index].clone())
			}
		}
	}

	/// Whether a final state of `child` is reachable from `state` on the padding step of `branch`, which takes
	/// at least one step.
	fn pad_reaches(
		&mut self,
		node: ExecId,
		child: ExecId,
		step: &[Symbol],
		branch: u32,
		state: StateId,
	) -> Result<bool, Limit> {
		if let Some(&known) = self.pad_steps[node].reaches.get(&(state, branch)) {
			return Ok(known);
		}

		let mut seen = FxHashSet::default();
		seen.insert(state);
		let mut stack = vec![state];
		while let Some(from) = stack.pop() {
			for (tuple, target) in self.post(child, from)?.iter() {
				if !reads(tuple, step) {
					continue;
				}

				if target.accepting {
					self.pad_steps[node].reaches.insert((state, branch), true);

					return Ok(true);
				}

				if seen.insert(target.state) {
					stack.push(target.state);
				}
			}
		}

		// Nothing reachable from the states seen accepts on the step either.
		let reaches = &mut self.pad_steps[node].reaches;
		reaches.extend(seen.into_iter().map(|seen| ((seen, branch), false)));

		Ok(false)
	}

	/// The macrostate of `node` for the child's `target` in `branch`.
	pub(super) fn pad_target(
		&mut self,
		node: ExecId,
		child: ExecId,
		pad: &Pad,
		branch: u32,
		target: Target,
	) -> Result<Target, Limit> {
		let accepting = target.accepting
			|| match self.pad_step(node, pad, branch) {
				Some(step) => self.pad_reaches(node, child, &step, branch, target.state)?,
				None => false,
			};
		let state = self.stores[node].intern_tagged(Tagged {
			branch,
			state: target.state,
		});

		Ok(Target { state, accepting })
	}

	pub(super) fn pad_closure_post(
		&mut self,
		node: ExecId,
		child: ExecId,
		pad: &Pad,
		state: StateId,
	) -> Result<Post, Limit> {
		let Tagged { branch, state } = self.stores[node].tagged(state);
		let child_post = self.post(child, state)?;
		let arity = self.exec.nodes[node].arity;
		let start = self.stores[node].len();
		let mut post = Post::with_capacity(arity, child_post.len());
		match pad {
			Pad::Step(_) => {
				for (tuple, target) in child_post.iter() {
					post.push_labels(tuple, self.pad_target(node, child, pad, branch, target)?);
				}
			}
			Pad::Last => {
				// Last-step reachability branches on exact steps, so symbolic dimensions expand incrementally.
				let universes: Vec<_> = (0..arity)
					.map(|tape| self.alphabets.universe(node, tape).clone())
					.collect();
				for (tuple, target) in child_post.iter() {
					for_each_concrete(&universes, tuple, |step| {
						self.budget.step(self.stores[node].len() - start);
						self.stopped()?;
						let index = self.pad_steps[node].steps.insert_full(step.into()).0;
						let branch = u32::try_from(index + 1).expect("fewer than 2^32 steps");
						post.push(step, self.pad_target(node, child, pad, branch, target)?);

						Ok(())
					})?;
				}

				post.dedup();
			}
		}

		self.budget.add(self.stores[node].len() - start);

		Ok(post)
	}
}
