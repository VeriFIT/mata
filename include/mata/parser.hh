/* parser.hh -- Mata Format (MF) parser.
 *
 * This file is a part of libmata.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef MATA_PARSER_HH_
#define MATA_PARSER_HH_

#include <cassert>
#include <list>
#include <map>
#include <ostream>
#include <string>
#include <vector>

#include "mata/util.hh"

/**
 * Parser from `.mata` format to automata (currently `Nfa` and `Afa` are supported).
 *
 * This includes parsing either from files or from other streams (strings, etc.).
 */
namespace Mata::Parser {

// TODO: make into a multimap
using KeyListStore = std::map<std::string, std::vector<std::string>>;
using BodyLine = std::vector<std::string>;

/** Parsed data (single section) */
struct ParsedSection {
	std::string type;
	KeyListStore dict;
	std::list<BodyLine> body;

	ParsedSection() : type(), dict(), body() {}

	/// Is the section empty?
	bool empty() const { return type.empty() && dict.empty() && body.empty(); }

	/// Equality operator
	bool operator==(const ParsedSection& rhs) const;
	bool operator!=(const ParsedSection& rhs) const { return !(*this == rhs); }

	/// subscript operator for the key-value store
	const std::vector<std::string>& operator[](const std::string& key) const;

	/// check whether the key-value store contains a key
	bool haskey(const std::string& key) const { return this->dict.end() != this->dict.find(key); }
};

/** Parsed data */
using Parsed = std::vector<ParsedSection>;

/** Parses a string into an intermediary structure */
Parsed parse_mf(const std::string& input, bool keepQuotes = false);

/** Parses a stream into an intermediary structure */
Parsed parse_mf(std::istream& input, bool keepQuotes = false);

/** Parses one section from a stream into an intermediary structure */
ParsedSection parse_mf_section(std::istream& input, bool keepQuotes = false);

/** Parses one section from a string into an intermediary structure */
ParsedSection parse_mf_section(const std::string& input, bool keepQuotes = false);

/// registers dispatcher
void init();

} // namespace Mata::Parser.

namespace std {
    /// Output stream operator
    std::ostream& operator<<(std::ostream& os, const Mata::Parser::ParsedSection& parsec);
}

#endif /* MATA_PARSER_HH_ */
