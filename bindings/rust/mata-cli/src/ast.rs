// Copyright (c) 2026 VeriFIT. GNU GPL v3. See LICENSE in the project root.

use crate::Error;
use crate::lexer::{self, Token, TokenKind};

pub(crate) type Expr = Spanned<Kind>;

#[derive(Clone, Debug)]
pub(crate) struct Spanned<T> {
	pub(crate) node: T,
	pub(crate) line: usize,
	pub(crate) column: usize,
}

impl<T> Spanned<T> {
	pub(crate) fn at(&self, message: String) -> Error {
		Error::at(message, self.line, self.column)
	}
}

#[derive(Clone, Debug)]
pub(crate) enum Kind {
	Bool(bool),
	Int(i64),
	String(String),
	Name(String),
	Input(String),
	None,
	List(Vec<Expr>),
	Call {
		function: Box<Expr>,
		arguments: Vec<Expr>,
		keywords: Vec<(String, Expr)>,
	},
	Definition {
		name: String,
		/// `None` for a value definition; `Some` holds the parameters, which may be empty.
		parameters: Option<Vec<Parameter>>,
		sort: Option<String>,
		body: Vec<Expr>,
	},
	Lambda {
		parameters: Vec<Parameter>,
		body: Vec<Expr>,
	},
	Literal {
		name: String,
		arguments: Vec<(String, Expr)>,
	},
	/// `(get-option :keyword)`.
	GetOption(String),
	/// `(get-info :keyword)`.
	GetInfo(String),
}

#[derive(Clone, Debug)]
pub(crate) enum FormKind {
	Define {
		name: String,
		/// `None` for `(define name value)`; `Some` holds the parameters of `(define (name …) …)`.
		parameters: Option<Vec<Parameter>>,
		sort: Option<String>,
		body: Vec<Expr>,
	},
	Declare {
		name: String,
		sort: String,
		variadic: bool,
	},
	/// `(declare-param name sort [default])`: a value given on the command line with `--param`.
	Parameter {
		name: String,
		sort: String,
		default: Option<Expr>,
	},
	/// `(mata-script "MAJOR.MINOR.PATCH" :requires (feature …) :profile name)`, only as the first form.
	/// `version::declared` reads the version before parsing; the parser checks it again.
	Header {
		requires: Vec<String>,
		profile: Option<String>,
	},
	SetOption {
		keyword: String,
		value: Expr,
	},
	SetInfo {
		keyword: String,
		value: Expr,
	},
	Push,
	Pop,
	Reset,
	Exit,
	Expression(Expr),
}

impl FormKind {
	/// The expressions the form evaluates: the body of a definition, the value of an option or
	/// information, the default of a parameter, or the expression itself.
	pub(crate) fn expressions(&self) -> Vec<&Expr> {
		match self {
			FormKind::Define { body, .. } => body.iter().collect(),
			FormKind::Parameter {
				default: Some(value),
				..
			}
			| FormKind::SetOption { value, .. }
			| FormKind::SetInfo { value, .. }
			| FormKind::Expression(value) => vec![value],
			_ => Vec::new(),
		}
	}
}

#[derive(Clone, Debug)]
pub(crate) struct Parameter {
	pub(crate) name: String,
	pub(crate) sort: Option<String>,
}

pub(crate) fn parse(source: &str) -> Result<Vec<Spanned<FormKind>>, Error> {
	Parser {
		tokens: lexer::lex(source)?,
		current: 0,
	}
	.script()
}

/// Parses forms from a REPL buffer. Returns `Err(None)` only for a trailing incomplete form.
pub(crate) fn parse_complete(source: &str) -> Result<Option<Vec<Spanned<FormKind>>>, Error> {
	match parse(source) {
		Ok(forms) => Ok(Some(forms)),
		Err(error) if error.incomplete => Ok(None),
		Err(error) => Err(error),
	}
}

/// Heads of the forms that are directives rather than applications.
const DIRECTIVES: [&str; 11] = [
	"mata-script",
	"define",
	"declare-input",
	"declare-inputs",
	"declare-param",
	"set-option",
	"set-info",
	"push",
	"pop",
	"reset",
	"exit",
];

struct Parser {
	tokens: Vec<Token>,
	current: usize,
}

