//! Formulas over variables encoded as tapes of bits.

use std::fmt;

use mata_lazy::{Automaton, Formula, Options, Pad, Symbol, Tape, Term};
use rustc_hash::FxHashMap;

use crate::closure::{Constant, Encodings};
use crate::{Error, Model};

/// The bit 0, as the symbol of a tape.
pub const ZERO: Symbol = Symbol::new(0);
/// The bit 1, as the symbol of a tape.
pub const ONE: Symbol = Symbol::new(1);
/// The alphabet of every tape.
pub(crate) const BITS: [Symbol; 2] = [ZERO, ONE];

/// How a value has many encodings: the columns that can be appended to a word without changing the values it
/// encodes.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum Padding {
	/// Columns of zeros, as for natural numbers least significant bit first, or for finite sets of positions
	/// (WS1S). The empty word encodes zeros (empty sets).
	Zero,
	/// The last column, again, as for integers in two's complement least significant bit first (LIA), whose last
	/// bit is the sign. Every word has at least one column; the empty word encodes nothing.
	Sign,
}

/// A variable of a [`Logic`].
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct Var(u32);

impl Var {
	/// The number of the variable, in the order of [`Logic::var`] calls, from 0.
	#[must_use]
	pub fn index(self) -> usize {
		self.0 as usize
	}
}

impl fmt::Display for Var {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		write!(f, "v{}", self.0)
	}
}

/// A formula of a [`Logic`]: a [`Term`] of its [`Formula`] with one tape per free variable, in the order of
/// [`vars`](Prop::vars).
///
/// Its relation holds exactly the encodings of the assignments that satisfy it: every word, with every padding,
/// of every satisfying assignment, and nothing else.
#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub struct Prop {
	term: Term,
	vars: Box<[Var]>,
}

impl Prop {
	/// The term in [`Logic::formula`].
	#[must_use]
	pub fn term(&self) -> Term {
		self.term
	}

	/// The free variables, ascending; tape `i` of [`term`](Prop::term) encodes `vars()[i]`.
	#[must_use]
	pub fn vars(&self) -> &[Var] {
		&self.vars
	}
}

/// Builds formulas over variables from atoms given as automata, and decides them with [`mata_lazy`].
///
/// An assignment is encoded as a word of columns, one bit per variable, read on one tape per variable in lock step.
/// An atom is an [`Automaton`] over the bits [`ZERO`] and [`ONE`] that accepts exactly the encodings of the
/// assignments that satisfy it, with every [`Padding`]. The connectives keep this invariant: the complement drops
/// the words that encode nothing, and a quantifier closes the relation under padding after it drops a tape.
///
/// ```
/// use mata_logic::{Logic, Padding, ZERO, ONE};
/// use mata_lazy::{Automaton, Options, Symbol};
///
/// /// `x = y` over natural numbers: both tapes read the same bits.
/// struct Equal;
///
/// impl Automaton for Equal {
///     type State = ();
///
///     fn arity(&self) -> usize { 2 }
///     fn initial_states(&self) -> impl IntoIterator<Item = ()> { [()] }
///     fn is_final(&self, (): ()) -> bool { true }
///     fn for_each_move(&self, (): (), mut visit: impl FnMut(&[Symbol], ())) {
///         visit(&[ZERO, ZERO], ());
///         visit(&[ONE, ONE], ());
///     }
/// }
///
/// let mut logic = Logic::new(Padding::Zero);
/// let (x, y) = (logic.var("x"), logic.var("y"));
/// let equal = logic.atom(&Equal, &[x, y])?;
/// // ∀x ∃y. x = y
/// let every = logic.exists(&equal, &[y])?;
/// let every = logic.forall(&every, &[x])?;
/// assert!(logic.counterexample(&every, &Options::new())?.is_none());
/// # Ok::<(), mata_logic::Error>(())
/// ```
#[derive(Debug)]
pub struct Logic {
	formula: Formula,
	padding: Padding,
	names: Vec<String>,
	/// The valid encodings over `n` tapes, by `n`.
	encodings: FxHashMap<usize, Term>,
}

impl Logic {
	/// A logic with no variables, whose values are padded as `padding` says.
	#[must_use]
	pub fn new(padding: Padding) -> Logic {
		Logic {
			formula: Formula::new(),
			padding,
			names: Vec::new(),
			encodings: FxHashMap::default(),
		}
	}

	/// How values are padded.
	#[must_use]
	pub fn padding(&self) -> Padding {
		self.padding
	}

	/// The formula that holds the terms of the props, for queries with options such as
	/// [`Options::explicit`].
	#[must_use]
	pub fn formula(&self) -> &Formula {
		&self.formula
	}

	/// A new variable. Names are for display only; two variables may share one.
	///
	/// # Panics
	///
	/// If 2³² variables exist already.
	pub fn var(&mut self, name: impl Into<String>) -> Var {
		let var = Var(u32::try_from(self.names.len()).expect("fewer than 2^32 variables"));
		self.names.push(name.into());
		var
	}

