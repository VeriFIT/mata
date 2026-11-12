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
	assert!(text.contains("script run|check|repl"), "{text}");
	assert!(
		text.ends_with('\n'),
		"the help text ends with a newline: {text:?}"
	);
	assert!(stderr(&output).is_empty(), "{}", stderr(&output));
}

#[test]
fn subcommand_help_documents_the_options() {
	let text = stdout(&mata(["help", "script", "run"]));
	assert!(text.contains("--expression TEXT"), "{text}");
	assert!(text.contains("--input NAME=PATH"), "{text}");
	assert!(text.contains("--output PATH"), "{text}");

	let text = stdout(&mata(["script", "check", "--help"]));
	assert!(text.contains("checked: <path>"), "{text}");
}

#[test]
fn nfa_help_lists_only_the_operations_a_command_line_can_call() {
	let output = mata(["nfa", "--help"]);
	assert_eq!(status(&output), 0);
	let text = stdout(&output);
	assert!(text.contains("\n  trim NFA\n"), "{text}");
	assert!(
		text.contains("\n  union-nondeterministic NFA NFA [NFA…]\n"),
		"{text}"
	);
	assert!(
		text.contains("\n  included? NFA NFA [--hint algorithm=antichains|naive]\n"),
		"{text}"
	);
	assert!(!text.contains("literal"), "{text}");
	assert!(!text.contains("list:"), "{text}");
}

#[test]
fn formats_lists_every_format_feature_and_operation() {
	let output = mata(["formats"]);
	assert_eq!(status(&output), 0);
	let text = stdout(&output);
	assert!(text.starts_with("format mata\nformat dot\n"), "{text}");
	assert!(text.contains("feature hints implemented\n"), "{text}");
	assert!(
		text.contains("feature loops-and-recursion not-implemented\n"),
		"{text}"
	);
	assert!(text.contains("operation nfa:trim (1 operand)\n"), "{text}");
	assert!(
		text.contains("operation nft:apply (2 operands)\n"),
		"{text}"
	);
}

#[test]
fn run_prints_the_value_of_an_expression() {
	let output = mata(["script", "run", "--expression", "(print (+ 1 2))"]);
	assert_eq!(status(&output), 0);
	assert_eq!(stdout(&output), "3\n");
}

#[test]
fn run_reads_positional_inputs_as_the_inputs_list() {
	let workspace = Workspace::new("positional");
	let first = workspace.file("first.mata", NFA);
	let second = workspace.file("second.mata", NFA);
	let output = mata([
		"script",
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
		"script",
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
		"script",
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
		"(mata-script \"0.1.0\")\n(declare-input lhs Nfa)\n(print (nfa:number-of-states lhs))\n",
	);
	let output = mata(["script", "check", &script]);
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
	let output = mata(["script", "check", &script]);
	assert_eq!(status(&output), 0);
	assert_eq!(stdout(&output), format!("checked: {script}\n"));
	let text = stderr(&output);
	assert!(text.contains("termination: guaranteed"), "{text}");
	assert!(text.contains("unused declaration 'rhs'"), "{text}");
}

#[test]
fn check_reports_where_a_call_has_the_wrong_number_of_arguments() {
	let output = mata([
		"script",
		"check",
		"--expression",
		"(print (nfa:trim (nfa:create-empty-string-nfa) (nfa:create-empty-string-nfa)))",
	]);
	assert_eq!(status(&output), 2);
	let text = stderr(&output);
	assert!(text.starts_with("mata: "), "{text}");
	assert!(
		text.contains("'nfa:trim' takes 1 arguments, 2 given at 1:"),
		"{text}"
	);
}

#[test]
fn check_rejects_recursion_that_the_language_does_not_implement() {
	let output = mata(["script", "check", "--expression", "(define (f x) (f x))"]);
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
		"script",
		"check",
		"--expression",
		"(mata-script \"0.1.0\") (define (mk n) (lambda (x) (+ x n)))",
	]);
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	assert_eq!(stdout(&output), "checked: <expression>\n");
	assert_eq!(stderr(&output), "termination: not-guaranteed\n");
}

#[test]
fn a_failed_assertion_exits_one_and_names_its_place() {
	let output = mata([
		"script",
		"run",
		"--expression",
		"(assert (= 1 1))\n(assert (= 1 2))",
	]);
	assert_eq!(status(&output), 1);
	assert_eq!(stderr(&output), "mata: assertion failed at 2:1\n");
	assert!(stdout(&output).is_empty(), "{}", stdout(&output));
}

#[test]
fn a_parse_error_exits_two_with_its_place() {
	let output = mata(["script", "run", "--expression", "(print "]);
	assert_eq!(status(&output), 2);
	assert_eq!(stderr(&output), "mata: expected expression at 1:8\n");
}

#[test]
fn a_timing_record_has_the_label_and_six_fractional_digits() {
	let output = mata(["script", "run", "--expression", "(time \"label\" (+ 1 1))"]);
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
		["script", "repl"],
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
	let output = mata(["script", "run", "--expression", script]);
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	assert_eq!(stdout(&output), "7\n3\n4\n2\n3\n");
}

#[test]
fn a_literal_builds_an_automaton_and_keeps_the_level_of_each_state() {
	let output = mata([
		"script",
		"run",
		"--expression",
		"(print (nfa:number-of-states (nfa:literal :initial (q0) :final (q1) :transitions ((q0 a q1)))))",
	]);
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	assert_eq!(stdout(&output), "2\n");

	let output = mata([
		"script",
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
		"script",
		"run",
		"--expression",
		"(nfa:literal :initial (q0) :final (q1) :bogus (x))",
	]);
	assert_eq!(status(&output), 2);
	assert!(
		stderr(&output).contains("'nfa:literal' has no keyword ':bogus'"),
		"{}",
		stderr(&output)
	);
}

const OTHER_NFA: &str = "@NFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final q0\nq0 b q0\n";

#[test]
fn run_checks_the_whole_script_before_it_runs_anything() {
	let output = mata([
		"script",
		"run",
		"--expression",
		"(print 1)\n(nfa:determinizee 2)",
	]);
	assert_eq!(status(&output), 2);
	assert!(stdout(&output).is_empty(), "{}", stdout(&output));
	assert_eq!(
		stderr(&output),
		"mata: unknown name 'nfa:determinizee' at 2:1\n"
	);
}

