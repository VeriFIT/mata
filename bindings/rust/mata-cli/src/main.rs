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

/// The smallest worker stack worth reserving instead of running on the calling thread.
#[cfg(not(target_arch = "wasm32"))]
const MIN_STACK_SIZE: usize = 16 * 1024 * 1024;

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
/// even `mata --help` would die with no message. A smaller reservation still leaves the limits
/// most of their margin, so the request shrinks to a quarter until one fits: one that only just
/// fits would leave the heap no room. Only below [`MIN_STACK_SIZE`], little more than the default
/// stack, does the interpreter run on the calling thread.
#[cfg(not(target_arch = "wasm32"))]
fn run_on_worker_stack() -> mata_cli::cli::Exit {
	let mut size = STACK_SIZE;
	loop {
		match std::thread::Builder::new()
			.stack_size(size)
			.spawn(mata_cli::cli::main)
		{
			Ok(thread) => {
				return thread
					.join()
					.map_err(|_| std::io::Error::other("the interpreter panicked"))
					.unwrap_or(mata_cli::cli::Exit::Usage);
			}
			Err(_) if size > MIN_STACK_SIZE => size /= 4,
			Err(_) => return mata_cli::cli::main(),
		}
	}
}