impl Parser {
	fn script(&mut self) -> Result<Vec<Spanned<FormKind>>, Error> {
		let mut forms = Vec::new();
		while !matches!(self.peek(), TokenKind::Eof) {
			forms.push(self.form()?);
		}
		Ok(forms)
	}

	fn form(&mut self) -> Result<Spanned<FormKind>, Error> {
		let start = self.next();
		if start.kind != TokenKind::LeftParen {
			let (line, column) = (start.line, start.column);
			return self.expression_from(start).map(|e| Spanned {
				node: FormKind::Expression(e),
				line,
				column,
			});
		}
		let Some(name) = self.name_keyword() else {
			return self.form_expression(&start);
		};
		if !DIRECTIVES.contains(&name.as_str()) {
			return self.form_expression(&start);
		}
		let (line, column) = (start.line, start.column);
		self.current += 1; // the head token; directive handlers start after it
		let node = match name.as_str() {
			"mata-script" => self.header()?,
			"define" => self.define()?,
			"declare-input" => self.declaration(false)?,
			"declare-inputs" => self.declaration(true)?,
			"declare-param" => {
				let name = self.name()?;
				let sort = self.sort()?;
				let default = if self.peek() == &TokenKind::RightParen {
					None
				} else {
					Some(self.expression()?)
				};
				FormKind::Parameter {
					name,
					sort,
					default,
				}
			}
			"set-option" => {
				let keyword = self.keyword()?;
				let value = self.expression()?;
				FormKind::SetOption { keyword, value }
			}
			"set-info" => {
				let keyword = self.keyword()?;
				let value = self.expression()?;
				FormKind::SetInfo { keyword, value }
			}
			"push" => FormKind::Push,
			"pop" => FormKind::Pop,
			"reset" => FormKind::Reset,
			"exit" => FormKind::Exit,
			_ => unreachable!("checked against DIRECTIVES above"),
		};
		self.close(&name)?;
		Ok(Spanned { node, line, column })
	}

	/// `(mata-script "VERSION" {:requires (feature …) | :profile name})`, after the head.
	fn header(&mut self) -> Result<FormKind, Error> {
		let token = self.next();
		let TokenKind::String(text) = &token.kind else {
			return Err(Self::error_at(
				&token,
				"mata-script needs the language version as a string, such as \"0.1.0\"".to_owned(),
			));
		};
		// `version::declared` reads a header in the framing before parsing; one it cannot see, such as
		// in the REPL, is checked here.
		crate::version::supported(text, token.line, token.column)?;
		let mut requires = None;
		let mut profile = None;
		while let TokenKind::Keyword(keyword) = self.peek().clone() {
			let place = self.next();
			let duplicate = match keyword.as_str() {
				"requires" => {
					self.open(":requires features")?;
					let mut features = Vec::new();
					while self.peek() != &TokenKind::RightParen {
						features.push(self.name()?);
					}
					self.current += 1;
					requires.replace(features).is_some()
				}
				"profile" => profile.replace(self.name()?).is_some(),
				_ => {
					return Err(Error::at(
						format!("mata-script has no keyword ':{keyword}'"),
						place.line,
						place.column,
					));
				}
			};
			if duplicate {
				return Err(Error::at(
					format!("duplicate keyword ':{keyword}'"),
					place.line,
					place.column,
				));
			}
		}
		Ok(FormKind::Header {
			requires: requires.unwrap_or_default(),
			profile,
		})
	}

	fn form_expression(&mut self, start: &Token) -> Result<Spanned<FormKind>, Error> {
		let expression = self.application(start)?;
		Ok(Spanned {
			node: FormKind::Expression(expression),
			line: start.line,
			column: start.column,
		})
	}

	fn define(&mut self) -> Result<FormKind, Error> {
		if self.peek() == &TokenKind::LeftParen {
			self.current += 1;
			let name = self.name()?;
			let parameters = Some(self.parameters("define", false)?);
			let body = self.body("function body")?;
			Ok(FormKind::Define {
				name,
				parameters,
				sort: None,
				body,
			})
		} else {
			let name = self.name()?;
			// Grammar: [ sort ] before the value; a plain name or '(' is ambiguous with
			// an application value, so only a known sort starts one (script-language.md sorts table).
			let sort = if Self::starts_sort(&self.tokens[self.current..]) {
				Some(self.sort()?)
			} else {
				None
			};
			let value = self.expression()?;
			Ok(FormKind::Define {
				name,
				parameters: None,
				sort,
				body: vec![value],
			})
		}
	}

