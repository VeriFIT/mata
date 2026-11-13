// Copyright (c) 2026 VeriFIT. GNU GPL v3. See LICENSE in the project root.

use std::rc::Rc;

use mata::strings::{self, ReplaceMode};
use mata::{
	CheckAlgorithm, Complementation, Direction, Minimization, ProductFinalCondition, Reduction,
	ResidualConstruction, UnmetRequirement,
};
use mata::{IntermediateAutomaton, Nfa, Symbol, Word, nft::Nft};

use crate::Error;
use crate::eval::Engine;
use crate::eval::{AutomatonKind, Value, as_bool, as_int, as_list, as_string};
use crate::version::{FIRST, Version};

/// One script operation: its script name, the handler that serves it, the argument counts it accepts,
/// the sorts of its operands and of its result where they are fixed, and the language version that
/// added it.
pub(crate) struct Operation {
	/// Name in a script, such as `nfa:union-nondeterministic`.
	pub name: &'static str,
	/// Handler in [`dispatch`].
	pub handler: &'static str,
	/// Smallest number of arguments the operation accepts.
	pub minimum: usize,
	/// Largest number of arguments the operation accepts, `None` for no limit.
	pub maximum: Option<usize>,
	/// Sorts of the positional operands, comma-separated. An empty entry leaves that operand unchecked;
	/// an empty string leaves all of them unchecked, as for operations that accept a prefix of them.
	pub operands: &'static str,
	/// Sort of the result, empty when it depends on the operands.
	pub result: &'static str,
	/// The language version that added the operation; a script declaring an older one does not see it.
	pub since: Version,
}

const fn op(
	name: &'static str,
	handler: &'static str,
	minimum: usize,
	maximum: Option<usize>,
	operands: &'static str,
	result: &'static str,
) -> Operation {
	Operation {
		name,
		handler,
		minimum,
		maximum,
		operands,
		result,
		since: FIRST,
	}
}

/// A hint an operation takes as a keyword argument: its name and the values it accepts, the default
/// first.
pub(crate) struct Hint {
	pub operation: &'static str,
	pub name: &'static str,
	pub values: &'static [&'static str],
}

const CHECK_ALGORITHMS: &[&str] = &["antichains", "naive"];

/// `strings::replace::ReplaceMode`, the default `All` first.
const REPLACE_MODES: &[&str] = &["all", "single"];

pub(crate) const HINTS: &[Hint] = &[
	Hint {
		operation: "nfa:complement",
		name: "algorithm",
		values: &["classical", "brzozowski"],
	},
	Hint {
		operation: "nfa:minimize",
		name: "algorithm",
		values: &["hopcroft", "brzozowski"],
	},
	Hint {
		operation: "nfa:reduce",
		name: "algorithm",
		values: &["simulation", "residual"],
	},
	Hint {
		operation: "nfa:reduce",
		name: "type",
		values: &["after", "with"],
	},
	Hint {
		operation: "nfa:reduce",
		name: "direction",
		values: &["forward", "backward"],
	},
	Hint {
		operation: "nfa:included?",
		name: "algorithm",
		values: CHECK_ALGORITHMS,
	},
	Hint {
		operation: "nfa:equivalent?",
		name: "algorithm",
		values: CHECK_ALGORITHMS,
	},
	Hint {
		operation: "nfa:universal?",
		name: "algorithm",
		values: CHECK_ALGORITHMS,
	},
	Hint {
		operation: "strings:create-identity-with-single-symbol-replace",
		name: "replace-mode",
		values: REPLACE_MODES,
	},
	Hint {
		operation: "strings:replace-reluctant-regex",
		name: "replace-mode",
		values: REPLACE_MODES,
	},
	Hint {
		operation: "strings:replace-reluctant-literal",
		name: "replace-mode",
		values: REPLACE_MODES,
	},
	Hint {
		operation: "strings:replace-reluctant-single-symbol",
		name: "replace-mode",
		values: REPLACE_MODES,
	},
	Hint {
		operation: "automaton:included?",
		name: "algorithm",
		values: CHECK_ALGORITHMS,
	},
	Hint {
		operation: "automaton:equivalent?",
		name: "algorithm",
		values: CHECK_ALGORITHMS,
	},
];

/// The hint `name` of `operation`, if it takes one.
pub(crate) fn hint(operation: &str, name: &str) -> Option<&'static Hint> {
	HINTS
		.iter()
		.find(|hint| hint.operation == operation && hint.name == name)
}

/// Every hint `operation` takes.
pub(crate) fn hints_of(operation: &str) -> impl Iterator<Item = &'static Hint> {
	HINTS.iter().filter(move |hint| hint.operation == operation)
}

/// An optional parameter of the library function that an operation takes as a keyword argument, named after
/// the parameter in kebab-case: `(nfa:concatenate a b :use-epsilon true)`. `sort` is `Bool`, `Int`,
/// `String`, or `Symbol`, which is given by its name, a `String`, as in `(symbol "a")`. A call without it
/// gets the library default. Handlers read it with [`optional`]: `optional::<bool>(engine, "use-epsilon")`.
pub(crate) struct OptionalParameter {
	pub operation: &'static str,
	pub name: &'static str,
	pub sort: &'static str,
}

impl OptionalParameter {
	/// The sort of the value a script gives.
	pub(crate) fn script_sort(&self) -> &'static str {
		if self.sort == "Symbol" {
			"String"
		} else {
			self.sort
		}
	}
}

pub(crate) const OPTIONAL_PARAMETERS: &[OptionalParameter] = &[
	OptionalParameter {
		operation: "nfa:concatenate",
		name: "use-epsilon",
		sort: "Bool",
	},
	OptionalParameter {
		operation: "nfa:in-language-prefix?",
		name: "use-epsilon",
		sort: "Bool",
	},
	OptionalParameter {
		operation: "nfa:product",
		name: "final-condition",
		sort: "String",
	},
	OptionalParameter {
		operation: "nfa:product",
		name: "first-epsilon",
		sort: "Symbol",
	},
	OptionalParameter {
		operation: "nfa:left-quotient",
		name: "first-epsilon",
		sort: "Symbol",
	},
	OptionalParameter {
		operation: "nfa:right-quotient",
		name: "first-epsilon",
		sort: "Symbol",
	},
	OptionalParameter {
		operation: "nfa:get-one-letter-automaton",
		name: "abstract-symbol",
		sort: "Symbol",
	},
	OptionalParameter {
		operation: "nfa:create-random-nfa-tabakov-vardi",
		name: "seed",
		sort: "Int",
	},
	OptionalParameter {
		operation: "nft:in-language?",
		name: "match-prefix",
		sort: "Bool",
	},
	OptionalParameter {
		operation: "nft:in-language?",
		name: "has-epsilon-cycles",
		sort: "Bool",
	},
	OptionalParameter {
		operation: "strings:create-identity",
		name: "num-of-levels",
		sort: "Int",
	},
	OptionalParameter {
		operation: "strings:replace-reluctant-regex",
		name: "begin-marker",
		sort: "Symbol",
	},
	OptionalParameter {
		operation: "strings:replace-reluctant-literal",
		name: "end-marker",
		sort: "Symbol",
	},
];

/// The optional parameter `name` of `operation`, if it takes one.
pub(crate) fn optional_parameter(
	operation: &str,
	name: &str,
) -> Option<&'static OptionalParameter> {
	OPTIONAL_PARAMETERS
		.iter()
		.find(|parameter| parameter.operation == operation && parameter.name == name)
}

/// Every optional parameter `operation` takes.
pub(crate) fn optional_parameters_of(
	operation: &str,
) -> impl Iterator<Item = &'static OptionalParameter> {
	OPTIONAL_PARAMETERS
		.iter()
		.filter(move |parameter| parameter.operation == operation)
}

/// Checks keyword argument `name` of `operation` written as `value`, a hint or an optional parameter in
/// the text of the command line: `true`/`false` for `Bool`, decimal for `Int`.
///
/// # Errors
///
/// The operation takes no such keyword, or the value does not fit it.
pub(crate) fn check_keyword(operation: &str, name: &str, value: &str) -> Result<(), String> {
	let Some(parameter) = optional_parameter(operation, name) else {
		return check_hint(operation, name, value);
	};
	let fits = match parameter.sort {
		"Bool" => matches!(value, "true" | "false"),
		"Int" => value.parse::<i64>().is_ok(),
		_ => true,
	};
	if fits {
		Ok(())
	} else {
		Err(format!(
			":{name} of {operation} must be of sort {}, not \"{value}\"",
			parameter.sort
		))
	}
}

/// The command-line text of keyword argument `name` of `operation` given as `value` in a script.
///
/// # Errors
///
/// `value` is not of the sort the keyword takes.
pub(crate) fn keyword_text(operation: &str, name: &str, value: &Value) -> Result<String, String> {
	match optional_parameter(operation, name).map(OptionalParameter::script_sort) {
		Some("Bool") => as_bool(value)
			.map(|value| value.to_string())
			.map_err(|_| format!("the value of :{name} must be a Bool")),
		Some("Int") => as_int(value)
			.map(|value| value.to_string())
			.map_err(|_| format!("the value of :{name} must be an Int")),
		Some(_) => as_string(value).map_err(|_| format!("the value of :{name} must be a String")),
		None => as_string(value).map_err(|_| format!("the value of hint :{name} must be a String")),
	}
}

/// The value of optional parameter `parameter` written as `text`, which [`check_keyword`] accepted.
fn optional_value(
	parameter: &OptionalParameter,
	text: &str,
	engine: &mut Engine,
) -> Result<Value, Error> {
	Ok(match parameter.sort {
		"Bool" => Value::Bool(text == "true"),
		"Int" => Value::Int(text.parse().map_err(|_| {
			Error::plain(format!(
				":{} must be an Int, not \"{text}\"",
				parameter.name
			))
		})?),
		"Symbol" => Value::Symbol(engine.intern_name(text).map_err(Error::plain)?),
		_ => Value::String(text.to_owned()),
	})
}

/// The value of an optional parameter, of the Rust type of its sort.
pub(crate) trait OptionalValue: Sized {
	fn from_value(value: &Value) -> Option<Self>;
}

impl OptionalValue for bool {
	fn from_value(value: &Value) -> Option<bool> {
		match value {
			Value::Bool(value) => Some(*value),
			_ => None,
		}
	}
}

impl OptionalValue for i64 {
	fn from_value(value: &Value) -> Option<i64> {
		match value {
			Value::Int(value) => Some(*value),
			_ => None,
		}
	}
}

impl OptionalValue for String {
	fn from_value(value: &Value) -> Option<String> {
		match value {
			Value::String(value) => Some(value.clone()),
			_ => None,
		}
	}
}

impl OptionalValue for Symbol {
	fn from_value(value: &Value) -> Option<Symbol> {
		match value {
			Value::Symbol(value) => Some(*value),
			_ => None,
		}
	}
}

/// Optional parameter `name` of the call being made (`bool` for `Bool`, `i64` for `Int`, `String`, or
/// `Symbol`), `None` when the call leaves it to the library default.
pub(crate) fn optional<T: OptionalValue>(engine: &Engine, name: &str) -> Option<T> {
	engine
		.call_parameters
		.iter()
		.find(|(key, _)| key == name)
		.and_then(|(_, value)| T::from_value(value))
}

/// The binary operations that a script and a direct command apply to more operands by folding from the
/// left: `(nfa:intersection a b c)` is `(nfa:intersection (nfa:intersection a b) c)`.
pub(crate) fn folds(name: &str) -> bool {
	matches!(
		name,
		"nfa:union-nondeterministic"
			| "nfa:intersection"
			| "nfa:concatenate"
			| "nft:union-nondeterministic"
			| "nft:intersection"
			| "nft:concatenate"
			| "nft:compose"
	)
}