#[test]
fn run_without_the_check_runs_forms_until_one_fails() {
	let output = mata([
		"script",
		"run",
		"--no-check",
		"--expression",
		"(print 1)\n(nfa:determinizee 2)\n(print 3)",
	]);
	assert_eq!(status(&output), 2);
	assert_eq!(stdout(&output), "1\n");
	assert_eq!(
		stderr(&output),
		"mata: unknown name 'nfa:determinizee' at 2:1\n"
	);
}

#[test]
fn run_without_the_check_still_rejects_a_required_feature_it_lacks() {
	let output = mata([
		"script",
		"run",
		"--no-check",
		"--expression",
		"(mata-script \"0.1.0\" :requires (loops-and-recursion))\n(print 1)",
	]);
	assert_eq!(status(&output), 2);
	assert!(stdout(&output).is_empty(), "{}", stdout(&output));
}

#[test]
fn a_pipeline_application_step_counts_the_piped_value_as_its_first_argument() {
	let script = "(mata-script \"0.1.0\")\n(set-option :alphabet \"bytes\")\n(define a (nfa:create-from-regex \"a\"))\n\
	              (print (-> a nfa:determinize (nfa:in-language? (word \"a\"))))";
	let checked = mata(["script", "check", "--expression", script]);
	assert_eq!(status(&checked), 0, "{}", stderr(&checked));
	let run = mata(["script", "run", "--expression", script]);
	assert_eq!(stdout(&run), "true\n");

	let wrong = mata([
		"script",
		"check",
		"--expression",
		"(-> (nfa:create-empty-string-nfa) (nfa:trim 1))",
	]);
	assert_eq!(status(&wrong), 2);
	assert!(
		stderr(&wrong).contains("'nfa:trim' takes 1 arguments, 2 given"),
		"{}",
		stderr(&wrong)
	);
}

#[test]
fn a_literal_is_not_a_direct_command() {
	let workspace = Workspace::new("literal-direct");
	let nfa = workspace.file("a.mata", NFA);
	let output = mata(["nfa", "literal", &nfa]);
	assert_eq!(status(&output), 2);
	assert!(
		stderr(&output).contains("operands that a command line cannot give"),
		"{}",
		stderr(&output)
	);
}

#[test]
fn a_binary_operation_folds_more_operands_from_the_left() {
	let workspace = Workspace::new("fold");
	let a = workspace.file("a.mata", NFA);
	let b = workspace.file("b.mata", OTHER_NFA);
	let script = "(declare-inputs xs (List Nfa))\n\
	              (print (nfa:language-empty? (nfa:intersection (list:nth xs 0) (list:nth xs 0) (list:nth xs 1))))\n\
	              (print (nfa:language-empty? (nfa:intersection (list:nth xs 0) (list:nth xs 0) (list:nth xs 0))))";
	let output = mata(["script", "run", "--expression", script, &a, &b]);
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	assert_eq!(stdout(&output), "true\nfalse\n");

	let direct = mata(["nfa", "union-nondeterministic", &a, &b, &a]);
	assert_eq!(status(&direct), 0, "{}", stderr(&direct));
	let back = mata_with_stdin(["nfa", "in-language", "-", "b b"], &stdout(&direct));
	assert_eq!(stdout(&back), "true\n");
}

#[test]
fn inputs_a_script_does_not_declare_are_errors() {
	let workspace = Workspace::new("extra-inputs");
	let a = workspace.file("a.mata", NFA);
	let script = "(declare-input lhs Nfa)\n(print (nfa:number-of-states lhs))";
	let extra = mata(["script", "run", "--expression", script, &a, &a]);
	assert_eq!(status(&extra), 2);
	assert_eq!(
		stderr(&extra),
		"mata: 2 positional inputs given, but the script takes 1\n"
	);
	let named = format!("other={a}");
	let unknown = mata([
		"script",
		"run",
		"--expression",
		script,
		&a,
		"--input",
		&named,
	]);
	assert_eq!(status(&unknown), 2);
	assert_eq!(
		stderr(&unknown),
		"mata: --input other: the script declares no input 'other'\n"
	);
}

#[test]
fn command_line_paths_are_relative_to_the_current_directory_and_script_paths_to_the_script() {
	let workspace = Workspace::new("paths");
	std::fs::create_dir_all(workspace.path.join("scripts")).expect("a directory");
	workspace.file("input.mata", NFA);
	workspace.file("scripts/local.mata", OTHER_NFA);
	let script = workspace.file(
		"scripts/paths.matasc",
		"(declare-input given Nfa)\n\
		 (print (nfa:number-of-states given))\n\
		 (print (nfa:number-of-states (nfa:load \"local.mata\")))\n",
	);
	let output = Command::new(env!("CARGO_BIN_EXE_mata"))
		.args(["script", "run", &script, "input.mata"])
		.current_dir(&workspace.path)
		.output()
		.expect("the mata binary runs");
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	assert_eq!(stdout(&output), "2\n1\n");
}

#[test]
fn hints_select_the_algorithm_and_wrong_hints_are_rejected_before_running() {
	let workspace = Workspace::new("hints");
	let a = workspace.file("a.mata", NFA);
	let b = workspace.file("b.mata", OTHER_NFA);
	let script = "(declare-input a Nfa)\n(declare-input b Nfa)\n\
	              (check \"naive\" (nfa:included? a b :algorithm \"naive\"))\n\
	              (set-option :default (nfa:included? :algorithm \"antichains\"))\n\
	              (check \"antichains\" (nfa:included? a a))";
	let output = mata(["script", "run", "--expression", script, &a, &b]);
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	assert_eq!(stdout(&output), "naive: false\nantichains: true\n");

	let unknown = mata([
		"script",
		"check",
		"--expression",
		"(nfa:trim (nfa:create-empty-string-nfa) :algorithm \"x\")",
	]);
	assert_eq!(status(&unknown), 2);
	assert!(
		stderr(&unknown).contains("operation nfa:trim takes no hints"),
		"{}",
		stderr(&unknown)
	);
	let value = mata(["nfa", "included", &a, &b, "--hint", "algorithm=fast"]);
	assert_eq!(status(&value), 2);
	assert_eq!(
		stderr(&value),
		"mata: hint :algorithm of nfa:included? is one of \"antichains\", \"naive\", not \"fast\"\n"
	);
	let direct = mata(["nfa", "included", &a, &a, "--hint", "algorithm=naive"]);
	assert_eq!((status(&direct), stdout(&direct)), (0, "true\n".to_owned()));
}