	fn declaration(&mut self, variadic: bool) -> Result<FormKind, Error> {
		Ok(FormKind::Declare {
			name: self.name()?,
			sort: self.sort()?,
			variadic,
		})
	}

	fn expression(&mut self) -> Result<Expr, Error> {
		let token = self.next();
		self.expression_from_token(token)
	}

	fn expression_from_token(&mut self, token: Token) -> Result<Expr, Error> {
		if token.kind == TokenKind::LeftParen {
			self.application(&token)
		} else {
			self.expression_from(token)
		}
	}

	fn expression_from(&mut self, token: Token) -> Result<Expr, Error> {
		let (line, column) = (token.line, token.column);
		let node = match token.kind {
			TokenKind::Name(name) => Kind::Name(name),
			TokenKind::Input(name) => Kind::Input(name),
			TokenKind::Integer(value) => Kind::Int(value),
			TokenKind::String(value) => Kind::String(value),
			TokenKind::Bool(value) => Kind::Bool(value),
			TokenKind::None => Kind::None,
			TokenKind::Keyword(keyword) => {
				return Err(Error::at(
					format!("unexpected keyword ':{keyword}'"),
					line,
					column,
				));
			}
			TokenKind::RightParen => {
				return Err(Error::at("unexpected ')'".to_owned(), line, column));
			}
			TokenKind::Eof => return self.expected("expression"),
			TokenKind::LeftParen => unreachable!(),
		};
		Ok(Expr { node, line, column })
	}

	fn application(&mut self, start: &Token) -> Result<Expr, Error> {
		if let TokenKind::Name(head) = self.peek().clone() {
			match head.as_str() {
				"let" | "loop" => return self.binding_application(start, &head),
				"lambda" => return self.lambda_application(start),
				"cond" => return self.cond_application(start),
				"get-option" | "get-info" => {
					self.current += 1;
					let keyword = self.keyword()?;
					self.close(&head)?;
					let node = if head == "get-option" {
						Kind::GetOption(keyword)
					} else {
						Kind::GetInfo(keyword)
					};
					return Ok(Expr {
						node,
						line: start.line,
						column: start.column,
					});
				}
				_ => {}
			}
		}
		let mut items = Vec::new();
		let mut keywords = Vec::new();
		while self.peek() != &TokenKind::RightParen {
			if let TokenKind::Keyword(keyword) = self.peek().clone() {
				self.current += 1;
				let value = if Self::is_literal_head(&items) {
					self.literal_data()?
				} else {
					self.expression()?
				};
				keywords.push((keyword, value));
			} else {
				items.push(self.expression()?);
			}
		}
		self.current += 1;
		let (line, column) = (start.line, start.column);
		if let Some(Kind::Name(name)) = items.first().map(|e| &e.node)
			&& matches!(name.as_str(), "nfa:literal" | "nft:literal")
		{
			if items.len() != 1 {
				return Err(Error::at(
					"literal fields must be keywords".to_owned(),
					line,
					column,
				));
			}
			return Ok(Expr {
				node: Kind::Literal {
					name: name.clone(),
					arguments: keywords,
				},
				line,
				column,
			});
		}
		let Some(head) = items.first().cloned() else {
			return Err(Error::at(
				"expected a function or special form".to_owned(),
				line,
				column,
			));
		};
		Ok(Expr {
			node: Kind::Call {
				function: Box::new(head),
				arguments: items[1..].to_vec(),
				keywords,
			},
			line,
			column,
		})
	}

	/// Whether the list being parsed is an automaton literal, whose keyword fields hold raw data.
	fn is_literal_head(items: &[Expr]) -> bool {
		items
			.first()
			.is_some_and(|item| matches!(&item.node, Kind::Name(name) if matches!(name.as_str(), "nfa:literal" | "nft:literal")))
	}

	fn literal_data(&mut self) -> Result<Expr, Error> {
		let token = self.next();
		let (line, column) = (token.line, token.column);
		let node = match token.kind {
			TokenKind::Name(name) => Kind::Name(name),
			TokenKind::Integer(value) => Kind::Int(value),
			TokenKind::LeftParen => {
				let mut items = Vec::new();
				while self.peek() != &TokenKind::RightParen {
					items.push(self.literal_data()?);
				}
				self.current += 1;
				Kind::List(items)
			}
			_ => {
				return Err(Self::error_at(&token, "expected literal data".to_owned()));
			}
		};
		Ok(Expr { node, line, column })
	}

