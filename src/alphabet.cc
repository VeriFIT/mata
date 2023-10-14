/* alphabet.cc -- File containing alphabets for automata
 */

#include <mata/alphabet.hh>

using mata::Symbol;
using mata::OnTheFlyAlphabet;

mata::utils::OrdVector<Symbol> OnTheFlyAlphabet::get_alphabet_symbols() const {
    utils::OrdVector<Symbol> result;
    for (const auto& str_sym_pair: symbol_map_) {
        result.insert(str_sym_pair.second);
    }
    return result;
} // OnTheFlyAlphabet::get_alphabet_symbols.

mata::utils::OrdVector<Symbol> OnTheFlyAlphabet::get_complement(const mata::utils::OrdVector<Symbol>& symbols) const {
    mata::utils::OrdVector<Symbol> symbols_alphabet{};
    symbols_alphabet.reserve(symbol_map_.size());
    for (const auto& str_sym_pair : symbol_map_) {
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

std::string mata::OnTheFlyAlphabet::reverse_translate_symbol(const Symbol symbol) const {
    for (const auto& symbol_mapping: symbol_map_) {
        if (symbol_mapping.second == symbol) {
            return symbol_mapping.first;
        }
    }
    throw std::runtime_error("symbol '" + std::to_string(symbol) + "' is out of range of enumeration");
}

void mata::OnTheFlyAlphabet::add_symbols_from(const std::vector<std::string>& symbol_names) {
    for (const std::string& symbol_name: symbol_names) {
        add_new_symbol(symbol_name);
    }
}

Symbol mata::OnTheFlyAlphabet::translate_symb(const std::string& str) {
    const auto it_insert_pair = symbol_map_.insert({str, next_symbol_value_});
    if (it_insert_pair.second) {
        return next_symbol_value_++;
    }
    return it_insert_pair.first->second;

    // TODO: How can the user specify to throw exceptions when we encounter an unknown symbol? How to specify that
    //  the alphabet should have only the previously fixed symbols?
    //auto it = symbol_map.find(str);
    //if (symbol_map.end() == it)
    //{
    //    throw std::runtime_error("unknown symbol \'" + str + "\'");
    //}

    //return it->second;
}

mata::Word mata::OnTheFlyAlphabet::translate_word(const mata::WordName& word_name) const {
    const size_t word_size{ word_name.size() };
    Word word;
    word.reserve(word_size);
    for (size_t i{ 0 }; i < word_size; ++i) {
        const auto symbol_mapping_it = symbol_map_.find(word_name[i]);
        if (symbol_mapping_it == symbol_map_.end()) {
            throw std::runtime_error("Unknown symbol \'" + word_name[i] + "\'");
        }
        word.push_back(symbol_mapping_it->second);
    }
    return word;
}

OnTheFlyAlphabet::InsertionResult mata::OnTheFlyAlphabet::add_new_symbol(const std::string& key) {
    InsertionResult insertion_result{ try_add_new_symbol(key, next_symbol_value_) };
    if (!insertion_result.second) { // If the insertion of key-value pair failed.
        throw std::runtime_error("multiple occurrences of the same symbol");
    }
    ++next_symbol_value_;
    return insertion_result;
}

OnTheFlyAlphabet::InsertionResult mata::OnTheFlyAlphabet::add_new_symbol(const std::string& key, Symbol value) {
    InsertionResult insertion_result{ try_add_new_symbol(key, value) };
    if (!insertion_result.second) { // If the insertion of key-value pair failed.
        throw std::runtime_error("multiple occurrences of the same symbol");
    }
    update_next_symbol_value(value);
    return insertion_result;
}

void mata::OnTheFlyAlphabet::update_next_symbol_value(Symbol value) {
    if (next_symbol_value_ <= value) {
        next_symbol_value_ = value + 1;
    }
}

std::ostream &std::operator<<(std::ostream &os, const mata::Alphabet& alphabet) {
    return os << std::to_string(alphabet);
}

Symbol mata::IntAlphabet::translate_symb(const std::string& symb) {
    Symbol symbol;
    std::istringstream stream{ symb };
    stream >> symbol;
    if (stream.fail() || !stream.eof()) {
        throw std::runtime_error("Cannot translate string '" + symb + "' to symbol.");
    }
    return symbol;
}

std::string mata::EnumAlphabet::reverse_translate_symbol(const Symbol symbol) const {
    if (symbols_.find(symbol) == symbols_.end()) {
        throw std::runtime_error("Symbol '" + std::to_string(symbol) + "' is out of range of enumeration.");
    }
    return std::to_string(symbol);
}

Symbol mata::EnumAlphabet::translate_symb(const std::string& str) {
    Symbol symbol;
    std::istringstream stream{ str };
    stream >> symbol;
    if (stream.fail() || !stream.eof()) {
        throw std::runtime_error("Cannot translate string '" + str + "' to symbol.");
    }
    if (symbols_.find(symbol) == symbols_.end()) {
        throw std::runtime_error("Unknown symbol'" + str + "' to be translated to Symbol.");
    }

    return symbol;
}

mata::Word mata::EnumAlphabet::translate_word(const mata::WordName& word_name) const {
    const size_t word_size{ word_name.size() };
    mata::Word word;
    Symbol symbol;
    std::stringstream stream;
    word.reserve(word_size);
    for (const auto& str_symbol: word_name) {
        stream << str_symbol;
        stream >> symbol;
        if (symbols_.find(symbol) == symbols_.end()) {
            throw std::runtime_error("Unknown symbol \'" + str_symbol + "\'");
        }
        word.push_back(symbol);
    }
    return word;
}

void mata::EnumAlphabet::add_new_symbol(const std::string& symbol) {
    std::istringstream str_stream{ symbol };
    Symbol converted_symbol;
    str_stream >> converted_symbol;
    add_new_symbol(converted_symbol);
}

void mata::EnumAlphabet::add_new_symbol(Symbol symbol) {
    symbols_.insert(symbol);
    update_next_symbol_value(symbol);
}

void mata::EnumAlphabet::update_next_symbol_value(Symbol value) {
    if (next_symbol_value_ <= value) {
        next_symbol_value_ = value + 1;
    }
}
