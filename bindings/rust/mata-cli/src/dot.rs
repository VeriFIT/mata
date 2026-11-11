// Copyright (c) 2026 VeriFIT. GNU GPL v3. See LICENSE in the project root.

use crate::Error;
use crate::eval::Value;
use crate::eval::{AutomatonKind, Engine};
use std::fmt::Write as _;

#[derive(Debug, Default)]
struct DotAutomaton {
	initial: Vec<u32>,
	final_states: Vec<u32>,
	states: u32,
	transitions: Vec<(u32, String, u32)>,
	/// The level of a state, read from the `N [label="N:level"]` lines a transducer's DOT carries.
	levels: std::collections::HashMap<u32, u32>,
}

/// Parses the DOT subset of `cli.md` and returns an `Automaton` value over the script alphabet.
pub(crate) fn parse(source: &str, engine: &mut Engine) -> Result<Value, Error> {
	let mut automaton = DotAutomaton::default();
	let mut marker_edges = Vec::new();
	for (line_number, raw_line) in source.lines().enumerate() {
		let line = raw_line.trim();
		if line.is_empty() || line.starts_with("//") || line.starts_with("/*") {
			continue;
		}
		if let Some(rest) = line.strip_prefix("digraph") {
			if !rest.contains('{') {
				return Err(Error::plain(format!(
					"line {}: expected 'digraph finiteAutomaton {{'",
					line_number + 1
				)));
			}
			continue;
		}
		if line == "}" {
			continue;
		}
		if line.starts_with("node [") {
			check_defaults(line, line_number + 1)?;
			continue;
		}
		if line.contains("->") {
			parse_edge(line, line_number + 1, &mut automaton, &mut marker_edges)?;
			continue;
		}
		if let Some(statement) = line.strip_suffix(';') {
			let mut parts = statement.splitn(2, '[');
			let name = parts.next().unwrap_or("").trim();
			let attributes = parts.next().unwrap_or("");
			if attributes.is_empty() && name.contains('=') {
				// Graph attributes carry no automaton structure; the writer emits `forcelabels` only so
				// that DOT draws the state labels that hold the levels.
				if name.replace(' ', "") == "forcelabels=true" {
					continue;
				}
				return Err(Error::plain(format!(
					"line {}: unsupported graph attribute '{name}'",
					line_number + 1
				)));
			}
			if attributes.contains("shape=none") {
				return Err(Error::plain(format!(
					"line {}: a marker node is used only through its edge",
					line_number + 1
				)));
			}
			let state = state_number(name, line_number + 1)?;
			automaton.states = automaton.states.max(state + 1);
			if attributes.contains("doublecircle") {
				automaton.final_states.push(state);
			}
			if let Some(level) = node_level(attributes, state, line_number + 1)? {
				automaton.levels.insert(state, level);
			}
			continue;
		}
		return Err(Error::plain(format!(
			"line {}: unsupported DOT construct '{}'",
			line_number + 1,
			line
		)));
	}
	for (from, to) in marker_edges {
		// The writer emits `iN -> N`: the prefix sits on the source, the state number on both ends.
		let Some(number) = from.strip_prefix('i') else {
			return Err(Error::plain(format!(
				"marker edge 'i{from} -> {to}' must start at a marker node"
			)));
		};
		let state: u32 = number.parse().map_err(|_| {
			Error::plain(format!("marker edge source '{from}' is not a state number"))
		})?;
		if to != number {
			return Err(Error::plain(format!(
				"marker edge 'i{from} -> {to}' must be of the form iN -> N"
			)));
		}
		automaton.initial.push(state);
		automaton.states = automaton.states.max(state + 1);
	}
	build(&automaton, engine)
}

/// Reads the `label="N:level"` a transducer's DOT puts on a state node; `None` when the node has no
/// label, which is every state of an NFA.
fn node_level(attributes: &str, state: u32, line_number: usize) -> Result<Option<u32>, Error> {
	if !attributes.contains("label=") {
		return Ok(None);
	}
	let start = attributes
		.find("label=\"")
		.map(|i| i + "label=\"".len())
		.ok_or_else(|| Error::plain(format!("line {line_number}: a node needs a quoted label")))?;
	let rest = &attributes[start..];
	let end = rest
		.find('"')
		.ok_or_else(|| Error::plain(format!("line {line_number}: unterminated node label")))?;
	let label = &rest[..end];
	let Some((name, level)) = label.split_once(':') else {
		return Err(Error::plain(format!(
			"line {line_number}: a node label must be 'N:level', found '{label}'"
		)));
	};
	let named: u32 = name.parse().map_err(|_| {
		Error::plain(format!(
			"line {line_number}: node label '{label}' must name the state by number"
		))
	})?;
	if named != state {
		return Err(Error::plain(format!(
			"line {line_number}: node label '{label}' does not name state {state}"
		)));
	}
	let level: u32 = level.parse().map_err(|_| {
		Error::plain(format!(
			"line {line_number}: node label '{label}' needs a level number"
		))
	})?;
	Ok(Some(level))
}

