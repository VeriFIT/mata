/* alphabet.cc -- File containing alphabets for automata
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

#include <mata/alphabet.hh>

using Mata::Symbol;
using Mata::OnTheFlyAlphabet;

Mata::Util::OrdVector<Symbol> OnTheFlyAlphabet::get_alphabet_symbols() const {
    Util::OrdVector<Symbol> result;
    for (const auto& str_sym_pair : symbol_map) {
        result.insert(str_sym_pair.second);
    }
    return result;
} // OnTheFlyAlphabet::get_alphabet_symbols.

Mata::Util::OrdVector<Symbol> OnTheFlyAlphabet::get_complement(const Mata::Util::OrdVector<Symbol>& symbols) const {
    Mata::Util::OrdVector<Symbol> symbols_alphabet{};
    symbols_alphabet.reserve(symbol_map.size());
    for (const auto& str_sym_pair : symbol_map) {
        symbols_alphabet.insert(str_sym_pair.second);
    }
    return symbols_alphabet.difference(symbols);
}

void OnTheFlyAlphabet::add_symbols_from(const StringToSymbolMap& new_symbol_map) {
    for (const auto& symbol_binding: new_symbol_map) {
        update_next_symbol_value(symbol_binding.second);
        try_add_new_symbol(symbol_binding.first, symbol_binding.second);
    }
}

std::string Mata::OnTheFlyAlphabet::reverse_translate_symbol(const Symbol symbol) const {
    for (const auto& symbol_mapping: symbol_map) {
        if (symbol_mapping.second == symbol) {
            return symbol_mapping.first;
        }
    }
    throw std::runtime_error("symbol '" + std::to_string(symbol) + "' is out of range of enumeration");
}

void Mata::OnTheFlyAlphabet::add_symbols_from(const std::vector<std::string>& symbol_names) {
    for (const std::string& symbol_name: symbol_names) {
        add_new_symbol(symbol_name);
    }
}

Symbol Mata::OnTheFlyAlphabet::translate_symb(const std::string& str) {
    const auto it_insert_pair = symbol_map.insert({str, next_symbol_value});
    if (it_insert_pair.second) {
        return next_symbol_value++;
    } else {
        return it_insert_pair.first->second;
    }

    // TODO: How can the user specify to throw exceptions when we encounter an unknown symbol? How to specify that
    //  the alphabet should have only the previously fixed symbols?
    //auto it = symbol_map.find(str);
    //if (symbol_map.end() == it)
    //{
    //    throw std::runtime_error("unknown symbol \'" + str + "\'");
    //}

    //return it->second;
}

std::vector<Symbol> Mata::OnTheFlyAlphabet::translate_word(const std::vector<std::string>& word) const {
    const size_t word_size{ word.size() };
    std::vector<Symbol> symbols;
    symbols.reserve(word_size);
    for (size_t i{ 0 }; i < word_size; ++i) {
        const auto symbol_mapping_it = symbol_map.find(word[i]);
        if (symbol_mapping_it == symbol_map.end()) {
            throw std::runtime_error("Unknown symbol \'" + word[i] + "\'");
        }
        symbols.push_back(symbol_mapping_it->second);
    }
    return symbols;
}

OnTheFlyAlphabet::InsertionResult Mata::OnTheFlyAlphabet::add_new_symbol(const std::string& key) {
    InsertionResult insertion_result{ try_add_new_symbol(key, next_symbol_value) };
    if (!insertion_result.second) { // If the insertion of key-value pair failed.
        throw std::runtime_error("multiple occurrences of the same symbol");
    }
    ++next_symbol_value;
    return insertion_result;
}

OnTheFlyAlphabet::InsertionResult Mata::OnTheFlyAlphabet::add_new_symbol(const std::string& key, Symbol value) {
    InsertionResult insertion_result{ try_add_new_symbol(key, value) };
    if (!insertion_result.second) { // If the insertion of key-value pair failed.
        throw std::runtime_error("multiple occurrences of the same symbol");
    }
    update_next_symbol_value(value);
    return insertion_result;
}

void Mata::OnTheFlyAlphabet::update_next_symbol_value(Symbol value) {
    if (next_symbol_value <= value) {
        next_symbol_value = value + 1;
    }
}

std::ostream &std::operator<<(std::ostream &os, const Mata::Alphabet& alphabet) {
    return os << std::to_string(alphabet);
}

Symbol Mata::IntAlphabet::translate_symb(const std::string& symb) {
    Symbol symbol;
    std::istringstream stream{ symb };
    stream >> symbol;
    if (stream.fail() || !stream.eof()) {
        throw std::runtime_error("Cannot translate string '" + symb + "' to symbol.");
    }
    return symbol;
}

std::string Mata::EnumAlphabet::reverse_translate_symbol(const Symbol symbol) const {
    if (m_symbols.find(symbol) == m_symbols.end()) {
        throw std::runtime_error("Symbol '" + std::to_string(symbol) + "' is out of range of enumeration.");
    }
    return std::to_string(symbol);
}

Symbol Mata::EnumAlphabet::translate_symb(const std::string& str) {
    Symbol symbol;
    std::istringstream stream{ str };
    stream >> symbol;
    if (stream.fail() || !stream.eof()) {
        throw std::runtime_error("Cannot translate string '" + str + "' to symbol.");
    }
    if (m_symbols.find(symbol) == m_symbols.end()) {
        throw std::runtime_error("Unknown symbol'" + str + "' to be translated to Symbol.");
    }

    return symbol;
}

std::vector<Symbol> Mata::EnumAlphabet::translate_word(const std::vector<std::string>& word) const {
    const size_t word_size{ word.size() };
    std::vector<Symbol> translated_symbols;
    Symbol symbol;
    std::stringstream stream;
    translated_symbols.reserve(word_size);
    for (const auto& str_symbol: word) {
        stream << str_symbol;
        stream >> symbol;
        if (m_symbols.find(symbol) == m_symbols.end()) {
            throw std::runtime_error("Unknown symbol \'" + str_symbol + "\'");
        }
        translated_symbols.push_back(symbol);
    }
    return translated_symbols;
}

void Mata::EnumAlphabet::add_new_symbol(const std::string& symbol) {
    std::istringstream str_stream{ symbol };
    Symbol converted_symbol;
    str_stream >> converted_symbol;
    add_new_symbol(converted_symbol);
}

void Mata::EnumAlphabet::add_new_symbol(Symbol symbol) {
    m_symbols.insert(symbol);
    update_next_symbol_value(symbol);
}

void Mata::EnumAlphabet::update_next_symbol_value(Symbol value) {
    if (next_symbol_value <= value) {
        next_symbol_value = value + 1;
    }
}
