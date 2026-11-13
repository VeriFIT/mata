//! A small SAT solver, shown on a hard sudoku.
//!
//! An assignment of n variables is a word of n symbols, `0` for false and `1` for true.
//! Each clause becomes a Mata automaton of the assignments that satisfy it,
//! so the models of the formula are the words of the intersection of these automata,
//! and lazymat searches that intersection without building it.

use std::error::Error;
use std::time::Instant;

use lazymat::{Formula, Options, Term};
use mata::{Nfa, NoSuchState, State, Symbol};

/// AI Escargot (Arto Inkala, 2006), with 0 for an empty cell.
const PUZZLE: [[u8; 9]; 9] = [
	[1, 0, 0, 0, 0, 7, 0, 9, 0],
	[0, 3, 0, 0, 2, 0, 0, 0, 8],
	[0, 0, 9, 6, 0, 0, 5, 0, 0],
	[0, 0, 5, 3, 0, 0, 9, 0, 0],
	[0, 1, 0, 0, 8, 0, 0, 0, 2],
	[6, 0, 0, 0, 0, 4, 0, 0, 0],
	[3, 0, 0, 0, 0, 0, 0, 1, 0],
	[0, 4, 0, 0, 0, 0, 0, 0, 7],
	[0, 0, 7, 0, 0, 0, 3, 0, 0],
];

const FALSE: Symbol = Symbol::new(0);
const TRUE: Symbol = Symbol::new(1);

/// A propositional variable, numbered from 0.
#[derive(Clone, Copy, Debug)]
struct Variable(usize);

/// A variable or its negation.
#[derive(Clone, Copy, Debug)]
struct Literal {
	variable: Variable,
	positive: bool,
}

/// A disjunction of literals.
type Clause = Vec<Literal>;

/// A formula in conjunctive normal form.
#[derive(Debug)]
struct Cnf {
	num_variables: usize,
	clauses: Vec<Clause>,
}

/// The position of each variable in the word.
///
/// The variables of unit clauses go first, so that a clause over them fails as soon as its other
/// variables are read.
fn positions(cnf: &Cnf) -> Vec<usize> {
	let mut in_unit = vec![false; cnf.num_variables];
	for clause in &cnf.clauses {
		if let [literal] = clause.as_slice() {
			in_unit[literal.variable.0] = true;
		}
	}

	let mut order: Vec<usize> = (0..cnf.num_variables).collect();
	order.sort_by_key(|&variable| !in_unit[variable]);

	let mut position = vec![0; cnf.num_variables];
	for (index, &variable) in order.iter().enumerate() {
		position[variable] = index;
	}

	position
}

/// The words of `position.len()` symbols that satisfy `clause`.
fn clause_automaton(clause: &[Literal], position: &[usize]) -> Result<Nfa, NoSuchState> {
	let length = position.len();
	let mut nfa = Nfa::new();
	// `open[i]` has read `i` symbols and no true literal, `done[i]` has read `i` symbols and a true literal.
	let open: Vec<State> = (0..=length).map(|_| nfa.add_state()).collect();
	let done: Vec<State> = (0..=length).map(|_| nfa.add_state()).collect();

	nfa.add_initial(open[0])?;
	nfa.add_final(done[length])?;
	for i in 0..length {
		for (symbol, value) in [(FALSE, false), (TRUE, true)] {
			let satisfies = clause
				.iter()
				.any(|literal| position[literal.variable.0] == i && literal.positive == value);
			let next = if satisfies { done[i + 1] } else { open[i + 1] };
			nfa.add_transition(open[i], symbol, next)?;
			nfa.add_transition(done[i], symbol, done[i + 1])?;
		}
	}

	Ok(nfa)
}

/// The intersection of `terms`, as a balanced tree.
fn intersection(formula: &mut Formula, terms: &[Term]) -> Result<Term, lazymat::Error> {
	if let [term] = terms {
		return Ok(*term);
	}

	let (left, right) = terms.split_at(terms.len() / 2);
	let left = intersection(formula, left)?;
	let right = intersection(formula, right)?;

	formula.intersection(left, right)
}