fn check_defaults(line: &str, line_number: usize) -> Result<(), Error> {
	if line.contains("doublecircle") || line.contains("label=") && !line.contains("label=\"\"") {
		return Err(Error::plain(format!(
			"line {line_number}: unsupported node default '{line}'"
		)));
	}
	Ok(())
}

fn parse_edge(
	line: &str,
	line_number: usize,
	automaton: &mut DotAutomaton,
	marker_edges: &mut Vec<(String, String)>,
) -> Result<(), Error> {
	let statement = line.strip_suffix(';').unwrap_or(line);
	let mut parts = statement.splitn(2, '[');
	let ends = parts.next().unwrap_or("").trim();
	let attributes = parts.next().unwrap_or("");
	let mut split = ends.split("->");
	let source = split.next().unwrap_or("").trim().to_owned();
	let target = split.next().unwrap_or("").trim().to_owned();
	if split.next().is_some() {
		return Err(Error::plain(format!(
			"line {line_number}: an edge has one target"
		)));
	}
	if attributes.contains("shape=none") || source.starts_with('i') && !is_number(&source) {
		marker_edges.push((source, target));
		return Ok(());
	}
	let source_state = state_number(&source, line_number)?;
	let target_state = state_number(&target, line_number)?;
	automaton.states = automaton.states.max(source_state + 1).max(target_state + 1);
	let label = if attributes.contains("label=") {
		let start = attributes
			.find("label=\"")
			.map(|i| i + "label=\"".len())
			.ok_or_else(|| {
				Error::plain(format!("line {line_number}: edge needs a quoted label"))
			})?;
		let rest = &attributes[start..];
		let end = rest
			.find('"')
			.ok_or_else(|| Error::plain(format!("line {line_number}: unterminated edge label")))?;
		rest[..end].to_owned()
	} else if attributes.trim().is_empty() || attributes.contains("tooltip=") {
		String::new()
	} else {
		return Err(Error::plain(format!(
			"line {line_number}: unsupported edge attributes '{attributes}'"
		)));
	};
	automaton
		.transitions
		.push((source_state, label, target_state));
	Ok(())
}

fn is_number(name: &str) -> bool {
	!name.is_empty() && name.chars().all(|c| c.is_ascii_digit())
}

fn state_number(name: &str, line_number: usize) -> Result<u32, Error> {
	if !is_number(name) {
		return Err(Error::plain(format!(
			"line {line_number}: state name '{name}' must be a number"
		)));
	}
	name.parse().map_err(|_| {
		Error::plain(format!(
			"line {line_number}: state name '{name}' is too large"
		))
	})
}

fn build(automaton: &DotAutomaton, engine: &mut Engine) -> Result<Value, Error> {
	if !automaton.levels.is_empty() {
		return build_nft(automaton, engine);
	}
	let mut nfa = mata::Nfa::with_states(automaton.states as usize);
	for state in &automaton.initial {
		nfa.add_initial(mata::State::new(*state))
			.map_err(|e| Error::plain(e.to_string()))?;
	}
	for state in &automaton.final_states {
		nfa.add_final(mata::State::new(*state))
			.map_err(|e| Error::plain(e.to_string()))?;
	}
	for (source, label, target) in &automaton.transitions {
		for symbol in expand_label(label, engine)? {
			nfa.add_transition(mata::State::new(*source), symbol, mata::State::new(*target))
				.map_err(|e| Error::plain(e.to_string()))?;
		}
	}
	Ok(Value::Automaton(std::rc::Rc::new(AutomatonKind::Nfa(
		std::rc::Rc::new(nfa),
	))))
}