#[test]
fn output_is_written_as_it_is_printed() {
	let mut child = Command::new(env!("CARGO_BIN_EXE_mata"))
		.args(["script", "repl"])
		.stdin(Stdio::piped())
		.stdout(Stdio::piped())
		.stderr(Stdio::piped())
		.spawn()
		.expect("the mata binary runs");
	let mut stdin = child.stdin.take().expect("a piped standard input");
	stdin
		.write_all(b"(echo \"first\")\n")
		.expect("the form is written");
	let mut stdout = std::io::BufReader::new(child.stdout.take().expect("a piped standard output"));
	let mut line = String::new();
	// The session is still open, so the line arrives only if it was flushed when it was printed.
	std::io::BufRead::read_line(&mut stdout, &mut line).expect("a line");
	assert_eq!(line, "first\n");
	drop(stdin);
	assert!(child.wait().expect("the session ends").success());
}

#[test]
fn records_have_unique_labels() {
	let output = mata([
		"script",
		"run",
		"--expression",
		"(check \"verdict\" true)\n(check \"verdict\" false)",
	]);
	assert_eq!(status(&output), 2);
	assert_eq!(stdout(&output), "verdict: true\n");
	assert_eq!(
		stderr(&output),
		"mata: the record label 'verdict' is already printed; give each record its own label at 2:1\n"
	);
}

#[test]
fn names_of_operations_and_of_the_language_cannot_be_defined() {
	for (script, message) in [
		("(define nfa:later 1)", "'nfa:later' is reserved"),
		("(define list 1)", "'list' is reserved"),
		("(define (f if) 1)", "'if' is reserved"),
		("(let ((nfa:x 1)) 2)", "'nfa:x' is reserved"),
	] {
		let output = mata(["script", "check", "--expression", script]);
		assert_eq!(status(&output), 2, "{script}");
		assert!(
			stderr(&output).contains(message),
			"{script}: {}",
			stderr(&output)
		);
	}
}

#[test]
fn parameters_are_typed_and_have_defaults() {
	let script = "(declare-param count Int 3)\n(declare-param name String)\n\
	              (echo (concat name \"=\" (to-string (+ count (string->int \"1\")))))";
	let given = mata([
		"script",
		"run",
		"--expression",
		script,
		"--param",
		"count=5",
		"--param",
		"name=size",
	]);
	assert_eq!((status(&given), stdout(&given)), (0, "size=6\n".to_owned()));
	let defaulted = mata(["script", "run", "--expression", script, "--param", "name=n"]);
	assert_eq!(stdout(&defaulted), "n=4\n");
	let wrong = mata([
		"script",
		"run",
		"--expression",
		script,
		"--param",
		"count=five",
		"--param",
		"name=n",
	]);
	assert_eq!(status(&wrong), 2);
	assert!(
		stderr(&wrong).contains("--param count: 'five' is not an Int"),
		"{}",
		stderr(&wrong)
	);
	let missing = mata(["script", "run", "--expression", script]);
	assert_eq!(status(&missing), 2);
	assert!(
		stderr(&missing).contains("no value for parameter 'name'"),
		"{}",
		stderr(&missing)
	);
}

#[test]
fn options_and_information_are_values() {
	let output = mata([
		"script",
		"run",
		"--expression",
		"(set-info :name \"demo\")\n(print (get-option :alphabet))\n(print (get-info :name))",
	]);
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	assert_eq!(stdout(&output), "on-the-fly\ndemo\n");
}

#[test]
fn an_executable_script_starts_with_a_shebang_line() {
	let workspace = Workspace::new("shebang");
	let script = workspace.file(
		"run.matasc",
		"#!/usr/bin/env -S mata script run\n(print (+ 1 1))\n",
	);
	let output = mata(["script", "run", &script]);
	assert_eq!((status(&output), stdout(&output)), (0, "2\n".to_owned()));
}

#[test]
fn standard_input_can_hold_an_input_or_a_script_to_check() {
	let output = mata_with_stdin(
		[
			"script",
			"run",
			"--expression",
			"(declare-input a Nfa)\n(print (nfa:number-of-states a))",
			"-",
		],
		NFA,
	);
	assert_eq!((status(&output), stdout(&output)), (0, "2\n".to_owned()));
	let checked = mata_with_stdin(
		["script", "check", "-"],
		"(mata-script \"0.1.0\")\n(print 1)\n",
	);
	assert_eq!(status(&checked), 0, "{}", stderr(&checked));
	assert_eq!(stdout(&checked), "checked: <stdin>\n");
}

#[test]
fn a_direct_query_may_leave_out_its_question_mark_and_takes_typed_operands() {
	let workspace = Workspace::new("direct-operands");
	let nfa = workspace.file("a.mata", NFA);
	let empty = mata(["nfa", "language-empty", &nfa]);
	assert_eq!((status(&empty), stdout(&empty)), (1, "false\n".to_owned()));
	let member = mata(["nfa", "in-language", &nfa, "a"]);
	assert_eq!((status(&member), stdout(&member)), (0, "true\n".to_owned()));
	let words = mata(["nfa", "get-words", &nfa, "3"]);
	assert_eq!((status(&words), stdout(&words)), (0, "a\n".to_owned()));
	let regex = mata(["nfa", "create-from-regex", "ab"]);
	assert_eq!(status(&regex), 0, "{}", stderr(&regex));
	let count = mata_with_stdin(["nfa", "number-of-states", "-"], &stdout(&regex));
	assert_eq!(stdout(&count), "3\n");
}