pub(crate) const OPERATIONS: &[Operation] = &[
	op("not", "not", 1, Some(1), "Bool", "Bool"),
	op("=", "eq", 2, Some(2), "", "Bool"),
	op("<", "lt", 2, Some(2), "", "Bool"),
	op("<=", "le", 2, Some(2), "", "Bool"),
	op("+", "add", 2, Some(2), "Int,Int", "Int"),
	op("-", "sub", 2, Some(2), "Int,Int", "Int"),
	op("concat", "concat", 0, None, "String*", "String"),
	op("to-string", "to_string", 1, Some(1), "", "String"),
	op("string->int", "string_to_int", 1, Some(1), "String", "Int"),
	op("none?", "is_none", 1, Some(1), "", "Bool"),
	op("get", "get", 1, Some(1), "", ""),
	op("word", "word", 0, None, "", "Word"),
	op("symbol", "symbol", 1, Some(1), "String", "Symbol"),
	op("list", "list", 0, None, "", ""),
	op(
		"list:length",
		"list_length",
		1,
		Some(1),
		"(List Any)",
		"Int",
	),
	op("list:nth", "list_nth", 2, Some(2), "(List Any),Int", ""),
	op(
		"list:take",
		"list_take",
		2,
		Some(2),
		"(List Any),Int",
		"(List Any)",
	),
	op(
		"list:drop",
		"list_drop",
		2,
		Some(2),
		"(List Any),Int",
		"(List Any)",
	),
	op(
		"list:range",
		"list_range",
		2,
		Some(2),
		"Int,Int",
		"(List Int)",
	),
	op(
		"list:map",
		"list_map",
		2,
		Some(2),
		"Function,(List Any)",
		"(List Any)",
	),
	op(
		"list:filter",
		"list_filter",
		2,
		Some(2),
		"Function,(List Any)",
		"(List Any)",
	),
	op(
		"list:reduce",
		"list_reduce",
		2,
		Some(2),
		"Function,(List Any)",
		"",
	),
	op(
		"list:fold",
		"list_fold",
		3,
		Some(3),
		"Function,,(List Any)",
		"",
	),
	op("print", "print", 1, Some(1), "", ""),
	op("echo", "echo", 1, Some(1), "String", "String"),
	op("write", "write", 2, Some(2), ",String", ""),
	op("check", "check", 1, Some(2), "", "Bool"),
	op("assert", "assert", 1, Some(1), "Bool", "Bool"),
	op(
		"get-witness",
		"get_witness",
		0,
		Some(0),
		"",
		"(Option Word)",
	),
	op(
		"script-alphabet",
		"script_alphabet",
		0,
		Some(0),
		"",
		"Alphabet",
	),
	op(
		"nfa:load",
		"nfa_load",
		1,
		Some(1),
		"String|(List String)",
		"",
	),
	op("nfa:parse", "nfa_parse", 1, Some(1), "String", "Nfa"),
	op("nfa:literal", "nfa_literal", 0, None, "", "Nfa"),
	op(
		"nfa:print-to-mata",
		"nfa_print_to_mata",
		1,
		Some(1),
		"Nfa",
		"String",
	),
	op(
		"nfa:print-to-dot",
		"nfa_print_to_dot",
		1,
		Some(1),
		"Nfa",
		"String",
	),
	op(
		"nfa:number-of-states",
		"nfa_num_states",
		1,
		Some(1),
		"Nfa",
		"Int",
	),
	op(
		"nfa:number-of-transitions",
		"nfa_num_transitions",
		1,
		Some(1),
		"Nfa",
		"Int",
	),
	op(
		"nfa:union-nondeterministic",
		"nfa_union",
		2,
		None,
		"Nfa,Nfa*",
		"Nfa",
	),
	op(
		"nfa:intersection",
		"nfa_intersection",
		2,
		None,
		"Nfa,Nfa*",
		"Nfa",
	),
	op(
		"nfa:language-difference",
		"nfa_difference",
		2,
		Some(2),
		"Nfa,Nfa",
		"Nfa",
	),
	op("nfa:concatenate", "nfa_concat", 2, None, "Nfa,Nfa*", "Nfa"),
	op("nfa:complement", "nfa_complement", 1, Some(1), "Nfa", "Nfa"),
	op(
		"nfa:determinize",
		"nfa_determinize",
		1,
		Some(1),
		"Nfa",
		"Nfa",
	),
	op("nfa:minimize", "nfa_minimize", 1, Some(1), "Nfa", "Nfa"),
	op("nfa:reduce", "nfa_reduce", 1, Some(1), "Nfa", "Nfa"),
	op("nfa:revert", "nfa_reverse", 1, Some(1), "Nfa", "Nfa"),
	op(
		"nfa:remove-epsilon",
		"nfa_remove_epsilon",
		1,
		Some(1),
		"Nfa",
		"Nfa",
	),
	op("nfa:trim", "nfa_trim", 1, Some(1), "Nfa", "Nfa"),
	op(
		"nfa:make-complete",
		"nfa_make_complete",
		1,
		Some(1),
		"Nfa",
		"Nfa",
	),
	op(
		"nfa:language-empty?",
		"nfa_lang_empty",
		1,
		Some(1),
		"Nfa",
		"Bool",
	),
	op(
		"nfa:intersection-empty?",
		"nfa_disjoint",
		2,
		Some(2),
		"Nfa,Nfa",
		"Bool",
	),
	op(
		"nfa:included?",
		"nfa_included",
		2,
		Some(2),
		"Nfa,Nfa",
		"Bool",
	),
	op(
		"nfa:equivalent?",
		"nfa_equivalent",
		2,
		Some(2),
		"Nfa,Nfa",
		"Bool",
	),
	op("nfa:universal?", "nfa_universal", 1, Some(1), "Nfa", "Bool"),
	op(
		"nfa:in-language?",
		"nfa_in_lang",
		2,
		Some(2),
		"Nfa,Word",
		"Bool",
	),
	op(
		"nfa:deterministic?",
		"nfa_deterministic",
		1,
		Some(1),
		"Nfa",
		"Bool",
	),
	op("nfa:complete?", "nfa_complete", 1, Some(1), "Nfa", "Bool"),
	op("nfa:minimal?", "nfa_minimal", 1, Some(1), "Nfa", "Bool"),
	op(
		"nfa:identical?",
		"nfa_identical",
		2,
		Some(2),
		"Nfa,Nfa",
		"Bool",
	),
	op("nfa:acyclic?", "nfa_acyclic", 1, Some(1), "Nfa", "Bool"),
	op(
		"nfa:get-word",
		"nfa_word",
		1,
		Some(1),
		"Nfa",
		"(Option Word)",
	),
	op(
		"nfa:get-shortest-word",
		"nfa_shortest_word",
		1,
		Some(1),
		"Nfa",
		"(Option Word)",
	),
	op(
		"nfa:get-words",
		"nfa_words",
		2,
		Some(2),
		"Nfa,Int",
		"(List Word)",
	),
	op(
		"nfa:get-accepting-run",
		"nfa_run",
		1,
		Some(1),
		"Nfa",
		"(Option Run)",
	),
	op(
		"nfa:create-from-regex",
		"nfa_regex",
		1,
		Some(1),
		"String",
		"Nfa",
	),
	op(
		"nfa:create-single-word-nfa",
		"nfa_single_word",
		1,
		Some(1),
		"Word",
		"Nfa",
	),
	op(
		"nfa:create-empty-string-nfa",
		"nfa_empty_word",
		0,
		Some(0),
		"",
		"Nfa",
	),
	op(
		"nfa:create-sigma-star-nfa",
		"nfa_sigma_star",
		0,
		Some(0),
		"",
		"Nfa",
	),
	op(
		"nfa:create-alphabet",
		"nfa_alphabet",
		0,
		None,
		"",
		"Alphabet",
	),
	op(
		"nfa:encode-word",
		"nfa_encode",
		1,
		Some(1),
		"(List String)",
		"Word",
	),
	op(
		"alphabet:get-alphabet-symbols",
		"alphabet_symbols",
		0,
		Some(1),
		"",
		"(List Symbol)",
	),
	op(
		"alphabet:translate-symbol",
		"alphabet_translate",
		1,
		Some(2),
		"",
		"Symbol",
	),
	op(
		"alphabet:reverse-translate-symbol",
		"alphabet_reverse_translate",
		1,
		Some(2),
		"",
		"String",
	),
	op(
		"nft:load",
		"nft_load",
		1,
		Some(1),
		"String|(List String)",
		"",
	),
	op("nft:parse", "nft_parse", 1, Some(1), "String", "Nft"),
	op("nft:literal", "nft_literal", 0, None, "", "Nft"),
	op(
		"nft:print-to-mata",
		"nft_print_to_mata",
		1,
		Some(1),
		"Nft",
		"String",
	),
	op(
		"nft:print-to-dot",
		"nft_print_to_dot",
		1,
		Some(1),
		"Nft",
		"String",
	),
	op(
		"nft:number-of-states",
		"nft_num_states",
		1,
		Some(1),
		"Nft",
		"Int",
	),
	op(
		"nft:number-of-transitions",
		"nft_num_transitions",
		1,
		Some(1),
		"Nft",
		"Int",
	),
	op(
		"nft:union-nondeterministic",
		"nft_union",
		2,
		None,
		"Nft,Nft*",
		"Nft",
	),
	op(
		"nft:intersection",
		"nft_intersection",
		2,
		None,
		"Nft,Nft*",
		"Nft",
	),
	op("nft:concatenate", "nft_concat", 2, None, "Nft,Nft*", "Nft"),
	op("nft:compose", "nft_compose", 2, None, "Nft,Nft*", "Nft"),
	op("nft:apply", "nft_apply", 2, Some(2), "Nft,Nfa|Word", "Nft"),
	op(
		"nft:project-out",
		"nft_project_out",
		2,
		Some(2),
		"Nft,(List Int)",
		"Nft",
	),
	op(
		"nft:project-to",
		"nft_project_to",
		2,
		Some(2),
		"Nft,(List Int)",
		"Nft",
	),
	op(
		"nft:invert-levels",
		"nft_invert_levels",
		1,
		Some(1),
		"Nft",
		"Nft",
	),
	op(
		"nft:determinize",
		"nft_determinize",
		1,
		Some(1),
		"Nft",
		"Nft",
	),
	op(
		"nft:remove-epsilon",
		"nft_remove_epsilon",
		1,
		Some(1),
		"Nft",
		"Nft",
	),
	op("nft:trim", "nft_trim", 1, Some(1), "Nft", "Nft"),
	op(
		"nft:language-empty?",
		"nft_lang_empty",
		1,
		Some(1),
		"Nft",
		"Bool",
	),
	op(
		"nft:included?",
		"nft_included",
		2,
		Some(2),
		"Nft,Nft",
		"Bool",
	),
	op(
		"nft:equivalent?",
		"nft_equivalent",
		2,
		Some(2),
		"Nft,Nft",
		"Bool",
	),
	op(
		"nft:in-language-by-levels?",
		"nft_in_lang_by_levels",
		2,
		Some(2),
		"Nft,(List Word)",
		"Bool",
	),
	op(
		"nft:deterministic?",
		"nft_deterministic",
		1,
		Some(1),
		"Nft",
		"Bool",
	),
	op("nft:to-nfa", "nft_to_nfa", 1, Some(1), "Nft", "Nfa"),
	op(
		"nft:from-nfa-with-levels-zero",
		"nft_identity",
		2,
		Some(2),
		"Nfa,Int",
		"Nft",
	),
	op(
		"nft:create-single-word-nft",
		"nft_single_word",
		1,
		Some(1),
		"Word",
		"Nft",
	),
	op(
		"nft:create-empty-string-nft",
		"nft_empty_word",
		1,
		Some(1),
		"Int",
		"Nft",
	),
	op("nfa:as-dfa", "nfa_as_dfa", 1, Some(1), "Nfa", "Nfa"),
	op(
		"nfa:concatenate-nth-power",
		"nfa_nth_power",
		2,
		Some(2),
		"Nfa,Int",
		"Nfa",
	),
	op(
		"nfa:union-deterministic-complete",
		"nfa_union_det_complete",
		2,
		Some(2),
		"Nfa,Nfa",
		"Nfa",
	),
	op("nfa:product", "nfa_product", 2, Some(2), "Nfa,Nfa", "Nfa"),
	op(
		"nfa:left-quotient",
		"nfa_left_quotient",
		2,
		Some(2),
		"Nfa,Nfa",
		"Nfa",
	),
	op(
		"nfa:right-quotient",
		"nfa_right_quotient",
		2,
		Some(2),
		"Nfa,Nfa",
		"Nfa",
	),
	op(
		"nfa:decode-utf8",
		"nfa_decode_utf8",
		1,
		Some(1),
		"Nfa",
		"Nfa",
	),
	op(
		"nfa:get-one-letter-automaton",
		"nfa_one_letter",
		1,
		Some(1),
		"Nfa",
		"Nfa",
	),
	op("nfa:flat?", "nfa_flat", 1, Some(1), "Nfa", "Bool"),
	op(
		"nfa:in-language-prefix?",
		"nfa_in_lang_prefix",
		2,
		Some(2),
		"Nfa,Word",
		"Bool",
	),
	op(
		"nfa:get-word-from-complement",
		"nfa_word_from_complement",
		1,
		Some(1),
		"Nfa",
		"(Option Word)",
	),
	op(
		"nfa:get-word-from-language-difference",
		"nfa_word_from_language_difference",
		2,
		Some(2),
		"Nfa,Nfa",
		"(Option Word)",
	),
	op(
		"nfa:create-random-nfa-tabakov-vardi",
		"nfa_random_tabakov_vardi",
		4,
		Some(4),
		"Int,Int,String,String",
		"Nfa",
	),
	op("nft:complement", "nft_complement", 1, Some(1), "Nft", "Nft"),
	op("nft:complete?", "nft_complete", 1, Some(1), "Nft", "Bool"),
	op(
		"nft:concatenate-nth-power",
		"nft_concat_power",
		2,
		Some(2),
		"Nft,Int",
		"Nft",
	),
	op(
		"nft:contains-jump-transitions?",
		"nft_contains_jumps",
		1,
		Some(1),
		"Nft",
		"Bool",
	),
	op(
		"nft:create-sigma-star-nft",
		"nft_sigma_star",
		1,
		Some(1),
		"Int",
		"Nft",
	),
	op(
		"nft:from-nfa-with-levels-advancing",
		"nft_from_interleaved",
		2,
		Some(2),
		"Nfa,Int",
		"Nft",
	),
	op(
		"nft:has-epsilon-cycle?",
		"nft_epsilon_cycle",
		1,
		Some(1),
		"Nft",
		"Bool",
	),
	op(
		"nft:identical?",
		"nft_identical",
		2,
		Some(2),
		"Nft,Nft",
		"Bool",
	),
	op(
		"nft:in-language?",
		"nft_in_lang",
		2,
		Some(2),
		"Nft,Word",
		"Bool",
	),
	op(
		"nft:insert-level",
		"nft_insert_level",
		2,
		Some(2),
		"Nft,Int",
		"Nft",
	),
	op(
		"nft:insert-levels",
		"nft_insert_levels",
		2,
		Some(2),
		"Nft,(List Bool)",
		"Nft",
	),
	op("nft:reduce", "nft_reduce", 1, Some(1), "Nft", "Nft"),
	op("nft:revert", "nft_revert", 1, Some(1), "Nft", "Nft"),
	op(
		"nft:to-nfa-update",
		"nft_to_nfa_update",
		1,
		Some(1),
		"Nft",
		"Nfa",
	),
	op("nft:universal?", "nft_universal", 1, Some(1), "Nft", "Bool"),
	op(
		"nft:unwind-jumps",
		"nft_unwind_jumps",
		1,
		Some(1),
		"Nft",
		"Nft",
	),
	op(
		"strings:get-shortest-words",
		"strings_shortest_words",
		1,
		Some(1),
		"Nfa",
		"(List Word)",
	),
	op(
		"strings:get-longest-words",
		"strings_longest_words",
		1,
		Some(1),
		"Nfa",
		"(Option (List Word))",
	),
	op(
		"strings:get-words-of-lengths",
		"strings_words_of_lengths",
		2,
		Some(2),
		"Nft,(List Int)",
		"(Option (List Word))",
	),
	op(
		"strings:get-accepted-symbols",
		"strings_accepted_symbols",
		1,
		Some(1),
		"Nfa",
		"(List Symbol)",
	),
	op(
		"strings:get-word-lengths",
		"strings_word_lengths",
		1,
		Some(1),
		"Nfa",
		"(List (List Int))",
	),
	op(
		"strings:language-epsilon?",
		"strings_lang_eps",
		1,
		Some(1),
		"Nfa",
		"Bool",
	),
	op(
		"strings:get-segments",
		"strings_segments",
		2,
		Some(2),
		"Nfa,(List Symbol)",
		"(List Nfa)",
	),
	op(
		"strings:reluctant-nfa",
		"strings_reluctant_nfa",
		1,
		Some(1),
		"Nfa",
		"Nfa",
	),
	op(
		"strings:create-identity",
		"strings_identity",
		1,
		Some(1),
		"Alphabet",
		"Nft",
	),
	op(
		"strings:create-identity-with-single-symbol-replace",
		"strings_identity_replace",
		3,
		Some(3),
		"Alphabet,Symbol,Symbol|Word",
		"Nft",
	),
	op(
		"strings:replace-reluctant-regex",
		"strings_replace_regex",
		3,
		Some(3),
		"String|Nfa,Word,Alphabet",
		"Nft",
	),
	op(
		"strings:replace-reluctant-literal",
		"strings_replace_literal",
		3,
		Some(3),
		"Word,Word,Alphabet",
		"Nft",
	),
	op(
		"strings:replace-reluctant-single-symbol",
		"strings_replace_symbol",
		3,
		Some(3),
		"Symbol,Symbol|Word,Alphabet",
		"Nft",
	),
	// The `automaton:` operations call the `nfa:` or `nft:` operation of the same name.
	op(
		"automaton:load",
		"automaton_load",
		1,
		Some(1),
		"String|(List String)",
		"",
	),
	op(
		"automaton:parse",
		"automaton_parse",
		1,
		Some(1),
		"String",
		"Automaton",
	),
	op(
		"automaton:print-to-mata",
		"automaton",
		1,
		Some(1),
		"Automaton",
		"String",
	),
	op(
		"automaton:print-to-dot",
		"automaton",
		1,
		Some(1),
		"Automaton",
		"String",
	),
	op(
		"automaton:number-of-states",
		"automaton",
		1,
		Some(1),
		"Automaton",
		"Int",
	),
	op(
		"automaton:number-of-transitions",
		"automaton",
		1,
		Some(1),
		"Automaton",
		"Int",
	),
	op(
		"automaton:union-nondeterministic",
		"automaton",
		2,
		None,
		"Automaton,Automaton*",
		"Automaton",
	),
	op(
		"automaton:intersection",
		"automaton",
		2,
		None,
		"Automaton,Automaton*",
		"Automaton",
	),
	op(
		"automaton:concatenate",
		"automaton",
		2,
		None,
		"Automaton,Automaton*",
		"Automaton",
	),
	op(
		"automaton:determinize",
		"automaton",
		1,
		Some(1),
		"Automaton",
		"Automaton",
	),
	op(
		"automaton:remove-epsilon",
		"automaton",
		1,
		Some(1),
		"Automaton",
		"Automaton",
	),
	op(
		"automaton:trim",
		"automaton",
		1,
		Some(1),
		"Automaton",
		"Automaton",
	),
	op(
		"automaton:language-empty?",
		"automaton",
		1,
		Some(1),
		"Automaton",
		"Bool",
	),
	op(
		"automaton:included?",
		"automaton",
		2,
		Some(2),
		"Automaton,Automaton",
		"Bool",
	),
	op(
		"automaton:equivalent?",
		"automaton",
		2,
		Some(2),
		"Automaton,Automaton",
		"Bool",
	),
	op(
		"automaton:deterministic?",
		"automaton",
		1,
		Some(1),
		"Automaton",
		"Bool",
	),
	op(
		"intermediate:parse-from-mata",
		"intermediate_parse",
		1,
		Some(1),
		"String",
		"(List IntermediateAutomaton)",
	),
	op(
		"intermediate:mintermize",
		"intermediate_mintermize",
		1,
		Some(1),
		"IntermediateAutomaton|(List IntermediateAutomaton)",
		"",
	),
	op(
		"nfa:construct",
		"nfa_construct",
		1,
		Some(1),
		"IntermediateAutomaton",
		"Nfa",
	),
	op(
		"nft:construct",
		"nft_construct",
		1,
		Some(1),
		"IntermediateAutomaton",
		"Nft",
	),
];

