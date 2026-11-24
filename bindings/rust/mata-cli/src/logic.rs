// Copyright (c) 2026 VeriFIT. GNU GPL v3. See LICENSE in the project root.

//! Decision procedures for formulas written in a logic front-end.

use mata::logic::ws1s::Ws1s;

use crate::Error;
use crate::eval::{Engine, Value, as_string};
use crate::ops::take;

/// Parses a WS1S formula and decides it with the same query options as a lazy term.
pub(crate) fn dispatch(
	handler: &str,
	name: &str,
	arguments: Vec<Value>,
	engine: &mut Engine,
) -> Result<Value, Error> {
	let [argument] = take(arguments, name)?;
	let text = as_string(&argument).map_err(Error::plain)?;
	let options = crate::lazy::options(engine)?;
	let mut ws1s = Ws1s::new();
	let formula = ws1s
		.parse(&text)
		.map_err(|error| Error::plain(error.to_string()))?;
	let answer = match handler {
		"ws1s_valid" => ws1s.valid(&formula, &options),
		"ws1s_satisfiable" => ws1s.satisfiable(&formula, &options),
		_ => {
			return Err(Error::plain(format!(
				"operation '{name}' is not available in the Rust bindings"
			)));
		}
	};
	answer
		.map(Value::Bool)
		.map_err(|error| Error::plain(error.to_string()))
}