/// Builds a transducer from a DOT that labels its states with levels. The `.mata` text is the only way
/// to give states a level: the binding adds level 0 only.
fn build_nft(automaton: &DotAutomaton, engine: &mut Engine) -> Result<Value, Error> {
	let levels = automaton.levels.values().copied().max().unwrap_or(0) + 1;
	let mut text = String::from("@NFT-explicit\n%Alphabet-auto\n");
	if !automaton.initial.is_empty() {
		let states = automaton
			.initial
			.iter()
			.map(u32::to_string)
			.collect::<Vec<_>>()
			.join(" ");
		writeln!(text, "%Initial {states}").expect("writing to a String cannot fail");
	}
	if !automaton.final_states.is_empty() {
		let states = automaton
			.final_states
			.iter()
			.map(u32::to_string)
			.collect::<Vec<_>>()
			.join(" ");
		writeln!(text, "%Final {states}").expect("writing to a String cannot fail");
	}
	let mut line = String::from("%Levels");
	for state in 0..automaton.states {
		write!(
			line,
			" {state}:{}",
			automaton.levels.get(&state).copied().unwrap_or(0)
		)
		.expect("writing to a String cannot fail");
	}
	text.push_str(&line);
	text.push('\n');
	writeln!(text, "%LevelsNum {levels}").expect("writing to a String cannot fail");
	for (source, label, target) in &automaton.transitions {
		for symbol in expand_label(label, engine)? {
			writeln!(text, "{source} {} {target}", named_symbol(symbol, engine))
				.expect("writing to a String cannot fail");
		}
	}
	engine.parse_text(&text, "nft")
}

/// The `.mata` spelling of a symbol: its script-alphabet name when that name survives a parse, and its
/// number otherwise.
fn named_symbol(symbol: mata::Symbol, engine: &Engine) -> String {
	if symbol == mata::Symbol::EPSILON {
		return "<eps>".to_owned();
	}
	if symbol == mata::nft::DONT_CARE {
		return "<dntcr>".to_owned();
	}
	match engine.script_alphabet.name(symbol) {
		Some(name) if is_plain_name(name) => name.to_owned(),
		_ => format!("<{}>", symbol.value()),
	}
}

/// Whether a name reads back as itself: no whitespace or comment character, no quote, and not one of the
/// prefixes that would make the parser read it as a heading or a quoted or numbered symbol.
fn is_plain_name(name: &str) -> bool {
	!name.is_empty()
		&& !name.contains(char::is_whitespace)
		&& !name.contains(['#', '"', '\\'])
		&& !name.starts_with(['%', '@'])
		&& (!name.starts_with('<') || !name.ends_with('>'))
}

fn expand_label(label: &str, engine: &mut Engine) -> Result<Vec<mata::Symbol>, Error> {
	if label.is_empty() {
		return Err(Error::plain(
			"an edge with an empty label is not allowed".into(),
		));
	}
	let mut symbols = Vec::new();
	if let Some(range) = label.strip_prefix('[').and_then(|l| l.strip_suffix(']')) {
		let bounds: Vec<&str> = range.split('-').collect();
		if bounds.len() != 2 || bounds[0].len() != 1 || bounds[1].len() != 1 {
			return Err(Error::plain(format!(
				"range label '{label}' must be [a-c] of single characters"
			)));
		}
		let (start, end) = (
			bounds[0].chars().next().unwrap(),
			bounds[1].chars().next().unwrap(),
		);
		if start > end {
			return Err(Error::plain(format!("range label '{label}' is backwards")));
		}
		for byte in (start as u8)..=(end as u8) {
			let name = (byte as char).to_string();
			let symbol = translate(&name, engine)?;
			symbols.push(symbol);
		}
		return Ok(symbols);
	}
	for name in label.split(',') {
		symbols.push(translate(name, engine)?);
	}
	Ok(symbols)
}

fn translate(name: &str, engine: &mut Engine) -> Result<mata::Symbol, Error> {
	match name {
		"<eps>" => Ok(mata::Symbol::EPSILON),
		"<dntcr>" => Ok(mata::nft::DONT_CARE),
		_ if name.starts_with('<')
			&& name.ends_with('>')
			&& name[1..name.len() - 1].chars().all(|c| c.is_ascii_digit()) =>
		{
			let number: u32 = name[1..name.len() - 1]
				.parse()
				.map_err(|_| Error::plain(format!("symbol '{name}' is not a number")))?;
			Ok(mata::Symbol::new(number))
		}
		_ if !name.is_empty() => engine
			.intern_name(name)
			.map_err(|e| Error::plain(format!("symbol '{name}': {e}"))),
		_ => Err(Error::plain("an empty symbol name is not allowed".into())),
	}
}
