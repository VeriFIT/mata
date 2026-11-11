// Copyright (c) 2026 VeriFIT. GNU GPL v3. See LICENSE in the project root.

//! End-to-end tests of the `mata` binary: the commands, the exit statuses, and the stream each one writes.

use std::ffi::OsStr;
use std::io::Write as _;
use std::path::PathBuf;
use std::process::{Command, Output, Stdio};
use std::sync::atomic::{AtomicUsize, Ordering};

const NFA: &str = "@NFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final q1\nq0 a q1\n";
const NFT: &str = "@NFT-explicit\n%Alphabet-auto\n%Initial q0\n%Final q0\n%Levels q0:0 q1:1\n%LevelsNum 2\nq0 a q1\nq1 b q0\n";

/// A temporary directory that removes itself, so every test owns its fixture files.
struct Workspace {
	path: PathBuf,
}

impl Workspace {
	fn new(label: &str) -> Workspace {
		static COUNTER: AtomicUsize = AtomicUsize::new(0);
		let path = std::env::temp_dir().join(format!(
			"mata-cli-test-{label}-{}-{}",
			std::process::id(),
			COUNTER.fetch_add(1, Ordering::Relaxed)
		));
		std::fs::create_dir_all(&path).expect("a temporary directory");
		Workspace { path }
	}

	/// Writes `content` to `name` and returns the path as the command line takes it.
	fn file(&self, name: &str, content: &str) -> String {
		let path = self.path.join(name);
		std::fs::write(&path, content).expect("a fixture file");
		path.to_str().expect("a UTF-8 path").to_owned()
	}

	fn join(&self, name: &str) -> String {
		self.path
			.join(name)
			.to_str()
			.expect("a UTF-8 path")
			.to_owned()
	}

	fn read(&self, name: &str) -> String {
		std::fs::read_to_string(self.path.join(name)).expect("a written file")
	}
}

impl Drop for Workspace {
	fn drop(&mut self) {
		let _ = std::fs::remove_dir_all(&self.path);
	}
}

fn mata<I, S>(arguments: I) -> Output
where
	I: IntoIterator<Item = S>,
	S: AsRef<OsStr>,
{
	Command::new(env!("CARGO_BIN_EXE_mata"))
		.args(arguments)
		.output()
		.expect("the mata binary runs")
}

fn mata_with_stdin<I, S>(arguments: I, stdin: &str) -> Output
where
	I: IntoIterator<Item = S>,
	S: AsRef<OsStr>,
{
	let mut child = Command::new(env!("CARGO_BIN_EXE_mata"))
		.args(arguments)
		.stdin(Stdio::piped())
		.stdout(Stdio::piped())
		.stderr(Stdio::piped())
		.spawn()
		.expect("the mata binary runs");
	child
		.stdin
		.take()
		.expect("a piped standard input")
		.write_all(stdin.as_bytes())
		.expect("the script is written");
	child.wait_with_output().expect("the mata binary finishes")
}

fn status(output: &Output) -> i32 {
	output.status.code().expect("an exit status")
}

fn stdout(output: &Output) -> String {
	String::from_utf8_lossy(&output.stdout).into_owned()
}

fn stderr(output: &Output) -> String {
	String::from_utf8_lossy(&output.stderr).into_owned()
}

#[test]
fn help_lists_the_commands_and_ends_with_a_newline() {
	let output = mata(["--help"]);
	assert_eq!(status(&output), 0);
	let text = stdout(&output);
	assert!(text.contains("usage: mata <command>"), "{text}");
	assert!(text.contains("scripts run|check|repl"), "{text}");
	assert!(
		text.ends_with('\n'),
		"the help text ends with a newline: {text:?}"
	);
	assert!(stderr(&output).is_empty(), "{}", stderr(&output));
}

#[test]
fn subcommand_help_documents_the_options() {
	let text = stdout(&mata(["help", "scripts", "run"]));
	assert!(text.contains("--expression TEXT"), "{text}");
	assert!(text.contains("--input NAME=PATH"), "{text}");
	assert!(text.contains("--output PATH"), "{text}");

	let text = stdout(&mata(["scripts", "check", "--help"]));
	assert!(text.contains("checked: <path>"), "{text}");
}