#[test]
fn a_file_with_several_automata_gives_one_operand_each() {
	let workspace = Workspace::new("sections");
	let both = workspace.file("both.mata", &format!("{NFA}\n{OTHER_NFA}"));
	let direct = mata(["nfa", "intersection-empty", &both]);
	assert_eq!((status(&direct), stdout(&direct)), (0, "true\n".to_owned()));
	let script =
		"(declare-input lhs Nfa)\n(declare-input rhs Nfa)\n(print (nfa:included? lhs rhs))";
	let run = mata(["script", "run", "--expression", script, &both]);
	assert_eq!((status(&run), stdout(&run)), (0, "false\n".to_owned()));
	let left_over = mata([
		"script",
		"run",
		"--expression",
		"(declare-input lhs Nfa)\n(print 1)",
		&both,
	]);
	assert_eq!(status(&left_over), 2);
}

#[test]
fn time_operations_records_every_library_call_and_the_loading() {
	let workspace = Workspace::new("time-operations");
	let nfa = workspace.file("a.mata", NFA);
	let output = mata([
		"script",
		"run",
		"--expression",
		"(set-option :time-operations true)\n(declare-input a Nfa)\n(nfa:trim (nfa:trim a))",
		&nfa,
	]);
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	let text = stdout(&output);
	let labels: Vec<&str> = text
		.lines()
		.map(|line| line.split_once(": ").expect("a record").0)
		.collect();
	assert_eq!(labels, ["parsing", "nfa:trim", "nfa:trim#2"]);
}

#[test]
fn a_bytes_alphabet_applies_to_the_declared_inputs() {
	let workspace = Workspace::new("bytes-inputs");
	let nfa = workspace.file("a.mata", NFA);
	let output = mata([
		"script",
		"run",
		"--expression",
		"(set-option :alphabet \"bytes\")\n(declare-input a Nfa)\n(print (nfa:in-language? a (word \"a\")))",
		&nfa,
	]);
	assert_eq!((status(&output), stdout(&output)), (0, "true\n".to_owned()));
}

#[test]
fn a_runtime_error_names_the_place_of_its_form() {
	let output = mata([
		"script",
		"run",
		"--expression",
		"(print 1)\n(print (list:nth (list 1) 4))",
	]);
	assert_eq!(status(&output), 2);
	assert_eq!(stdout(&output), "1\n");
	assert_eq!(
		stderr(&output),
		"mata: list:nth index out of bounds at 2:2\n"
	);
}

#[test]
fn the_repl_reads_utf8_shows_automata_by_size_and_exits_two_after_an_error() {
	let output = mata_with_stdin(
		["script", "repl"],
		"(echo \"ε-přechod\")\n(nfa:create-from-regex \"ab\")\n(nope 1)\n(print 2)\n",
	);
	assert_eq!(status(&output), 2);
	assert_eq!(
		stdout(&output),
		"ε-přechod\nε-přechod\n#<Nfa with 3 states and 2 transitions>\n2\n2\n"
	);
	assert_eq!(stderr(&output), "mata: unknown name 'nope' at 1:1\n");
}

#[test]
fn convert_refuses_two_inputs_that_would_overwrite_one_file() {
	let workspace = Workspace::new("convert-collision");
	std::fs::create_dir_all(workspace.path.join("one")).expect("a directory");
	std::fs::create_dir_all(workspace.path.join("two")).expect("a directory");
	std::fs::create_dir_all(workspace.path.join("out")).expect("a directory");
	let first = workspace.file("one/x.mata", NFA);
	let second = workspace.file("two/x.mata", NFA);
	let output = mata([
		"convert",
		&first,
		&second,
		"--output",
		&workspace.join("out"),
	]);
	assert_eq!(status(&output), 2);
	assert!(
		stderr(&output).contains("would both be written to 'x.dot'"),
		"{}",
		stderr(&output)
	);
	assert!(
		std::fs::read_dir(workspace.path.join("out"))
			.expect("the directory")
			.next()
			.is_none()
	);
}

#[test]
fn print_to_dot_labels_transitions_with_symbol_names() {
	let workspace = Workspace::new("print-dot");
	let nfa = workspace.file("a.mata", NFA);
	let output = mata(["nfa", "print-to-dot", &nfa]);
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	assert!(
		stdout(&output).contains("0 -> 1 [label=\"a\"];"),
		"{}",
		stdout(&output)
	);
}

#[test]
fn the_header_selects_the_language_version_requires_features_and_sets_the_profile() {
	let ok = mata([
		"script",
		"run",
		"--expression",
		"(mata-script \"0.1.4\" :requires (hints) :profile core)\n(print 1)",
	]);
	assert_eq!((status(&ok), stdout(&ok)), (0, "1\n".to_owned()));
	for (script, message) in [
		(
			"(mata-script \"0.2.0\") (this is not (parsed",
			"the script needs Mata Script 0.2.0; this mata runs Mata Script 0.1.x",
		),
		(
			"(mata-script \"1.0.0\")",
			"the script needs Mata Script 1.0.0",
		),
		(
			"(mata-script \"0.1\")",
			"invalid language version '0.1'; expected MAJOR.MINOR.PATCH",
		),
		(
			"(mata-script \"0.1.0\" :requires (loops-and-recursion))",
			"requires feature 'loops-and-recursion', which this mata does not implement",
		),
		(
			"(mata-script \"0.1.0\" :requires (teleport))",
			"unknown feature 'teleport'",
		),
		(
			"(print 1)\n(mata-script \"0.1.0\")",
			"mata-script must be the first form",
		),
		(
			"(mata-script \"0.1.0\" :profile core)\n(print ((lambda (x) x) 1))",
			"a portable core script cannot use a lambda",
		),
	] {
		let output = mata(["script", "run", "--expression", script]);
		assert_eq!(status(&output), 2, "{script}");
		assert!(stdout(&output).is_empty(), "{script}: {}", stdout(&output));
		assert!(
			stderr(&output).contains(message),
			"{script}: {}",
			stderr(&output)
		);
	}
}

#[test]
fn information_keys_are_known_or_start_with_x() {
	let ok = mata([
		"script",
		"check",
		"--expression",
		"(mata-script \"0.1.0\")\n(set-info :name \"n\")\n(set-info :expects true)\n(set-info :x-anything 3)",
	]);
	assert_eq!(status(&ok), 0, "{}", stderr(&ok));
	for (script, message) in [
		("(set-info :nme \"n\")", "unknown info key ':nme'"),
		(
			"(set-info :expects \"yes\")",
			"info :expects must be a Bool, found String",
		),
		(
			"(set-info :profile \"core\")",
			"the profile is set in the header",
		),
	] {
		let output = mata(["script", "check", "--expression", script]);
		assert_eq!(status(&output), 2, "{script}");
		assert!(
			stderr(&output).contains(message),
			"{script}: {}",
			stderr(&output)
		);
	}
}

