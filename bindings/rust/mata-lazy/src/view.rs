//! A read-only view of a formula's DAG, for policies and tools that plan queries.

use crate::formula::{Formula, Node};
use crate::ids::{Id, NodeId};
use crate::{Error, Plan, Policy, Tape, Term};

/// The terms that one term depends on, children first, as [`Formula::view`] gives them.
#[derive(Clone, Debug, PartialEq, Eq)]
#[cfg_attr(feature = "serde", derive(serde::Serialize))]
#[non_exhaustive]
pub struct PlanView {
	/// The viewed term.
	pub root: Term,
	/// The terms that `root` depends on and `root` itself, by [`id`](Term::id), so children come first.
	pub nodes: Vec<NodeView>,
}

impl PlanView {
	/// The node of `term`, if `root` depends on it.
	#[must_use]
	pub fn node(&self, term: Term) -> Option<&NodeView> {
		self.nodes
			.binary_search_by_key(&term, |node| node.term)
			.ok()
			.map(|index| &self.nodes[index])
	}

	/// The view as JSON, with terms written as their [`id`](Term::id)s.
	///
	/// # Panics
	///
	/// Never: a view is numbers and names, which JSON always holds.
	#[cfg(feature = "serde")]
	#[must_use]
	pub fn to_json(&self) -> String {
		serde_json::to_string_pretty(self).expect("a view is plain data")
	}
}

/// One term of a [`PlanView`].
#[derive(Clone, Debug, PartialEq, Eq)]
#[cfg_attr(feature = "serde", derive(serde::Serialize))]
#[non_exhaustive]
pub struct NodeView {
	/// The term.
	pub term: Term,
	/// The builder that made it.
	pub kind: NodeKind,
	/// The number of tapes.
	pub arity: usize,
	/// The operands, in the order passed to the builder.
	pub children: Vec<Term>,
	/// How many times the terms of the view use it as an operand.
	pub fan_out: usize,
	/// Whether a word of the term may read padding.
	pub pads: bool,
	/// The automaton of a leaf.
	pub leaf: Option<LeafInfo>,
}

/// The builder of a [`NodeView`].
#[derive(Clone, Debug, PartialEq, Eq)]
#[cfg_attr(
	feature = "serde",
	derive(serde::Serialize),
	serde(rename_all = "snake_case")
)]
#[non_exhaustive]
pub enum NodeKind {
	/// [`leaf`](Formula::leaf) or [`leaf_over`](Formula::leaf_over).
	Leaf,
	/// [`union`](Formula::union).
	Union,
	/// [`intersection`](Formula::intersection).
	Intersection,
	/// [`complement`](Formula::complement).
	Complement,
	/// [`identity`](Formula::identity).
	Identity,
	/// [`project`](Formula::project).
	Project {
		/// The kept tapes, in order.
		tapes: Vec<usize>,
	},
	/// [`pad_closure`](Formula::pad_closure).
	PadClosure,
	/// [`sync_product`](Formula::sync_product), which also builds compositions and images.
	Sync {
		/// The synchronized pairs of a left and a right tape.
		pairs: Vec<(usize, usize)>,
		/// The tapes of the result.
		layout: Vec<Tape>,
	},
	/// [`explicit`](Formula::explicit) or [`explicit_with`](Formula::explicit_with).
	Explicit {
		/// Its budget.
		max_states: Option<usize>,
	},
	/// [`prefixes`](Formula::prefixes).
	Prefixes,
}

/// The automaton of a leaf, as the formula holds it (trimmed, without silent moves).
#[derive(Clone, Debug, PartialEq, Eq)]
#[cfg_attr(feature = "serde", derive(serde::Serialize))]
#[non_exhaustive]
pub struct LeafInfo {
	/// The number of states.
	pub states: usize,
	/// The number of moves.
	pub transitions: usize,
	/// Whether it has at most one initial state and no state has two moves on one tuple or a move on
	/// [`ANY`](crate::Symbol::ANY).
	pub deterministic: bool,
	/// The number of concrete symbols of each tape: those it reads and those it was given.
	pub tape_alphabet_sizes: Vec<usize>,
}

impl Formula {
	/// The terms that `term` depends on, with their kinds, operands and leaf sizes.
	///
	/// # Errors
	///
	/// [`ForeignTerm`](Error::ForeignTerm).
	pub fn view(&self, term: Term) -> Result<PlanView, Error> {
		let root = self.node(term)?;
		let mut seen = vec![false; root.index() + 1];
		seen[root.index()] = true;
		let mut stack = vec![root];
		while let Some(node) = stack.pop() {
			for child in self.nodes[node].children() {
				if !std::mem::replace(&mut seen[child.index()], true) {
					stack.push(child);
				}
			}
		}

		let mut fan_out = vec![0; seen.len()];
		for (index, _) in seen.iter().enumerate().filter(|&(_, &seen)| seen) {
			for child in self.nodes[NodeId::new(index)].children() {
				fan_out[child.index()] += 1;
			}
		}

		let nodes = seen
			.iter()
			.enumerate()
			.filter(|&(_, &seen)| seen)
			.map(|(index, _)| self.node_view(NodeId::new(index), fan_out[index]))
			.collect();

		Ok(PlanView { root: term, nodes })
	}

	/// The plan of `policy` for the query of `term`.
	///
	/// # Errors
	///
	/// [`ForeignTerm`](Error::ForeignTerm).
	pub fn plan_with(&self, term: Term, policy: &(impl Policy + ?Sized)) -> Result<Plan, Error> {
		Ok(policy.plan(&self.view(term)?))
	}

	fn node_view(&self, node: NodeId, fan_out: usize) -> NodeView {
		let (kind, leaf) = match &self.nodes[node] {
			Node::Leaf(leaf) => {
				let leaf = &self.leaves[*leaf];
				let info = LeafInfo {
					states: leaf.num_states(),
					transitions: leaf.num_moves(),
					deterministic: leaf.is_deterministic(),
					tape_alphabet_sizes: leaf.tape_symbols.iter().map(Vec::len).collect(),
				};

				(NodeKind::Leaf, Some(info))
			}
			Node::Union(..) => (NodeKind::Union, None),
			Node::Intersection(..) => (NodeKind::Intersection, None),
			Node::Complement(_) => (NodeKind::Complement, None),
			Node::Identity(_) => (NodeKind::Identity, None),
			Node::Project(_, tapes) => (
				NodeKind::Project {
					tapes: tapes.to_vec(),
				},
				None,
			),
			Node::PadClosure(..) => (NodeKind::PadClosure, None),
			Node::Sync(_, _, spec) => (
				NodeKind::Sync {
					pairs: spec.pairs.iter().map(|pair| (pair.lhs, pair.rhs)).collect(),
					layout: spec.layout.iter().map(|tape| tape.source).collect(),
				},
				None,
			),
			Node::Explicit(_, options) => (
				NodeKind::Explicit {
					max_states: options.max_states,
				},
				None,
			),
			Node::Prefixes(_) => (NodeKind::Prefixes, None),
		};

		NodeView {
			term: self.term(node),
			kind,
			arity: self.arities[node],
			children: self.nodes[node]
				.children()
				.map(|child| self.term(child))
				.collect(),
			fan_out,
			pads: self.pads[node],
			leaf,
		}
	}
}