#[test]
fn nfa_help_lists_the_operations_with_their_operands() {
	let output = mata(["nfa", "--help"]);
	assert_eq!(status(&output), 0);
	let text = stdout(&output);
	assert!(text.contains("nfa:trim (1 operand)"), "{text}");
	assert!(
		text.contains("nfa:union-nondeterministic (2 operands)"),
		"{text}"
	);
	assert!(text.contains("nfa:literal (any operands)"), "{text}");
}

#[test]
fn formats_lists_every_format_and_operation() {
	let output = mata(["formats"]);
	assert_eq!(status(&output), 0);
	let text = stdout(&output);
	assert!(text.starts_with("format mata\nformat dot\n"), "{text}");
	assert!(text.contains("operation nfa:trim (1 operand)\n"), "{text}");
	assert!(
		text.contains("operation nft:apply (2 operands)\n"),
		"{text}"
	);
}

#[test]
fn run_prints_the_value_of_an_expression() {
	let output = mata(["scripts", "run", "--expression", "(print (+ 1 2))"]);
	assert_eq!(status(&output), 0);
	assert_eq!(stdout(&output), "3\n");
}

#[test]
fn run_reads_positional_inputs_as_the_inputs_list() {
	let workspace = Workspace::new("positional");
	let first = workspace.file("first.mata", NFA);
	let second = workspace.file("second.mata", NFA);
	let output = mata([
		"scripts",
		"run",
		"--expression",
		"(print inputs)\n(print $2)",
		"--",
		&first,
		&second,
	]);
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	assert_eq!(stdout(&output), format!("{first}\n{second}\n{second}\n"));
}

#[test]
fn run_loads_a_named_input_at_its_declaration() {
	let workspace = Workspace::new("named");
	let nfa = workspace.file("a.mata", NFA);
	let input = format!("lhs={nfa}");
	let output = mata([
		"scripts",
		"run",
		"--expression",
		"(declare-input lhs Nfa)\n(print (nfa:number-of-states lhs))",
		"--input",
		&input,
	]);
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	assert_eq!(stdout(&output), "2\n");
}

#[test]
fn run_writes_the_output_to_the_given_file() {
	let workspace = Workspace::new("output");
	let path = workspace.join("out.txt");
	let output = mata([
		"scripts",
		"run",
		"--expression",
		"(print 42)",
		"--output",
		&path,
	]);
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	assert!(stdout(&output).is_empty(), "{}", stdout(&output));
	assert_eq!(workspace.read("out.txt"), "42\n");
}

#[test]
fn check_splits_the_report_between_the_streams() {
	let workspace = Workspace::new("check");
	let script = workspace.file(
		"ok.matasc",
		"(declare-input lhs Nfa)\n(print (nfa:number-of-states lhs))\n",
	);
	let output = mata(["scripts", "check", &script]);
	assert_eq!(status(&output), 0);
	assert_eq!(stdout(&output), format!("checked: {script}\n"));
	assert_eq!(stderr(&output), "termination: guaranteed\n");
}

#[test]
fn check_warns_about_an_unused_declaration_without_failing() {
	let workspace = Workspace::new("unused");
	let script = workspace.file(
		"unused.matasc",
		"(declare-input lhs Nfa)\n(declare-input rhs Nfa)\n(print (nfa:number-of-states lhs))\n",
	);
	let output = mata(["scripts", "check", &script]);
	assert_eq!(status(&output), 0);
	assert_eq!(stdout(&output), format!("checked: {script}\n"));
	let text = stderr(&output);
	assert!(text.contains("termination: guaranteed"), "{text}");
	assert!(text.contains("unused declaration 'rhs'"), "{text}");
}

#[test]
fn check_reports_where_a_call_has_the_wrong_number_of_arguments() {
	let output = mata([
		"scripts",
		"check",
		"--expression",
		"(print (nfa:union-nondeterministic $1))",
	]);
	assert_eq!(status(&output), 2);
	let text = stderr(&output);
	assert!(text.starts_with("mata: "), "{text}");
	assert!(
		text.contains("'nfa:union-nondeterministic' takes 2 arguments, 1 given at 1:"),
		"{text}"
	);
}

