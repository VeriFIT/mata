//! Policies, which compute a [`Plan`] from the [`PlanView`] of a formula.

use crate::{Annotation, NodeKind, Plan, PlanView};

/// Computes the annotations of a query from the [view](crate::Formula::view) of its term.
///
/// Every plan keeps the answer of the query (see [`Plan`]), so a policy decides only what a query costs.
pub trait Policy {
	/// The plan for the query of `view.root`.
	fn plan(&self, view: &PlanView) -> Plan;
}

/// Every operation lazy, even an [`explicit`](crate::Formula::explicit) node of the formula.
///
/// Without a plan, a query keeps the formula's explicit nodes; this policy is the purely lazy baseline.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct AllLazy;

impl Policy for AllLazy {
	fn plan(&self, view: &PlanView) -> Plan {
		annotate_operations(view, Annotation::lazy())
	}
}

/// Every operation built explicitly, without a budget: each node is explored completely, children first, as an
/// eager library would. This is the baseline to compare the lazy engine with.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct AllEager;

impl Policy for AllEager {
	fn plan(&self, view: &PlanView) -> Plan {
		annotate_operations(view, Annotation::eager())
	}
}

/// Builds the largest subterms that are surely small explicitly, and keeps the rest lazy.
///
/// It bounds the macrostates of each term from the states of the leaves below it: a product multiplies the
/// bounds of its operands, a union adds them, a complement raises 2 to the bound of its operand, and the other
/// operations keep their operand's. A term other than the root whose bound is at most
/// [`max_states`](Heuristic::max_states), used by the root or by a term over that bound, is built explicitly with
/// that budget. The root stays lazy, so that the search stops at the first witness.
///
/// For `(A ∩ B) ∩ C` with small leaves `A` and `B` and a large `C`, `A ∩ B` is built explicitly and the outer
/// intersection runs lazily.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Heuristic {
	max_states: usize,
}

impl Default for Heuristic {
	fn default() -> Heuristic {
		Heuristic::new()
	}
}

impl Heuristic {
	/// A bound of 4096 macrostates.
	#[must_use]
	pub fn new() -> Heuristic {
		Heuristic { max_states: 4096 }
	}

	/// Builds explicitly the terms bounded by `max_states` macrostates.
	#[must_use]
	pub fn max_states(mut self, max_states: usize) -> Heuristic {
		self.max_states = max_states;

		self
	}
}

impl Policy for Heuristic {
	fn plan(&self, view: &PlanView) -> Plan {
		// log2 of the bound of each node; children come first.
		let mut bounds: Vec<f64> = Vec::with_capacity(view.nodes.len());
		let bound = |bounds: &[f64], term| {
			let index = view
				.nodes
				.binary_search_by_key(&term, |node| node.term)
				.expect("a child is in the view");

			bounds[index]
		};

		for node in &view.nodes {
			let children = node.children.iter().map(|&child| bound(&bounds, child));
			let log = match &node.kind {
				NodeKind::Leaf => {
					let states = node.leaf.as_ref().map_or(1, |leaf| leaf.states.max(1));

					log2(states)
				}
				NodeKind::Union => {
					let logs: Vec<f64> = children.collect();
					let max = logs.iter().copied().fold(f64::NEG_INFINITY, f64::max);

					max + logs
						.iter()
						.map(|log| (log - max).exp2())
						.sum::<f64>()
						.log2()
				}
				NodeKind::Complement => children.map(f64::exp2).sum(),
				// Products multiply; the unary operations keep their operand's bound.
				_ => children.sum(),
			};

			bounds.push(log);
		}

		let small_bound = log2(self.max_states);
		let small = |index: usize| bounds[index] <= small_bound;
		// Whether the root or a term over the bound uses the node.
		let mut wanted = vec![false; view.nodes.len()];
		for (index, node) in view.nodes.iter().enumerate() {
			if node.term == view.root || !small(index) {
				for &child in &node.children {
					let child = view
						.nodes
						.binary_search_by_key(&child, |node| node.term)
						.expect("a child is in the view");
					wanted[child] = true;
				}
			}
		}

		view.nodes
			.iter()
			.enumerate()
			.filter(|&(index, node)| {
				wanted[index]
					&& small(index)
					&& !matches!(node.kind, NodeKind::Leaf | NodeKind::Explicit { .. })
			})
			.fold(Plan::new(), |plan, (_, node)| {
				plan.annotate(node.term, Annotation::eager_within(self.max_states))
			})
	}
}

/// A plan given from outside, such as one that a person or a tool wrote after reading the [`PlanView`]; it does
/// not depend on the view. With the feature `serde`, `Formula::plan_from_json` reads it from JSON.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct External(pub Plan);

impl Policy for External {
	fn plan(&self, _: &PlanView) -> Plan {
		self.0.clone()
	}
}

/// `annotation` for each node of `view` other than leaves.
fn annotate_operations(view: &PlanView, annotation: Annotation) -> Plan {
	view.nodes
		.iter()
		.filter(|node| node.kind != NodeKind::Leaf)
		.fold(Plan::new(), |plan, node| {
			plan.annotate(node.term, annotation)
		})
}

/// log2 of `count`, saturated at `u32::MAX`, as the engine bounds leaves.
fn log2(count: usize) -> f64 {
	f64::from(u32::try_from(count).unwrap_or(u32::MAX)).log2()
}
