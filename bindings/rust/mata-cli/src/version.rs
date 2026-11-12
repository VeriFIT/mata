// Copyright (c) 2026 VeriFIT. GNU GPL v3. See LICENSE in the project root.

//! The Mata Script language version, which a script declares in its `(mata-script …)` header, and the
//! optional features a script can require.

use std::fmt;

use crate::Error;

/// A language version, `MAJOR.MINOR.PATCH`.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub(crate) struct Version {
	pub(crate) major: u32,
	pub(crate) minor: u32,
	pub(crate) patch: u32,
}

/// The language version this interpreter implements.
pub(crate) const LANGUAGE: Version = Version {
	major: 0,
	minor: 1,
	patch: 0,
};

/// The first language version, which introduced the original catalogue of operations.
pub(crate) const FIRST: Version = Version {
	major: 0,
	minor: 1,
	patch: 0,
};

impl Version {
	/// Reads `MAJOR.MINOR.PATCH`, three decimal numbers.
	pub(crate) fn parse(text: &str) -> Option<Version> {
		let mut parts = text.split('.');
		let mut number = || {
			let part = parts.next()?;
			(!part.is_empty() && part.bytes().all(|byte| byte.is_ascii_digit()))
				.then(|| part.parse().ok())
				.flatten()
		};
		let version = Version {
			major: number()?,
			minor: number()?,
			patch: number()?,
		};
		parts.next().is_none().then_some(version)
	}

	/// Whether an interpreter of this version runs a script that declares `script`. A patch version
	/// never changes what a script means. Before 1.0.0 every minor version may break, so only the same
	/// minor version runs; from 1.0.0, a minor version only adds.
	pub(crate) fn runs(self, script: Version) -> bool {
		if self.major == 0 {
			script.major == 0 && script.minor == self.minor
		} else {
			script.major == self.major && script.minor <= self.minor
		}
	}

	/// The versions this interpreter runs, for an error message.
	pub(crate) fn supported(self) -> String {
		if self.major == 0 {
			format!("{}.{}.x", self.major, self.minor)
		} else {
			format!("{}.0.0 to {}.{}.x", self.major, self.major, self.minor)
		}
	}

	/// Whether a name added in this version is visible to a script that declares `script`. Names come
	/// with minor versions; a patch version adds none.
	pub(crate) fn visible_in(self, script: Version) -> bool {
		(self.major, self.minor) <= (script.major, script.minor)
	}
}

/// Reads the version of the `(mata-script "VERSION" …)` header without parsing the rest of the script, so
/// a script written for another version is rejected before syntax this interpreter does not know is
/// read. `None` when the script has no header.
///
/// # Errors
///
/// The version is not `MAJOR.MINOR.PATCH`, or this interpreter does not run it.
pub(crate) fn declared(source: &str) -> Result<Option<Version>, Error> {
	let mut rest = source;
	if rest.starts_with("#!") {
		rest = rest.find('\n').map_or("", |end| &rest[end..]);
	}
	rest = skip_layout(rest);
	let Some(after) = rest.strip_prefix('(') else {
		return Ok(None);
	};
	let Some(after) = skip_layout(after).strip_prefix("mata-script") else {
		return Ok(None);
	};
	if after.starts_with(crate::lexer::is_name_char) {
		return Ok(None);
	}
	let after = skip_layout(after);
	let start = source.len() - after.len();
	let (line, column) = place(source, start);
	let text = after
		.strip_prefix('"')
		.and_then(|quoted| quoted.split_once('"'))
		.map(|(text, _)| text)
		.ok_or_else(|| {
			Error::at(
				"mata-script needs the language version as a string, such as \"0.1.0\"".into(),
				line,
				column,
			)
		})?;
	supported(text, line, column).map(Some)
}

/// Whitespace and `;` comments, which may stand between any two tokens of the header.
fn skip_layout(mut rest: &str) -> &str {
	loop {
		rest = rest.trim_start();
		if !rest.starts_with(';') {
			return rest;
		}
		rest = rest.find('\n').map_or("", |end| &rest[end..]);
	}
}

/// Reads the version `text` of a header at `line` and `column`.
///
/// # Errors
///
/// The version is not `MAJOR.MINOR.PATCH`, or this interpreter does not run it.
pub(crate) fn supported(text: &str, line: usize, column: usize) -> Result<Version, Error> {
	let version = Version::parse(text).ok_or_else(|| {
		Error::at(
			format!("invalid language version '{text}'; expected MAJOR.MINOR.PATCH"),
			line,
			column,
		)
	})?;
	if !LANGUAGE.runs(version) {
		return Err(Error::at(
			format!(
				"the script needs Mata Script {version}; this mata runs Mata Script {}",
				LANGUAGE.supported()
			),
			line,
			column,
		));
	}
	Ok(version)
}

