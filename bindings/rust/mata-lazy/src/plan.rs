//! Per-query annotations of a formula's nodes: lazy or eager, and the operand order of products.

use std::collections::BTreeMap;

use rustc_hash::{FxHashMap, FxHashSet};

use crate::formula::{Formula, Node};
use crate::ids::NodeId;
use crate::{Error, ExplicitOptions, Options, Term};

/// Annotations of the terms of one formula, passed to a query with [`Options::plan`].
///
/// A plan changes what a query costs, never its answer: the relation, and so whether it is empty, stays the
/// same. Only a limit of the query ([`max_states`](Options::max_states), [`time_limit`](Options::time_limit)) can
/// turn an answer into an error, and a witness can differ. A [`Policy`](crate::Policy) computes a plan from the
/// [`view`](Formula::view) of a formula.
///
/// ```
/// use mata_lazy::{Annotation, Automaton, Formula, Options, Order, Plan, Symbol};
///
/// /// The words over {a, b} whose `index`th letter is `a`.
/// struct NthIsA(u32);
///
/// impl Automaton for NthIsA {
///     type State = u32;
///
///     fn arity(&self) -> usize {
///         1
///     }
///
///     fn initial_states(&self) -> impl IntoIterator<Item = u32> {
///         [0]
///     }
///
///     fn is_final(&self, state: u32) -> bool {
///         state > self.0
///     }
///
///     fn for_each_move(&self, state: u32, mut visit: impl FnMut(&[Symbol], u32)) {
///         let next = (state + 1).min(self.0 + 1);
///         visit(&[Symbol::from(b'a')], next);
///         if state != self.0 {
///             visit(&[Symbol::from(b'b')], next);
///         }
///     }
/// }
///
/// let mut formula = Formula::new();
/// let a = formula.leaf(&NthIsA(0))?;
/// let b = formula.leaf(&NthIsA(1))?;
/// let c = formula.leaf(&NthIsA(2))?;
/// let ab = formula.intersection(a, b)?;
/// let abc = formula.intersection(ab, c)?;
///
/// // `a ∩ b` is built eagerly, the outer intersection runs lazily with `c` on the left.
/// let plan = Plan::new()
///     .annotate(ab, Annotation::eager())
///     .annotate(abc, Annotation::lazy().order(Order::Swapped));
/// let outcome = formula.check(abc, &Options::new().plan(plan))?;
/// assert!(!outcome.is_empty());
/// assert_eq!(outcome.stats.explicit.len(), 1);
/// # Ok::<(), mata_lazy::Error>(())
/// ```
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Plan {
	annotations: BTreeMap<Term, Annotation>,
}

impl Plan {
	/// No annotations: each node is lazy unless the formula or [`Options::explicit`] makes it explicit, and the
	/// engine orders operands.
	#[must_use]
	pub fn new() -> Plan {
		Plan::default()
	}

	/// Annotates `term`, replacing an earlier annotation of it.
	#[must_use]
	pub fn annotate(mut self, term: Term, annotation: Annotation) -> Plan {
		self.annotations.insert(term, annotation);

		self
	}

	/// The annotation of `term`, if it has one.
	#[must_use]
	pub fn annotation(&self, term: Term) -> Option<&Annotation> {
		self.annotations.get(&term)
	}

	/// The annotated terms, by [`id`](Term::id).
	pub fn iter(&self) -> impl ExactSizeIterator<Item = (Term, &Annotation)> {
		self.annotations
			.iter()
			.map(|(&term, annotation)| (term, annotation))
	}

	/// The number of annotated terms.
	#[must_use]
	pub fn len(&self) -> usize {
		self.annotations.len()
	}

	/// Whether no term is annotated.
	#[must_use]
	pub fn is_empty(&self) -> bool {
		self.annotations.is_empty()
	}
}

/// What a [`Plan`] says about one term.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash)]
#[non_exhaustive]
pub struct Annotation {
	/// Whether the term is built explicitly.
	pub eval: Eval,
	/// Which operand of a product goes on the left.
	pub order: Order,
}

impl Annotation {
	/// The term stays lazy, even when the formula or [`Options::explicit`] makes it explicit.
	#[must_use]
	pub fn lazy() -> Annotation {
		Annotation {
			eval: Eval::Lazy,
			order: Order::Auto,
		}
	}

