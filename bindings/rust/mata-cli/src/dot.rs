// Copyright (c) 2026 VeriFIT. GNU GPL v3. See LICENSE in the project root.

use crate::Error;
use crate::eval::Value;
use crate::eval::{AutomatonKind, Engine};
use std::collections::{BTreeSet, HashMap};
use std::fmt::Write as _;

#[derive(Debug, Default)]
struct DotAutomaton {
	initial: Vec<u32>,
	final_states: Vec<u32>,
	/// Every state a statement names; they must be `0` to the highest without a gap.
	states: BTreeSet<u32>,
	/// `None` for an edge without a label.
	transitions: Vec<(u32, Option<String>, u32)>,
	/// The level of a state, read from the `N [label="N:level"]` lines a transducer's DOT carries.
	levels: HashMap<u32, u32>,
	/// `num_of_levels=N`, which `write_nft` adds because the library's DOT carries no level count.
	num_of_levels: Option<u32>,
	/// Whether the graph is a transducer's: it has `forcelabels`, `num_of_levels` or a state level.
	transducer: bool,
}

/// Where a line is relative to the one graph of an input.
enum Place {
	Before,
	Inside,
	After,
}

/// Parses the DOT subset of `cli.md` and returns an `Automaton` value over the script alphabet.
pub(crate) fn parse(source: &str, engine: &mut Engine) -> Result<Value, Error> {
	let mut automaton = DotAutomaton::default();
	let mut marker_edges = Vec::new();
	let mut place = Place::Before;
	for (line_number, raw_line) in source.lines().enumerate() {
		let line_number = line_number + 1;
		let line = raw_line.trim();
		if line.is_empty() || line.starts_with("//") || line.starts_with("/*") {
			continue;
		}
		match place {
			Place::Before => {
				if line
					.strip_prefix("digraph")
					.is_some_and(|rest| rest.ends_with('{'))
				{
					place = Place::Inside;
					continue;
				}
				return Err(Error::plain(format!(
					"line {line_number}: expected 'digraph finiteAutomaton {{'"
				)));
			}
			// Each graph is one automaton, so a second one is not merged into the first.
			Place::After => {
				return Err(Error::plain(format!(
					"line {line_number}: '{line}' follows the end of the graph; an input holds one graph"
				)));
			}
			Place::Inside => {}
		}
		if line == "}" {
			place = Place::After;
			continue;
		}
		let statement = line.strip_suffix(';').unwrap_or(line);
		let (head, attributes) = match statement.split_once('[') {
			Some((head, list)) => (head.trim(), Some(attribute_list(list, line_number)?)),
			None => (statement.trim(), None),
		};
		if head == "node" {
			check_defaults(attributes.as_deref().unwrap_or_default(), line_number)?;
		} else if head.contains("->") {
			parse_edge(
				head,
				attributes,
				line_number,
				&mut automaton,
				&mut marker_edges,
			)?;
		} else if attributes.is_none() && head.contains('=') {
			graph_attribute(head, line_number, &mut automaton)?;
		} else if head.starts_with("digraph") {
			return Err(Error::plain(format!(
				"line {line_number}: a graph inside a graph is not supported"
			)));
		} else {
			parse_node(
				head,
				&attributes.unwrap_or_default(),
				line_number,
				&mut automaton,
			)?;
		}
	}
	match place {
		Place::Before => return Err(Error::plain("the input holds no digraph".into())),
		Place::Inside => return Err(Error::plain("the graph is not closed with '}'".into())),
		Place::After => {}
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
		automaton.states.insert(state);
	}
	// The writer draws every state, so a gap is a malformed graph; checking it also keeps one huge state
	// number from allocating that many states.
	if let Some(missing) = (0..)
		.zip(&automaton.states)
		.find_map(|(expected, &state)| (expected != state).then_some(expected))
	{
		return Err(Error::plain(format!(
			"state {missing} is not in the graph, but state {} is; states are numbered from 0 without a gap",
			automaton.states.last().copied().unwrap_or_default()
		)));
	}
	build(&automaton, engine)
}

