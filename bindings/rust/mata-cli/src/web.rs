// Copyright (c) 2026 VeriFIT. GNU GPL v3. See LICENSE in the project root.

//! The Script mode of the website playground, which runs this interpreter compiled to WebAssembly: the
//! module `mata-playground-script` calls [`run`] (`src/bin/mata-playground-script.rs`).

use std::collections::HashMap;
use std::time::Duration;

use crate::Error;
use crate::cli::Exit;
use crate::scripts::{Inputs, Source};

/// Runs `script` as `mata script run --expression SCRIPT` does, with the named inputs and parameters of
/// `--input` and `--param`, one `NAME=VALUE` per line of `inputs` and `parameters`.
///
/// The values the script prints go to standard output and the errors to standard error, each as soon
/// as it is written, so a run that is stopped from outside keeps what it printed. Paths, those of
/// `inputs` included, are relative to the current directory.
///
/// After `time_limit`, the run stops with an error before its next top-level form, function call or
/// loop round. A library operation that is running is not interrupted, so the caller still needs a
/// limit of its own.
#[must_use]
pub fn run(script: &str, inputs: &str, parameters: &str, time_limit: Option<Duration>) -> Exit {
	let result = pairs(inputs, "--input").and_then(|named| {
		let inputs = Inputs {
			positional: Vec::new(),
			named,
			parameters: pairs(parameters, "--param")?,
			time_limit,
		};
		crate::scripts::run(&Source::Expression(script.to_owned()), inputs, None, true)
	});
	result.unwrap_or_else(|error| {
		eprintln!("mata: {}", error.render());
		Exit::Usage
	})
}

/// The `NAME=VALUE` lines of `text`, as `option` takes them.
fn pairs(text: &str, option: &str) -> Result<HashMap<String, String>, Error> {
	let mut pairs = HashMap::new();
	for line in text.lines().filter(|line| !line.is_empty()) {
		crate::cli::insert_pair(&mut pairs, line, option)?;
	}
	Ok(pairs)
}
