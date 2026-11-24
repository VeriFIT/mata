//! The parser of WS1S formulas: tokens, then instructions in postfix order, by operator precedence without
//! recursion.

use std::ops::Range;

use super::atom::{Atom, Cmp};
use crate::Error;

/// The deepest nesting of connectives and quantifiers in a formula. The engine recurses once per level of a
/// term, and a WebAssembly stack cannot grow.
pub(crate) const MAX_DEPTH: u32 = 1000;

const QUANTIFIERS: [&str; 4] = ["ex1", "ex2", "all1", "all2"];
const KEYWORDS: [&str; 9] = [
	"ex1", "ex2", "all1", "all2", "in", "sub", "true", "false", "empty",
];

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum Token<'a> {
	Ident(&'a str),
	Number(u32),
	Open,
	Close,
	Colon,
	Comma,
	Not,
	And,
	Or,
	Implies,
	Iff,
	Less,
	LessEq,
	Equal,
	Plus,
	End,
}

/// A variable of a formula.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) enum Name {
	/// Free, by its index in [`Parsed::free`].
	Free(usize),
	/// Bound, by its index in [`Parsed::bound`].
	Bound(usize),
}

/// A step of building a formula, in postfix order: it pops its operands and pushes its result.
#[derive(Clone, Debug, PartialEq, Eq)]
pub(crate) enum Instr {
	Constant(bool),
	/// The second argument repeats the first one if the atom has one argument.
	Atom {
		atom: Atom,
		args: [Name; 2],
	},
	Not,
	/// The conjunction of this many operands, at least 2.
	And(usize),
	/// The disjunction of this many operands, at least 2.
	Or(usize),
	Implies,
	Iff,
	Quantifier {
		universal: bool,
		first_order: bool,
		/// The bound variables, by their indices in [`Parsed::bound`].
		vars: Range<usize>,
	},
}

/// A parsed formula.
#[derive(Debug)]
pub(crate) struct Parsed<'a> {
	pub(crate) instrs: Vec<Instr>,
	/// The free variables, in the order they first occur.
	pub(crate) free: Vec<&'a str>,
	/// The bound variables, one per binding.
	pub(crate) bound: Vec<&'a str>,
}

/// Whether a variable name is first-order: it starts with a lowercase letter.
pub(crate) fn is_first_order(name: &str) -> bool {
	name.starts_with(|c: char| c.is_ascii_lowercase())
}

/// Parses `text`.
pub(crate) fn parse(text: &str) -> Result<Parsed<'_>, Error> {
	Parser {
		tokens: tokenize(text)?,
		next: 0,
		ops: Vec::new(),
		scope: Vec::new(),
		depths: Vec::new(),
		parsed: Parsed {
			instrs: Vec::new(),
			free: Vec::new(),
			bound: Vec::new(),
		},
	}
	.run()
}

fn error(offset: usize, message: impl Into<String>) -> Error {
	Error::Parse {
		offset,
		message: message.into(),
	}
}