/// The operation a script of language version `script` calls by `name`, if it sees one.
pub(crate) fn visible(name: &str, script: Version) -> Option<&'static Operation> {
	operation(name).filter(|operation| operation.since.visible_in(script))
}

/// The operation named `name` in the whole catalogue of this interpreter.
pub(crate) fn operation(name: &str) -> Option<&'static Operation> {
	OPERATIONS.iter().find(|operation| operation.name == name)
}

/// Whether an operation accepts a call with `given` arguments.
pub(crate) fn accepts(operation: &Operation, given: usize) -> bool {
	given >= operation.minimum && operation.maximum.is_none_or(|maximum| given <= maximum)
}

/// The sort operand `index` of `operation` must have, if it is fixed. A last entry ending in `*`
/// applies to every later operand too.
pub(crate) fn operand_sort(operation: &Operation, index: usize) -> Option<&'static str> {
	if operation.operands.is_empty() {
		return None;
	}
	let entries: Vec<&'static str> = operation.operands.split(',').collect();
	let entry = match entries.get(index) {
		Some(entry) => *entry,
		None => entries.last().filter(|entry| entry.ends_with('*'))?,
	};
	let entry = entry.trim_end_matches('*');
	(!entry.is_empty()).then_some(entry)
}

/// The error for a call whose argument count the operation does not accept.
pub(crate) fn arity_message(operation: &Operation, given: usize) -> String {
	match operation.maximum {
		Some(maximum) if maximum == operation.minimum => format!(
			"'{}' takes {maximum} arguments, {given} given",
			operation.name
		),
		Some(maximum) if operation.minimum == 0 => format!(
			"'{}' takes at most {maximum} arguments, {given} given",
			operation.name
		),
		Some(maximum) => format!(
			"'{}' takes {} to {maximum} arguments, {given} given",
			operation.name, operation.minimum
		),
		None => format!(
			"'{}' takes at least {} arguments, {given} given",
			operation.name, operation.minimum
		),
	}
}

/// Checks the hints of one call against the operation's [`HINTS`].
///
/// # Errors
///
/// The operation takes no such hint, or the value is not one the hint accepts.
pub(crate) fn check_hint(operation: &str, name: &str, value: &str) -> Result<(), String> {
	let Some(hint) = hint(operation, name) else {
		let keywords = hints_of(operation)
			.map(|hint| format!(":{}", hint.name))
			.chain(
				optional_parameters_of(operation).map(|parameter| format!(":{}", parameter.name)),
			)
			.collect::<Vec<_>>();
		return Err(if keywords.is_empty() {
			format!("operation {operation} takes no hints")
		} else {
			format!(
				"operation {operation} has no hint ':{name}'; it takes {}",
				keywords.join(", ")
			)
		});
	};
	if hint.values.contains(&value) {
		Ok(())
	} else {
		Err(format!(
			"hint :{name} of {operation} is one of {}, not \"{value}\"",
			hint.values
				.iter()
				.map(|value| format!("\"{value}\""))
				.collect::<Vec<_>>()
				.join(", ")
		))
	}
}

/// Calls operation `name` with positional `arguments` and the `hints` of this call, which override
/// the defaults set with `(set-option :default …)`.
pub(crate) fn call(
	name: &str,
	arguments: Vec<Value>,
	hints: &[(String, String)],
	engine: &mut crate::eval::Engine,
) -> Result<Value, Error> {
	let operation =
		operation(name).ok_or_else(|| Error::plain(format!("unknown operation '{name}'")))?;
	let given = arguments.len();
	if !accepts(operation, given) {
		return Err(Error::plain(arity_message(operation, given)));
	}
	let mut chosen = engine.defaults_of(name);
	for (key, value) in hints {
		check_keyword(name, key, value).map_err(Error::plain)?;
		chosen.retain(|(known, _)| known != key);
		chosen.push((key.clone(), value.clone()));
	}
	let mut parameters = Vec::new();
	for (key, text) in &chosen {
		if let Some(parameter) = optional_parameter(name, key) {
			parameters.push((key.clone(), optional_value(parameter, text, engine)?));
		}
	}
	let saved = std::mem::replace(&mut engine.hints, chosen);
	let saved_parameters = std::mem::replace(&mut engine.call_parameters, parameters);
	let result = if folds(name) && given > 2 {
		let mut operands = arguments.into_iter();
		let first = operands.next().expect("at least three operands");
		operands.try_fold(first, |folded, next| {
			dispatch(operation.handler, name, vec![folded, next], engine)
		})
	} else {
		dispatch(operation.handler, name, arguments, engine)
	};
	engine.hints = saved;
	engine.call_parameters = saved_parameters;
	result
}

