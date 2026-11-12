//! Builds (or finds) `libmata`, compiles the cxx bridge with the shim, and links both.

use std::env;
use std::fs;
use std::path::{Path, PathBuf};
use std::process;

fn main() {
	println!("cargo::rerun-if-changed=src");
	println!("cargo::rerun-if-changed=cxx");
	for var in [
		"MATA_DIR",
		"MATA_SOURCE_DIR",
		"MATA_CMAKE_BUILD_TYPE",
		"DOCS_RS",
		// Read by the cmake crate, which does not ask cargo to rerun when they change.
		"CMAKE",
		"CMAKE_GENERATOR",
		"CMAKE_TOOLCHAIN_FILE",
		"CMAKE_PREFIX_PATH",
		"CC",
		"CFLAGS",
		"CXX",
		"CXXFLAGS",
	] {
		println!("cargo::rerun-if-env-changed={var}");
	}

	// Documentation needs no native code.
	if env::var_os("DOCS_RS").is_some() {
		println!("cargo::rustc-env=MATA_LIBRARY_VERSION=unknown");
		return;
	}

	let build_type = env::var("MATA_CMAKE_BUILD_TYPE").unwrap_or_else(|_| "Release".to_owned());
	let emscripten = env::var("CARGO_CFG_TARGET_OS").is_ok_and(|os| os == "emscripten");
	let (prefix, lib) = match env::var_os("MATA_DIR") {
		Some(dir) => installed_mata(PathBuf::from(dir)),
		None => {
			let prefix = build_mata(&build_type, emscripten);
			let lib = prefix.join("lib");
			(prefix, lib)
		}
	};
	let include = prefix.join("include");
	check_state_bits(&include);
	check_thread_safe(&include);
	println!(
		"cargo::rustc-env=MATA_LIBRARY_VERSION={}",
		library_version(&lib)
	);

	println!("cargo::metadata=root={}", prefix.display());
	println!("cargo::metadata=include={}", include.display());

	// Let crates that depend on this one include Mata's headers from their own bridges.
	cxx_build::CFG
		.exported_header_dirs
		.push(Box::leak(include.clone().into_boxed_path()));

	let mut shim = cxx_build::bridges(["src/lib.rs", "src/nft.rs"]);
	shim.files(["cxx/common.cc", "cxx/shim.cc", "cxx/nft.cc"])
		.include(&include)
		.std("c++23");
	// The shim must see Mata's debug assertions as libmata did.
	if !build_type.eq_ignore_ascii_case("Debug") {
		shim.define("NDEBUG", None);
	}
	// Toolchains that always define `_FORTIFY_SOURCE` (Nix's cc wrapper, hardened distributions) warn
	// about each file compiled at `-O0`; `-Og` silences them and stays debuggable.
	if env::var("OPT_LEVEL").is_ok_and(|level| level == "0") {
		shim.opt_level_str("g");
	}
	if emscripten {
		shim.flag(WASM_EXCEPTIONS);
	}
	shim.compile("mata-sys-shim");

	println!("cargo::rustc-link-search=native={}", lib.display());
	println!("cargo::rustc-link-lib=static=mata");
	if emscripten {
		// Rust links with `emcc`, which adds the C++ runtime only for C++.
		for runtime in ["c++", "c++abi", "exceptions.js"] {
			println!("cargo::rustc-link-lib={runtime}");
		}
	}
}

/// Mata reports errors with C++ exceptions, which Emscripten catches only with this flag.
const WASM_EXCEPTIONS: &str = "-fwasm-exceptions";

/// Builds and installs Mata with CMake into `OUT_DIR`, returning the install prefix.
fn build_mata(build_type: &str, emscripten: bool) -> PathBuf {
	// The package on crates.io carries the sources in `mata/`; in the repository, this crate lives in
	// `bindings/rust/mata-sys`.
	let source = env::var_os("MATA_SOURCE_DIR").map_or_else(
		|| {
			let manifest = PathBuf::from(
				env::var("CARGO_MANIFEST_DIR").expect("cargo sets CARGO_MANIFEST_DIR"),
			);
			let packaged = manifest.join("mata");
			if packaged.join("CMakeLists.txt").is_file() {
				packaged
			} else {
				manifest.join("../../..")
			}
		},
		PathBuf::from,
	);
	if !source.join("CMakeLists.txt").is_file() {
		fail(&format!(
			"no Mata sources (CMakeLists.txt) in {}: set MATA_SOURCE_DIR to a Mata source tree, or MATA_DIR \
             to an installed Mata",
			source.display()
		));
	}
	for entry in [
		"CMakeLists.txt",
		"VERSION",
		"cmake",
		"src",
		"include",
		"3rdparty",
	] {
		println!("cargo::rerun-if-changed={}", source.join(entry).display());
	}

	let mut config = cmake::Config::new(&source);
	if emscripten {
		config.cflag(WASM_EXCEPTIONS).cxxflag(WASM_EXCEPTIONS);
		if !toolchain_file_set() {
			config.define("CMAKE_TOOLCHAIN_FILE", emscripten_toolchain());
		}
	}
	config
		.profile(build_type)
		.define("BUILD_TESTING", "OFF")
		.define("MATA_BUILD_EXAMPLES", "OFF")
		.define("MATA_USE_CCACHE", "OFF")
		.define("MATA_INSTALL", "ON")
		// The bridge passes states as `u32`.
		.define("MATA_STATE_BITS", "32")
		// Mata fetches Catch2 from GitHub when tests are on; never touch the network.
		.define("FETCHCONTENT_FULLY_DISCONNECTED", "ON")
		// GNUInstallDirs would pick `lib64` on some distributions.
		.define("CMAKE_INSTALL_LIBDIR", "lib")
		.define("CMAKE_INSTALL_INCLUDEDIR", "include")
		.build()
}

