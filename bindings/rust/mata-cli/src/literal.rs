// Copyright (c) 2026 VeriFIT. GNU GPL v3. See LICENSE in the project root.

use std::collections::HashMap;
use std::fmt::Write as _;
use std::rc::Rc;

use crate::ast::{Expr, Kind};
use crate::eval::Engine;

/// The state names of a literal, as the literal lists them.
struct Parts {
	initial: Vec<String>,
	final_states: Vec<String>,
	transitions: Vec<(String, String, String)>,
}

/// Builds an automaton from `:initial`, `:final`, `:transitions` and, for transducers, `:levels` and
/// `:states`. Names are interned through the script alphabet, as the design specifies.
pub(crate) fn build(
	name: &str,
	arguments: &[(String, Expr)],
	engine: &mut Engine,
) -> Result<crate::eval::Value, String> {
	let allowed: &[&str] = match name {
		"nfa:literal" => &["initial", "final", "transitions"],
		"nft:literal" => &["levels", "initial", "final", "states", "transitions"],
		_ => return Err(format!("unknown literal '{name}'")),
	};
	let mut fields: HashMap<&str, &Expr> = HashMap::new();
	for (keyword, value) in arguments {
		if !allowed.contains(&keyword.as_str()) {
			return Err(format!("'{name}' takes no keyword ':{keyword}'"));
		}
		if fields.insert(keyword.as_str(), value).is_some() {
			return Err(format!("duplicate field ':{keyword}'"));
		}
	}
	match name {
		"nfa:literal" => {
			let parts = Parts {
				initial: names(
					field(&fields, "initial").ok_or("nfa:literal needs :initial")?,
					"initial",
				)?,
				final_states: names(
					field(&fields, "final").ok_or("nfa:literal needs :final")?,
					"final",
				)?,
				transitions: states_of(&fields)?,
			};
			build_nfa(&parts, engine)
		}
		"nft:literal" => {
			let Some(levels) = field(&fields, "levels") else {
				return Err("nft:literal needs :levels".into());
			};
			let levels = integer(levels)?;
			if levels < 1 {
				return Err(":levels must be at least 1".into());
			}
			let levels = usize::try_from(levels).map_err(|_| ":levels is too large".to_owned())?;
			let parts = Parts {
				initial: names(
					field(&fields, "initial").ok_or("nft:literal needs :initial")?,
					"initial",
				)?,
				final_states: names(
					field(&fields, "final").ok_or("nft:literal needs :final")?,
					"final",
				)?,
				transitions: states_of(&fields)?,
			};
			let state_levels = match field(&fields, "states") {
				Some(value) => state_levels(value)?,
				None => Vec::new(),
			};
			build_nft(levels, &parts, &state_levels, engine)
		}
		_ => Err(format!("unknown literal '{name}'")),
	}
}

fn field<'a>(fields: &HashMap<&'a str, &'a Expr>, name: &str) -> Option<&'a Expr> {
	fields.get(name).copied()
}

/// The `:transitions` field, which a literal may leave out.
fn states_of(fields: &HashMap<&str, &Expr>) -> Result<Vec<(String, String, String)>, String> {
	match field(fields, "transitions") {
		Some(value) => transitions(value),
		None => Ok(Vec::new()),
	}
}

fn names(value: &Expr, what: &str) -> Result<Vec<String>, String> {
	match &value.node {
		Kind::List(items) => items
			.iter()
			.map(|item| match &item.node {
				Kind::Name(name) => Ok(name.clone()),
				_ => Err(format!(":{what} must be a list of names")),
			})
			.collect(),
		_ => Err(format!(":{what} must be a list of names")),
	}
}

fn integer(value: &Expr) -> Result<i64, String> {
	match &value.node {
		Kind::Int(value) => Ok(*value),
		_ => Err("expected an integer".into()),
	}
}

fn state_levels(value: &Expr) -> Result<Vec<(String, i64)>, String> {
	match &value.node {
		Kind::List(items) => items
			.iter()
			.map(|item| match &item.node {
				Kind::List(pair) if pair.len() == 2 => {
					let Kind::Name(name) = &pair[0].node else {
						return Err(":states must be (name level) pairs".into());
					};
					let Kind::Int(level) = &pair[1].node else {
						return Err(":states must be (name level) pairs".into());
					};
					Ok((name.clone(), *level))
				}
				_ => Err(":states must be (name level) pairs".into()),
			})
			.collect(),
		_ => Err(":states must be (name level) pairs".into()),
	}
}

fn transitions(value: &Expr) -> Result<Vec<(String, String, String)>, String> {
	match &value.node {
		Kind::List(items) => items
			.iter()
			.map(|item| match &item.node {
				Kind::List(parts) if parts.len() == 3 => {
					let [source, symbol, target] = [(&parts[0]), (&parts[1]), (&parts[2])];
					let Kind::Name(source) = &source.node else {
						return Err(":transitions must be (state symbol state) triples".into());
					};
					let symbol = match &symbol.node {
						Kind::Name(symbol) => symbol.clone(),
						_ => return Err(":transitions must be (state symbol state) triples".into()),
					};
					let Kind::Name(target) = &target.node else {
						return Err(":transitions must be (state symbol state) triples".into());
					};
					Ok((source.clone(), symbol, target.clone()))
				}
				_ => Err(":transitions must be (state symbol state) triples".into()),
			})
			.collect(),
		_ => Err(":transitions must be (state symbol state) triples".into()),
	}
}