#[test]
fn check_warns_about_a_script_without_a_header() {
	let output = mata(["script", "check", "--expression", "(print 1)"]);
	assert_eq!(status(&output), 0);
	assert_eq!(
		stderr(&output),
		"termination: guaranteed\nno (mata-script \"0.1.0\") header; the script is read as the version of this mata\n"
	);
}

#[test]
fn version_names_the_command_the_language_and_the_library() {
	let output = mata(["version"]);
	assert_eq!(status(&output), 0);
	let text = stdout(&output);
	let lines: Vec<&str> = text.lines().collect();
	assert_eq!(lines.len(), 3, "{text}");
	assert!(lines[0].starts_with("mata "), "{text}");
	assert_eq!(lines[1], "Mata Script 0.1.0");
	assert!(lines[2].starts_with("libmata "), "{text}");
}

#[test]
fn an_operation_passed_as_a_value_is_a_function() {
	let workspace = Workspace::new("operation-value");
	let a = workspace.file("a.mata", NFA);
	let output = mata([
		"script",
		"run",
		"--expression",
		"(mata-script \"0.1.0\")\n(declare-inputs automata (List Nfa))\n\
		 (print (nfa:number-of-states (list:reduce nfa:intersection automata)))",
		&a,
		&a,
	]);
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	assert_eq!(stdout(&output), "2\n");
}

// Review of #1039: script check options
#[test]
fn script_check_takes_short_options_and_a_separator() {
	let output = mata([
		"script",
		"check",
		"-e",
		"(mata-script \"0.1.0\")\n(print 1)",
	]);
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	assert!(
		stdout(&output).starts_with("checked: <expression>"),
		"{}",
		stdout(&output)
	);
	let output = mata(["script", "check", "-h"]);
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	assert!(
		stdout(&output).starts_with("usage: mata script check"),
		"{}",
		stdout(&output)
	);
	let workspace = Workspace::new("check-separator");
	let script = workspace.file("-s.matasc", "(mata-script \"0.1.0\")\n(print 1)\n");
	let output = mata(["script", "check", "--", &script]);
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	let output = mata(["script", "check", "-x"]);
	assert_eq!(status(&output), 2);
	assert!(
		stderr(&output).contains("unknown option '-x'"),
		"{}",
		stderr(&output)
	);
}

// Review of #1039: parser incompleteness, header versions, REPL exit and output

#[test]
fn the_repl_reads_a_form_that_goes_on_over_several_lines() {
	let output = mata_with_stdin(
		["script", "repl"],
		"(let ((x 1)\n  ) x)\n\
		 (cond ((= 1 1) 2)\n  (else 3))\n\
		 (nfa:literal :initial (q0) :final (q1) :transitions ((q0 a q1)\n  ))\n\
		 \"a\nb\"\n\
		 (define (g a\n  b) b)\n\
		 (g 1 4)\n",
	);
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	assert_eq!(
		stdout(&output),
		"1\n2\n#<Nfa with 2 states and 1 transitions>\na\nb\n4\n"
	);
}

#[test]
fn a_broken_form_is_reported_at_its_place_and_the_repl_reads_on() {
	let output = mata_with_stdin(["script", "repl"], "(define (f x))\n(+ 1 2)\n");
	assert_eq!(status(&output), 2);
	assert_eq!(stdout(&output), "3\n");
	assert_eq!(stderr(&output), "mata: expected function body at 1:14\n");
	let output = mata(["script", "run", "--expression", "(define (f x))\n\n"]);
	assert_eq!(status(&output), 2);
	assert_eq!(stderr(&output), "mata: expected function body at 1:14\n");
}

#[test]
fn a_local_definition_keeps_its_sort() {
	let output = mata([
		"script",
		"run",
		"--no-check",
		"--expression",
		"(define (f) (define x Nfa 1) x)\n(print (f))",
	]);
	assert_eq!(status(&output), 2);
	assert!(stdout(&output).is_empty(), "{}", stdout(&output));
	assert!(
		stderr(&output).contains("expected Nfa, found Int"),
		"{}",
		stderr(&output)
	);
	let output = mata([
		"script",
		"check",
		"--expression",
		"(define (f) (define x (List Nfaa) (list 1)) x)",
	]);
	assert_eq!(status(&output), 2);
	assert_eq!(
		stderr(&output),
		"mata: unknown sort '(List Nfaa)' at 1:13\n"
	);
}

#[test]
fn every_header_has_its_version_checked() {
	for script in [
		"(;\n mata-script \"9.0.0\")\n(print 1)",
		"(mata-script ; the version\n \"9.0.0\")\n(print @)",
	] {
		let output = mata(["script", "run", "--expression", script]);
		assert_eq!(status(&output), 2, "{script}");
		assert!(stdout(&output).is_empty(), "{script}: {}", stdout(&output));
		assert!(
			stderr(&output).contains("the script needs Mata Script 9.0.0"),
			"{script}: {}",
			stderr(&output)
		);
	}
	let output = mata([
		"script",
		"run",
		"--expression",
		"(mata-script ; the version\n \"0.1.0\")\n(print 1)",
	]);
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	assert_eq!(stdout(&output), "1\n");
	let output = mata_with_stdin(["script", "repl"], "(mata-script \"9.0.0\")\n");
	assert_eq!(status(&output), 2);
	assert!(
		stderr(&output).contains("the script needs Mata Script 9.0.0"),
		"{}",
		stderr(&output)
	);
}

#[test]
fn exit_ends_the_repl_session() {
	let output = mata_with_stdin(["script", "repl"], "(+ 1 1)\n(exit) (+ 2 2)\n(+ 3 3)\n");
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	assert_eq!(stdout(&output), "2\n");
}