/// Whether a CMake toolchain file was given, in a variable that the cmake crate reads.
fn toolchain_file_set() -> bool {
	let target = env::var("TARGET").expect("cargo sets TARGET");
	let vars = [
		"CMAKE_TOOLCHAIN_FILE".to_owned(),
		"TARGET_CMAKE_TOOLCHAIN_FILE".to_owned(),
		format!("CMAKE_TOOLCHAIN_FILE_{target}"),
		format!("CMAKE_TOOLCHAIN_FILE_{}", target.replace('-', "_")),
	];
	for var in &vars {
		println!("cargo::rerun-if-env-changed={var}");
	}
	vars.iter().any(|var| env::var_os(var).is_some())
}

/// The CMake toolchain file of the Emscripten whose `emcc` is on `PATH`.
fn emscripten_toolchain() -> PathBuf {
	println!("cargo::rerun-if-env-changed=EMSDK");
	env::var_os("PATH")
		.iter()
		.flat_map(env::split_paths)
		.map(|dir| dir.join("emcc"))
		.find(|emcc| emcc.is_file())
		.and_then(|emcc| fs::canonicalize(emcc).ok())
		.and_then(|emcc| {
			let file = emcc
				.parent()?
				.join("cmake/Modules/Platform/Emscripten.cmake");
			file.is_file().then_some(file)
		})
		.unwrap_or_else(|| {
			fail(
				"building for Emscripten needs its `emcc` on PATH (run `source emsdk_env.sh` of an \
                 installed emsdk), or CMAKE_TOOLCHAIN_FILE set to Emscripten's `Emscripten.cmake`",
			)
		})
}

/// Checks that `prefix` holds an installed Mata; returns it and the directory of `libmata.a`.
fn installed_mata(prefix: PathBuf) -> (PathBuf, PathBuf) {
	let lib = ["lib", "lib64"]
		.into_iter()
		.map(|dir| prefix.join(dir))
		.find(|dir| dir.join("libmata.a").is_file());
	let (Some(lib), true) = (lib, prefix.join("include/mata/nfa/nfa.hh").is_file()) else {
		fail(&format!(
			"MATA_DIR={} is not a Mata installation (expected lib/libmata.a or lib64/libmata.a, and \
             include/mata/nfa/nfa.hh)",
			prefix.display()
		));
	};
	println!(
		"cargo::rerun-if-changed={}",
		lib.join("libmata.a").display()
	);
	println!(
		"cargo::rerun-if-changed={}",
		prefix.join("include").display()
	);
	(prefix, lib)
}

/// The version the installed CMake package of Mata declares, or `unknown` when it has no version file.
fn library_version(lib: &Path) -> String {
	let file = lib.join("cmake/mata/mataConfigVersion.cmake");
	println!("cargo::rerun-if-changed={}", file.display());
	fs::read_to_string(&file)
		.ok()
		.and_then(|text| {
			text.lines().find_map(|line| {
				line.trim()
					.strip_prefix("set(PACKAGE_VERSION \"")?
					.strip_suffix("\")")
					.map(str::to_owned)
			})
		})
		.unwrap_or_else(|| "unknown".to_owned())
}

/// Fails unless Mata was configured with 32-bit states (`MATA_STATE_BITS` in `mata/config.hh`).
fn check_state_bits(include: &Path) {
	let config = fs::read_to_string(include.join("mata/config.hh")).unwrap_or_default();
	if !config.contains("#define MATA_STATE_BITS 32") {
		fail(&format!(
			"the Mata in {} does not have 32-bit states: mata-sys needs a Mata configured with \
             MATA_STATE_BITS=32 (the default since pull request #939)",
			include.display()
		));
	}
}

/// Fails if the headers still have the static buffers that all automata shared before pull request #868, which
/// would make the `Send` and `Sync` of the `mata` crate unsound.
fn check_thread_safe(include: &Path) {
	let shared_buffers = [
		("mata/nfa/delta.hh", "static SymbolPost symbol_post"),
		("mata/utils/ord-vector.hh", "static OrdVector tmp"),
	];
	if let Some((file, _)) = shared_buffers.into_iter().find(|(file, buffer)| {
		fs::read_to_string(include.join(file)).is_ok_and(|text| text.contains(buffer))
	}) {
		fail(&format!(
			"the Mata in {} shares static buffers between automata ({file}), which makes it unsafe to use \
             from several threads: use a Mata with pull request #868",
			include.display()
		));
	}
}

fn fail(message: &str) -> ! {
	println!("cargo::error={message}");
	process::exit(1);
}
