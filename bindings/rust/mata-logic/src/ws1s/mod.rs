//! WS1S, the weak monadic second-order logic of one successor: formulas over positions and finite sets of
//! positions, parsed from a MONA-like syntax and decided by a [`Logic`] with [`Padding::Zero`].
//!
//! ```
//! use mata_lazy::Options;
//! use mata_logic::ws1s::{Value, Ws1s};
//!
//! let mut ws1s = Ws1s::new();
//! let phi = ws1s.parse("(x = y + 1 & y = z + 1) => x = z + 2")?;
//! assert!(ws1s.valid(&phi, &Options::new())?);
//!
//! let psi = ws1s.parse("x in X & 3 < x & all1 y: y in X => y <= x")?;
//! let model = ws1s.model(&psi, &Options::new())?.expect("satisfiable");
//! let Some(&Value::Position(x)) = model.get("x") else { panic!("x is a position") };
//! assert!(x > 3);
//! # Ok::<(), mata_logic::Error>(())
//! ```
//!
//! # Semantics
//!
//! A first-order variable is a position, a natural number. A second-order variable is a finite set of positions.
//! Quantifiers range over all of them, so `all1 x: ex1 y: x < y` is valid (there is no last position), and
//! `ex2 X: 0 in X & all1 x: x in X => x + 1 in X` is not (no finite set is closed under successor). A formula
//! with free variables is valid if every assignment satisfies it.
//!
//! # Syntax
//!
//! | Form | Meaning |
//! |---|---|
//! | `x`, `y1`, `pos_2` | a first-order variable: the name starts with a lowercase ASCII letter |
//! | `X`, `Y1`, `Set_2` | a second-order variable: the name starts with an uppercase ASCII letter |
//! | `3`, `x + 3` | a position: a constant, or a variable plus a constant |
//! | `empty` | the empty set |
//! | `t = u`, `t < u`, `t <= u` | positions compared |
//! | `t in X` | membership |
//! | `X = Y`, `X sub Y` | sets equal, subset |
//! | `true`, `false` | constants |
//! | `~φ`, `φ & ψ`, `φ \| ψ`, `φ => ψ`, `φ <=> ψ` | connectives, from the tightest to the loosest |
//! | `ex1 x, y: φ`, `all1 x: φ` | first-order quantifiers |
//! | `ex2 X, Y: φ`, `all2 X: φ` | second-order quantifiers |
//!
//! Names and digits are ASCII; a name continues with letters, digits and `_`. The keywords `ex1`, `ex2`, `all1`,
//! `all2`, `in`, `sub`, `true`, `false` and `empty` are not names. The case of a name fixes its kind, so a
//! quantifier binds names of its order only. `=>` is right associative, `<=>` left associative. A quantifier
//! reaches as far right as possible: `a & ex1 x: b | c` is `a & (ex1 x: (b | c))`. A bound name hides the free
//! variable and the outer bindings of the same name. Every free name is one variable of the [`Ws1s`]: formulas
//! parsed by the same [`Ws1s`] share their free variables. The connectives and quantifiers nest at most 1000
//! deep; a long chain of `&` or `|` counts as a balanced tree.
//!
//! # Encoding
//!
//! An assignment is a word with one tape per variable, the bit at step `i` telling whether position `i` is in the
//! set; zero columns may follow. A position is a set with one element: each atom accepts only singletons on its
//! first-order tapes, the first-order quantifiers conjoin this constraint to their body (`ex1 x: φ` is
//! `∃x. sing(x) ∧ φ`, `all1 x: φ` is `¬∃x. sing(x) ∧ ¬φ`), and so do the queries for the free first-order
//! variables. The negation of an atom thus holds words that are no assignment, but these never reach an answer.

mod atom;
mod parse;

use std::fmt;

use mata_lazy::Options;
use rustc_hash::{FxHashMap, FxHashSet};

use self::atom::{Atom, Leaf};
use self::parse::{Instr, Name, Parsed, is_first_order};
use crate::{Error, Logic, Model, Padding, Prop, Var};

/// The value of a variable in an [`Assignment`].
#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub enum Value {
	/// The position of a first-order variable.
	Position(usize),
	/// The positions of a second-order variable, ascending.
	Set(Vec<usize>),
}

impl fmt::Display for Value {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		match self {
			Value::Position(position) => write!(f, "{position}"),
			Value::Set(positions) => {
				f.write_str("{")?;
				for (index, position) in positions.iter().enumerate() {
					if index > 0 {
						f.write_str(", ")?;
					}
					write!(f, "{position}")?;
				}
				f.write_str("}")
			}
		}
	}
}

/// Values of the free variables of a prop, displayed as `x = 3, X = {1, 4}`.
#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub struct Assignment {
	values: Vec<(String, Value)>,
}