/// The value of hint `name` for the call being made, or its first (default) value.
fn hint_value(engine: &Engine, operation: &str, name: &str) -> &'static str {
	let hint = hint(operation, name).expect("every hint a handler reads is in HINTS");
	let chosen = engine
		.hints
		.iter()
		.find(|(key, _)| key == name)
		.map(|(_, value)| value.as_str());
	hint.values
		.iter()
		.copied()
		.find(|value| Some(*value) == chosen)
		.unwrap_or(hint.values[0])
}

fn check_algorithm(engine: &Engine, operation: &str) -> CheckAlgorithm {
	if hint_value(engine, operation, "algorithm") == "naive" {
		CheckAlgorithm::Naive
	} else {
		CheckAlgorithm::Antichains
	}
}

fn dispatch(
	handler: &str,
	name: &str,
	arguments: Vec<Value>,
	engine: &mut crate::eval::Engine,
) -> Result<Value, Error> {
	match handler {
		"not" => {
			let [value] = take(arguments, name)?;
			Ok(Value::Bool(!as_bool(&value).map_err(Error::plain)?))
		}
		"eq" => {
			let [lhs, rhs] = take(arguments, name)?;
			Ok(Value::Bool(values_equal(&lhs, &rhs)))
		}
		"lt" => {
			let [lhs, rhs] = take(arguments, name)?;
			match (&lhs, &rhs) {
				(Value::Int(lhs), Value::Int(rhs)) => Ok(Value::Bool(lhs < rhs)),
				(Value::Word(lhs), Value::Word(rhs)) => Ok(Value::Bool(lhs < rhs)),
				_ => Err(Error::plain("'< needs Int or Word operands".into())),
			}
		}
		"le" => {
			let [lhs, rhs] = take(arguments, name)?;
			match (&lhs, &rhs) {
				(Value::Int(lhs), Value::Int(rhs)) => Ok(Value::Bool(lhs <= rhs)),
				(Value::Word(lhs), Value::Word(rhs)) => Ok(Value::Bool(lhs <= rhs)),
				_ => Err(Error::plain("'<=' needs Int or Word operands".into())),
			}
		}
		"add" => {
			let [lhs, rhs] = take(arguments, name)?;
			match (&lhs, &rhs) {
				(Value::Int(lhs), Value::Int(rhs)) => Ok(Value::Int(
					lhs.checked_add(*rhs)
						.ok_or_else(|| Error::plain("integer overflow".into()))?,
				)),
				_ => Err(Error::plain("'+' needs Int operands".into())),
			}
		}
		"sub" => {
			let [lhs, rhs] = take(arguments, name)?;
			match (&lhs, &rhs) {
				(Value::Int(lhs), Value::Int(rhs)) => Ok(Value::Int(
					lhs.checked_sub(*rhs)
						.ok_or_else(|| Error::plain("integer overflow".into()))?,
				)),
				_ => Err(Error::plain("'-' needs Int operands".into())),
			}
		}
		"concat" => {
			let mut text = String::new();
			for argument in &arguments {
				text.push_str(&as_string(argument).map_err(Error::plain)?);
			}
			Ok(Value::String(text))
		}
		"to_string" => {
			let [value] = take(arguments, name)?;
			Ok(Value::String(match &value {
				Value::String(text) => text.clone(),
				other => crate::display_value(other, engine)
					.trim_end_matches('\n')
					.to_owned(),
			}))
		}
		"string_to_int" => {
			let [value] = take(arguments, name)?;
			let text = as_string(&value).map_err(Error::plain)?;
			text.trim()
				.parse::<i64>()
				.map(Value::Int)
				.map_err(|_| Error::plain(format!("'{text}' is not an Int")))
		}
		"is_none" => {
			let [value] = take(arguments, name)?;
			Ok(Value::Bool(matches!(value, Value::None)))
		}
		"get" => {
			let [value] = take(arguments, name)?;
			match value {
				Value::None => Ok(Value::None),
				other => Ok(other),
			}
		}
		"word" => {
			let mut symbols = Vec::new();
			for argument in &arguments {
				let name = as_string(argument).map_err(Error::plain)?;
				symbols.push(engine.intern_name(&name).map_err(Error::plain)?);
			}
			Ok(Value::Word(Word::from(symbols)))
		}
		"symbol" => {
			let [name] = take(arguments, name)?;
			let name = as_string(&name).map_err(Error::plain)?;
			let symbol = engine.intern_name(&name).map_err(Error::plain)?;
			Ok(Value::Symbol(symbol))
		}
		_ => dispatch_list_structure(handler, name, arguments, engine),
	}
}

/// Serves `list` and the operations that read a list by position: `list:length`, `list:nth`, `list:take`,
/// `list:drop`, and `list:range`.
fn dispatch_list_structure(
	handler: &str,
	name: &str,
	arguments: Vec<Value>,
	engine: &mut Engine,
) -> Result<Value, Error> {
	match handler {
		"list" => Ok(Value::List(Rc::new(arguments))),
		"list_length" => {
			let [list] = take(arguments, name)?;
			integer(as_list(&list).map_err(Error::plain)?.len())
		}
		"list_nth" => {
			let [list, index] = take(arguments, name)?;
			let list = as_list(&list).map_err(Error::plain)?;
			let index = length(
				as_int(&index)?,
				"list:nth needs a non-negative index".into(),
			)?;
			list.get(index)
				.cloned()
				.ok_or_else(|| Error::plain("list:nth index out of bounds".into()))
		}
		"list_take" | "list_drop" => {
			let [list, count] = take(arguments, name)?;
			let list = as_list(&list).map_err(Error::plain)?;
			let count = length(
				as_int(&count)?,
				format!("{name} needs a non-negative count"),
			)?;
			let count = count.min(list.len());
			let taken = if handler == "list_take" {
				list[..count].to_vec()
			} else {
				list[count..].to_vec()
			};
			Ok(Value::List(Rc::new(taken)))
		}
		"list_range" => {
			let [start, end] = take(arguments, name)?;
			let start = as_int(&start)?;
			let end = as_int(&end)?;
			if start > end || start < 0 || end - start > i64::from(i32::MAX) {
				return Ok(Value::List(Rc::new(Vec::new())));
			}
			Ok(Value::List(Rc::new((start..end).map(Value::Int).collect())))
		}
		_ => dispatch_list_iteration(handler, name, arguments, engine),
	}
}

/// Serves the operations that walk a list with a function: `list:map`, `list:filter`, `list:reduce`,
/// and `list:fold`.
fn dispatch_list_iteration(
	handler: &str,
	name: &str,
	arguments: Vec<Value>,
	engine: &mut Engine,
) -> Result<Value, Error> {
	match handler {
		"list_map" => {
			let [function, list] = take(arguments, name)?;
			let list = as_list(&list).map_err(Error::plain)?;
			let mut mapped = Vec::with_capacity(list.len());
			for element in list {
				mapped.push(engine.call_function(
					function.clone(),
					vec![element],
					&[],
					&crate::eval::dummy_expr(),
				)?);
			}
			Ok(Value::List(Rc::new(mapped)))
		}
		"list_filter" => {
			let [function, list] = take(arguments, name)?;
			let list = as_list(&list).map_err(Error::plain)?;
			let mut kept = Vec::new();
			for element in list {
				let keep = engine.call_function(
					function.clone(),
					vec![element.clone()],
					&[],
					&crate::eval::dummy_expr(),
				)?;
				if as_bool(&keep).map_err(Error::plain)? {
					kept.push(element);
				}
			}
			Ok(Value::List(Rc::new(kept)))
		}
		"list_reduce" => {
			let [function, list] = take(arguments, name)?;
			let list = as_list(&list).map_err(Error::plain)?;
			let mut values = list.into_iter();
			let mut accumulated = values
				.next()
				.ok_or_else(|| Error::plain("list:reduce of an empty list".into()))?;
			for element in values {
				accumulated = engine.call_function(
					function.clone(),
					vec![accumulated, element],
					&[],
					&crate::eval::dummy_expr(),
				)?;
			}
			Ok(accumulated)
		}
		"list_fold" => {
			let [function, initial, list] = take(arguments, name)?;
			let list = as_list(&list).map_err(Error::plain)?;
			let mut accumulated = initial;
			for element in list {
				accumulated = engine.call_function(
					function.clone(),
					vec![accumulated, element],
					&[],
					&crate::eval::dummy_expr(),
				)?;
			}
			Ok(accumulated)
		}
		_ => dispatch_runtime(handler, name, arguments, engine),
	}
}

/// Serves the operations that write output, read the engine state, or work on the script alphabet:
/// `print`, `echo`, `write`, `check`, `assert`, `get:witness`, and the `alphabet:` operations.
fn dispatch_runtime(
	handler: &str,
	name: &str,
	arguments: Vec<Value>,
	engine: &mut Engine,
) -> Result<Value, Error> {
	match handler {
		"print" => {
			let [value] = take(arguments, name)?;
			let text = crate::display_value(&value, engine);
			engine.emit(&text)?;
			Ok(value)
		}
		"echo" => {
			let [value] = take(arguments, name)?;
			let text = as_string(&value).map_err(Error::plain)?;
			engine.emit(&format!("{text}\n"))?;
			Ok(value)
		}
		"write" => {
			let [value, path] = take(arguments, name)?;
			let path = as_string(&path).map_err(Error::plain)?;
			let text = crate::display_value(&value, engine);
			let path = engine.path_for_write(&path);
			std::fs::write(&path, text)
				.map_err(|e| Error::plain(format!("cannot write '{}': {}", path.display(), e)))?;
			Ok(value)
		}
		"check" => {
			// `(check condition)` or `(check "label" condition)`, which prints a `label: verdict` record.
			let (label, value) = match <[Value; 2]>::try_from(arguments) {
				Ok([label, value]) => (Some(as_string(&label).map_err(Error::plain)?), value),
				Err(arguments) => {
					let [value] = take(arguments, name)?;
					(None, value)
				}
			};
			let verdict = as_bool(&value).map_err(Error::plain)?;
			let line = match label {
				Some(label) => engine.record(&label, &verdict.to_string())?,
				None => format!("{verdict}\n"),
			};
			engine.emit(&line)?;
			Ok(value)
		}
		"assert" => {
			let [value] = take(arguments, name)?;
			let verdict = as_bool(&value).map_err(Error::plain)?;
			if !verdict {
				engine.fail_assertion();
			}
			Ok(value)
		}
		"get_witness" => Ok(engine.last_witness.clone().unwrap_or(Value::None)),
		"script_alphabet" => Ok(Value::Alphabet(Rc::new(engine.script_alphabet.clone()))),
		"alphabet_symbols" => {
			let alphabet = optional_alphabet(&arguments, engine)?;
			let symbols: Vec<Value> = alphabet.symbols().map(Value::Symbol).collect();
			Ok(Value::List(Rc::new(symbols)))
		}
		"alphabet_translate" => {
			let (alphabet, name) =
				optional_alphabet_with(arguments, engine, "alphabet:translate-symbol")?;
			let name = as_string(&name).map_err(Error::plain)?;
			alphabet
				.symbol(&name)
				.map(Value::Symbol)
				.ok_or_else(|| Error::plain(format!("unknown symbol name '{name}'")))
		}
		"alphabet_reverse_translate" => {
			let (alphabet, symbol) =
				optional_alphabet_with(arguments, engine, "alphabet:reverse-translate-symbol")?;
			let Value::Symbol(symbol) = symbol else {
				return Err(Error::plain(
					"alphabet:reverse-translate-symbol needs a Symbol".into(),
				));
			};
			alphabet
				.name(symbol)
				.map(|n| Value::String(n.to_owned()))
				.ok_or_else(|| Error::plain("the symbol has no name".into()))
		}
		_ => dispatch_nfa_inspection(handler, name, arguments, engine),
	}
}