#[test]
fn check_rejects_recursion_that_the_language_does_not_implement() {
	let output = mata(["scripts", "check", "--expression", "(define (f x) (f x))"]);
	assert_eq!(status(&output), 2);
	assert!(
		stderr(&output).contains("recursion needs :loops-and-recursion, which is not implemented"),
		"{}",
		stderr(&output)
	);
}

#[test]
fn check_reports_a_function_value_that_can_escape() {
	let output = mata([
		"scripts",
		"check",
		"--expression",
		"(define (mk n) (lambda (x) (+ x n)))",
	]);
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	assert_eq!(stdout(&output), "checked: <expression>\n");
	assert_eq!(stderr(&output), "termination: not-guaranteed\n");
}

#[test]
fn a_failed_assertion_exits_one() {
	let output = mata(["scripts", "run", "--expression", "(assert (= 1 2))"]);
	assert_eq!(status(&output), 1);
	assert!(stdout(&output).is_empty(), "{}", stdout(&output));
}

#[test]
fn a_parse_error_exits_two_with_its_place() {
	let output = mata(["scripts", "run", "--expression", "(print "]);
	assert_eq!(status(&output), 2);
	assert_eq!(stderr(&output), "mata: expected expression at 1:8\n");
}

#[test]
fn a_timing_record_has_the_label_and_six_fractional_digits() {
	let output = mata(["scripts", "run", "--expression", "(time \"label\" (+ 1 1))"]);
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	let text = stdout(&output);
	let rest = text
		.strip_prefix("label: ")
		.unwrap_or_else(|| panic!("a timing record, found {text:?}"));
	let seconds = rest.trim_end_matches('\n');
	let (whole, fraction) = seconds.split_once('.').expect("a decimal point");
	assert!(whole.chars().all(|c| c.is_ascii_digit()), "{seconds}");
	assert_eq!(fraction.len(), 6, "{seconds}");
	assert!(fraction.chars().all(|c| c.is_ascii_digit()), "{seconds}");
}

#[test]
fn a_direct_operation_reads_a_file_and_pipes_into_the_next_command() {
	let workspace = Workspace::new("direct");
	let nfa = workspace.file("a.mata", NFA);
	let trimmed = mata(["nfa", "trim", &nfa]);
	assert_eq!(status(&trimmed), 0, "{}", stderr(&trimmed));
	assert_eq!(stdout(&trimmed), NFA);

	let counted = mata_with_stdin(["nfa", "number-of-states", "-"], &stdout(&trimmed));
	assert_eq!(status(&counted), 0, "{}", stderr(&counted));
	assert_eq!(stdout(&counted), "2\n");
}

#[test]
fn a_direct_query_exits_zero_for_true_and_one_for_false() {
	let workspace = Workspace::new("query");
	let nfa = workspace.file("a.mata", NFA);
	let output = mata(["nfa", "language-empty?", &nfa]);
	assert_eq!(status(&output), 1);
	assert_eq!(stdout(&output), "false\n");

	let output = mata(["nfa", "number-of-states", &nfa]);
	assert_eq!(status(&output), 0);
	assert_eq!(stdout(&output), "2\n");
}

#[test]
fn a_direct_operation_writes_to_the_given_file() {
	let workspace = Workspace::new("direct-output");
	let nfa = workspace.file("a.mata", NFA);
	let path = workspace.join("count.txt");
	let output = mata(["nfa", "number-of-states", &nfa, "--output", &path]);
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	assert!(stdout(&output).is_empty(), "{}", stdout(&output));
	assert_eq!(workspace.read("count.txt"), "2\n");
}

#[test]
fn convert_round_trips_an_nfa_through_dot() {
	let workspace = Workspace::new("convert-nfa");
	let nfa = workspace.file("a.mata", NFA);
	let dot = mata(["convert", &nfa]);
	assert_eq!(status(&dot), 0, "{}", stderr(&dot));
	let text = stdout(&dot);
	assert!(text.contains("0 -> 1 [label=\"a\"];"), "{text}");
	assert!(text.contains("i0 -> 0;"), "{text}");

	let back = mata_with_stdin(["convert", "--from", "dot", "--to", "mata", "-"], &text);
	assert_eq!(status(&back), 0, "{}", stderr(&back));
	assert_eq!(stdout(&back), NFA);
}

