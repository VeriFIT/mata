//! Runs the fuzz checks on saved inputs, printing every query and its time.

fn main() -> std::io::Result<()> {
	for path in std::env::args().skip(1) {
		eprintln!("== {path}");
		let input = std::fs::read(&path)?;
		if let Err(error) = lazymat_fuzz::check(&input, true) {
			eprintln!("not a formula: {error}");
		}
	}

	Ok(())
}