#[test]
fn the_repl_writes_its_output_file_as_it_prints() {
	let workspace = Workspace::new("repl-output");
	let path = workspace.join("out.txt");
	let mut child = Command::new(env!("CARGO_BIN_EXE_mata"))
		.args(["script", "repl", "--output", &path])
		.stdin(Stdio::piped())
		.stdout(Stdio::piped())
		.stderr(Stdio::piped())
		.spawn()
		.expect("the mata binary runs");
	let mut stdin = child.stdin.take().expect("a piped standard input");
	// The error of the second form shows that the first one has run.
	stdin
		.write_all(b"(+ 1 1)\n(undefined)\n")
		.expect("the forms are written");
	let mut stderr = std::io::BufReader::new(child.stderr.take().expect("a piped standard error"));
	let mut line = String::new();
	std::io::BufRead::read_line(&mut stderr, &mut line).expect("a line");
	assert!(line.starts_with("mata: "), "{line}");
	assert_eq!(workspace.read("out.txt"), "2\n");
	drop(stdin);
	assert_eq!(child.wait().expect("the session ends").code(), Some(2));
}

#[test]
fn the_output_cannot_be_an_input() {
	let workspace = Workspace::new("output-input");
	let a = workspace.file("a.mata", NFA);
	for arguments in [vec!["a.mata"], vec!["--input", "a=./a.mata"]] {
		let output = Command::new(env!("CARGO_BIN_EXE_mata"))
			.current_dir(&workspace.path)
			.args([
				"script",
				"run",
				"--expression",
				"(declare-input a Nfa)\n(print (nfa:number-of-states a))",
				"--output",
				&a,
			])
			.args(&arguments)
			.output()
			.expect("the mata binary runs");
		assert_eq!(status(&output), 2, "{arguments:?}");
		assert!(
			stderr(&output).contains("is the input"),
			"{arguments:?}: {}",
			stderr(&output)
		);
		assert_eq!(workspace.read("a.mata"), NFA, "{arguments:?}");
	}
}

// Review of #1039: checker

#[test]
fn the_check_drops_the_definitions_of_a_popped_scope() {
	let redefined = mata([
		"script",
		"run",
		"--expression",
		"(push) (define x 1) (pop) (define x 2) (print x)",
	]);
	assert_eq!(status(&redefined), 0, "{}", stderr(&redefined));
	assert_eq!(stdout(&redefined), "2\n");
	let dropped = mata([
		"script",
		"check",
		"--expression",
		"(push) (define x 1) (pop) (print x)",
	]);
	assert_eq!(status(&dropped), 2);
	assert!(
		stderr(&dropped).contains("unknown name 'x'"),
		"{}",
		stderr(&dropped)
	);
}

#[test]
fn the_check_compares_a_define_annotation_with_the_value() {
	let wrong = mata([
		"script",
		"run",
		"--expression",
		"(print 1) (define x Nfa 2)",
	]);
	assert_eq!(status(&wrong), 2);
	assert!(stdout(&wrong).is_empty(), "{}", stdout(&wrong));
	assert!(
		stderr(&wrong).contains("expected Nfa, found Int"),
		"{}",
		stderr(&wrong)
	);
	// An annotation narrows a value that may hold the sort, as `as` does.
	let narrowed = mata([
		"script",
		"check",
		"--expression",
		"(declare-input a Automaton) (define b Nfa a) (print (nfa:determinize b))",
	]);
	assert_eq!(status(&narrowed), 0, "{}", stderr(&narrowed));
}

#[test]
fn a_declare_input_takes_one_automaton_so_a_list_sort_is_an_error() {
	let output = mata([
		"script",
		"check",
		"--expression",
		"(declare-input x (List Nfa)) (print (list:length x))",
	]);
	assert_eq!(status(&output), 2);
	assert!(
		stderr(&output).contains("a declared input must be Nfa, Nft, or Automaton"),
		"{}",
		stderr(&output)
	);
}

#[test]
fn only_the_function_a_list_function_applies_keeps_the_termination_guarantee() {
	let termination = |script: &str| {
		let output = mata(["script", "check", "--expression", script]);
		assert_eq!(status(&output), 0, "{script}: {}", stderr(&output));
		stderr(&output)
			.lines()
			.next()
			.unwrap_or_default()
			.to_owned()
	};
	assert_eq!(
		termination("(print (list:fold (lambda (a x) (+ a x)) 0 (list:range 0 3)))"),
		"termination: guaranteed"
	);
	assert_eq!(
		termination("(define g (lambda (acc x) (acc acc x))) (list:fold g g (list:range 0 1))"),
		"termination: not-guaranteed"
	);
	assert_eq!(
		termination("(define f (lambda (x) x)) (print (-> f (list:fold f (list 1))))"),
		"termination: not-guaranteed"
	);
}

#[test]
fn the_portable_core_rejects_forms_and_sorts_outside_of_it() {
	let core = "(mata-script \"0.1.0\" :profile core)\n";
	for (script, message) in [
		("(print (if true 1 2))", "'if' is not in the portable core"),
		(
			"(print (cond (true 1) (else 2)))",
			"'cond' is not in the portable core",
		),
		("(print (do 1 2))", "'do' is not in the portable core"),
		(
			"(print (get-option :alphabet))",
			"'get-option' is not in the portable core",
		),
		("(push)", "'push' is not in the portable core"),
		(
			"(declare-input t Nft)\n(print t)",
			"sort 'Nft' is not in the portable core",
		),
	] {
		let output = mata([
			"script",
			"check",
			"--expression",
			&format!("{core}{script}"),
		]);
		assert_eq!(status(&output), 2, "{script}");
		assert!(
			stderr(&output).contains(message),
			"{script}: {}",
			stderr(&output)
		);
	}
	let within = mata([
		"script",
		"check",
		"--expression",
		&format!(
			"{core}(declare-inputs xs (List Nfa))\n\
			 (define m (-> (list:nth xs 0) nfa:determinize nfa:minimize))\n\
			 (let ((a 1)) (print a))\n(check (time \"t\" (nfa:universal? m)))"
		),
	]);
	assert_eq!(status(&within), 0, "{}", stderr(&within));
}