	/// The name of `var`, or `None` if `var` is not a variable of this logic.
	#[must_use]
	pub fn name(&self, var: Var) -> Option<&str> {
		self.names.get(var.index()).map(String::as_str)
	}

	/// An atom: the relation of `automaton`, whose tape `i` encodes `vars[i]`.
	///
	/// The relation must hold exactly the encodings of the assignments that satisfy the atom, with every padding,
	/// and read only [`ZERO`] and [`ONE`] (and [`ANY`](Symbol::ANY), which ranges over them); the other connectives
	/// rely on it. An atom without variables is decided now: it is true if `automaton` accepts the empty word.
	///
	/// # Errors
	///
	/// [`AtomArity`](Error::AtomArity) if `automaton` does not have one tape per variable,
	/// [`ForeignVar`](Error::ForeignVar), [`RepeatedVar`](Error::RepeatedVar), or [`Lazy`](Error::Lazy) from
	/// [`Formula::leaf_over`].
	pub fn atom(&mut self, automaton: &impl Automaton, vars: &[Var]) -> Result<Prop, Error> {
		if automaton.arity() != vars.len() {
			return Err(Error::AtomArity {
				arity: automaton.arity(),
				vars: vars.len(),
			});
		}

		self.check_vars(vars)?;
		let leaf = self.formula.leaf_over(automaton, [BITS])?;
		if vars.is_empty() {
			// A relation without tapes counts steps; a prop without variables holds every length or none.
			let holds = !self.formula.is_empty(leaf)?;
			return self.constant(holds);
		}

		let mut order: Vec<usize> = (0..vars.len()).collect();
		order.sort_unstable_by_key(|&tape| vars[tape]);
		let term = if order.is_sorted() {
			leaf
		} else {
			self.formula.project(leaf, &order)?
		};

		Ok(Prop {
			term,
			vars: order.iter().map(|&tape| vars[tape]).collect(),
		})
	}

	/// The formula without variables that is `value`.
	///
	/// # Errors
	///
	/// [`Lazy`](Error::Lazy), which does not happen with a formula built only through this logic.
	pub fn constant(&mut self, value: bool) -> Result<Prop, Error> {
		let term = self.formula.leaf(&Constant(value))?;

		Ok(Prop {
			term,
			vars: Box::default(),
		})
	}

	/// `lhs ∧ rhs`.
	///
	/// # Errors
	///
	/// [`Lazy`](Error::Lazy) if a prop belongs to another logic.
	pub fn and(&mut self, lhs: &Prop, rhs: &Prop) -> Result<Prop, Error> {
		let mut synchronized = Vec::new();
		let mut layout = Vec::new();
		let mut vars = Vec::new();
		let (mut i, mut j) = (0, 0);
		loop {
			let (tape, var) = match (lhs.vars.get(i), rhs.vars.get(j)) {
				(None, None) => break,
				(Some(&left), Some(&right)) if left == right => {
					synchronized.push((i, j));
					i += 1;
					j += 1;
					(Tape::Lhs(i - 1), left)
				}
				(Some(&left), right) if right.is_none_or(|&right| left < right) => {
					i += 1;
					(Tape::Lhs(i - 1), left)
				}
				(_, Some(&right)) => {
					j += 1;
					(Tape::Rhs(j - 1), right)
				}
				(Some(_), None) => unreachable!("the guard above takes it"),
			};
			layout.push(tape);
			vars.push(var);
		}

		let term = self
			.formula
			.sync_product(lhs.term, rhs.term, &synchronized, &layout)?;

		Ok(Prop {
			term,
			vars: vars.into(),
		})
	}

	/// `lhs ∨ rhs`.
	///
	/// # Errors
	///
	/// [`Lazy`](Error::Lazy) if a prop belongs to another logic.
	pub fn or(&mut self, lhs: &Prop, rhs: &Prop) -> Result<Prop, Error> {
		let lhs = self.cylindrify(lhs, &rhs.vars)?;
		let rhs = self.cylindrify(rhs, &lhs.vars)?;
		let term = self.formula.union(lhs.term, rhs.term)?;

		Ok(Prop {
			term,
			vars: lhs.vars,
		})
	}

	/// `¬prop`.
	///
	/// # Errors
	///
	/// [`Lazy`](Error::Lazy) if `prop` belongs to another logic.
	pub fn not(&mut self, prop: &Prop) -> Result<Prop, Error> {
		let complement = self.formula.complement(prop.term)?;
		// The complement also holds the words that encode nothing.
		let term = match self.padding {
			Padding::Sign if !prop.vars.is_empty() => {
				let encodings = self.encodings(prop.vars.len())?;
				self.formula.intersection(complement, encodings)?
			}
			Padding::Sign | Padding::Zero => complement,
		};

		Ok(Prop {
			term,
			vars: prop.vars.clone(),
		})
	}