/// The `key=value` pairs of an attribute list, given the text after its `[`. A quoted value is the text
/// between its quotes, escapes kept.
fn attribute_list(list: &str, line_number: usize) -> Result<Vec<(&str, &str)>, Error> {
	let malformed = || {
		Error::plain(format!(
			"line {line_number}: malformed attribute list '[{list}'"
		))
	};
	let mut rest = list
		.trim_end()
		.strip_suffix(']')
		.ok_or_else(malformed)?
		.trim();
	let mut attributes = Vec::new();
	while !rest.is_empty() {
		let (key, after) = rest.split_once('=').ok_or_else(malformed)?;
		let key = key.trim();
		let after = after.trim_start();
		let (value, after) = if let Some(quoted) = after.strip_prefix('"') {
			let end = closing_quote(quoted).ok_or_else(|| {
				Error::plain(format!("line {line_number}: unterminated quoted value"))
			})?;
			(&quoted[..end], &quoted[end + 1..])
		} else {
			let end = after
				.find(|c: char| !(c.is_ascii_alphanumeric() || matches!(c, '_' | '.' | '-')))
				.unwrap_or(after.len());
			(&after[..end], &after[end..])
		};
		if key.is_empty() || !key.chars().all(|c| c.is_ascii_alphanumeric() || c == '_') {
			return Err(malformed());
		}
		attributes.push((key, value));
		let after = after.trim_start();
		rest = match after.strip_prefix([',', ';']) {
			Some(next) => next.trim_start(),
			// Without a separator, only the end of the list or another `key=` may follow.
			None if after.is_empty() || after.starts_with(|c: char| c.is_ascii_alphabetic()) => {
				after
			}
			None => return Err(malformed()),
		};
	}
	Ok(attributes)
}

/// The index of the quote that ends a quoted DOT string, skipping `\"`.
fn closing_quote(text: &str) -> Option<usize> {
	let mut escaped = false;
	for (index, c) in text.char_indices() {
		match c {
			_ if escaped => escaped = false,
			'\\' => escaped = true,
			'"' => return Some(index),
			_ => {}
		}
	}
	None
}

/// `forcelabels=true`, which the transducer writer emits so that DOT draws the state labels, and
/// `num_of_levels=N`; both mark a transducer.
fn graph_attribute(
	statement: &str,
	line_number: usize,
	automaton: &mut DotAutomaton,
) -> Result<(), Error> {
	let (key, value) = statement.split_once('=').unwrap_or((statement, ""));
	match (key.trim(), value.trim()) {
		("forcelabels", "true") => {}
		("num_of_levels", count) => {
			let count = count
				.parse()
				.ok()
				.filter(|&count: &u32| count > 0)
				.ok_or_else(|| {
					Error::plain(format!(
						"line {line_number}: num_of_levels needs a positive number, found '{count}'"
					))
				})?;
			automaton.num_of_levels = Some(count);
		}
		_ => {
			return Err(Error::plain(format!(
				"line {line_number}: unsupported graph attribute '{statement}'"
			)));
		}
	}
	automaton.transducer = true;
	Ok(())
}

fn parse_node(
	name: &str,
	attributes: &[(&str, &str)],
	line_number: usize,
	automaton: &mut DotAutomaton,
) -> Result<(), Error> {
	let state = state_number(name, line_number)?;
	automaton.states.insert(state);
	for &(key, value) in attributes {
		match (key, value) {
			// The last shape of a state wins, as in Graphviz.
			("shape", "doublecircle") => {
				if !automaton.final_states.contains(&state) {
					automaton.final_states.push(state);
				}
			}
			("shape", "circle") => automaton.final_states.retain(|&other| other != state),
			("shape", "none") => {
				return Err(Error::plain(format!(
					"line {line_number}: a marker node is used only through its edge"
				)));
			}
			("label", label) => {
				automaton
					.levels
					.insert(state, node_level(label, state, line_number)?);
				automaton.transducer = true;
			}
			_ => {
				return Err(Error::plain(format!(
					"line {line_number}: unsupported node attribute '{key}={value}'"
				)));
			}
		}
	}
	Ok(())
}

/// Reads the `label="N:level"` a transducer's DOT puts on a state node.
fn node_level(label: &str, state: u32, line_number: usize) -> Result<u32, Error> {
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
	level.parse().map_err(|_| {
		Error::plain(format!(
			"line {line_number}: node label '{label}' needs a level number"
		))
	})
}

fn check_defaults(attributes: &[(&str, &str)], line_number: usize) -> Result<(), Error> {
	for &(key, value) in attributes {
		if !matches!((key, value), ("shape", "circle" | "none") | ("label", "")) {
			return Err(Error::plain(format!(
				"line {line_number}: unsupported node default '{key}={value}'"
			)));
		}
	}
	Ok(())
}