#[test]
fn a_script_without_declarations_reads_a_named_input_as_a_dollar_name() {
	let workspace = Workspace::new("named-undeclared");
	let a = workspace.file("a.mata", NFA);
	let named = format!("spec={a}");
	let output = mata([
		"script",
		"run",
		"--expression",
		"(print $spec)",
		"--input",
		&named,
	]);
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	assert_eq!(stdout(&output), format!("{a}\n"));
	// `declare-inputs` takes only positional inputs.
	let variadic = format!("xs={a}");
	let output = mata([
		"script",
		"run",
		"--expression",
		"(declare-inputs xs (List Nfa)) (print 1)",
		"--input",
		&variadic,
	]);
	assert_eq!(status(&output), 2);
	assert_eq!(
		stderr(&output),
		"mata: --input xs: the script declares no input 'xs'\n"
	);
}

#[test]
fn the_check_follows_sorts_through_a_pipeline() {
	let output = mata(["script", "run", "--expression", "(print 1) (-> 2 nfa:trim)"]);
	assert_eq!(status(&output), 2);
	assert!(stdout(&output).is_empty(), "{}", stdout(&output));
	assert!(
		stderr(&output).contains("argument 1 of 'nfa:trim' must be Nfa, found Int"),
		"{}",
		stderr(&output)
	);
	let result = mata([
		"script",
		"check",
		"--expression",
		"(print (nfa:determinize (-> 2 (+ 1))))",
	]);
	assert_eq!(status(&result), 2);
	assert!(
		stderr(&result).contains("argument 1 of 'nfa:determinize' must be Nfa, found Int"),
		"{}",
		stderr(&result)
	);
}

#[test]
fn the_elements_of_a_generic_list_result_fit_any_list() {
	let output = mata([
		"script",
		"run",
		"--expression",
		"(print (nfa:encode-word (list:take (list \"a\") 1)))",
	]);
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	assert_eq!(stdout(&output), "a\n");
}

#[test]
fn parameter_values_are_checked_before_any_input_is_loaded() {
	let script = "(declare-input a Nfa) (declare-param n Int) (print n)";
	let bad = mata([
		"script",
		"run",
		"--expression",
		script,
		"missing.mata",
		"--param",
		"n=five",
	]);
	assert_eq!(status(&bad), 2);
	assert!(
		stderr(&bad).contains("--param n: 'five' is not an Int"),
		"{}",
		stderr(&bad)
	);
	let missing = mata(["script", "run", "--expression", script, "missing.mata"]);
	assert_eq!(status(&missing), 2);
	assert!(
		stderr(&missing).contains("no value for parameter 'n'"),
		"{}",
		stderr(&missing)
	);
}

// Review of #1039: DOT reader and writer

fn dot_to_mata(dot: &str) -> Output {
	mata_with_stdin(["convert", "--from", "dot", "--to", "mata", "-"], dot)
}

#[test]
fn a_dot_state_number_beyond_the_states_drawn_is_an_error() {
	for state in ["4294967295", "4000000000"] {
		let output = dot_to_mata(&format!("digraph finiteAutomaton {{\n{state};\n}}\n"));
		assert_eq!(status(&output), 2, "{}", stderr(&output));
		assert!(
			stderr(&output).contains("states are numbered from 0 without a gap"),
			"{}",
			stderr(&output)
		);
	}
}

#[test]
fn a_numbered_dot_label_reads_back_and_keeps_its_number_from_names() {
	let workspace = Workspace::new("dot-numbers");
	let nfa = workspace.file(
		"a.mata",
		"@NFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final q1\nq0 a q1\nq1 3 q0\n",
	);
	let mata_text = stdout(&mata(["convert", "--to", "mata", &nfa]));
	let dot = stdout(&mata(["convert", &nfa]));
	assert!(dot.contains("[label=\"1\"]"), "{dot}");
	let back = dot_to_mata(&dot);
	assert_eq!(status(&back), 0, "{}", stderr(&back));
	assert_eq!(stdout(&back), mata_text);

	let later_number = dot_to_mata(
		"digraph finiteAutomaton {\n0 -> 1 [label=\"a\"];\n1 -> 0 [label=\"0\"];\ni0 -> 0;\n}\n",
	);
	assert_eq!(status(&later_number), 0, "{}", stderr(&later_number));
	let text = stdout(&later_number);
	assert!(
		text.contains("q0 a q1\n") && text.contains("q1 0 q0\n"),
		"{text}"
	);
}

#[test]
fn a_dot_input_holds_one_graph() {
	let workspace = Workspace::new("dot-graphs");
	let nfa = workspace.file("a.mata", NFA);
	let two = stdout(&mata(["convert", &nfa, &nfa]));
	let output = dot_to_mata(&two);
	assert_eq!(status(&output), 2, "{}", stderr(&output));
	assert!(
		stderr(&output).contains("an input holds one graph"),
		"{}",
		stderr(&output)
	);
	let unclosed = dot_to_mata("digraph finiteAutomaton {\n0;\n");
	assert_eq!(status(&unclosed), 2, "{}", stderr(&unclosed));
}

#[test]
fn a_dot_round_trip_keeps_the_level_count_of_a_transducer() {
	let workspace = Workspace::new("dot-levels");
	for (name, nft) in [
		(
			"word.mata",
			"@NFT-explicit\n%Alphabet-auto\n%Initial q0\n%Final q0\n%Levels q0:0\n%LevelsNum 2\n",
		),
		(
			"empty.mata",
			"@NFT-explicit\n%Alphabet-auto\n%LevelsNum 2\n",
		),
	] {
		let path = workspace.file(name, nft);
		let dot = stdout(&mata(["convert", &path]));
		assert!(dot.contains("num_of_levels=2;"), "{dot}");
		let back = dot_to_mata(&dot);
		assert_eq!(status(&back), 0, "{}", stderr(&back));
		assert_eq!(stdout(&back), nft);
	}
}

#[test]
fn dot_labels_keep_names_with_commas_brackets_and_angles() {
	let workspace = Workspace::new("dot-names");
	let nfa = workspace.file(
		"a.mata",
		"@NFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final q1\nq0 a,b q1\nq0 [a-c] q1\nq0 <5> q1\n",
	);
	let mata_text = stdout(&mata(["convert", "--to", "mata", &nfa]));
	let back = dot_to_mata(&stdout(&mata(["convert", &nfa])));
	assert_eq!(status(&back), 0, "{}", stderr(&back));
	assert_eq!(stdout(&back), mata_text);
}