impl Assignment {
	/// The value of the variable called `name`, or `None` if the assignment does not give it one.
	#[must_use]
	pub fn get(&self, name: &str) -> Option<&Value> {
		self.values
			.iter()
			.find(|(known, _)| known == name)
			.map(|(_, value)| value)
	}

	/// The variables by name with their values, in the order of [`Prop::vars`].
	pub fn iter(&self) -> impl ExactSizeIterator<Item = (&str, &Value)> {
		self.values
			.iter()
			.map(|(name, value)| (name.as_str(), value))
	}
}

impl fmt::Display for Assignment {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		for (index, (name, value)) in self.iter().enumerate() {
			if index > 0 {
				f.write_str(", ")?;
			}
			write!(f, "{name} = {value}")?;
		}
		Ok(())
	}
}

/// Parses and decides WS1S formulas; see the [module](self) for the syntax and the semantics.
#[derive(Debug)]
pub struct Ws1s {
	logic: Logic,
	/// The free variables of the parsed formulas, by name.
	free: FxHashMap<String, Var>,
	/// The first-order variables, free and bound.
	first_order: FxHashSet<Var>,
	/// `sing(x)` per first-order variable `x`.
	singletons: FxHashMap<Var, Prop>,
}

impl Default for Ws1s {
	fn default() -> Ws1s {
		Ws1s::new()
	}
}

impl Ws1s {
	/// A WS1S logic without formulas.
	#[must_use]
	pub fn new() -> Ws1s {
		Ws1s {
			logic: Logic::new(Padding::Zero),
			free: FxHashMap::default(),
			first_order: FxHashSet::default(),
			singletons: FxHashMap::default(),
		}
	}

	/// The logic that holds the props, for its [`formula`](Logic::formula) and the names of the variables.
	#[must_use]
	pub fn logic(&self) -> &Logic {
		&self.logic
	}

	/// The logic that holds the props, to combine them with its connectives. Its quantifiers do not constrain
	/// first-order variables to positions; quantify them in a parsed formula instead.
	pub fn logic_mut(&mut self) -> &mut Logic {
		&mut self.logic
	}

	/// Parses `text` into a prop over its free variables.
	///
	/// A first-order variable is free in the prop even if no position satisfies its atoms, but an atom that the
	/// parser decides, such as `x in empty` or `empty sub X`, drops its variables.
	///
	/// # Errors
	///
	/// [`Parse`](Error::Parse) if `text` is not a formula, with the byte offset of the error.
	pub fn parse(&mut self, text: &str) -> Result<Prop, Error> {
		let parsed = parse::parse(text)?;
		self.build(parsed)
	}

	/// The prop of `parsed`, whose instructions are well nested.
	fn build(&mut self, parsed: Parsed<'_>) -> Result<Prop, Error> {
		let free: Vec<Var> = parsed
			.free
			.iter()
			.map(|&name| self.free_var(name))
			.collect();
		let bound: Vec<Var> = parsed
			.bound
			.iter()
			.map(|&name| self.new_var(name))
			.collect();
		let var = |name: Name| match name {
			Name::Free(index) => free[index],
			Name::Bound(index) => bound[index],
		};

		let mut stack: Vec<Prop> = Vec::new();
		for instr in parsed.instrs {
			let prop = match instr {
				Instr::Constant(value) => self.logic.constant(value)?,
				Instr::Atom { atom, args } => self.atom(atom, args.map(var))?,
				Instr::Not => {
					let operand = stack.pop().expect("an operand");
					self.logic.not(&operand)?
				}
				Instr::And(operands) | Instr::Or(operands) => {
					let conjunction = matches!(instr, Instr::And(_));
					let operands = stack.split_off(stack.len() - operands);
					self.balanced(operands, conjunction)?
				}
				Instr::Implies | Instr::Iff => {
					let rhs = stack.pop().expect("an operand");
					let lhs = stack.pop().expect("an operand");
					if instr == Instr::Implies {
						self.logic.implies(&lhs, &rhs)?
					} else {
						self.logic.iff(&lhs, &rhs)?
					}
				}
				Instr::Quantifier {
					universal,
					first_order,
					vars,
				} => {
					let body = stack.pop().expect("an operand");
					self.quantify(universal, first_order, &bound[vars], &body)?
				}
			};
			stack.push(prop);
		}

		let prop = stack.pop().expect("a formula");
		debug_assert!(stack.is_empty(), "one formula");
		Ok(prop)
	}

