// Copyright (c) 2026 VeriFIT. GNU GPL v3. See LICENSE in the project root.

//! The `mata` executable: maps `cli::Exit` to the process exit code.
fn main() {
	let exit = mata_cli::cli::main();
	std::process::exit(match exit {
		mata_cli::cli::Exit::Success => 0,
		mata_cli::cli::Exit::False | mata_cli::cli::Exit::Assertion => 1,
		mata_cli::cli::Exit::Usage => 2,
	});
}