/// Serves the operations that load, parse, print, and count an `Nfa`: `nfa:load`, `nfa:parse`,
/// `nfa:literal`, `nfa:print-to-mata`, `nfa:print-to-dot`, `nfa:number-of-states`, and
/// `nfa:number-of-transitions`.
fn dispatch_nfa_inspection(
	handler: &str,
	name: &str,
	arguments: Vec<Value>,
	engine: &mut Engine,
) -> Result<Value, Error> {
	match handler {
		"nfa_load" => {
			let [paths] = take(arguments, name)?;
			engine.load(&paths, "nfa")
		}
		"nfa_parse" => {
			let [text] = take(arguments, name)?;
			let text = as_string(&text).map_err(Error::plain)?;
			engine.parse_text(&text, "nfa")
		}
		"nfa_literal" => Err(Error::plain(
			"nfa:literal is written with its fields, as (nfa:literal :initial (…) :final (…) …)"
				.into(),
		)),
		"nfa_print_to_mata" => {
			let [value] = take(arguments, name)?;
			let nfa = expect_nfa(&value)?;
			Ok(Value::String(
				nfa.display_with(&engine.script_alphabet).to_string(),
			))
		}
		"nfa_print_to_dot" => {
			let [value] = take(arguments, name)?;
			let nfa = expect_nfa(&value)?;
			Ok(Value::String(crate::dot::write_nfa(&nfa, engine)))
		}
		"nfa_num_states" => {
			let [value] = take(arguments, name)?;
			integer(expect_nfa(&value)?.num_states())
		}
		"nfa_num_transitions" => {
			let [value] = take(arguments, name)?;
			integer(expect_nfa(&value)?.num_transitions())
		}
		_ => dispatch_nfa_transformations(handler, name, arguments, engine),
	}
}

/// Serves the operations that build a new `Nfa` from operands: the binary operations and the unary
/// transformations up to `nfa:make-complete`.
fn dispatch_nfa_transformations(
	handler: &str,
	name: &str,
	arguments: Vec<Value>,
	engine: &mut Engine,
) -> Result<Value, Error> {
	match handler {
		"nfa_union" => {
			let [lhs, rhs] = take(arguments, name)?;
			Ok(Value::Nfa(Rc::new(
				expect_nfa(&lhs)?.union(&*expect_nfa(&rhs)?),
			)))
		}
		"nfa_intersection" => {
			let [lhs, rhs] = take(arguments, name)?;
			Ok(Value::Nfa(Rc::new(
				expect_nfa(&lhs)?.intersection(&*expect_nfa(&rhs)?),
			)))
		}
		"nfa_difference" => {
			let [lhs, rhs] = take(arguments, name)?;
			Ok(Value::Nfa(Rc::new(
				expect_nfa(&lhs)?.difference(&*expect_nfa(&rhs)?),
			)))
		}
		"nfa_concat" => {
			let [lhs, rhs] = take(arguments, name)?;
			let (lhs, rhs) = (expect_nfa(&lhs)?, expect_nfa(&rhs)?);
			Ok(Value::Nfa(Rc::new(
				if optional::<bool>(engine, "use-epsilon") == Some(true) {
					lhs.concat_over_epsilon(&rhs)
				} else {
					lhs.concat(&rhs)
				},
			)))
		}
		"nfa_complement" => {
			let [value] = take(arguments, name)?;
			let algorithm = match hint_value(engine, name, "algorithm") {
				"brzozowski" => Complementation::Brzozowski,
				_ => Complementation::Classical,
			};
			Ok(Value::Nfa(Rc::new(
				expect_nfa(&value)?.complement_with(&engine.script_alphabet, algorithm),
			)))
		}
		"nfa_determinize" => {
			let [value] = take(arguments, name)?;
			Ok(Value::Nfa(Rc::new(expect_nfa(&value)?.determinize())))
		}
		"nfa_minimize" => {
			let [value] = take(arguments, name)?;
			let algorithm = match hint_value(engine, name, "algorithm") {
				"brzozowski" => Minimization::Brzozowski,
				_ => Minimization::Hopcroft,
			};
			Ok(Value::Nfa(Rc::new(
				expect_nfa(&value)?.minimize_with(algorithm),
			)))
		}
		"nfa_reduce" => {
			let [value] = take(arguments, name)?;
			let reduction = if hint_value(engine, name, "algorithm") == "residual" {
				Reduction::Residual {
					construction: if hint_value(engine, name, "type") == "with" {
						ResidualConstruction::During
					} else {
						ResidualConstruction::After
					},
					direction: if hint_value(engine, name, "direction") == "backward" {
						Direction::Backward
					} else {
						Direction::Forward
					},
				}
			} else {
				Reduction::Simulation
			};
			Ok(Value::Nfa(Rc::new(
				expect_nfa(&value)?.reduce_with(reduction),
			)))
		}
		"nfa_reverse" => {
			let [value] = take(arguments, name)?;
			Ok(Value::Nfa(Rc::new(expect_nfa(&value)?.reverse())))
		}
		"nfa_remove_epsilon" => {
			let [value] = take(arguments, name)?;
			let mut nfa = (*expect_nfa(&value)?).clone();
			nfa.remove_epsilon();
			Ok(Value::Nfa(Rc::new(nfa)))
		}
		"nfa_trim" => {
			let [value] = take(arguments, name)?;
			let mut nfa = (*expect_nfa(&value)?).clone();
			nfa.trim();
			Ok(Value::Nfa(Rc::new(nfa)))
		}
		"nfa_make_complete" => {
			let [value] = take(arguments, name)?;
			let mut nfa = (*expect_nfa(&value)?).clone();
			nfa.make_complete(&engine.script_alphabet);
			Ok(Value::Nfa(Rc::new(nfa)))
		}
		_ => dispatch_nfa_bridge_transformations(handler, name, arguments, engine),
	}
}

/// Serves the transformations the `mata` crate gained with the bridge functions of these operations:
/// `nfa:as-dfa` through `nfa:get-one-letter-automaton`.
fn dispatch_nfa_bridge_transformations(
	handler: &str,
	name: &str,
	arguments: Vec<Value>,
	engine: &mut Engine,
) -> Result<Value, Error> {
	match handler {
		"nfa_as_dfa" => {
			let [value] = take(arguments, name)?;
			Ok(Value::Nfa(Rc::new(expect_nfa(&value)?.as_dfa())))
		}
		"nfa_nth_power" => {
			let [value, power] = take(arguments, name)?;
			let power = u32::try_from(length(
				as_int(&power)?,
				"nfa:concatenate-nth-power needs a non-negative power".into(),
			)?)
			.map_err(|_| {
				Error::plain("nfa:concatenate-nth-power takes a power below 2^32".into())
			})?;
			Ok(Value::Nfa(Rc::new(
				expect_nfa(&value)?.concatenate_nth_power(power),
			)))
		}
		"nfa_union_det_complete" => {
			let [lhs, rhs] = take(arguments, name)?;
			let nfa = expect_nfa(&lhs)?
				.union_det_complete(&*expect_nfa(&rhs)?, &engine.script_alphabet)
				.map_err(|error| unmet(&error))?;
			Ok(Value::Nfa(Rc::new(nfa)))
		}
		"nfa_product" => {
			let [lhs, rhs] = take(arguments, name)?;
			let condition = match optional::<String>(engine, "final-condition").as_deref() {
				None | Some("and") => ProductFinalCondition::And,
				Some("or") => ProductFinalCondition::Or,
				Some(other) => {
					return Err(Error::plain(format!(
						":final-condition of nfa:product is \"and\" or \"or\", not \"{other}\""
					)));
				}
			};
			let first_epsilon =
				optional::<Symbol>(engine, "first-epsilon").unwrap_or(Symbol::EPSILON);
			Ok(Value::Nfa(Rc::new(expect_nfa(&lhs)?.product(
				&*expect_nfa(&rhs)?,
				condition,
				first_epsilon,
			))))
		}
		"nfa_left_quotient" => {
			let [lhs, rhs] = take(arguments, name)?;
			let first_epsilon =
				optional::<Symbol>(engine, "first-epsilon").unwrap_or(Symbol::EPSILON);
			Ok(Value::Nfa(Rc::new(
				expect_nfa(&lhs)?.left_quotient(&*expect_nfa(&rhs)?, first_epsilon),
			)))
		}
		"nfa_right_quotient" => {
			let [lhs, rhs] = take(arguments, name)?;
			let first_epsilon =
				optional::<Symbol>(engine, "first-epsilon").unwrap_or(Symbol::EPSILON);
			Ok(Value::Nfa(Rc::new(
				expect_nfa(&lhs)?.right_quotient(&*expect_nfa(&rhs)?, first_epsilon),
			)))
		}
		"nfa_decode_utf8" => {
			let [value] = take(arguments, name)?;
			let nfa = expect_nfa(&value)?
				.decode_utf8()
				.map_err(|error| unmet(&error))?;
			Ok(Value::Nfa(Rc::new(nfa)))
		}
		"nfa_one_letter" => {
			let [value] = take(arguments, name)?;
			let abstract_symbol = optional::<Symbol>(engine, "abstract-symbol")
				.unwrap_or_else(|| Symbol::new(u32::from(b'x')));
			Ok(Value::Nfa(Rc::new(
				expect_nfa(&value)?.one_letter_automaton(abstract_symbol),
			)))
		}
		_ => dispatch_nfa_predicates(handler, name, arguments, engine),
	}
}

/// The verdict of a decision that `holds` makes without a counterexample, or, when the script asks for
/// witnesses, that `find` makes by searching one; the counterexample becomes the witness.
fn decide(
	engine: &mut Engine,
	holds: impl FnOnce(&Engine) -> bool,
	find: impl FnOnce(&Engine) -> Option<Word>,
) -> Value {
	if !(engine.options.produce_witnesses.on() || engine.witness_requested.on()) {
		engine.last_witness = None;
		return Value::Bool(holds(engine));
	}
	let word = find(engine);
	let result = word.is_none();
	engine.last_witness = word.map(Value::Word);
	Value::Bool(result)
}

/// Serves the operations that decide a property of an `Nfa` and keep the witness when they fail:
/// `nfa:language-empty?` through `nfa:in-language-prefix?`.
fn dispatch_nfa_predicates(
	handler: &str,
	name: &str,
	arguments: Vec<Value>,
	engine: &mut Engine,
) -> Result<Value, Error> {
	match handler {
		"nfa_lang_empty" => {
			let [value] = take(arguments, name)?;
			let nfa = expect_nfa(&value)?;
			Ok(decide(
				engine,
				|_| nfa.is_lang_empty(),
				|_| nfa.find_accepted_word(),
			))
		}
		"nfa_disjoint" => {
			let [lhs, rhs] = take(arguments, name)?;
			let lhs = expect_nfa(&lhs)?;
			let rhs = expect_nfa(&rhs)?;
			Ok(decide(
				engine,
				|_| lhs.is_disjoint_from(&rhs),
				|_| lhs.find_common_word(&rhs),
			))
		}
		"nfa_included" => {
			let [lhs, rhs] = take(arguments, name)?;
			let lhs = expect_nfa(&lhs)?;
			let rhs = expect_nfa(&rhs)?;
			let algorithm = check_algorithm(engine, name);
			Ok(decide(
				engine,
				|_| lhs.is_included_in_with(&rhs, algorithm),
				|_| lhs.find_word_not_in_with(&rhs, algorithm),
			))
		}
		"nfa_equivalent" => {
			let [lhs, rhs] = take(arguments, name)?;
			let lhs = expect_nfa(&lhs)?;
			let rhs = expect_nfa(&rhs)?;
			let algorithm = check_algorithm(engine, name);
			Ok(decide(
				engine,
				|_| lhs.is_equivalent_to_with(&rhs, algorithm),
				|_| {
					lhs.find_distinguishing_word_with(&rhs, algorithm)
						.map(|distinction| distinction.word)
				},
			))
		}
		"nfa_universal" => {
			let [value] = take(arguments, name)?;
			let nfa = expect_nfa(&value)?;
			let algorithm = check_algorithm(engine, name);
			Ok(decide(
				engine,
				|engine| nfa.is_universal_with(&engine.script_alphabet, algorithm),
				|engine| nfa.find_rejected_word_with(&engine.script_alphabet, algorithm),
			))
		}
		"nfa_in_lang" => {
			let [value, word] = take(arguments, name)?;
			let nfa = expect_nfa(&value)?;
			let word = expect_word(&word)?;
			Ok(Value::Bool(nfa.accepts(&word)))
		}
		"nfa_deterministic" => {
			let [value] = take(arguments, name)?;
			Ok(Value::Bool(expect_nfa(&value)?.is_deterministic()))
		}
		"nfa_complete" => {
			let [value] = take(arguments, name)?;
			Ok(Value::Bool(
				expect_nfa(&value)?.is_complete(&engine.script_alphabet),
			))
		}
		"nfa_minimal" => {
			let [value] = take(arguments, name)?;
			Ok(Value::Bool(expect_nfa(&value)?.is_minimal()))
		}
		"nfa_identical" => {
			let [lhs, rhs] = take(arguments, name)?;
			Ok(Value::Bool(
				expect_nfa(&lhs)?.is_identical(&*expect_nfa(&rhs)?),
			))
		}
		"nfa_acyclic" => {
			let [value] = take(arguments, name)?;
			Ok(Value::Bool(expect_nfa(&value)?.is_acyclic()))
		}
		"nfa_flat" => {
			let [value] = take(arguments, name)?;
			Ok(Value::Bool(expect_nfa(&value)?.is_flat()))
		}
		"nfa_in_lang_prefix" => {
			let [value, word] = take(arguments, name)?;
			let nfa = expect_nfa(&value)?;
			let word = expect_word(&word)?;
			let use_epsilon = optional::<bool>(engine, "use-epsilon").unwrap_or(false);
			Ok(Value::Bool(nfa.is_in_language_prefix(&word, use_epsilon)))
		}
		_ => dispatch_nfa_queries(handler, name, arguments, engine),
	}
}