fn tokenize(text: &str) -> Result<Vec<(Token<'_>, usize)>, Error> {
	let bytes = text.as_bytes();
	let mut tokens = Vec::new();
	let mut offset = 0;
	while offset < bytes.len() {
		let start = offset;
		let rest = &bytes[offset..];
		let (token, length) = match rest[0] {
			byte if byte.is_ascii_whitespace() => {
				offset += 1;
				continue;
			}
			b'(' => (Token::Open, 1),
			b')' => (Token::Close, 1),
			b':' => (Token::Colon, 1),
			b',' => (Token::Comma, 1),
			b'~' => (Token::Not, 1),
			b'&' => (Token::And, 1),
			b'|' => (Token::Or, 1),
			b'+' => (Token::Plus, 1),
			_ if rest.starts_with(b"<=>") => (Token::Iff, 3),
			_ if rest.starts_with(b"<=") => (Token::LessEq, 2),
			b'<' => (Token::Less, 1),
			_ if rest.starts_with(b"=>") => (Token::Implies, 2),
			b'=' => (Token::Equal, 1),
			byte if byte.is_ascii_digit() => {
				let length = rest.iter().take_while(|byte| byte.is_ascii_digit()).count();
				let number = text[start..start + length]
					.parse()
					.map_err(|_| error(start, "the number does not fit 32 bits"))?;
				(Token::Number(number), length)
			}
			byte if byte.is_ascii_alphabetic() => {
				let length = rest
					.iter()
					.take_while(|&&byte| byte.is_ascii_alphanumeric() || byte == b'_')
					.count();
				(Token::Ident(&text[start..start + length]), length)
			}
			_ => {
				let found = text[start..].chars().next().unwrap_or_default();
				return Err(error(start, format!("unexpected character `{found}`")));
			}
		};
		tokens.push((token, start));
		offset += length;
	}

	tokens.push((Token::End, text.len()));
	Ok(tokens)
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum Binary {
	Iff,
	Implies,
	Or,
	And,
}

impl Binary {
	fn precedence(self) -> u8 {
		match self {
			Binary::Iff => 1,
			Binary::Implies => 2,
			Binary::Or => 3,
			Binary::And => 4,
		}
	}
}

/// An operator waiting for its operands.
#[derive(Debug)]
enum Op {
	Open(usize),
	Not(usize),
	Quantifier {
		universal: bool,
		first_order: bool,
		vars: Range<usize>,
		/// The length of the scope outside the quantifier.
		scope: usize,
		offset: usize,
	},
	Binary {
		op: Binary,
		operands: usize,
		offset: usize,
	},
}

/// A term of an atom.
#[derive(Clone, Copy, Debug)]
enum Term {
	/// A position plus a constant.
	Position(Name, i64),
	Number(i64),
	Set(Name),
	Empty,
}

struct Parser<'a> {
	tokens: Vec<(Token<'a>, usize)>,
	next: usize,
	/// The operators of the shunting-yard algorithm.
	ops: Vec<Op>,
	/// The bound variables in scope, innermost last, with their indices in [`Parsed::bound`].
	scope: Vec<(&'a str, usize)>,
	/// The nesting depth of each operand that [`Parsed::instrs`] pushes.
	depths: Vec<u32>,
	parsed: Parsed<'a>,
}

impl<'a> Parser<'a> {
	fn run(mut self) -> Result<Parsed<'a>, Error> {
		loop {
			// An operand: prefix operators, then an atom.
			loop {
				let (token, offset) = self.advance();
				match token {
					Token::Not => self.ops.push(Op::Not(offset)),
					Token::Open => self.ops.push(Op::Open(offset)),
					Token::Ident(word) if QUANTIFIERS.contains(&word) => {
						self.quantifier(word, offset)?;
					}
					_ => {
						self.atom(token, offset)?;
						break;
					}
				}
			}

			// Closing parentheses, then a binary operator or the end.
			loop {
				let (token, offset) = self.advance();
				let op = match token {
					Token::Close => {
						self.close(offset)?;
						continue;
					}
					Token::End => {
						self.finish()?;
						return Ok(self.parsed);
					}
					Token::And => Binary::And,
					Token::Or => Binary::Or,
					Token::Implies => Binary::Implies,
					Token::Iff => Binary::Iff,
					_ => {
						return Err(error(
							offset,
							"expected `&`, `|`, `=>`, `<=>`, `)` or the end",
						));
					}
				};
				self.binary(op, offset)?;
				break;
			}
		}
	}

	fn advance(&mut self) -> (Token<'a>, usize) {
		let token = self.tokens[self.next];
		if token.0 != Token::End {
			self.next += 1;
		}
		token
	}

	fn peek(&self) -> Token<'a> {
		self.tokens[self.next].0
	}

	/// After the keyword: `x, y, …:`.
	fn quantifier(&mut self, keyword: &str, offset: usize) -> Result<(), Error> {
		let first_order = keyword.ends_with('1');
		let start = self.parsed.bound.len();
		loop {
			let (token, at) = self.advance();
			let Token::Ident(name) = token else {
				return Err(error(at, "expected a variable"));
			};
			if KEYWORDS.contains(&name) {
				return Err(error(at, format!("`{name}` is a keyword, not a variable")));
			}

			if is_first_order(name) != first_order {
				let message = if first_order {
					format!(
						"`{keyword}` binds positions, whose names start with a lowercase letter"
					)
				} else {
					format!("`{keyword}` binds sets, whose names start with an uppercase letter")
				};
				return Err(error(at, message));
			}

			if self.parsed.bound[start..].contains(&name) {
				return Err(error(at, format!("`{name}` is bound twice")));
			}

			self.parsed.bound.push(name);
			match self.advance() {
				(Token::Comma, _) => {}
				(Token::Colon, _) => break,
				(_, at) => return Err(error(at, "expected `,` or `:`")),
			}
		}

		let vars = start..self.parsed.bound.len();
		let scope = self.scope.len();
		self.scope
			.extend(vars.clone().map(|index| (self.parsed.bound[index], index)));
		self.ops.push(Op::Quantifier {
			universal: keyword.starts_with("all"),
			first_order,
			vars,
			scope,
			offset,
		});
		Ok(())
	}

	fn binary(&mut self, op: Binary, offset: usize) -> Result<(), Error> {
		while let Some(top) = self.ops.last() {
			let pops = match *top {
				Op::Open(_) | Op::Quantifier { .. } => false,
				Op::Not(_) => true,
				// `<=>` is left associative, `=>` right associative, and `&`, `|` gather their operands.
				Op::Binary { op: top, .. } => {
					top.precedence() > op.precedence() || (top == op && op == Binary::Iff)
				}
			};
			if !pops {
				break;
			}

			let top = self.ops.pop().expect("the loop saw it");
			self.reduce(top)?;
		}

		if let Some(Op::Binary {
			op: top, operands, ..
		}) = self.ops.last_mut()
			&& *top == op
			&& matches!(op, Binary::And | Binary::Or)
		{
			*operands += 1;
		} else {
			self.ops.push(Op::Binary {
				op,
				operands: 2,
				offset,
			});
		}

		Ok(())
	}

	fn close(&mut self, offset: usize) -> Result<(), Error> {
		loop {
			match self.ops.pop() {
				None => return Err(error(offset, "unmatched `)`")),
				Some(Op::Open(_)) => return Ok(()),
				Some(op) => self.reduce(op)?,
			}
		}
	}

	fn finish(&mut self) -> Result<(), Error> {
		while let Some(op) = self.ops.pop() {
			if let Op::Open(offset) = op {
				return Err(error(offset, "unclosed `(`"));
			}

			self.reduce(op)?;
		}

		Ok(())
	}

	fn reduce(&mut self, op: Op) -> Result<(), Error> {
		match op {
			Op::Open(_) => unreachable!("parentheses are matched, not reduced"),
			Op::Not(offset) => self.emit(Instr::Not, offset),
			Op::Quantifier {
				universal,
				first_order,
				vars,
				scope,
				offset,
			} => {
				self.scope.truncate(scope);
				self.emit(
					Instr::Quantifier {
						universal,
						first_order,
						vars,
					},
					offset,
				)
			}
			Op::Binary {
				op,
				operands,
				offset,
			} => {
				let instr = match op {
					Binary::Iff => Instr::Iff,
					Binary::Implies => Instr::Implies,
					Binary::Or => Instr::Or(operands),
					Binary::And => Instr::And(operands),
				};
				self.emit(instr, offset)
			}
		}
	}

	/// Appends `instr`, if the formula does not nest too deep then.
	fn emit(&mut self, instr: Instr, offset: usize) -> Result<(), Error> {
		let mut pop = |count: usize| {
			let depths = self.depths.split_off(self.depths.len() - count);
			depths.into_iter().max().expect("an operand")
		};
		let depth = match instr {
			Instr::Constant(_) | Instr::Atom { .. } => 1,
			Instr::Not | Instr::Quantifier { .. } => pop(1) + 1,
			Instr::Implies | Instr::Iff => pop(2) + 1,
			// A balanced tree of binary connectives.
			Instr::And(operands) | Instr::Or(operands) => {
				pop(operands) + operands.next_power_of_two().trailing_zeros()
			}
		};
		if depth > MAX_DEPTH {
			return Err(error(
				offset,
				format!("the formula nests deeper than {MAX_DEPTH} levels"),
			));
		}

		self.depths.push(depth);
		self.parsed.instrs.push(instr);
		Ok(())
	}

	/// An atom, `true` or `false`, from its first token.
	fn atom(&mut self, token: Token<'a>, offset: usize) -> Result<(), Error> {
		match token {
			Token::Ident("true") => return self.emit(Instr::Constant(true), offset),
			Token::Ident("false") => return self.emit(Instr::Constant(false), offset),
			_ => {}
		}

		let lhs = self.term(token, offset)?;
		let (relation, at) = self.advance();
		if !matches!(
			relation,
			Token::Equal | Token::Less | Token::LessEq | Token::Ident("in" | "sub")
		) {
			return Err(error(at, "expected `=`, `<`, `<=`, `in` or `sub`"));
		}

		let (token, rhs_offset) = self.advance();
		let rhs = self.term(token, rhs_offset)?;
		let instr = match relation {
			Token::Equal => match (lhs, rhs) {
				(Term::Set(_) | Term::Empty, Term::Set(_) | Term::Empty) => {
					sets(Atom::Equal, lhs, rhs)
				}
				_ => positions(Cmp::Eq, 0, (lhs, offset), (rhs, rhs_offset))?,
			},
			Token::Less => positions(Cmp::Ge, 1, (lhs, offset), (rhs, rhs_offset))?,
			Token::LessEq => positions(Cmp::Ge, 0, (lhs, offset), (rhs, rhs_offset))?,
			Token::Ident("in") => {
				let set = match rhs {
					Term::Set(set) => Some(set),
					Term::Empty => None,
					_ => return Err(error(rhs_offset, "expected a set")),
				};
				match (lhs, set) {
					(Term::Position(x, k), Some(set)) => Instr::Atom {
						atom: Atom::Member(position_offset(k, offset)?),
						args: [x, set],
					},
					(Term::Number(k), Some(set)) => Instr::Atom {
						atom: Atom::Contains(position_offset(k, offset)?),
						args: [set, set],
					},
					(Term::Position(..) | Term::Number(_), None) => Instr::Constant(false),
					(Term::Set(_) | Term::Empty, _) => {
						return Err(error(offset, "expected a position"));
					}
				}
			}
			Token::Ident("sub") => {
				for (term, at) in [(lhs, offset), (rhs, rhs_offset)] {
					if let Term::Position(..) | Term::Number(_) = term {
						return Err(error(at, "expected a set"));
					}
				}
				sets(Atom::Subset, lhs, rhs)
			}
			_ => unreachable!("checked above"),
		};

		self.emit(instr, offset)
	}

	fn term(&mut self, token: Token<'a>, offset: usize) -> Result<Term, Error> {
		match token {
			Token::Number(number) => Ok(Term::Number(i64::from(number))),
			Token::Ident("empty") => Ok(Term::Empty),
			Token::Ident(name) if !KEYWORDS.contains(&name) => {
				let var = self.resolve(name);
				if !is_first_order(name) {
					return Ok(Term::Set(var));
				}

				if self.peek() != Token::Plus {
					return Ok(Term::Position(var, 0));
				}

				self.advance();
				match self.advance() {
					(Token::Number(number), _) => Ok(Term::Position(var, i64::from(number))),
					(_, at) => Err(error(at, "expected a number")),
				}
			}
			_ => Err(error(offset, "expected a formula")),
		}
	}

	fn resolve(&mut self, name: &'a str) -> Name {
		if let Some(&(_, index)) = self.scope.iter().rev().find(|&&(bound, _)| bound == name) {
			return Name::Bound(index);
		}

		let free = &mut self.parsed.free;
		Name::Free(
			free.iter()
				.position(|&known| known == name)
				.unwrap_or_else(|| {
					free.push(name);
					free.len() - 1
				}),
		)
	}
}

