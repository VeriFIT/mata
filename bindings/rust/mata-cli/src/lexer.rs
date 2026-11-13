// Copyright (c) 2026 VeriFIT. GNU GPL v3. See LICENSE in the project root.

use crate::Error;

/// A lexical token and its one-based source position.
#[derive(Clone, Debug, PartialEq, Eq)]
pub(crate) struct Token {
	pub(crate) kind: TokenKind,
	pub(crate) line: usize,
	pub(crate) column: usize,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub(crate) enum TokenKind {
	LeftParen,
	RightParen,
	Name(String),
	Keyword(String),
	Input(String),
	Integer(i64),
	String(String),
	Bool(bool),
	None,
	Eof,
}

/// Tokenizes `source`, without interpreting backslashes in strings. A first line starting with `#!` is
/// skipped, so a script can be an executable file.
pub(crate) fn lex(source: &str) -> Result<Vec<Token>, Error> {
	let mut lexer = Lexer {
		source,
		offset: 0,
		line: 1,
		column: 1,
	};
	if source.starts_with("#!") {
		while lexer.peek().is_some_and(|c| c != '\n') {
			lexer.bump();
		}
	}
	lexer.tokens()
}

struct Lexer<'a> {
	source: &'a str,
	offset: usize,
	line: usize,
	column: usize,
}

impl Lexer<'_> {
	fn tokens(mut self) -> Result<Vec<Token>, Error> {
		let mut tokens = Vec::new();
		loop {
			self.skip_layout();
			let (line, column) = (self.line, self.column);
			let kind = match self.peek() {
				None => TokenKind::Eof,
				Some('(') => {
					self.bump();
					TokenKind::LeftParen
				}
				Some(')') => {
					self.bump();
					TokenKind::RightParen
				}
				Some('"') => TokenKind::String(self.string(line, column)?),
				Some(':') => {
					self.bump();
					TokenKind::Keyword(self.symbol("keyword", line, column)?)
				}
				Some('$') => {
					self.bump();
					TokenKind::Input(self.symbol("input reference", line, column)?)
				}
				Some(c) if c.is_ascii_digit() => {
					let word = self.take_name();
					let value = word.parse::<i64>().map_err(|_| {
						Error::at(format!("invalid integer '{word}'"), line, column)
					})?;
					TokenKind::Integer(value)
				}
				Some(c) if is_name_char(c) => {
					let name = self.take_name();
					match name.as_str() {
						"true" => TokenKind::Bool(true),
						"false" => TokenKind::Bool(false),
						"none" => TokenKind::None,
						_ => TokenKind::Name(name),
					}
				}
				Some(c) => {
					return Err(Error::at(
						format!("unexpected character '{c}'"),
						line,
						column,
					));
				}
			};
			let done = kind == TokenKind::Eof;
			tokens.push(Token { kind, line, column });
			if done {
				return Ok(tokens);
			}
		}
	}

	fn symbol(&mut self, what: &str, line: usize, column: usize) -> Result<String, Error> {
		let name = self.take_name();
		if name.is_empty() {
			Err(Error::at(format!("expected {what} name"), line, column))
		} else {
			Ok(name)
		}
	}

	fn string(&mut self, line: usize, column: usize) -> Result<String, Error> {
		self.bump();
		let mut value = String::new();
		while let Some(c) = self.bump() {
			match c {
				'"' if self.peek() == Some('"') => {
					self.bump();
					value.push('"');
				}
				'"' => return Ok(value),
				_ => value.push(c),
			}
		}
		// A string may span lines, so the REPL reads on.
		let mut error = Error::at("unterminated string".to_owned(), line, column);
		error.incomplete = true;
		Err(error)
	}

	fn take_name(&mut self) -> String {
		let start = self.offset;
		while self.peek().is_some_and(is_name_char) {
			self.bump();
		}
		self.source[start..self.offset].to_owned()
	}

	fn skip_layout(&mut self) {
		loop {
			while self.peek().is_some_and(char::is_whitespace) {
				self.bump();
			}
			if self.peek() != Some(';') {
				break;
			}
			while self.peek().is_some_and(|c| c != '\n') {
				self.bump();
			}
		}
	}

	fn peek(&self) -> Option<char> {
		self.source[self.offset..].chars().next()
	}

	fn bump(&mut self) -> Option<char> {
		let c = self.peek()?;
		self.offset += c.len_utf8();
		if c == '\n' {
			self.line += 1;
			self.column = 1;
		} else {
			self.column += 1;
		}
		Some(c)
	}
}

pub(crate) fn is_name_char(c: char) -> bool {
	c.is_alphanumeric()
		|| matches!(
			c,
			'-' | '_' | '?' | '!' | '*' | '+' | '/' | '<' | '=' | '>' | '.' | ':'
		)
}

#[cfg(test)]
mod tests {
	use super::*;

	#[test]
	fn strings_do_not_use_backslash_escapes() {
		let tokens = lex(r#"(echo "a\|b and ""quoted""") ; comment"#).unwrap();
		assert_eq!(
			tokens[2].kind,
			TokenKind::String("a\\|b and \"quoted\"".into())
		);
		assert_eq!(tokens.last().unwrap().kind, TokenKind::Eof);
	}

	#[test]
	fn broken_string_has_its_start_position() {
		assert_eq!(lex("\n \"unfinished").unwrap_err().at, Some((2, 2)));
	}
}
