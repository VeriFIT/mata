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
	let exit = run_on_worker_stack();
	#[cfg(target_arch = "wasm32")]
	let exit = mata_cli::cli::main();
	std::process::exit(match exit {
		mata_cli::cli::Exit::Success => 0,
		mata_cli::cli::Exit::False | mata_cli::cli::Exit::Assertion => 1,
		mata_cli::cli::Exit::Usage => 2,
	});
}

/// Runs the interpreter on a worker thread with [`STACK_SIZE`] of stack, so that the depth limits
/// report their errors before the stack runs out. The stack is a reservation, not committed
/// memory, but a process whose address space is smaller than the reservation cannot make it, and
/// even `mata --help` would die with no message. Running on the default stack instead leaves the
/// limits one 8 MiB stack away from an overflow, so the reservation is tried first and given up
/// only when the process cannot afford it.
#[cfg(not(target_arch = "wasm32"))]
fn run_on_worker_stack() -> mata_cli::cli::Exit {
	match std::thread::Builder::new()
		.stack_size(STACK_SIZE)
		.spawn(mata_cli::cli::main)
	{
		Ok(thread) => thread
			.join()
			.map_err(|_| std::io::Error::other("the interpreter panicked"))
			.unwrap_or(mata_cli::cli::Exit::Usage),
		Err(_) => mata_cli::cli::main(),
	}
}