	/// An assignment that satisfies `prop`, or `None` if it is unsatisfiable.
	///
	/// # Errors
	///
	/// [`Lazy`](Error::Lazy): a limit of `options`, or `prop` belongs to another logic.
	pub fn model(&mut self, prop: &Prop, options: &Options) -> Result<Option<Assignment>, Error> {
		let mut guarded = prop.clone();
		for &var in prop.vars() {
			if self.first_order.contains(&var) {
				let singleton = self.singleton(var)?;
				guarded = self.logic.and(&guarded, &singleton)?;
			}
		}

		Ok(self
			.logic
			.model(&guarded, options)?
			.map(|model| self.decode(&model)))
	}

	/// An assignment that falsifies `prop`, or `None` if it is valid.
	///
	/// # Errors
	///
	/// As [`model`](Ws1s::model).
	pub fn counterexample(
		&mut self,
		prop: &Prop,
		options: &Options,
	) -> Result<Option<Assignment>, Error> {
		let not_prop = self.logic.not(prop)?;
		self.model(&not_prop, options)
	}

	/// Whether some assignment satisfies `prop`.
	///
	/// # Errors
	///
	/// As [`model`](Ws1s::model).
	pub fn satisfiable(&mut self, prop: &Prop, options: &Options) -> Result<bool, Error> {
		Ok(self.model(prop, options)?.is_some())
	}

	/// Whether every assignment satisfies `prop`.
	///
	/// # Errors
	///
	/// As [`model`](Ws1s::model).
	pub fn valid(&mut self, prop: &Prop, options: &Options) -> Result<bool, Error> {
		Ok(self.counterexample(prop, options)?.is_none())
	}

	fn free_var(&mut self, name: &str) -> Var {
		if let Some(&var) = self.free.get(name) {
			return var;
		}

		let var = self.new_var(name);
		self.free.insert(name.to_owned(), var);
		var
	}

	fn new_var(&mut self, name: &str) -> Var {
		let var = self.logic.var(name);
		if is_first_order(name) {
			self.first_order.insert(var);
		}
		var
	}

	fn atom(&mut self, atom: Atom, args: [Var; 2]) -> Result<Prop, Error> {
		let vars: &[Var] = if atom.arguments() == 1 || args[0] == args[1] {
			&args[..1]
		} else {
			&args
		};
		let leaf = Leaf {
			atom,
			tapes: args.map(|arg| usize::from(arg != vars[0])),
			arity: vars.len(),
		};
		self.logic.atom(&leaf, vars)
	}

	fn singleton(&mut self, var: Var) -> Result<Prop, Error> {
		if let Some(prop) = self.singletons.get(&var) {
			return Ok(prop.clone());
		}

		let prop = self.atom(Atom::Singleton, [var, var])?;
		self.singletons.insert(var, prop.clone());
		Ok(prop)
	}

	/// The conjunction or disjunction of `operands`, as a balanced tree.
	fn balanced(&mut self, mut operands: Vec<Prop>, conjunction: bool) -> Result<Prop, Error> {
		while operands.len() > 1 {
			let mut next = Vec::with_capacity(operands.len().div_ceil(2));
			for pair in operands.chunks(2) {
				next.push(match pair {
					[lhs, rhs] if conjunction => self.logic.and(lhs, rhs)?,
					[lhs, rhs] => self.logic.or(lhs, rhs)?,
					[single] => single.clone(),
					_ => unreachable!("chunks of at most 2"),
				});
			}
			operands = next;
		}

		Ok(operands.pop().expect("at least one operand"))
	}

	fn quantify(
		&mut self,
		universal: bool,
		first_order: bool,
		vars: &[Var],
		body: &Prop,
	) -> Result<Prop, Error> {
		if !first_order {
			return if universal {
				self.logic.forall(body, vars)
			} else {
				self.logic.exists(body, vars)
			};
		}

		// ∃x. sing(x) ∧ φ, and ∀x. φ as ¬∃x. sing(x) ∧ ¬φ.
		let mut guarded = if universal {
			self.logic.not(body)?
		} else {
			body.clone()
		};
		for &var in vars {
			let singleton = self.singleton(var)?;
			guarded = self.logic.and(&guarded, &singleton)?;
		}

		let exists = self.logic.exists(&guarded, vars)?;
		if universal {
			self.logic.not(&exists)
		} else {
			Ok(exists)
		}
	}

	fn decode(&self, model: &Model) -> Assignment {
		let values = model
			.vars()
			.iter()
			.map(|&var| {
				let bits = model.bits(var).expect("a variable of the model");
				let mut positions = bits
					.iter()
					.enumerate()
					.filter(|&(_, &bit)| bit)
					.map(|(position, _)| position);
				let value = if self.first_order.contains(&var) {
					let position = positions.next().expect("a position is a singleton");
					Value::Position(position)
				} else {
					Value::Set(positions.collect())
				};
				let name = self.logic.name(var).expect("a variable of the logic");
				(name.to_owned(), value)
			})
			.collect();

		Assignment { values }
	}
}