	fn binding_application(&mut self, start: &Token, head: &str) -> Result<Expr, Error> {
		self.current += 1; // the head name
		self.open(head)?;
		let mut bindings = Vec::new();
		while self.peek() != &TokenKind::RightParen {
			let binding = self.next();
			if binding.kind != TokenKind::LeftParen {
				return Err(Self::error_at(
					&binding,
					format!("a {head} binding must be (name expression)"),
				));
			}
			let name = self.name()?;
			let value = self.expression()?;
			self.close("a binding")?;
			bindings.push(Expr {
				node: Kind::List(vec![
					Expr {
						node: Kind::Name(name),
						line: binding.line,
						column: binding.column,
					},
					value,
				]),
				line: binding.line,
				column: binding.column,
			});
		}
		self.current += 1; // ')' of the bindings
		let body = self.body(&format!("{head} body"))?;
		self.close(head)?;
		let mut items = vec![
			Expr {
				node: Kind::Name(head.to_owned()),
				line: start.line,
				column: start.column,
			},
			Expr {
				node: Kind::List(bindings),
				line: start.line,
				column: start.column,
			},
		];
		items.extend(body);
		Ok(Expr {
			node: Kind::List(items),
			line: start.line,
			column: start.column,
		})
	}

	fn lambda_application(&mut self, start: &Token) -> Result<Expr, Error> {
		self.current += 1; // "lambda"
		let parameters = self.parameters("lambda", true)?;
		let body = self.body("lambda body")?;
		self.close("lambda")?;
		Ok(Expr {
			node: Kind::Lambda { parameters, body },
			line: start.line,
			column: start.column,
		})
	}

	fn cond_application(&mut self, start: &Token) -> Result<Expr, Error> {
		self.current += 1; // "cond"
		let mut clauses = Vec::new();
		let mut saw_else = false;
		while self.peek() != &TokenKind::RightParen {
			let clause = self.next();
			if clause.kind != TokenKind::LeftParen {
				return Err(Self::error_at(
					&clause,
					"a cond clause must be (condition expression)".to_owned(),
				));
			}
			let condition = self.next();
			let is_else = matches!(&condition.kind, TokenKind::Name(name) if name == "else");
			if is_else {
				saw_else = true;
			} else if saw_else {
				return Err(Error::at(
					"an else clause must be the last cond clause".to_owned(),
					clause.line,
					clause.column,
				));
			}
			let condition = if is_else {
				Expr {
					node: Kind::Name("else".to_owned()),
					line: condition.line,
					column: condition.column,
				}
			} else {
				self.expression_from_token(condition)?
			};
			let value = self.expression()?;
			self.close("a cond clause")?;
			clauses.push(Expr {
				node: Kind::List(vec![condition, value]),
				line: clause.line,
				column: clause.column,
			});
		}
		if clauses.is_empty() {
			return Err(self.here("cond needs at least one clause".to_owned()));
		}
		self.current += 1; // ')'
		let mut items = vec![Expr {
			node: Kind::Name("cond".to_owned()),
			line: start.line,
			column: start.column,
		}];
		items.extend(clauses);
		Ok(Expr {
			node: Kind::List(items),
			line: start.line,
			column: start.column,
		})
	}

	/// Parses a parenthesised parameter list; the opening `(` is expected and the closing `)` consumed.
	fn parameters(&mut self, form: &str, needs_open: bool) -> Result<Vec<Parameter>, Error> {
		if needs_open {
			self.open(&format!("{form} parameters"))?;
		}
		let mut parameters = Vec::new();
		while self.peek() != &TokenKind::RightParen {
			let parameter = if self.peek() == &TokenKind::LeftParen {
				self.current += 1;
				let name = self.name()?;
				let sort = self.sort()?;
				self.close("a parameter")?;
				Parameter {
					name,
					sort: Some(sort),
				}
			} else {
				Parameter {
					name: self.name()?,
					sort: None,
				}
			};
			if parameters
				.iter()
				.any(|p: &Parameter| p.name == parameter.name)
			{
				return Err(Error::at(
					format!("duplicate parameter '{}'", parameter.name),
					self.peek_token().line,
					self.peek_token().column,
				));
			}
			parameters.push(parameter);
		}
		self.current += 1; // ')'
		Ok(parameters)
	}

