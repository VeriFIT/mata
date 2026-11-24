//! The prefix closure, whose macrostates are its child's: one accepts when it reaches an accepting macrostate of
//! the child, which a search from it decides once per query.

use std::rc::Rc;

use rustc_hash::FxHashSet;

use super::Engine;
use super::budget::Limit;
use super::exec::Op;
use super::post::{Post, Target};
use crate::ids::{ExecId, StateId};

impl Engine<'_> {
	/// The moves of `child` from `state`, each target accepting when it reaches an accepting macrostate.
	pub(super) fn prefixes_post(
		&mut self,
		node: ExecId,
		child: ExecId,
		state: StateId,
	) -> Result<Post, Limit> {
		let post = self.post(child, state)?;

		post.reflagged(|target| self.is_live(node, child, target))
	}

	/// Whether `target`, a macrostate of `child` below the prefix closure `node`, accepts or reaches a macrostate
	/// that accepts.
	pub(super) fn is_live(
		&mut self,
		node: ExecId,
		child: ExecId,
		target: Target,
	) -> Result<bool, Limit> {
		if target.accepting || self.is_trimmed(child) {
			return Ok(true);
		}

		if let Some(&live) = self.live[node].get(&target.state) {
			return Ok(live);
		}

		// Depth first from `target`. The path to a macrostate that accepts or is known live is live; without one,
		// every macrostate visited is dead.
		let mut visited = FxHashSet::default();
		visited.insert(target.state);
		let mut path: Vec<(StateId, Rc<Post>, usize)> =
			vec![(target.state, self.post(child, target.state)?, 0)];
		while let Some((_, post, next)) = path.last_mut() {
			if *next == post.len() {
				path.pop();
				continue;
			}

			let successor = post.target(*next);
			*next += 1;
			self.budget.step(0);
			self.stopped()?;

			let known = self.live[node].get(&successor.state).copied();
			if successor.accepting || known == Some(true) {
				for (state, ..) in path {
					self.live[node].insert(state, true);
				}

				return Ok(true);
			}

			if known == Some(false) || !visited.insert(successor.state) {
				continue;
			}

			let post = self.post(child, successor.state)?;
			path.push((successor.state, post, 0));
		}

		self.live[node].extend(visited.into_iter().map(|state| (state, false)));

		Ok(false)
	}

	/// Whether every macrostate of `child` reaches an accepting one without a search: those of a leaf do, as leaves
	/// are trimmed, unless the leaf reads the wildcard on a tape whose universe is empty, as such moves are dropped.
	fn is_trimmed(&self, child: ExecId) -> bool {
		let owner = self.exec.owner(child);
		let Op::Leaf(leaf) = &self.exec.nodes[owner].op else {
			return false;
		};

		(0..self.exec.nodes[owner].arity).all(|tape| {
			!leaf.tape_has_any[tape] || !self.alphabets.universe(owner, tape).is_empty()
		})
	}
}
