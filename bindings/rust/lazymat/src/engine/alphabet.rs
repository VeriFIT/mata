//! The alphabet of every tape of every node.
//!
//! Tapes that must read the same symbols (both operands of an intersection, the synchronized tapes of a
//! product, ...) form one class. The alphabet of a class is every concrete symbol that a leaf tape of the
//! class reads or declares, plus the query's alphabets for the tapes of the root.

use std::rc::Rc;

use super::exec::{Exec, Op};
use crate::ids::{ExecId, Id};
use crate::{Symbol, Tape};

#[derive(Debug)]
pub(crate) struct Alphabets {
	/// The first slot of each node, and last the number of slots; a node has one slot per tape.
	offsets: Vec<usize>,
	class_of: Vec<usize>,
	universes: Vec<Rc<[Symbol]>>,
}

impl Alphabets {
	/// `root_symbols[tape]` are the extra symbols of each tape of the root.
	pub(crate) fn new(exec: &Exec<'_>, root_symbols: &[Vec<Symbol>]) -> Alphabets {
		let mut offsets = Vec::with_capacity(exec.nodes.len() + 1);
		let mut num_slots = 0;
		offsets.push(num_slots);
		for node in &exec.nodes {
			num_slots += node.arity;
			offsets.push(num_slots);
		}

		let slot = |node: ExecId, tape: usize| offsets[node.index()] + tape;
		let (class_of, num_classes) = tape_classes(exec, num_slots, slot).into_classes();

		let mut symbols: Vec<Vec<Symbol>> = vec![Vec::new(); num_classes];
		for (id, node) in exec.nodes.iter_enumerated() {
			if let Op::Leaf(leaf) = &node.op
				&& exec.reachable[id]
			{
				for (tape, tape_symbols) in leaf.tape_symbols.iter().enumerate() {
					symbols[class_of[slot(id, tape)]].extend_from_slice(tape_symbols);
				}
			}
		}

		for (tape, extra) in root_symbols.iter().enumerate() {
			symbols[class_of[slot(exec.root, tape)]].extend_from_slice(extra);
		}

		let universes = symbols
			.into_iter()
			.map(|mut class| {
				class.sort_unstable();
				class.dedup();

				class.into()
			})
			.collect();

		Alphabets {
			offsets,
			class_of,
			universes,
		}
	}

	pub(crate) fn class(&self, node: ExecId, tape: usize) -> usize {
		self.class_of[self.offsets[node.index()] + tape]
	}

	/// The concrete symbols of `tape` of `node`, ascending.
	pub(crate) fn universe(&self, node: ExecId, tape: usize) -> &Rc<[Symbol]> {
		&self.universes[self.class(node, tape)]
	}

	pub(crate) fn class_universe(&self, class: usize) -> &[Symbol] {
		&self.universes[class]
	}

	/// The number of tuples over the tapes of `node`, or `None` if `usize` cannot count them.
	pub(crate) fn tuple_count(&self, node: ExecId) -> Option<usize> {
		let classes = &self.class_of[self.offsets[node.index()]..self.offsets[node.index() + 1]];

		classes.iter().try_fold(1_usize, |count, &class| {
			count.checked_mul(self.universes[class].len())
		})
	}
}

/// The classes of the tapes that read the same symbols, with tape `t` of node `n` at slot `slot(n, t)`.
fn tape_classes(
	exec: &Exec<'_>,
	num_slots: usize,
	slot: impl Fn(ExecId, usize) -> usize,
) -> UnionFind {
	let mut classes = UnionFind::new(num_slots);
	for (id, node) in exec.nodes.iter_enumerated() {
		if !exec.reachable[id] {
			continue;
		}

		match &node.op {
			Op::Leaf(_) => {}
			// Each tape reads what the same tape of every child reads.
			Op::Union(_)
			| Op::Intersection(..)
			| Op::Complement(_)
			| Op::Explicit { .. }
			| Op::Transparent(_) => {
				for child in node.op.children() {
					for tape in 0..node.arity {
						classes.union(slot(id, tape), slot(child, tape));
					}
				}
			}
			Op::Identity(child) => {
				classes.union(slot(id, 0), slot(*child, 0));
				classes.union(slot(id, 1), slot(*child, 0));
			}
			Op::Project(child, tapes) => {
				for (tape, &kept) in tapes.iter().enumerate() {
					classes.union(slot(id, tape), slot(*child, kept));
				}
			}
			Op::Sync(a, b, spec) => {
				for pair in &spec.pairs {
					classes.union(slot(*a, pair.lhs), slot(*b, pair.rhs));
				}

				for (tape, result) in spec.layout.iter().enumerate() {
					let source = match result.source {
						Tape::Lhs(i) => slot(*a, i),
						Tape::Rhs(j) => slot(*b, j),
					};

					classes.union(slot(id, tape), source);
				}
			}
			Op::Diagonal(u, x) => {
				classes.union(slot(id, 0), slot(*u, 0));
				classes.union(slot(id, 0), slot(*x, 0));
				classes.union(slot(id, 0), slot(*x, 1));
			}
		}
	}

	classes
}

struct UnionFind {
	parent: Vec<usize>,
}

impl UnionFind {
	fn new(size: usize) -> UnionFind {
		UnionFind {
			parent: (0..size).collect(),
		}
	}

	fn find(&mut self, mut slot: usize) -> usize {
		while self.parent[slot] != slot {
			self.parent[slot] = self.parent[self.parent[slot]];
			slot = self.parent[slot];
		}

		slot
	}

	/// Keeps the first slot of every class as its root.
	fn union(&mut self, a: usize, b: usize) {
		let (a, b) = (self.find(a), self.find(b));
		if a != b {
			self.parent[a.max(b)] = a.min(b);
		}
	}

	/// The class of every slot.
	/// Numbered densely in the order of the classes' first slots, and the number of classes.
	fn into_classes(mut self) -> (Vec<usize>, usize) {
		let num_slots = self.parent.len();
		let mut class_of = Vec::with_capacity(num_slots);
		let mut num_classes = 0;
		for slot in 0..num_slots {
			let root = self.find(slot);
			if root == slot {
				class_of.push(num_classes);
				num_classes += 1;
			} else {
				class_of.push(class_of[root]);
			}
		}

		(class_of, num_classes)
	}
}