/// Serves the operations that extract words from an `Nfa` or build an `Nfa` from words and regular
/// expressions: `nfa:get-word` through `nfa:encode-word`.
fn dispatch_nfa_queries(
	handler: &str,
	name: &str,
	arguments: Vec<Value>,
	engine: &mut Engine,
) -> Result<Value, Error> {
	match handler {
		"nfa_word" => {
			let [value] = take(arguments, name)?;
			Ok(expect_nfa(&value)?
				.find_accepted_word()
				.map_or(Value::None, Value::Word))
		}
		"nfa_shortest_word" => {
			let [value] = take(arguments, name)?;
			Ok(expect_nfa(&value)?
				.find_shortest_word()
				.map_or(Value::None, Value::Word))
		}
		"nfa_words" => {
			let [value, max] = take(arguments, name)?;
			let nfa = expect_nfa(&value)?;
			let max = length(
				as_int(&max)?,
				"nfa:get-words needs a non-negative bound".into(),
			)?;
			Ok(Value::List(Rc::new(
				nfa.words_up_to(max).map(Value::Word).collect(),
			)))
		}
		"nfa_run" => {
			let [value] = take(arguments, name)?;
			Ok(expect_nfa(&value)?
				.find_accepting_run()
				.map_or(Value::None, |run| Value::Run(Rc::new(run))))
		}
		_ => dispatch_nfa_bridge_queries(handler, name, arguments, engine),
	}
}

/// Serves the queries the `mata` crate gained with the bridge functions of these operations:
/// `nfa:get-word-from-complement` through `nfa:create-random-nfa-tabakov-vardi`.
fn dispatch_nfa_bridge_queries(
	handler: &str,
	name: &str,
	arguments: Vec<Value>,
	engine: &mut Engine,
) -> Result<Value, Error> {
	match handler {
		"nfa_word_from_complement" => {
			let [value] = take(arguments, name)?;
			Ok(expect_nfa(&value)?
				.find_word_from_complement(&engine.script_alphabet)
				.map_or(Value::None, Value::Word))
		}
		"nfa_word_from_language_difference" => {
			let [lhs, rhs] = take(arguments, name)?;
			Ok(expect_nfa(&lhs)?
				.find_word_from_language_difference(&*expect_nfa(&rhs)?)
				.map_or(Value::None, Value::Word))
		}
		"nfa_random_tabakov_vardi" => {
			let [states, symbols, ratio, density] = take(arguments, name)?;
			let states = length(
				as_int(&states)?,
				"nfa:create-random-nfa-tabakov-vardi needs a non-negative number of states".into(),
			)?;
			let symbols = length(
				as_int(&symbols)?,
				"nfa:create-random-nfa-tabakov-vardi needs a non-negative alphabet size".into(),
			)?;
			let ratio = as_decimal(&ratio, name, "the transition ratio")?;
			let density = as_decimal(&density, name, "the final state density")?;
			let seed = optional::<i64>(engine, "seed")
				.map(|seed| {
					u32::try_from(seed).map_err(|_| {
						Error::plain(format!(":seed of {name} must be below 2^32, not {seed}"))
					})
				})
				.transpose()?;
			let nfa = Nfa::random_tabakov_vardi(states, symbols, ratio, density, seed)
				.map_err(|error| unmet(&error))?;
			Ok(Value::Nfa(Rc::new(nfa)))
		}
		"nfa_regex" => {
			let [pattern] = take(arguments, name)?;
			let pattern = as_string(&pattern).map_err(Error::plain)?;
			let nfa = Nfa::from_regex(&pattern).map_err(|e| Error::plain(e.to_string()))?;
			Ok(Value::Nfa(Rc::new(nfa)))
		}
		"nfa_single_word" => {
			let [word] = take(arguments, name)?;
			let word = expect_word(&word)?;
			Ok(Value::Nfa(Rc::new(Nfa::from_word(&word))))
		}
		"nfa_empty_word" => Ok(Value::Nfa(Rc::new(Nfa::empty_word()))),
		"nfa_sigma_star" => Ok(Value::Nfa(Rc::new(Nfa::sigma_star(
			&engine.script_alphabet,
		)))),
		"nfa_alphabet" => {
			let mut alphabet = mata::Alphabet::new();
			for argument in arguments {
				let nfa = expect_nfa(&argument)?;
				for symbol in nfa.used_symbols() {
					// Matches C++ Nfa::fill_alphabet: names are the decimal symbol numbers.
					alphabet
						.insert(symbol)
						.map_err(|e| Error::plain(e.to_string()))?;
				}
			}
			Ok(Value::Alphabet(Rc::new(alphabet)))
		}
		"nfa_encode" => {
			let [word] = take(arguments, name)?;
			let Value::List(names) = &word else {
				return Err(Error::plain("nfa:encode-word needs a list of names".into()));
			};
			let mut symbols = Vec::new();
			for name in names.iter() {
				let name = as_string(name).map_err(Error::plain)?;
				symbols.push(engine.intern_name(&name).map_err(Error::plain)?);
			}
			Ok(Value::Word(Word::from(symbols)))
		}
		_ => dispatch_nft_inspection(handler, name, arguments, engine),
	}
}

/// Serves the operations that load, parse, print, and count an `Nft`: `nft:load`, `nft:parse`,
/// `nft:literal`, `nft:print-to-mata`, `nft:print-to-dot`, `nft:number-of-states`, and
/// `nft:number-of-transitions`.
fn dispatch_nft_inspection(
	handler: &str,
	name: &str,
	arguments: Vec<Value>,
	engine: &mut Engine,
) -> Result<Value, Error> {
	match handler {
		"nft_load" => {
			let [paths] = take(arguments, name)?;
			engine.load(&paths, "nft")
		}
		"nft_parse" => {
			let [text] = take(arguments, name)?;
			let text = as_string(&text).map_err(Error::plain)?;
			engine.parse_text(&text, "nft")
		}
		"nft_literal" => Err(Error::plain(
			"nft:literal is written with its fields, as (nft:literal :levels 2 :initial (…) …)"
				.into(),
		)),
		"nft_print_to_mata" => {
			let [value] = take(arguments, name)?;
			let nft = expect_nft(&value)?;
			Ok(Value::String(
				nft.display_with(&engine.script_alphabet).to_string(),
			))
		}
		"nft_print_to_dot" => {
			let [value] = take(arguments, name)?;
			let nft = expect_nft(&value)?;
			Ok(Value::String(crate::dot::write_nft(&nft, engine)))
		}
		"nft_num_states" => {
			let [value] = take(arguments, name)?;
			integer(expect_nft(&value)?.num_states())
		}
		"nft_num_transitions" => {
			let [value] = take(arguments, name)?;
			integer(expect_nft(&value)?.num_edges())
		}
		_ => dispatch_nft_transformations(handler, name, arguments, engine),
	}
}

/// Serves the operations that build a new `Nft` from operands: `nft:union`, `nft:intersection`,
/// `nft:concatenate`, `nft:compose`, `nft:apply`, the projections, and the unary transformations.
fn dispatch_nft_transformations(
	handler: &str,
	name: &str,
	arguments: Vec<Value>,
	engine: &mut Engine,
) -> Result<Value, Error> {
	match handler {
		"nft_union" => {
			let [lhs, rhs] = take(arguments, name)?;
			let joined = expect_nft(&lhs)?
				.union(&*expect_nft(&rhs)?)
				.map_err(|e| Error::plain(e.to_string()))?;
			Ok(Value::Nft(Rc::new(joined)))
		}
		"nft_intersection" => {
			let [lhs, rhs] = take(arguments, name)?;
			let joined = expect_nft(&lhs)?
				.intersection(&*expect_nft(&rhs)?)
				.map_err(|e| Error::plain(e.to_string()))?;
			Ok(Value::Nft(Rc::new(joined)))
		}
		"nft_concat" => {
			let [lhs, rhs] = take(arguments, name)?;
			let joined = expect_nft(&lhs)?
				.concat(&*expect_nft(&rhs)?)
				.map_err(|e| Error::plain(e.to_string()))?;
			Ok(Value::Nft(Rc::new(joined)))
		}
		"nft_compose" => {
			let [lhs, rhs] = take(arguments, name)?;
			let composed = expect_nft(&lhs)?
				.compose(&*expect_nft(&rhs)?)
				.map_err(|e| Error::plain(e.to_string()))?;
			Ok(Value::Nft(Rc::new(composed)))
		}
		"nft_apply" => {
			let [value, argument] = take(arguments, name)?;
			let nft = expect_nft(&value)?;
			let nfa = match &argument {
				Value::Word(word) => Nfa::from_word(word),
				_ => (*expect_nfa(&argument)?).clone(),
			};
			let applied = nft
				.apply(&nfa, 0)
				.map_err(|e| Error::plain(e.to_string()))?;
			Ok(Value::Nft(Rc::new(applied)))
		}
		"nft_project_out" => {
			let [value, levels] = take(arguments, name)?;
			let nft = expect_nft(&value)?;
			let levels = level_list(&levels)?;
			let projected = nft
				.project_out(&levels)
				.map_err(|e| Error::plain(e.to_string()))?;
			Ok(Value::Nft(Rc::new(projected)))
		}
		"nft_project_to" => {
			let [value, levels] = take(arguments, name)?;
			let nft = expect_nft(&value)?;
			let levels = level_list(&levels)?;
			let projected = nft
				.project_to(&levels)
				.map_err(|e| Error::plain(e.to_string()))?;
			Ok(Value::Nft(Rc::new(projected)))
		}
		"nft_invert_levels" => {
			let [value] = take(arguments, name)?;
			Ok(Value::Nft(Rc::new(expect_nft(&value)?.invert_levels())))
		}
		"nft_determinize" => {
			let [value] = take(arguments, name)?;
			Ok(Value::Nft(Rc::new(expect_nft(&value)?.determinize())))
		}
		"nft_remove_epsilon" => {
			let [value] = take(arguments, name)?;
			let mut nft = (*expect_nft(&value)?).clone();
			nft.remove_epsilon();
			Ok(Value::Nft(Rc::new(nft)))
		}
		"nft_trim" => {
			let [value] = take(arguments, name)?;
			let mut nft = (*expect_nft(&value)?).clone();
			nft.trim();
			Ok(Value::Nft(Rc::new(nft)))
		}
		_ => dispatch_nft_predicates(handler, name, arguments, engine),
	}
}