fn parse_edge(
	ends: &str,
	attributes: Option<Vec<(&str, &str)>>,
	line_number: usize,
	automaton: &mut DotAutomaton,
	marker_edges: &mut Vec<(String, String)>,
) -> Result<(), Error> {
	let mut split = ends.split("->");
	let source = split.next().unwrap_or("").trim().to_owned();
	let target = split.next().unwrap_or("").trim().to_owned();
	if split.next().is_some() {
		return Err(Error::plain(format!(
			"line {line_number}: an edge has one target"
		)));
	}
	if source.starts_with('i') && !is_number(&source) {
		if attributes.is_some() {
			return Err(Error::plain(format!(
				"line {line_number}: a marker edge has no attributes"
			)));
		}
		marker_edges.push((source, target));
		return Ok(());
	}
	let source_state = state_number(&source, line_number)?;
	let target_state = state_number(&target, line_number)?;
	automaton.states.insert(source_state);
	automaton.states.insert(target_state);
	let mut label = None;
	let mut tooltip = None;
	for (key, value) in attributes.unwrap_or_default() {
		match key {
			"label" => label = Some(value.to_owned()),
			// The library writes a shortened label with the whole one as the tooltip, its `<` and `>`
			// escaped.
			"tooltip" => tooltip = Some(value.replace("&lt;", "<").replace("&gt;", ">")),
			_ => {
				return Err(Error::plain(format!(
					"line {line_number}: unsupported edge attribute '{key}={value}'"
				)));
			}
		}
	}
	let label = tooltip.or(label);
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
	let labels = transition_symbols(automaton, engine)?;
	if automaton.transducer {
		return build_nft(automaton, &labels, engine);
	}
	let mut nfa = mata::Nfa::with_states(automaton.states.len());
	for state in &automaton.initial {
		nfa.add_initial(mata::State::new(*state))
			.map_err(|e| Error::plain(e.to_string()))?;
	}
	for state in &automaton.final_states {
		nfa.add_final(mata::State::new(*state))
			.map_err(|e| Error::plain(e.to_string()))?;
	}
	for ((source, _, target), symbols) in automaton.transitions.iter().zip(labels) {
		for symbol in symbols {
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
fn build_nft(
	automaton: &DotAutomaton,
	labels: &[Vec<mata::Symbol>],
	engine: &mut Engine,
) -> Result<Value, Error> {
	let highest = automaton.levels.values().copied().max();
	// The library's DOT carries no level count, so without `num_of_levels` it is the highest level plus one.
	let levels = match automaton.num_of_levels {
		Some(count) => {
			if let Some(level) = highest.filter(|&level| level >= count) {
				return Err(Error::plain(format!(
					"state level {level} is not below num_of_levels={count}"
				)));
			}
			u64::from(count)
		}
		None => highest.map_or(1, |level| u64::from(level) + 1),
	};
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
	for &state in &automaton.states {
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
	for ((source, _, target), symbols) in automaton.transitions.iter().zip(labels) {
		for &symbol in symbols {
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

/// One entry of an edge label.
enum Item {
	/// `<eps>`, `<dntcr>` or a name the script alphabet already has.
	Known(mata::Symbol),
	/// `<n>`, or a number the script alphabet has no name for, as the library writes an unnamed symbol.
	Number(mata::Symbol),
	Name(String),
}

/// The symbols of every transition label. Numbered symbols are added to the script alphabet before any
/// new name takes a number, so a name never gets the number of a symbol a label lists.
fn transition_symbols(
	automaton: &DotAutomaton,
	engine: &mut Engine,
) -> Result<Vec<Vec<mata::Symbol>>, Error> {
	let labels = automaton
		.transitions
		.iter()
		.map(|(_, label, _)| label_items(label.as_deref(), engine))
		.collect::<Result<Vec<_>, _>>()?;
	for item in labels.iter().flatten() {
		if let Item::Number(symbol) = item
			&& !symbol.is_epsilon()
		{
			engine
				.script_alphabet
				.insert(*symbol)
				.map_err(|e| Error::plain(e.to_string()))?;
		}
	}
	labels
		.into_iter()
		.map(|items| {
			items
				.into_iter()
				.map(|item| match item {
					Item::Known(symbol) | Item::Number(symbol) => Ok(symbol),
					Item::Name(name) => engine
						.intern_name(&name)
						.map_err(|e| Error::plain(format!("symbol '{name}': {e}"))),
				})
				.collect()
		})
		.collect()
}

/// The entries of a label: an edge without one is the symbol 0, a range `[a-c]` names every character
/// from `a` to `c`, and otherwise the label lists entries separated by commas. `\\`, `\"`, `\,`, `\[` and
/// `\<` are the escaped character, which then has no meaning in the label's structure.
fn label_items(label: Option<&str>, engine: &Engine) -> Result<Vec<Item>, Error> {
	let Some(label) = label else {
		return Ok(vec![Item::Number(mata::Symbol::new(0))]);
	};
	if label.is_empty() {
		return Err(Error::plain(
			"an edge with an empty label is not allowed".into(),
		));
	}
	if let Some(range) = label.strip_prefix('[').and_then(|l| l.strip_suffix(']')) {
		let bounds: Vec<&str> = range.split('-').collect();
		if bounds.len() != 2 || bounds[0].len() != 1 || bounds[1].len() != 1 {
			return Err(Error::plain(format!(
				"range label '{label}' must be [a-c] of single characters"
			)));
		}
		let (start, end) = (bounds[0].as_bytes()[0], bounds[1].as_bytes()[0]);
		if start > end {
			return Err(Error::plain(format!("range label '{label}' is backwards")));
		}
		return (start..=end)
			.map(|byte| label_item(&char::from(byte).to_string(), false, engine))
			.collect();
	}
	let mut items = Vec::new();
	let mut name = String::new();
	let mut escaped = false;
	let mut chars = label.chars();
	while let Some(c) = chars.next() {
		match c {
			'\\' => match chars.next() {
				Some(literal @ ('\\' | '"' | ',' | '[' | '<')) => {
					name.push(literal);
					escaped = true;
				}
				other => {
					return Err(Error::plain(format!(
						"label '{label}' has the unsupported escape '\\{}'",
						other.map(String::from).unwrap_or_default()
					)));
				}
			},
			',' => {
				items.push(label_item(&name, escaped, engine)?);
				name.clear();
				escaped = false;
			}
			_ => name.push(c),
		}
	}
	items.push(label_item(&name, escaped, engine)?);
	Ok(items)
}

/// One label entry; an `escaped` one is always a name.
fn label_item(name: &str, escaped: bool, engine: &Engine) -> Result<Item, Error> {
	if name.is_empty() {
		return Err(Error::plain("an empty symbol name is not allowed".into()));
	}
	let explicit = name
		.strip_prefix('<')
		.and_then(|n| n.strip_suffix('>'))
		.filter(|number| !escaped && is_number(number));
	// A name the alphabet knows wins over bare digits, as in the `.mata` parser; `<n>` is always a
	// number, since `\<` spells a name that starts with `<`.
	if explicit.is_none()
		&& let Some(symbol) = engine.script_alphabet.symbol(name)
	{
		return Ok(Item::Known(symbol));
	}
	let number = match explicit {
		Some(number) => number,
		None if escaped => return Ok(Item::Name(name.to_owned())),
		None if name == "<dntcr>" => return Ok(Item::Known(mata::nft::DONT_CARE)),
		None if is_number(name) => name,
		None => return Ok(Item::Name(name.to_owned())),
	};
	let number = number
		.parse()
		.map_err(|_| Error::plain(format!("symbol '{name}' is too large")))?;
	Ok(Item::Number(mata::Symbol::new(number)))
}

/// The DOT of an NFA, with labels `parse` reads back as the same symbols.
pub(crate) fn write_nfa(nfa: &mata::Nfa, engine: &Engine) -> String {
	nfa.dot().alphabet(&label_alphabet(engine)).to_string()
}

/// The DOT of a transducer, with labels `parse` reads back as the same symbols and a `num_of_levels`
/// graph attribute: the library writes no level count, and the levels of the states do not always show
/// it.
pub(crate) fn write_nft(nft: &mata::nft::Nft, engine: &Engine) -> String {
	let text = nft.dot().alphabet(&label_alphabet(engine)).to_string();
	let (header, body) = text.split_once('\n').unwrap_or((&text, ""));
	format!("{header}\nnum_of_levels={};\n{body}", nft.num_levels())
}

/// The script alphabet with every name spelled as a DOT label entry that reads back as its symbol.
fn label_alphabet(engine: &Engine) -> mata::Alphabet {
	let mut alphabet = mata::Alphabet::new();
	for (symbol, name) in &engine.script_alphabet {
		// An unnamed symbol is written as its number, which reads back as that symbol.
		if let Some(name) = name {
			alphabet
				.insert_named(&label_name(name, symbol), symbol)
				.expect("a named symbol of the script alphabet is an ordinary symbol");
		}
	}
	alphabet
}

/// The spelling of a name in a DOT label: the characters that structure a label are escaped, and a name
/// that would read as a number or would break the line is written as its symbol number `<n>`.
fn label_name(name: &str, symbol: mata::Symbol) -> String {
	if name.is_empty() || is_number(name) || name.contains(char::is_control) {
		return format!("<{}>", symbol.value());
	}
	let mut spelled = String::with_capacity(name.len());
	for (index, c) in name.char_indices() {
		if matches!(c, '\\' | '"' | ',') || index == 0 && matches!(c, '[' | '<') {
			spelled.push('\\');
		}
		spelled.push(c);
	}
	spelled
}
