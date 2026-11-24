// Copyright (c) 2026 VeriFIT. GNU GPL v3. See LICENSE in the project root.

//! Link options of `mata-playground-script`, the WebAssembly module of the website playground's
//! Script mode (`src/bin/mata-playground-script.rs`).

fn main() {
	println!("cargo::rerun-if-changed=build.rs");
	let emscripten = std::env::var("CARGO_CFG_TARGET_OS").is_ok_and(|os| os == "emscripten");
	if !emscripten || std::env::var_os("CARGO_FEATURE_WEB").is_none() {
		return;
	}
	for option in [
		// An ES module exporting a factory, like the C++ module of the playground (bindings/wasm).
		"-sMODULARIZE=1",
		"-sEXPORT_ES6=1",
		"-sEXPORT_NAME=createMataPlaygroundScript",
		"-sENVIRONMENT=web,worker,node",
		// The worker calls the entry point once per run, so the empty `main` is not run.
		"-sINVOKE_RUN=0",
		"-sEXPORTED_FUNCTIONS=_mata_script_run",
		// The worker writes the inputs to the in-memory file system.
		"-sEXPORTED_RUNTIME_METHODS=ccall,FS",
		"-sFORCE_FILESYSTEM=1",
		"-sMAXIMUM_MEMORY=2GB",
	] {
		println!("cargo::rustc-link-arg-bin=mata-playground-script={option}");
	}
}