/// `lhs = rhs` or `lhs ⊆ rhs` (`atom`) between sets.
fn sets(atom: Atom, lhs: Term, rhs: Term) -> Instr {
	match (lhs, rhs) {
		(Term::Set(lhs), Term::Set(rhs)) => Instr::Atom {
			atom,
			args: [lhs, rhs],
		},
		(Term::Set(set), Term::Empty) => Instr::Atom {
			atom: Atom::Empty,
			args: [set, set],
		},
		(Term::Empty, Term::Set(set)) if atom == Atom::Equal => Instr::Atom {
			atom: Atom::Empty,
			args: [set, set],
		},
		_ => Instr::Constant(true),
	}
}

/// `lhs = rhs` (`cmp` is `Eq`, `shift` 0), `lhs < rhs` (`Ge`, 1) or `lhs ≤ rhs` (`Ge`, 0) between positions:
/// `rhs − lhs cmp shift`.
fn positions(
	cmp: Cmp,
	shift: i64,
	(lhs, lhs_offset): (Term, usize),
	(rhs, rhs_offset): (Term, usize),
) -> Result<Instr, Error> {
	let side = |term, offset| match term {
		Term::Position(var, k) => Ok((Some(var), k)),
		Term::Number(k) => Ok((None, k)),
		Term::Set(_) | Term::Empty => Err(error(offset, "expected a position")),
	};
	let (x, a) = side(lhs, lhs_offset)?;
	let (y, b) = side(rhs, rhs_offset)?;
	// (y + b) − (x + a) cmp shift
	let bound = a - b + shift;
	Ok(match (x, y) {
		(Some(x), Some(y)) => Instr::Atom {
			atom: Atom::Distance(cmp, bound),
			args: [x, y],
		},
		// y cmp a + shift − b
		(None, Some(y)) => Instr::Atom {
			atom: Atom::Position(cmp, bound),
			args: [y, y],
		},
		// b − (x + a) cmp shift, so x cmp' b − a − shift
		(Some(x), None) => {
			let flipped = match cmp {
				Cmp::Ge => Cmp::Le,
				Cmp::Eq | Cmp::Le => cmp,
			};
			Instr::Atom {
				atom: Atom::Position(flipped, b - a - shift),
				args: [x, x],
			}
		}
		(None, None) => Instr::Constant(match cmp {
			Cmp::Eq => b - a == shift,
			Cmp::Ge => b - a >= shift,
			Cmp::Le => b - a <= shift,
		}),
	})
}

/// The constant `k` of `x + k ∈ X` or `k ∈ X`.
fn position_offset(k: i64, offset: usize) -> Result<u32, Error> {
	u32::try_from(k).map_err(|_| error(offset, "the number does not fit 32 bits"))
}
