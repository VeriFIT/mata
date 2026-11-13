/* parser.hh -- Mata Format (MF) parser.
 */

#ifndef MATA_PARSER_HH_
#define MATA_PARSER_HH_

#include <list>
#include <map>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

/**
 * Parser from `.mata` format to automata (currently `Nfa` and `Nft` are supported).
 *
 * This includes parsing either from files or from other streams (strings, etc.).
 */
namespace mata::parser {

using KeyListStore = std::map<std::string, std::vector<std::string>>;

/**
 * One token of a body line, with the flag telling whether the reader read it in quotes.
 * @c create_node() lets a quoted token be an operand even when it starts with @c & @c | or @c !, which the
 * unquoted reader would take for an operator of the transition formula.
 */
struct BodyToken {
	std::string token{};
	bool quoted{false};

	BodyToken() = default;
	BodyToken(std::string token, const bool quoted) : token{std::move(token)}, quoted{quoted} {}
	/// An unquoted token, so that a body line reads as a list of plain names in tests and call sites.
	BodyToken(const char* token) : token{token} {}
	bool operator==(const BodyToken& rhs) const = default;
};

inline std::ostream& operator<<(std::ostream& os, const BodyToken& token) { return os << token.token; }

using BodyLine = std::vector<BodyToken>;

/** Parsed data (single section) */
struct ParsedSection {
	std::string type{};
	KeyListStore dict{};
	std::vector<BodyLine> body{};

	ParsedSection() = default;

	/// Is the section empty?
	bool empty() const { return type.empty() && dict.empty() && body.empty(); }

	/// Equality operator
	bool operator==(const ParsedSection& rhs) const;

	/// subscript operator for the key-value store
	const std::vector<std::string>& operator[](const std::string& key) const;

	/// check whether the key-value store contains a key
	bool haskey(const std::string& key) const { return dict.contains(key); }
};

/** Parsed data */
using Parsed = std::vector<ParsedSection>;

/** Parses a string into an intermediary structure */
Parsed parse_mf(const std::string& input, bool keep_quotes = false);

/** Parses a stream into an intermediary structure */
Parsed parse_mf(std::istream& input, bool keep_quotes = false);

/** Parses one section from a stream into an intermediary structure */
ParsedSection parse_mf_section(std::istream& input, bool keep_quotes = false);

/** Parses one section from a string into an intermediary structure */
ParsedSection parse_mf_section(const std::string& input, bool keep_quotes = false);

/** Parses one section from the beginning of @p input into an intermediary structure, advancing @p input past it. */
ParsedSection parse_mf_section_view(std::string_view& input, bool keep_quotes = false);

/// registers dispatcher
void init();

} // namespace mata::parser

namespace std {
/// Output stream operator
std::ostream& operator<<(std::ostream& os, const mata::parser::ParsedSection& parsec);
} // namespace std

#endif /* MATA_PARSER_HH_ */