#[test]
fn convert_round_trips_a_transducer_through_dot() {
	let workspace = Workspace::new("convert-nft");
	let nft = workspace.file("t.nft.mata", NFT);
	let dot = mata(["convert", &nft]);
	assert_eq!(status(&dot), 0, "{}", stderr(&dot));
	let text = stdout(&dot);
	assert!(text.contains("0 [label=\"0:0\"];"), "{text}");
	assert!(text.contains("1 [label=\"1:1\"];"), "{text}");

	let back = mata_with_stdin(["convert", "--from", "dot", "--to", "mata", "-"], &text);
	assert_eq!(status(&back), 0, "{}", stderr(&back));
	assert_eq!(stdout(&back), NFT);

	let again = mata_with_stdin(["convert", "--from", "dot", "--to", "dot", "-"], &text);
	assert_eq!(status(&again), 0, "{}", stderr(&again));
	assert_eq!(stdout(&again), text);
}

#[test]
fn convert_writes_one_file_per_input_into_a_directory() {
	let workspace = Workspace::new("convert-directory");
	let first = workspace.file("first.mata", NFA);
	let second = workspace.file("second.mata", NFA);
	let directory = workspace.join("out");
	std::fs::create_dir(&directory).expect("a directory for the conversions");
	let output = mata([
		"convert", "--to", "dot", &first, &second, "--output", &directory,
	]);
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	assert!(stdout(&output).is_empty(), "{}", stdout(&output));
	for name in ["first.dot", "second.dot"] {
		let converted = std::fs::read_to_string(std::path::Path::new(&directory).join(name))
			.unwrap_or_else(|e| panic!("{name}: {e}"));
		assert!(
			converted.starts_with("digraph finiteAutomaton {"),
			"{converted}"
		);
	}
}

#[test]
fn convert_rejects_inputs_of_different_kinds() {
	let workspace = Workspace::new("convert-mixed");
	let nfa = workspace.file("a.mata", NFA);
	let nft = workspace.file("t.nft.mata", NFT);
	let output = mata(["convert", "--to", "mata", &nfa, &nft]);
	assert_eq!(status(&output), 2);
	assert!(
		stderr(&output).contains("holds an NFT, but the first input holds an NFA"),
		"{}",
		stderr(&output)
	);
}

#[test]
fn repl_writes_the_value_of_every_form_read_from_standard_input() {
	let output = mata_with_stdin(
		["scripts", "repl"],
		"(nfa:number-of-states (nfa:create-from-regex \"a\"))\n",
	);
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	assert_eq!(stdout(&output), "2\n");
}

#[test]
fn the_language_binds_functions_lists_and_conditionals() {
	let script = "(define (mk n) (lambda (x) (+ x n)))\n\
	              (define add2 (mk 2))\n\
	              (print (add2 5))\n\
	              (print (let ((a 1) (b 2)) (+ a b)))\n\
	              (print (cond ((= 1 2) 3) (else 4)))\n\
	              (print (list:map (lambda (y) (+ y 1)) (list 1 2)))";
	let output = mata(["scripts", "run", "--expression", script]);
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	assert_eq!(stdout(&output), "7\n3\n4\n2\n3\n");
}

#[test]
fn a_literal_builds_an_automaton_and_keeps_the_level_of_each_state() {
	let output = mata([
		"scripts",
		"run",
		"--expression",
		"(print (nfa:number-of-states (nfa:literal :initial (q0) :final (q1) :transitions ((q0 a q1)))))",
	]);
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	assert_eq!(stdout(&output), "2\n");

	let output = mata([
		"scripts",
		"run",
		"--expression",
		"(print (nft:print-to-mata (nft:literal :levels 2 :initial (q0) :final (q0) :states ((q1 1)) :transitions ((q0 a q1) (q1 b q0)))))",
	]);
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	let text = stdout(&output);
	assert!(text.contains("%Levels q0:0 q1:1\n"), "{text}");
	assert!(text.contains("%LevelsNum 2\n"), "{text}");
}

#[test]
fn a_literal_rejects_a_keyword_it_does_not_take() {
	let output = mata([
		"scripts",
		"run",
		"--expression",
		"(nfa:literal :initial (q0) :final (q1) :bogus (x))",
	]);
	assert_eq!(status(&output), 2);
	assert!(
		stderr(&output).contains("takes no keyword ':bogus'"),
		"{}",
		stderr(&output)
	);
}