/// A model of `cnf`, or `None` if it is unsatisfiable.
fn solve(cnf: &Cnf) -> Result<Option<Vec<bool>>, Box<dyn Error>> {
	if cnf.clauses.is_empty() {
		return Ok(Some(vec![false; cnf.num_variables]));
	}

	let position = positions(cnf);
	let mut formula = Formula::new();
	let mut terms = Vec::new();
	for clause in &cnf.clauses {
		terms.push(formula.leaf(&clause_automaton(clause, &position)?)?);
	}

	let root = intersection(&mut formula, &terms)?;
	// Without complements, subsumption costs more than it saves.
	let options = Options::new().subsumption(false);

	let start = Instant::now();
	let outcome = formula.check(root, &options)?;
	println!(
		"{} variables, {} clauses: {} macrostates explored in {:.2?}",
		cnf.num_variables,
		cnf.clauses.len(),
		outcome.stats.explored,
		start.elapsed()
	);

	let Some(witness) = outcome.witness else {
		return Ok(None);
	};

	let word = witness.tape(0).expect("the formula has one tape");
	let model = position
		.iter()
		.map(|&index| word[index] == TRUE.into())
		.collect();

	Ok(Some(model))
}

/// The variable "cell (`row`, `column`) holds `digit`".
fn holds(row: usize, column: usize, digit: u8) -> Variable {
	Variable(81 * row + 9 * column + usize::from(digit) - 1)
}

/// Whether two cells share a row, a column or a box.
fn sees((r1, c1): (usize, usize), (r2, c2): (usize, usize)) -> bool {
	r1 == r2 || c1 == c2 || (r1 / 3 == r2 / 3 && c1 / 3 == c2 / 3)
}

/// The rules of sudoku and the given digits.
fn sudoku(puzzle: &[[u8; 9]; 9]) -> Cnf {
	let cells: Vec<(usize, usize)> = (0..9)
		.flat_map(|row| (0..9).map(move |column| (row, column)))
		.collect();
	let mut cnf = Cnf {
		num_variables: 9 * 9 * 9,
		clauses: Vec::new(),
	};
	let yes = |variable| Literal {
		variable,
		positive: true,
	};
	let no = |variable| Literal {
		variable,
		positive: false,
	};

	// Every cell holds a digit, and only one.
	for &(row, column) in &cells {
		cnf.clauses.push(
			(1..=9)
				.map(|digit| yes(holds(row, column, digit)))
				.collect(),
		);
		for first in 1..=9 {
			for second in first + 1..=9 {
				cnf.clauses.push(vec![
					no(holds(row, column, first)),
					no(holds(row, column, second)),
				]);
			}
		}
	}

	// Cells that see each other hold different digits.
	for (index, &(r1, c1)) in cells.iter().enumerate() {
		for &(r2, c2) in &cells[index + 1..] {
			if sees((r1, c1), (r2, c2)) {
				for digit in 1..=9 {
					cnf.clauses
						.push(vec![no(holds(r1, c1, digit)), no(holds(r2, c2, digit))]);
				}
			}
		}
	}

	// The given digits.
	for &(row, column) in &cells {
		let digit = puzzle[row][column];
		if digit != 0 {
			cnf.clauses.push(vec![yes(holds(row, column, digit))]);
		}
	}

	cnf
}

/// The digits of a model.
fn board(model: &[bool]) -> [[u8; 9]; 9] {
	let mut board = [[0; 9]; 9];

	for (row, line) in board.iter_mut().enumerate() {
		for (column, cell) in line.iter_mut().enumerate() {
			*cell = (1..=9)
				.find(|&digit| model[holds(row, column, digit).0])
				.unwrap_or(0);
		}
	}

	board
}

/// Prints a board, with `.` for an empty cell.
fn print_board(board: &[[u8; 9]; 9]) {
	for (row, line) in board.iter().enumerate() {
		if row > 0 && row % 3 == 0 {
			println!("------+-------+------");
		}

		let cells: Vec<String> = line
			.iter()
			.map(|&digit| match digit {
				0 => ".".to_string(),
				digit => digit.to_string(),
			})
			.collect();
		let boxes: Vec<String> = cells.chunks(3).map(|group| group.join(" ")).collect();
		println!("{}", boxes.join(" | "));
	}
}

fn main() -> Result<(), Box<dyn Error>> {
	println!("Puzzle:");
	print_board(&PUZZLE);
	println!();

	let cnf = sudoku(&PUZZLE);

	match solve(&cnf)? {
		Some(model) => {
			println!();
			println!("Solution:");
			print_board(&board(&model));
		}
		None => println!("No solution."),
	}

	Ok(())
}