/// The one-based line and column of the byte `offset` of `source`.
fn place(source: &str, offset: usize) -> (usize, usize) {
	let before = &source[..offset];
	let line = before.matches('\n').count() + 1;
	let column = before
		.rsplit('\n')
		.next()
		.map_or(0, |text| text.chars().count())
		+ 1;
	(line, column)
}

impl fmt::Display for Version {
	fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
		write!(formatter, "{}.{}.{}", self.major, self.minor, self.patch)
	}
}

/// An optional part of the language that a header can require with `:requires`.
pub(crate) struct Feature {
	pub(crate) name: &'static str,
	pub(crate) implemented: bool,
}

pub(crate) const FEATURES: &[Feature] = &[
	Feature {
		name: "hints",
		implemented: true,
	},
	Feature {
		name: "witnesses",
		implemented: true,
	},
	Feature {
		name: "transducers",
		implemented: true,
	},
	Feature {
		name: "loops-and-recursion",
		implemented: false,
	},
	Feature {
		name: "optimizer",
		implemented: false,
	},
	Feature {
		name: "trace",
		implemented: false,
	},
	Feature {
		name: "automaton",
		implemented: false,
	},
	Feature {
		name: "strings",
		implemented: true,
	},
	Feature {
		name: "intermediate",
		implemented: false,
	},
];

/// Why a header's `:requires` cannot be met, or `None` when this interpreter has the feature.
pub(crate) fn missing_feature(name: &str) -> Option<String> {
	match FEATURES.iter().find(|feature| feature.name == name) {
		None => Some(format!("unknown feature '{name}'")),
		Some(feature) if !feature.implemented => Some(format!(
			"the script requires feature '{name}', which this mata does not implement"
		)),
		Some(_) => None,
	}
}

#[cfg(test)]
mod tests {
	use super::*;

	fn version(major: u32, minor: u32, patch: u32) -> Version {
		Version {
			major,
			minor,
			patch,
		}
	}

	#[test]
	fn a_version_is_three_decimal_numbers() {
		assert_eq!(Version::parse("1.12.3"), Some(version(1, 12, 3)));
		for text in [
			"1", "1.2", "1.2.", ".1.2", "1.2.3.4", "a.1.2", "+1.2.3", "1.-2.0", "",
		] {
			assert_eq!(Version::parse(text), None, "{text}");
		}
	}

	#[test]
	fn before_one_only_the_same_minor_runs_and_after_it_older_minors_do() {
		let interpreter = version(0, 1, 0);
		assert!(interpreter.runs(version(0, 1, 0)));
		assert!(interpreter.runs(version(0, 1, 7)));
		assert!(!interpreter.runs(version(0, 0, 9)));
		assert!(!interpreter.runs(version(0, 2, 0)));
		assert!(!interpreter.runs(version(1, 1, 0)));
		let interpreter = version(1, 4, 2);
		assert!(interpreter.runs(version(1, 0, 0)));
		assert!(interpreter.runs(version(1, 4, 9)));
		assert!(!interpreter.runs(version(1, 5, 0)));
		assert!(!interpreter.runs(version(2, 0, 0)));
		assert!(!interpreter.runs(version(0, 4, 0)));
	}
}

#[cfg(test)]
mod visibility_tests {
	use super::*;

	#[test]
	fn a_name_added_in_a_minor_version_is_hidden_from_older_scripts_only() {
		let added = Version {
			major: 0,
			minor: 2,
			patch: 0,
		};
		let older = Version {
			major: 0,
			minor: 1,
			patch: 9,
		};
		let same = Version {
			major: 0,
			minor: 2,
			patch: 0,
		};
		assert!(!added.visible_in(older));
		assert!(added.visible_in(same));
		assert!(FIRST.visible_in(LANGUAGE));
	}

	#[test]
	fn the_header_version_is_read_before_the_rest_of_the_script() {
		assert_eq!(declared("(print 1)").unwrap(), None);
		assert_eq!(
			declared("#!/usr/bin/env mata\n; a comment\n(mata-script \"0.1.3\" :profile core)")
				.unwrap(),
			Some(Version {
				major: 0,
				minor: 1,
				patch: 3
			})
		);
		let error = declared("\n  (mata-script \"9.0.0\" (unparsable").unwrap_err();
		assert_eq!(error.at, Some((2, 16)));
		assert!(
			error.message.contains("Mata Script 9.0.0"),
			"{}",
			error.message
		);
	}
}