#[test]
fn dot_attributes_are_read_by_name_not_by_their_text() {
	let output =
		dot_to_mata("digraph finiteAutomaton {\n0 [tooltip=\"doublecircle\"];\ni0 -> 0;\n}\n");
	assert_eq!(status(&output), 2, "{}", stdout(&output));
	assert!(
		stderr(&output).contains("unsupported node attribute 'tooltip=doublecircle'"),
		"{}",
		stderr(&output)
	);
	let colored = dot_to_mata("digraph finiteAutomaton {\n0 -> 0 [label=\"a\", color=red];\n}\n");
	assert_eq!(status(&colored), 2, "{}", stdout(&colored));
}

#[test]
fn a_dot_edge_without_a_label_is_symbol_zero() {
	let output =
		dot_to_mata("digraph finiteAutomaton {\n0 -> 1;\n1 -> 0 [label=\"a\"];\ni0 -> 0;\n}\n");
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	let text = stdout(&output);
	assert!(
		text.contains("q0 0 q1\n") && text.contains("q1 a q0\n"),
		"{text}"
	);
	let empty = dot_to_mata("digraph finiteAutomaton {\n0 -> 1 [label=\"\"];\n}\n");
	assert_eq!(status(&empty), 2, "{}", stdout(&empty));
}

#[test]
fn a_dot_label_decodes_its_escapes() {
	let dot = "digraph finiteAutomaton {\nnode [shape=circle];\n0 -> 0 [label=\"a\\\"b\"];\n\
		node [shape=none, label=\"\"];\ni0 -> 0;\n}\n";
	let again = mata_with_stdin(["convert", "--from", "dot", "--to", "dot", "-"], dot);
	assert_eq!(status(&again), 0, "{}", stderr(&again));
	assert_eq!(stdout(&again), dot);
	let unknown = dot_to_mata("digraph finiteAutomaton {\n0 -> 0 [label=\"a\\nb\"];\n}\n");
	assert_eq!(status(&unknown), 2, "{}", stdout(&unknown));
	assert!(
		stderr(&unknown).contains("unsupported escape"),
		"{}",
		stderr(&unknown)
	);
}

// Review of #1039: operations and literals

#[test]
fn an_nfa_literal_reads_epsilon_as_an_epsilon_transition() {
	let output = mata([
		"script",
		"run",
		"--expression",
		"(define a (nfa:literal :initial (q0) :final (q1) :transitions ((q0 epsilon q1))))\n\
		 (print (nfa:in-language? a (word)))\n(print (nfa:in-language? a (word \"epsilon\")))",
	]);
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	assert_eq!(stdout(&output), "true\nfalse\n");
}

#[test]
fn an_nft_literal_without_state_levels_has_its_states() {
	let output = mata([
		"script",
		"run",
		"--expression",
		"(print (nft:number-of-states (nft:literal :levels 1 :initial (q0) :final (q0))))\n\
		 (define t (nft:literal :levels 1 :initial (q0) :final (q1) :transitions ((q0 a q1))))\n\
		 (print (nft:in-language-by-levels? t (list (word \"a\"))))",
	]);
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	assert_eq!(stdout(&output), "1\ntrue\n");
}

#[test]
fn nft_determinism_sees_dont_care_overlap_a_symbol() {
	let workspace = Workspace::new("nft-dont-care");
	let t = workspace.file(
		"t.mata",
		"@NFT-explicit\n%Alphabet-auto\n%LevelsNum 1\n%Initial q0\n%Final q1 q2\nq0 a q1\nq0 <dntcr> q2\n",
	);
	let output = mata([
		"script",
		"run",
		"--expression",
		&format!(
			"(define t (nft:load \"{t}\"))\n(print (nfa:deterministic? (nft:to-nfa t)))\n\
			 (print (nft:deterministic? t))"
		),
	]);
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	assert_eq!(stdout(&output), "true\nfalse\n");
}

#[test]
fn a_single_word_nft_of_the_empty_word_relates_the_empty_word() {
	let output = mata([
		"script",
		"run",
		"--expression",
		"(define t (nft:create-single-word-nft (word)))\n\
		 (print (nft:in-language-by-levels? t (list (word))))\n\
		 (print (nft:in-language-by-levels? t (list (word \"a\"))))",
	]);
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	assert_eq!(stdout(&output), "true\nfalse\n");
}

#[test]
fn a_witness_is_a_word_that_the_checker_types_as_one() {
	let script = "(define a (nfa:literal :initial (q0) :final (q1) :transitions ((q0 a q1) (q0 b q1))))\n\
		 (define b (nfa:literal :initial (q0) :final (q1) :transitions ((q0 a q1))))\n\
		 (print (witness (nfa:included? a b)))\n(print (witness (nfa:universal? b)))\n\
		 (print (witness (nfa:language-empty? a)))\n(print (witness (nfa:included? b a)))";
	let output = mata(["script", "run", "--expression", script]);
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	assert_eq!(stdout(&output), "b\nε\na\nnone\n");
	let narrowed = format!("{script}\n(print (as (witness (nfa:included? a b)) Run))");
	let output = mata(["script", "check", "--expression", &narrowed]);
	assert_eq!(status(&output), 2);
	assert!(
		stderr(&output).contains("expected Run, found (Option Word)"),
		"{}",
		stderr(&output)
	);
}

#[test]
fn witnesses_are_kept_only_when_the_script_asks_for_them() {
	let output = mata([
		"script",
		"run",
		"--expression",
		"(define a (nfa:literal :initial (q0) :final (q1) :transitions ((q0 a q1) (q0 b q1))))\n\
		 (define b (nfa:literal :initial (q0) :final (q1) :transitions ((q0 a q1))))\n\
		 (print (nfa:included? a b))\n(print (get-witness))\n\
		 (set-option :produce-witnesses true)\n\
		 (print (nfa:included? a b))\n(print (get-witness))",
	]);
	assert_eq!(status(&output), 0, "{}", stderr(&output));
	assert_eq!(stdout(&output), "false\nnone\nfalse\nb\n");
}
