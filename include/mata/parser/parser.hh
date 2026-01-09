/* parser.hh -- Mata Format (MF) parser.
 */

#ifndef MATA_PARSER_HH_
#define MATA_PARSER_HH_

#include <list>
#include <map>
#include <ostream>
#include <string>
#include <vector>

/**
 * Parser from `.mata` format to automata (currently `Nfa` and `Afa` are supported).
 *
 * This includes parsing either from files or from other streams (strings, etc.).
 */
namespace mata::parser {

// TODO: make into a multimap
using KeyListStore = std::map<std::string, std::vector<std::string>>;
using BodyLine = std::vector<std::string>;

/** Parsed data (single section) */
struct ParsedSection {
    std::string type{};
    KeyListStore dict{};
    std::list<BodyLine> body{};

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

/// registers dispatcher
void init();

} // namespace mata::Parser.

namespace std {
    /// Output stream operator
    std::ostream& operator<<(std::ostream& os, const mata::parser::ParsedSection& parsec);
}

#endif /* MATA_PARSER_HH_ */
