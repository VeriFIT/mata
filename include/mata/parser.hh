/* parser.hh -- VTF format parser
 *
 * Copyright (c) 2018 Ondrej Lengal <ondra.lengal@gmail.com>
 *
 * This file is a part of libmata.
 *
 * this program is free software; you can redistribute it and/or modify
 * it under the terms of the gnu general public license as published by
 * the free software foundation; either version 3 of the license, or
 * (at your option) any later version.
 *
 * this program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * merchantability or fitness for a particular purpose.  see the
 * gnu general public license for more details.
 */


#ifndef _MATA_PARSER_HH_
#define _MATA_PARSER_HH_

#include <cassert>
#include <list>
#include <map>
#include <ostream>
#include <string>
#include <vector>

// MATA headers
#include <mata/util.hh>

namespace Mata
{
namespace Parser
{

// TODO: make into a multimap
using KeyListStore = std::map<std::string, std::vector<std::string>>;
using BodyLine = std::vector<std::string>;

/** Parsed data (single section) */
struct ParsedSection
{
	std::string type;
	KeyListStore dict;
	std::list<BodyLine> body;

	ParsedSection() : type(), dict(), body()
	{  // {{{
		// TODO: definitely remove this?
		// this->dict.insert({"",{}});   // for non-found values
	} // }}}

	/// Is the section empty?
	bool empty() const { return type.empty() && dict.empty() && body.empty(); }

	/// Output stream operator
	friend std::ostream& operator<<(std::ostream& os, const ParsedSection& parsec)
	{ // {{{
		os << "@" << parsec.type << "\n";
		for (const auto& string_list_pair : parsec.dict)
		{
			os << "%" << string_list_pair.first;
			for (const std::string& str : string_list_pair.second)
			{
				os << " " << str;
			}

			os << "\n";
		}

		os << "# Body:\n";
		for (const auto& body_line : parsec.body)
		{
			bool first = true;
			for (const std::string& str : body_line)
			{
				if (!first) { os << " ";}
				first = false;
				os << str;
			}
			os << "\n";
		}

		return os;
	} // operator<< }}}

	/// Equality operator
	bool operator==(const ParsedSection& rhs) const
	{ // {{{
		return
			this->type == rhs.type &&
			this->dict == rhs.dict &&
			this->body == rhs.body;
	} // }}}
	bool operator!=(const ParsedSection& rhs) const { return !(*this == rhs); }

	/// subscript operator for the key-value store
	const std::vector<std::string>& operator[](const std::string& key) const
	{ // {{{
		auto it = this->dict.find(key);
		if (this->dict.end() == it) {
			assert(false);
			// return this->dict.at("");
		}

		return it->second;
	} // operator[] }}}

	/// check whether the key-value store contains a key
	bool haskey(const std::string& key) const
	{
		return this->dict.end() != this->dict.find(key);
	}
};



/** Parsed data */
using Parsed = std::vector<ParsedSection>;

/** Parses a string into an intermediary structure */
Parsed parse_mf(
	const std::string&  input,
	bool                keepQuotes = false);

/** Parses a stream into an intermediary structure */
Parsed parse_mf(
	std::istream&  input,
	bool           keepQuotes = false);

/** Parses one section from a stream into an intermediary structure */
ParsedSection parse_mf_section(
	std::istream&  input,
	bool           keepQuotes = false);

/** Parses one section from a string into an intermediary structure */
ParsedSection parse_mf_section(
	const std::string&  input,
	bool                keepQuotes = false);

/// registers dispatcher
void init();

// CLOSING NAMESPACES AND GUARDS
} /* Parser */
} /* Mata */

#endif /* _MATA_PARSER_HH_ */