	/// `lhs → rhs`.
	///
	/// # Errors
	///
	/// [`Lazy`](Error::Lazy) if a prop belongs to another logic.
	pub fn implies(&mut self, lhs: &Prop, rhs: &Prop) -> Result<Prop, Error> {
		let not_lhs = self.not(lhs)?;
		self.or(&not_lhs, rhs)
	}

	/// `lhs ↔ rhs`.
	///
	/// # Errors
	///
	/// [`Lazy`](Error::Lazy) if a prop belongs to another logic.
	pub fn iff(&mut self, lhs: &Prop, rhs: &Prop) -> Result<Prop, Error> {
		let forward = self.implies(lhs, rhs)?;
		let backward = self.implies(rhs, lhs)?;
		self.and(&forward, &backward)
	}

	/// `∃vars. prop`. Variables of `vars` that are not free in `prop` are skipped.
	///
	/// A prop without free variables is decided now.
	///
	/// # Errors
	///
	/// [`ForeignVar`](Error::ForeignVar), [`RepeatedVar`](Error::RepeatedVar), or [`Lazy`](Error::Lazy) if
	/// `prop` belongs to another logic.
	pub fn exists(&mut self, prop: &Prop, vars: &[Var]) -> Result<Prop, Error> {
		self.check_vars(vars)?;
		let kept: Vec<usize> = (0..prop.vars.len())
			.filter(|&tape| !vars.contains(&prop.vars[tape]))
			.collect();
		if kept.len() == prop.vars.len() {
			return Ok(prop.clone());
		}

		if kept.is_empty() {
			let holds = !self.formula.is_empty(prop.term)?;
			return self.constant(holds);
		}

		let projected = self.formula.project(prop.term, &kept)?;
		// The dropped tapes may have needed longer words than the kept ones: the kept words are then in the
		// relation only with padding.
		let pad = match self.padding {
			Padding::Zero => Pad::Step(vec![ZERO; kept.len()].into()),
			Padding::Sign => Pad::Last,
		};
		let term = self.formula.pad_closure(projected, pad)?;

		Ok(Prop {
			term,
			vars: kept.iter().map(|&tape| prop.vars[tape]).collect(),
		})
	}

	/// `∀vars. prop`, as `¬∃vars. ¬prop`.
	///
	/// # Errors
	///
	/// As [`exists`](Logic::exists).
	pub fn forall(&mut self, prop: &Prop, vars: &[Var]) -> Result<Prop, Error> {
		let not_prop = self.not(prop)?;
		let exists = self.exists(&not_prop, vars)?;
		self.not(&exists)
	}

	/// An assignment that satisfies `prop`, or `None` if it is unsatisfiable.
	///
	/// # Errors
	///
	/// [`Lazy`](Error::Lazy): a limit of `options`, or `prop` belongs to another logic.
	pub fn model(&self, prop: &Prop, options: &Options) -> Result<Option<Model>, Error> {
		let outcome = self.formula.check(prop.term, options)?;

		Ok(outcome
			.witness
			.map(|witness| Model::new(prop.vars.clone(), &witness)))
	}

	/// An assignment that falsifies `prop`, or `None` if it is valid.
	///
	/// # Errors
	///
	/// As [`model`](Logic::model).
	pub fn counterexample(
		&mut self,
		prop: &Prop,
		options: &Options,
	) -> Result<Option<Model>, Error> {
		let not_prop = self.not(prop)?;
		self.model(&not_prop, options)
	}

	fn check_vars(&self, vars: &[Var]) -> Result<(), Error> {
		for (position, &var) in vars.iter().enumerate() {
			if var.index() >= self.names.len() {
				return Err(Error::ForeignVar { var });
			}

			if vars[..position].contains(&var) {
				return Err(Error::RepeatedVar { var });
			}
		}

		Ok(())
	}

	/// `prop` over the free variables of `prop` and `others`: the variables it lacks take any value.
	fn cylindrify(&mut self, prop: &Prop, others: &[Var]) -> Result<Prop, Error> {
		let missing: Box<[Var]> = others
			.iter()
			.filter(|var| prop.vars.binary_search(var).is_err())
			.copied()
			.collect();
		if missing.is_empty() {
			return Ok(prop.clone());
		}

		let any = Prop {
			term: self.encodings(missing.len())?,
			vars: missing,
		};
		self.and(prop, &any)
	}

	/// The words that encode something, over `tapes` tapes.
	fn encodings(&mut self, tapes: usize) -> Result<Term, Error> {
		if let Some(&term) = self.encodings.get(&tapes) {
			return Ok(term);
		}

		let encodings = Encodings {
			tapes,
			nonempty: self.padding == Padding::Sign,
		};
		let term = self.formula.leaf_over(&encodings, [BITS])?;
		self.encodings.insert(tapes, term);

		Ok(term)
	}
}