	/// Consumes an opening `(` that must introduce `what`.
	fn open(&mut self, what: &str) -> Result<(), Error> {
		if self.peek() == &TokenKind::LeftParen {
			self.current += 1;
			Ok(())
		} else {
			Err(self.here(format!("expected '(' to open {what}")))
		}
	}

	fn body(&mut self, what: &str) -> Result<Vec<Expr>, Error> {
		let mut body = Vec::new();
		while self.peek() != &TokenKind::RightParen {
			if self.peek() == &TokenKind::LeftParen
				&& matches!(self.tokens.get(self.current + 1).map(|t| &t.kind), Some(TokenKind::Name(name)) if name == "define")
			{
				let form = self.form()?;
				let Spanned {
					node:
						FormKind::Define {
							name,
							parameters,
							sort,
							body: definition,
						},
					line,
					column,
				} = form
				else {
					unreachable!()
				};
				body.push(Expr {
					node: Kind::Definition {
						name,
						parameters,
						sort,
						body: definition,
					},
					line,
					column,
				});
			} else {
				body.push(self.expression()?);
			}
		}
		if body.is_empty() {
			return self.expected(what);
		}
		Ok(body)
	}

	/// Sorts from the script-language.md sorts table; `(` sorts are only `(List …)` and `(Option …)`.
	fn starts_sort(tokens: &[Token]) -> bool {
		match tokens.first().map(|token| &token.kind) {
			Some(TokenKind::Name(name)) => crate::check::SIMPLE_SORTS.contains(&name.as_str()),
			Some(TokenKind::LeftParen) => matches!(
				tokens.get(1).map(|token| &token.kind),
				Some(TokenKind::Name(name)) if name == "List" || name == "Option"
			),
			_ => false,
		}
	}

	fn sort(&mut self) -> Result<String, Error> {
		let token = self.next();
		match token.kind {
			TokenKind::Name(name) => Ok(name),
			TokenKind::LeftParen => {
				let name = self.name()?;
				let mut sorts = Vec::new();
				while self.peek() != &TokenKind::RightParen {
					sorts.push(self.sort()?);
				}
				self.current += 1;
				Ok(format!("({name} {})", sorts.join(" ")))
			}
			_ => Err(Self::error_at(&token, "expected a sort".to_owned())),
		}
	}

	fn name_keyword(&self) -> Option<String> {
		if let TokenKind::Name(name) = self.peek() {
			Some(name.clone())
		} else {
			None
		}
	}
	fn name(&mut self) -> Result<String, Error> {
		let token = self.next();
		if let TokenKind::Name(name) = token.kind {
			Ok(name)
		} else {
			Err(Self::error_at(&token, "expected a name".to_owned()))
		}
	}
	fn keyword(&mut self) -> Result<String, Error> {
		let token = self.next();
		if let TokenKind::Keyword(keyword) = token.kind {
			Ok(keyword)
		} else {
			Err(Self::error_at(&token, "expected a keyword".to_owned()))
		}
	}
	fn close(&mut self, name: &str) -> Result<(), Error> {
		if self.peek() == &TokenKind::RightParen {
			self.current += 1;
			Ok(())
		} else if matches!(self.peek(), TokenKind::Eof) {
			self.expected(&format!("')' to close {name}"))
		} else {
			Err(self.here(format!("too many elements in {name}")))
		}
	}
	fn expected<T>(&self, what: &str) -> Result<T, Error> {
		Err(self.here(format!("expected {what}")))
	}
	fn here(&self, message: String) -> Error {
		Self::error_at(self.peek_token(), message)
	}
	/// An error at `token`; one at the end of the input is incomplete, so the REPL reads on.
	fn error_at(token: &Token, message: String) -> Error {
		let mut error = Error::at(message, token.line, token.column);
		error.incomplete = token.kind == TokenKind::Eof;
		error
	}
	fn peek(&self) -> &TokenKind {
		&self.peek_token().kind
	}
	fn peek_token(&self) -> &Token {
		&self.tokens[self.current]
	}
	fn next(&mut self) -> Token {
		let token = self.tokens[self.current].clone();
		if !matches!(token.kind, TokenKind::Eof) {
			self.current += 1;
		}
		token
	}
}