/// Serves the operations that decide a property of an `Nft` or build one from words and levels:
/// `nft:language-empty?` through `nft:create-empty-string-nft`.
fn dispatch_nft_predicates(
	handler: &str,
	name: &str,
	arguments: Vec<Value>,
	engine: &mut Engine,
) -> Result<Value, Error> {
	match handler {
		"nft_lang_empty" => {
			let [value] = take(arguments, name)?;
			Ok(Value::Bool(expect_nft(&value)?.is_lang_empty()))
		}
		"nft_included" => {
			let [lhs, rhs] = take(arguments, name)?;
			let included = expect_nft(&lhs)?
				.is_included_in(&*expect_nft(&rhs)?)
				.map_err(|e| Error::plain(e.to_string()))?;
			Ok(Value::Bool(included))
		}
		"nft_equivalent" => {
			let [lhs, rhs] = take(arguments, name)?;
			let equivalent = expect_nft(&lhs)?
				.is_equivalent_to(&*expect_nft(&rhs)?)
				.map_err(|e| Error::plain(e.to_string()))?;
			Ok(Value::Bool(equivalent))
		}
		"nft_in_lang_by_levels" => {
			let [value, words] = take(arguments, name)?;
			let nft = expect_nft(&value)?;
			let words = as_list(&words).map_err(Error::plain)?;
			let mut converted = Vec::new();
			for word in words {
				converted.push(expect_word(&word)?);
			}
			Ok(Value::Bool(nft.accepts(&converted)))
		}
		"nft_deterministic" => {
			let [value] = take(arguments, name)?;
			Ok(Value::Bool(expect_nft(&value)?.is_deterministic()))
		}
		"nft_to_nfa" => {
			let [value] = take(arguments, name)?;
			Ok(Value::Nfa(Rc::new(expect_nft(&value)?.to_nfa())))
		}
		"nft_identity" => {
			let [value, levels] = take(arguments, name)?;
			let nfa = expect_nfa(&value)?;
			let levels = length(
				as_int(&levels)?,
				"nft:from-nfa-with-levels-zero needs a level count".into(),
			)?;
			let nft = Nft::identity(&nfa, levels).map_err(|e| Error::plain(e.to_string()))?;
			Ok(Value::Nft(Rc::new(nft)))
		}
		"nft_single_word" => {
			let [word] = take(arguments, name)?;
			let word = expect_word(&word)?;
			// A path must read something, so the empty word gets Mata's empty-word transducer.
			if word.iter().all(|symbol| symbol.is_epsilon()) {
				let nft = Nft::empty_word(1).map_err(|e| Error::plain(e.to_string()))?;
				return Ok(Value::Nft(Rc::new(nft)));
			}
			let mut nft = Nft::new(1).map_err(|e| Error::plain(e.to_string()))?;
			let source = nft.add_state();
			let target = nft.add_state();
			nft.add_initial(source)
				.map_err(|e| Error::plain(e.to_string()))?;
			nft.add_final(target)
				.map_err(|e| Error::plain(e.to_string()))?;
			let path: &[&[Symbol]] = &[word.as_ref()];
			nft.add_path(source, path, target)
				.map_err(|e| Error::plain(e.to_string()))?;
			Ok(Value::Nft(Rc::new(nft)))
		}
		"nft_empty_word" => {
			let [levels] = take(arguments, name)?;
			let levels = length(
				as_int(&levels)?,
				"nft:create-empty-string-nft needs a level count".into(),
			)?;
			let nft = Nft::empty_word(levels).map_err(|e| Error::plain(e.to_string()))?;
			Ok(Value::Nft(Rc::new(nft)))
		}
		_ => dispatch_nft_properties(handler, name, arguments, engine),
	}
}

/// Serves `nft:complement` through `nft:in-language?`: the alphabet-based transformations, the
/// constructors of a fixed relation, and the membership and equality predicates.
fn dispatch_nft_properties(
	handler: &str,
	name: &str,
	arguments: Vec<Value>,
	engine: &mut Engine,
) -> Result<Value, Error> {
	let nft_error = |e: mata::NftError| Error::plain(e.to_string());
	match handler {
		"nft_complement" => {
			let [value] = take(arguments, name)?;
			let complement = expect_nft(&value)?
				.complement(&engine.script_alphabet)
				.map_err(nft_error)?;
			Ok(Value::Nft(Rc::new(complement)))
		}
		"nft_complete" => {
			let [value] = take(arguments, name)?;
			let complete = expect_nft(&value)?
				.is_complete(&engine.script_alphabet)
				.map_err(nft_error)?;
			Ok(Value::Bool(complete))
		}
		"nft_concat_power" => {
			let [value, power] = take(arguments, name)?;
			let power = u32::try_from(as_int(&power)?).map_err(|_| {
				Error::plain(format!(
					"nft:concatenate-nth-power needs a power from 0 to {}",
					u32::MAX
				))
			})?;
			Ok(Value::Nft(Rc::new(expect_nft(&value)?.concat_power(power))))
		}
		"nft_contains_jumps" => {
			let [value] = take(arguments, name)?;
			Ok(Value::Bool(expect_nft(&value)?.contains_jumps()))
		}
		"nft_sigma_star" => {
			let [levels] = take(arguments, name)?;
			let levels = length(
				as_int(&levels)?,
				"nft:create-sigma-star-nft needs a level count".into(),
			)?;
			let nft = Nft::sigma_star(levels).map_err(nft_error)?;
			Ok(Value::Nft(Rc::new(nft)))
		}
		"nft_from_interleaved" => {
			let [value, levels] = take(arguments, name)?;
			let nfa = expect_nfa(&value)?;
			let levels = length(
				as_int(&levels)?,
				"nft:from-nfa-with-levels-advancing needs a level count".into(),
			)?;
			let nft = Nft::from_interleaved(&nfa, levels).map_err(nft_error)?;
			Ok(Value::Nft(Rc::new(nft)))
		}
		"nft_epsilon_cycle" => {
			let [value] = take(arguments, name)?;
			Ok(Value::Bool(expect_nft(&value)?.has_epsilon_cycle()))
		}
		"nft_identical" => {
			let [lhs, rhs] = take(arguments, name)?;
			Ok(Value::Bool(
				expect_nft(&lhs)?.is_identical(&*expect_nft(&rhs)?),
			))
		}
		"nft_in_lang" => {
			let [value, word] = take(arguments, name)?;
			let word = expect_word(&word)?;
			// The library defaults: the whole word, and possibly epsilon cycles.
			let match_prefix = optional::<bool>(engine, "match-prefix").unwrap_or(false);
			let has_epsilon_cycles = optional::<bool>(engine, "has-epsilon-cycles").unwrap_or(true);
			let accepted = expect_nft(&value)?
				.accepts_interleaved(word.as_ref(), match_prefix, has_epsilon_cycles)
				.map_err(nft_error)?;
			Ok(Value::Bool(accepted))
		}
		_ => dispatch_nft_shape(handler, name, arguments, engine),
	}
}

/// Serves `nft:insert-level` through `nft:unwind-jumps`: the transformations of the levels and of
/// the edges of a transducer.
fn dispatch_nft_shape(
	handler: &str,
	name: &str,
	arguments: Vec<Value>,
	engine: &mut Engine,
) -> Result<Value, Error> {
	let nft_error = |e: mata::NftError| Error::plain(e.to_string());
	match handler {
		"nft_insert_level" => {
			let [value, level] = take(arguments, name)?;
			let level = length(as_int(&level)?, "levels must be non-negative".into())?;
			let inserted = expect_nft(&value)?.insert_level(level).map_err(nft_error)?;
			Ok(Value::Nft(Rc::new(inserted)))
		}
		"nft_insert_levels" => {
			let [value, mask] = take(arguments, name)?;
			let mask = as_list(&mask)?
				.iter()
				.map(as_bool)
				.collect::<Result<Vec<bool>, String>>()?;
			let inserted = expect_nft(&value)?
				.insert_levels(&mask)
				.map_err(nft_error)?;
			Ok(Value::Nft(Rc::new(inserted)))
		}
		"nft_reduce" => {
			let [value] = take(arguments, name)?;
			Ok(Value::Nft(Rc::new(expect_nft(&value)?.reduce())))
		}
		"nft_revert" => {
			let [value] = take(arguments, name)?;
			Ok(Value::Nft(Rc::new(expect_nft(&value)?.revert())))
		}
		"nft_to_nfa_update" => {
			let [value] = take(arguments, name)?;
			Ok(Value::Nfa(Rc::new(expect_nft(&value)?.to_nfa())))
		}
		"nft_universal" => {
			let [value] = take(arguments, name)?;
			Ok(Value::Bool(
				expect_nft(&value)?.is_universal(&engine.script_alphabet),
			))
		}
		"nft_unwind_jumps" => {
			let [value] = take(arguments, name)?;
			Ok(Value::Nft(Rc::new(expect_nft(&value)?.unwind_jumps())))
		}
		_ => dispatch_strings(handler, name, arguments, engine),
	}
}

/// Serves the queries of `strings:`: the words, symbols and segments of an automaton, that is Mata's
/// `mata::applications::strings` with its `seg_nfa` namespace.
fn dispatch_strings(
	handler: &str,
	name: &str,
	arguments: Vec<Value>,
	engine: &mut Engine,
) -> Result<Value, Error> {
	match handler {
		"strings_shortest_words" => {
			let [value] = take(arguments, name)?;
			Ok(word_list(strings::shortest_words(&*expect_nfa(&value)?)))
		}
		"strings_longest_words" => {
			let [value] = take(arguments, name)?;
			Ok(strings::longest_words(&*expect_nfa(&value)?).map_or(Value::None, word_list))
		}
		"strings_words_of_lengths" => {
			let [value, lengths] = take(arguments, name)?;
			let nft = expect_nft(&value)?;
			let mut converted = Vec::new();
			for item in as_list(&lengths).map_err(Error::plain)? {
				converted.push(length(
					as_int(&item)?,
					format!("{name} needs non-negative lengths"),
				)?);
			}
			let words = strings::words_of_lengths(&nft, &converted)
				.map_err(|e| Error::plain(e.to_string()))?;
			Ok(words.map_or(Value::None, word_list))
		}
		"strings_accepted_symbols" => {
			let [value] = take(arguments, name)?;
			let symbols = strings::accepted_symbols(&*expect_nfa(&value)?);
			Ok(Value::List(Rc::new(
				symbols.into_iter().map(Value::Symbol).collect(),
			)))
		}
		"strings_word_lengths" => {
			let [value] = take(arguments, name)?;
			let mut pairs = Vec::new();
			for (offset, period) in strings::word_lengths(&*expect_nfa(&value)?) {
				pairs.push(Value::List(Rc::new(vec![
					integer(offset)?,
					integer(period)?,
				])));
			}
			Ok(Value::List(Rc::new(pairs)))
		}
		"strings_lang_eps" => {
			let [value] = take(arguments, name)?;
			Ok(Value::Bool(strings::is_lang_eps(&*expect_nfa(&value)?)))
		}
		"strings_segments" => {
			let [value, epsilons] = take(arguments, name)?;
			let mut symbols = Vec::new();
			for item in as_list(&epsilons).map_err(Error::plain)? {
				symbols.push(expect_symbol(&item)?);
			}
			let segments = strings::segments(&*expect_nfa(&value)?, &symbols);
			Ok(Value::List(Rc::new(
				segments
					.into_iter()
					.map(|segment| Value::Nfa(Rc::new(segment)))
					.collect(),
			)))
		}
		"strings_reluctant_nfa" => {
			let [value] = take(arguments, name)?;
			Ok(Value::Nfa(Rc::new(strings::reluctant_nfa(&*expect_nfa(
				&value,
			)?))))
		}
		_ => dispatch_strings_replace(handler, name, arguments, engine),
	}
}

/// Serves the constructions of `strings:`: the identity and the replace transducers, Mata's
/// `mata::applications::strings::replace`.
fn dispatch_strings_replace(
	handler: &str,
	name: &str,
	arguments: Vec<Value>,
	engine: &mut Engine,
) -> Result<Value, Error> {
	let mode = || {
		if hint_value(engine, name, "replace-mode") == "single" {
			ReplaceMode::Single
		} else {
			ReplaceMode::All
		}
	};
	let replace = |result: Result<Nft, mata::ReplaceError>| {
		result
			.map(|nft| Value::Nft(Rc::new(nft)))
			.map_err(|e| Error::plain(e.to_string()))
	};
	match handler {
		"strings_identity" => {
			let [alphabet] = take(arguments, name)?;
			let levels = match optional::<i64>(engine, "num-of-levels") {
				Some(levels) => length(levels, format!("{name} needs a level count"))?,
				None => 2,
			};
			replace(strings::create_identity(
				&*expect_alphabet(&alphabet)?,
				levels,
			))
		}
		"strings_identity_replace" => {
			let [alphabet, from, replacement] = take(arguments, name)?;
			replace(strings::create_identity_with_single_symbol_replace(
				&*expect_alphabet(&alphabet)?,
				expect_symbol(&from)?,
				&symbol_or_word(&replacement)?,
				mode(),
			))
		}
		"strings_replace_regex" => {
			let [regex, replacement, alphabet] = take(arguments, name)?;
			let replacement = expect_word(&replacement)?;
			let alphabet = expect_alphabet(&alphabet)?;
			let marker =
				optional::<Symbol>(engine, "begin-marker").unwrap_or(strings::BEGIN_MARKER);
			replace(match &regex {
				Value::String(pattern) => strings::replace_reluctant_regex(
					pattern,
					&replacement,
					&alphabet,
					mode(),
					marker,
				),
				other => strings::replace_reluctant_regex_nfa(
					&*expect_nfa(other)?,
					&replacement,
					&alphabet,
					mode(),
					marker,
				),
			})
		}
		"strings_replace_literal" => {
			let [literal, replacement, alphabet] = take(arguments, name)?;
			let marker = optional::<Symbol>(engine, "end-marker").unwrap_or(strings::END_MARKER);
			replace(strings::replace_reluctant_literal(
				&expect_word(&literal)?,
				&expect_word(&replacement)?,
				&*expect_alphabet(&alphabet)?,
				mode(),
				marker,
			))
		}
		"strings_replace_symbol" => {
			let [from, replacement, alphabet] = take(arguments, name)?;
			replace(strings::replace_reluctant_single_symbol(
				expect_symbol(&from)?,
				&symbol_or_word(&replacement)?,
				&*expect_alphabet(&alphabet)?,
				mode(),
			))
		}
		_ => dispatch_automaton(handler, name, arguments, engine),
	}
}

