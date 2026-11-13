//! The fuzz target of [`check`](lazymat_fuzz::check).

#![no_main]

use libfuzzer_sys::{Corpus, fuzz_target};

fuzz_target!(|input: &[u8]| -> Corpus {
	match lazymat_fuzz::check(input, false) {
		Ok(()) => Corpus::Keep,
		Err(_) => Corpus::Reject,
	}
});
