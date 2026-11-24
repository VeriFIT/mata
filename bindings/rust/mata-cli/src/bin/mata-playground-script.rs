// Copyright (c) 2026 VeriFIT. GNU GPL v3. See LICENSE in the project root.

//! The WebAssembly module of the website playground's Script mode, built for
//! `wasm32-unknown-emscripten` with the link options of `build.rs`. The page's worker calls
//! [`mata_script_run`] through Emscripten's `ccall`; `main` is not run.

use std::ffi::{CStr, c_char};
use std::time::Duration;

use mata_cli::cli::Exit;

/// Runs a script with [`mata_cli::web::run`] and returns the exit status of `mata script run`.
/// `inputs` and `parameters` hold one `NAME=VALUE` per line; a `time_limit_ms` of 0 sets no limit.
///
/// # Safety
///
/// `script`, `inputs` and `parameters` point to NUL-terminated strings.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn mata_script_run(
	script: *const c_char,
	inputs: *const c_char,
	parameters: *const c_char,
	time_limit_ms: u32,
) -> i32 {
	let [script, inputs, parameters] = [script, inputs, parameters].map(|text| {
		// SAFETY: the caller passes NUL-terminated strings.
		unsafe { CStr::from_ptr(text) }.to_string_lossy()
	});
	let time_limit = (time_limit_ms > 0).then(|| Duration::from_millis(time_limit_ms.into()));
	match mata_cli::web::run(&script, &inputs, &parameters, time_limit) {
		Exit::Success => 0,
		Exit::False | Exit::Assertion => 1,
		Exit::Usage => 2,
	}
}

fn main() {}
