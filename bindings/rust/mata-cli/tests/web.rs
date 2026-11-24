// Copyright (c) 2026 VeriFIT. GNU GPL v3. See LICENSE in the project root.

//! Tests of `mata_cli::web::run`, the entry point of the website playground's Script mode.
#![cfg(feature = "web")]

use std::path::PathBuf;
use std::time::{Duration, Instant};

use mata_cli::cli::Exit;
use mata_cli::web::run;

const NFA: &str = "@NFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final q1\nq0 a q1\n";

/// A fresh path in the temporary directory, removed when dropped.
struct TempPath(PathBuf);

impl TempPath {
	fn new(label: &str) -> TempPath {
		TempPath(std::env::temp_dir().join(format!("mata-cli-web-{label}-{}", std::process::id())))
	}

	fn as_str(&self) -> &str {
		self.0.to_str().expect("a UTF-8 path")
	}
}

impl Drop for TempPath {
	fn drop(&mut self) {
		let _ = std::fs::remove_file(&self.0);
	}
}

#[test]
fn exit_status_follows_the_script() {
	let empty = "(check (nfa:language-empty? (nfa:create-from-regex \"a\")))";
	assert_eq!(run(empty, "", "", None), Exit::Success);
	assert_eq!(run("(assert false)", "", "", None), Exit::Assertion);
	assert_eq!(run("(nfa:no-such-operation)", "", "", None), Exit::Usage);
}

#[test]
fn named_inputs_and_parameters_reach_the_declarations() {
	let input = TempPath::new("input");
	std::fs::write(&input.0, NFA).expect("an input file");
	let script = "(declare-input a Nfa)\n(declare-param letter String)\n\
		(assert (nfa:in-language? a (word letter)))";
	let inputs = format!("a={}\n", input.as_str());
	assert_eq!(run(script, &inputs, "letter=a", None), Exit::Success);
	assert_eq!(run(script, &inputs, "letter=b", None), Exit::Assertion);
	// A missing input, a line that is not NAME=VALUE, and a name given twice are errors.
	assert_eq!(run(script, "", "letter=a", None), Exit::Usage);
	assert_eq!(run(script, &inputs, "letter", None), Exit::Usage);
	assert_eq!(
		run(script, &inputs, "letter=a\nletter=b", None),
		Exit::Usage
	);
}

#[test]
fn time_limit_stops_an_endless_loop_after_the_forms_before_it() {
	let written = TempPath::new("written");
	let script = format!(
		"(set-option :loops-and-recursion true)\n(write \"before\" {:?})\n(loop ((i 0)) (recur i))",
		written.as_str()
	);
	let started = Instant::now();
	assert_eq!(
		run(&script, "", "", Some(Duration::from_millis(200))),
		Exit::Usage
	);
	assert!(started.elapsed() < Duration::from_secs(30));
	assert_eq!(
		std::fs::read_to_string(&written.0).expect("the file the script wrote"),
		"before\n"
	);
}

#[test]
fn a_time_limit_that_is_not_reached_changes_nothing() {
	let script = "(set-option :loops-and-recursion true)\n\
		(assert (= (loop ((i 0)) (if (< i 100) (recur (+ i 1)) i)) 100))";
	assert_eq!(
		run(script, "", "", Some(Duration::from_secs(600))),
		Exit::Success
	);
	assert_eq!(run(script, "", "", None), Exit::Success);
}