/// Numbers the states in the order the literal names them.
fn numbers(parts: &Parts) -> HashMap<String, usize> {
	let mut numbers: HashMap<String, usize> = HashMap::new();
	for name in parts
		.initial
		.iter()
		.chain(&parts.final_states)
		.chain(parts.transitions.iter().flat_map(|(s, _, t)| [s, t]))
	{
		let next = numbers.len();
		numbers.entry(name.clone()).or_insert(next);
	}
	numbers
}

/// The state with this number, or an error when the literal names more states than `State` holds.
fn state(number: usize) -> Result<mata::State, String> {
	let number = u32::try_from(number).map_err(|_| "the literal has too many states".to_owned())?;
	Ok(mata::State::new(number))
}

/// The symbol of a transition: `epsilon` is the ε-transition, every other name goes through the
/// script alphabet.
fn transition_symbol(name: &str, engine: &mut Engine) -> Result<mata::Symbol, String> {
	if name == "epsilon" {
		Ok(mata::Symbol::EPSILON)
	} else {
		engine.intern_name(name)
	}
}

fn build_nfa(parts: &Parts, engine: &mut Engine) -> Result<crate::eval::Value, String> {
	let state_numbers = numbers(parts);
	let mut nfa = mata::Nfa::with_states(state_numbers.len());
	for name in &parts.initial {
		nfa.add_initial(state(state_numbers[name])?)
			.map_err(|e| e.to_string())?;
	}
	for name in &parts.final_states {
		nfa.add_final(state(state_numbers[name])?)
			.map_err(|e| e.to_string())?;
	}
	for (source, symbol, target) in &parts.transitions {
		let symbol = transition_symbol(symbol, engine)?;
		nfa.add_transition(
			state(state_numbers[source])?,
			symbol,
			state(state_numbers[target])?,
		)
		.map_err(|e| e.to_string())?;
	}
	Ok(crate::eval::Value::Nfa(Rc::new(nfa)))
}

fn build_nft(
	levels: usize,
	parts: &Parts,
	state_levels: &[(String, i64)],
	engine: &mut Engine,
) -> Result<crate::eval::Value, String> {
	// The Rust binding only adds level-0 states, so a literal whose states carry levels is rendered
	// as `.mata` text and parsed back, which keeps the state names and levels of the design.
	if state_levels.is_empty()
		&& parts
			.transitions
			.iter()
			.all(|(_, symbol, _)| symbol != "epsilon")
	{
		return build_nft_simple(levels, parts, engine);
	}
	let mut text = String::from("@NFT-explicit\n%Alphabet-auto\n");
	// `%Levels` holds `state:level` pairs; a state the literal does not give a level keeps level 0.
	let mut names: Vec<&str> = Vec::new();
	for name in parts
		.initial
		.iter()
		.chain(&parts.final_states)
		.chain(parts.transitions.iter().flat_map(|(s, _, t)| [s, t]))
		.chain(state_levels.iter().map(|(name, _)| name))
	{
		if !names.contains(&name.as_str()) {
			names.push(name.as_str());
		}
	}
	if !names.is_empty() {
		let given: HashMap<&str, i64> = state_levels
			.iter()
			.map(|(name, level)| (name.as_str(), *level))
			.collect();
		let mut line = String::from("%Levels");
		for name in &names {
			write!(line, " {name}:{}", given.get(name).copied().unwrap_or(0))
				.expect("writing to a String cannot fail");
		}
		text.push_str(&line);
		text.push('\n');
	}
	writeln!(text, "%LevelsNum {levels}").expect("writing to a String cannot fail");
	if !parts.initial.is_empty() {
		writeln!(text, "%Initial {}", parts.initial.join(" "))
			.expect("writing to a String cannot fail");
	}
	if !parts.final_states.is_empty() {
		writeln!(text, "%Final {}", parts.final_states.join(" "))
			.expect("writing to a String cannot fail");
	}
	for (source, symbol, target) in &parts.transitions {
		let symbol = if symbol == "epsilon" {
			"<eps>"
		} else {
			symbol.as_str()
		};
		writeln!(text, "{source} {symbol} {target}").expect("writing to a String cannot fail");
	}
	engine.parse_text(&text, "nft").map_err(|e| e.to_string())
}

fn build_nft_simple(
	levels: usize,
	parts: &Parts,
	engine: &mut Engine,
) -> Result<crate::eval::Value, String> {
	let state_numbers = numbers(parts);
	let mut nft = mata::nft::Nft::new(levels).map_err(|e| e.to_string())?;
	for _ in 0..state_numbers.len() {
		nft.add_state();
	}
	for name in &parts.initial {
		nft.add_initial(state(state_numbers[name])?)
			.map_err(|e| e.to_string())?;
	}
	for name in &parts.final_states {
		nft.add_final(state(state_numbers[name])?)
			.map_err(|e| e.to_string())?;
	}
	for (source, symbol, target) in &parts.transitions {
		let symbols = vec![transition_symbol(symbol, engine)?; levels];
		nft.add_transition(
			state(state_numbers[source])?,
			&symbols,
			state(state_numbers[target])?,
		)
		.map_err(|e| e.to_string())?;
	}
	Ok(crate::eval::Value::Nft(Rc::new(nft)))
}
