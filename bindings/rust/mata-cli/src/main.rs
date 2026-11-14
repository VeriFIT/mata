// Copyright (c) 2026 VeriFIT. GNU GPL v3. See LICENSE in the project root.

//! The `mata` executable: maps `cli::Exit` to the process exit code.

/// The stack the interpreter runs on, off WebAssembly. The parser, the checker, the evaluator and
/// the drop of nested values each walk the depth of a script's nesting, which the limits of
/// `mata-cli` report as errors long before the stack runs out, but the error has to arrive before
/// the overflow, and the default main-thread stack (8 MiB on Linux) sits too close to where the
/// debug build reports a nested `let`. The stack is a reservation; pages are only touched when a
/// script actually nests that deep.
#[cfg(not(target_arch = "wasm32"))]
const STACK_SIZE: usize = 1024 * 1024 * 1024;

fn main() {
	#[cfg(not(target_arch = "wasm32"))]
	let exit = {
		std::thread::Builder::new()
			.stack_size(STACK_SIZE)
			.spawn(mata_cli::cli::main)
			.and_then(|thread| {
				thread
					.join()
					.map_err(|_| std::io::Error::other("the interpreter panicked"))
			})
			.unwrap_or(mata_cli::cli::Exit::Usage)
	};
	#[cfg(target_arch = "wasm32")]
	let exit = mata_cli::cli::main();
	std::process::exit(match exit {
		mata_cli::cli::Exit::Success => 0,
		mata_cli::cli::Exit::False | mata_cli::cli::Exit::Assertion => 1,
		mata_cli::cli::Exit::Usage => 2,
	});
}