	/// The term is built explicitly, without a budget.
	#[must_use]
	pub fn eager() -> Annotation {
		Annotation {
			eval: Eval::Eager { max_states: None },
			order: Order::Auto,
		}
	}

	/// The term is built explicitly, and stays lazy when its exploration passes `max_states` (as
	/// [`ExplicitOptions::max_states`]).
	#[must_use]
	pub fn eager_within(max_states: usize) -> Annotation {
		Annotation {
			eval: Eval::Eager {
				max_states: Some(max_states),
			},
			order: Order::Auto,
		}
	}

	/// The same annotation with operand order `order`.
	#[must_use]
	pub fn order(mut self, order: Order) -> Annotation {
		self.order = order;

		self
	}
}

/// Whether a term is built lazily or explicitly.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash)]
pub enum Eval {
	/// As the formula and [`Options::explicit`] say.
	#[default]
	Keep,
	/// Lazily, even when the formula or [`Options::explicit`] makes it explicit.
	Lazy,
	/// Explicitly, as [`Formula::explicit_with`] builds it. Over the budget `max_states`, the node stays lazy.
	Eager {
		/// The budget, as [`ExplicitOptions::max_states`]; `None` for none.
		max_states: Option<usize>,
	},
}

/// The operand order of an [intersection](Formula::intersection) or a [synchronized
/// product](Formula::sync_product) (also a composition or an image).
///
/// A product explores the moves of its left operand and joins each with the matching moves of the right one,
/// so the left operand should be the one with fewer moves.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash)]
pub enum Order {
	/// The engine's choice: the operand with the smaller bound on its macrostates goes left in an
	/// intersection, and a synchronized product keeps its order.
	#[default]
	Auto,
	/// The operands as they were passed to the builder.
	AsWritten,
	/// The right operand of the builder goes left.
	Swapped,
}

/// What the options of one query decide per node of the formula, checked against it.
#[derive(Debug, Default)]
pub(crate) struct Annotations {
	/// Nodes built explicitly, with their options. For an explicit node of the formula, the options replace its
	/// own.
	pub(crate) explicit: FxHashMap<NodeId, ExplicitOptions>,
	/// Explicit nodes of the formula that stay lazy.
	pub(crate) lazy: FxHashSet<NodeId>,
	/// Operand orders other than [`Order::Auto`].
	pub(crate) order: FxHashMap<NodeId, Order>,
}

impl Annotations {
	/// The annotations of `options`: its explicit terms, then its plan.
	pub(crate) fn new(formula: &Formula, options: &Options) -> Result<Annotations, Error> {
		let mut annotations = Annotations::default();
		for &term in &options.explicit {
			let node = formula.node(term)?;
			// An explicit node of the formula keeps its own options.
			if !matches!(formula.nodes[node], Node::Explicit(..)) {
				annotations.explicit.insert(node, ExplicitOptions::new());
			}
		}

		for (term, annotation) in options.plan.iter() {
			let node = formula.node(term)?;
			check_order(formula, node, annotation.order)?;
			match annotation.eval {
				Eval::Keep => {}
				Eval::Lazy => {
					annotations.explicit.remove(&node);
					annotations.lazy.insert(node);
				}
				Eval::Eager { max_states } => {
					annotations.lazy.remove(&node);
					annotations
						.explicit
						.insert(node, ExplicitOptions { max_states });
				}
			}

			if annotation.order != Order::Auto {
				annotations.order.insert(node, annotation.order);
			}
		}

		Ok(annotations)
	}
}

/// [`InvalidPlan`](Error::InvalidPlan) if `order` is not [`Order::Auto`] and `node` has no operand order.
fn check_order(formula: &Formula, node: NodeId, order: Order) -> Result<(), Error> {
	if order == Order::Auto
		|| matches!(formula.nodes[node], Node::Intersection(..) | Node::Sync(..))
	{
		return Ok(());
	}

	Err(Error::InvalidPlan {
		reason: format!(
			"term {} has no operand order, as it is not an intersection or a synchronized product",
			formula.term(node).id()
		),
	})
}