/// Serves the `automaton:` operations, which call the `nfa:` or `nft:` operation of the same name by
/// the kind of their operands, and the operations on intermediate automata: `intermediate:` and
/// `nfa:construct` / `nft:construct`.
fn dispatch_automaton(
	handler: &str,
	name: &str,
	arguments: Vec<Value>,
	engine: &mut Engine,
) -> Result<Value, Error> {
	match handler {
		"automaton" => {
			let rest = name
				.strip_prefix("automaton:")
				.expect("an automaton: operation");
			let target = format!("{}:{rest}", automaton_namespace(name, &arguments)?);
			// A default set for the `automaton:` operation does not apply to a kind without the hint.
			let defaults = engine.defaults_of(name);
			let mut hints = Vec::new();
			for (key, value) in &engine.hints {
				match check_hint(&target, key, value) {
					Ok(()) => hints.push((key.clone(), value.clone())),
					Err(_) if defaults.contains(&(key.clone(), value.clone())) => {}
					Err(message) => return Err(Error::plain(message)),
				}
			}
			Ok(match call(&target, arguments, &hints, engine)? {
				Value::Nfa(nfa) => Value::Automaton(Rc::new(AutomatonKind::Nfa(nfa))),
				Value::Nft(nft) => Value::Automaton(Rc::new(AutomatonKind::Nft(nft))),
				other => other,
			})
		}
		"automaton_load" => {
			let [paths] = take(arguments, name)?;
			engine.load(&paths, "automaton")
		}
		"automaton_parse" => {
			let [text] = take(arguments, name)?;
			let text = as_string(&text).map_err(Error::plain)?;
			engine.parse_text(&text, "automaton")
		}
		"intermediate_parse" => {
			let [text] = take(arguments, name)?;
			let text = as_string(&text).map_err(Error::plain)?;
			let parsed = IntermediateAutomaton::parse_mata_all(&text)
				.map_err(|e| Error::plain(e.to_string()))?;
			Ok(intermediate_list(parsed))
		}
		"intermediate_mintermize" => {
			let [value] = take(arguments, name)?;
			if let Value::List(items) = &value {
				let automata = items
					.iter()
					.map(intermediate_of)
					.collect::<Result<Vec<_>, _>>()?;
				let mintermized = IntermediateAutomaton::mintermize(&automata)
					.map_err(|e| Error::plain(e.to_string()))?;
				return Ok(intermediate_list(mintermized));
			}
			let mintermized = IntermediateAutomaton::mintermize(&[intermediate_of(&value)?])
				.map_err(|e| Error::plain(e.to_string()))?
				.into_iter()
				.next()
				.expect("one mintermized automaton per input");
			Ok(Value::Intermediate(Rc::new(mintermized)))
		}
		"nfa_construct" => {
			let [value] = take(arguments, name)?;
			let nfa = intermediate_of(&value)?
				.construct_nfa(&mut engine.script_alphabet)
				.map_err(|e| Error::plain(e.to_string()))?;
			Ok(Value::Nfa(Rc::new(nfa)))
		}
		"nft_construct" => {
			let [value] = take(arguments, name)?;
			let nft = intermediate_of(&value)?
				.construct_nft(&mut engine.script_alphabet)
				.map_err(|e| Error::plain(e.to_string()))?;
			Ok(Value::Nft(Rc::new(nft)))
		}
		_ => Err(Error::plain(format!(
			"operation '{name}' is not available in the Rust bindings"
		))),
	}
}

fn word_list(words: Vec<Word>) -> Value {
	Value::List(Rc::new(words.into_iter().map(Value::Word).collect()))
}

fn expect_alphabet(value: &Value) -> Result<Rc<mata::Alphabet>, String> {
	match value {
		Value::Alphabet(alphabet) => Ok(alphabet.clone()),
		_ => Err(format!(
			"expected an Alphabet, found {}",
			crate::eval::value_sort(value)
		)),
	}
}

fn expect_symbol(value: &Value) -> Result<Symbol, String> {
	match value {
		Value::Symbol(symbol) => Ok(*symbol),
		_ => Err(format!(
			"expected a Symbol, found {}",
			crate::eval::value_sort(value)
		)),
	}
}

/// A `Symbol` operand of a library overload pair as the one-letter word of the `Word` overload.
fn symbol_or_word(value: &Value) -> Result<Word, String> {
	match value {
		Value::Symbol(symbol) => Ok(vec![*symbol]),
		other => expect_word(other),
	}
}

/// The namespace of the operation that `automaton:` operation `name` calls: `nfa` or `nft`, as its
/// automaton operands are, which must all be of one kind.
fn automaton_namespace(name: &str, arguments: &[Value]) -> Result<&'static str, Error> {
	let mut namespace = None;
	for argument in arguments {
		let kind = match argument {
			Value::Nfa(_) => "nfa",
			Value::Nft(_) => "nft",
			Value::Automaton(automaton) => match **automaton {
				AutomatonKind::Nfa(_) => "nfa",
				AutomatonKind::Nft(_) => "nft",
			},
			_ => continue,
		};
		if namespace.is_some_and(|known| known != kind) {
			return Err(Error::plain(format!(
				"'{name}' got an Nfa and an Nft; convert one with nft:from-nfa-with-levels-zero or nft:to-nfa"
			)));
		}
		namespace = Some(kind);
	}
	namespace.ok_or_else(|| Error::plain(format!("'{name}' needs an Nfa or an Nft operand")))
}

fn intermediate_list(automata: Vec<IntermediateAutomaton>) -> Value {
	Value::List(Rc::new(
		automata
			.into_iter()
			.map(|automaton| Value::Intermediate(Rc::new(automaton)))
			.collect(),
	))
}

fn intermediate_of(value: &Value) -> Result<&IntermediateAutomaton, String> {
	match value {
		Value::Intermediate(automaton) => Ok(automaton.as_ref()),
		_ => Err(format!(
			"expected an IntermediateAutomaton, found {}",
			crate::eval::value_sort(value)
		)),
	}
}

fn take<const N: usize>(values: Vec<Value>, name: &str) -> Result<[Value; N], Error> {
	values.try_into().map_err(|values: Vec<Value>| {
		Error::plain(format!(
			"'{name}' takes {} arguments, {} given",
			N,
			values.len()
		))
	})
}

/// A number a parameter of a library function takes as a floating-point value, written as a decimal string
/// in `operation`; `what` names the parameter in the error.
fn as_decimal(value: &Value, operation: &str, what: &str) -> Result<f64, Error> {
	let text = as_string(value)?;
	text.parse().map_err(|_| {
		Error::plain(format!(
			"{what} of {operation} is a decimal number, not \"{text}\""
		))
	})
}

/// A library call that a requirement of its operation stopped, as an error of the interpreter.
fn unmet(error: &UnmetRequirement) -> Error {
	Error::plain(error.message().to_owned())
}

/// A count as the script integer type; counts beyond `i64` are rejected rather than truncated.
fn integer(count: usize) -> Result<Value, Error> {
	i64::try_from(count)
		.map(Value::Int)
		.map_err(|_| Error::plain("the count does not fit a script integer".into()))
}

/// A non-negative script integer as a length, rejected with `message` when it is negative.
fn length(count: i64, message: String) -> Result<usize, Error> {
	usize::try_from(count).map_err(|_| Error::plain(message))
}

/// The alphabet an `alphabet:` operation runs on: the given `Alphabet` value, or the script alphabet.
fn optional_alphabet(arguments: &[Value], engine: &Engine) -> Result<mata::Alphabet, Error> {
	match arguments {
		[] => Ok(engine.script_alphabet.clone()),
		[Value::Alphabet(alphabet)] => Ok((**alphabet).clone()),
		[other] => Err(Error::plain(format!(
			"an alphabet operation needs an Alphabet, found {}",
			crate::eval::value_sort(other)
		))),
		_ => Err(Error::plain(
			"an alphabet operation takes at most one argument".into(),
		)),
	}
}

/// As `optional_alphabet`, with the remaining argument of a member-function call after the object.
fn optional_alphabet_with(
	mut arguments: Vec<Value>,
	engine: &Engine,
	name: &str,
) -> Result<(mata::Alphabet, Value), Error> {
	let alphabet = match arguments.first() {
		Some(Value::Alphabet(alphabet)) => {
			let alphabet = (**alphabet).clone();
			arguments.remove(0);
			alphabet
		}
		_ => engine.script_alphabet.clone(),
	};
	let [argument] = take(arguments, name)?;
	Ok((alphabet, argument))
}

fn values_equal(lhs: &Value, rhs: &Value) -> bool {
	match (lhs, rhs) {
		(Value::Bool(lhs), Value::Bool(rhs)) => lhs == rhs,
		(Value::Int(lhs), Value::Int(rhs)) => lhs == rhs,
		(Value::String(lhs), Value::String(rhs)) => lhs == rhs,
		(Value::Word(lhs), Value::Word(rhs)) => lhs == rhs,
		(Value::Symbol(lhs), Value::Symbol(rhs)) => lhs == rhs,
		(Value::None, Value::None) => true,
		_ => false,
	}
}

pub(crate) fn expect_nfa(value: &Value) -> Result<Rc<Nfa>, String> {
	match value {
		Value::Nfa(nfa) => Ok(nfa.clone()),
		Value::Automaton(automaton) => match &**automaton {
			AutomatonKind::Nfa(nfa) => Ok(nfa.clone()),
			AutomatonKind::Nft(_) => Err("expected an Nfa, found an Nft".into()),
		},
		_ => Err(format!(
			"expected an Nfa, found {}",
			crate::eval::value_sort(value)
		)),
	}
}

pub(crate) fn expect_nft(value: &Value) -> Result<Rc<Nft>, String> {
	match value {
		Value::Nft(nft) => Ok(nft.clone()),
		Value::Automaton(automaton) => match &**automaton {
			AutomatonKind::Nft(nft) => Ok(nft.clone()),
			AutomatonKind::Nfa(_) => Err("expected an Nft, found an Nfa".into()),
		},
		_ => Err(format!(
			"expected an Nft, found {}",
			crate::eval::value_sort(value)
		)),
	}
}

fn expect_word(value: &Value) -> Result<Word, String> {
	match value {
		Value::Word(word) => Ok(word.clone()),
		Value::List(list) => {
			let mut symbols = Vec::new();
			for item in list.as_slice() {
				match item {
					Value::Symbol(symbol) => symbols.push(*symbol),
					Value::String(name) => {
						return Err(format!("'{name}' must be translated with (word …)"));
					}
					_ => return Err("expected a Word or a list of Symbols".into()),
				}
			}
			Ok(Word::from(symbols))
		}
		_ => Err("expected a Word".into()),
	}
}

fn level_list(value: &Value) -> Result<Vec<usize>, String> {
	let list = as_list(value)?;
	let mut levels = Vec::new();
	for item in list {
		let level = usize::try_from(as_int(&item)?)
			.map_err(|_| "levels must be non-negative".to_owned())?;
		levels.push(level);
	}
	Ok(levels)
}
