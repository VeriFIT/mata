/* alphabet.cc -- File containing alphabets for automata
 */

#include <mata/alphabet.hh>

using mata::Symbol;
using mata::OnTheFlyAlphabet;

mata::utils::OrdVector<Symbol> OnTheFlyAlphabet::get_alphabet_symbols() const {
    utils::OrdVector<Symbol> result;
    for (const auto& symbol : symbol_map_ | std::views::values) {
        result.insert(symbol);
    }
    return result;
} // OnTheFlyAlphabet::get_alphabet_symbols.

mata::utils::OrdVector<Symbol> OnTheFlyAlphabet::get_complement(const mata::utils::OrdVector<Symbol>& symbols) const {
    mata::utils::OrdVector<Symbol> symbols_alphabet{};
    symbols_alphabet.reserve(symbol_map_.size());
    for (const auto& symbol : symbol_map_ | std::views::values) {
        symbols_alphabet.insert(symbol);
    }
    return symbols_alphabet.difference(symbols);
}

void OnTheFlyAlphabet::add_symbols_from(const StringToSymbolMap& new_symbol_map) {
    for (const auto& [symbol_name, symbol]: new_symbol_map) {
        update_next_symbol_value(symbol);
        try_add_new_symbol(symbol_name, symbol);
    }
}

std::string mata::OnTheFlyAlphabet::reverse_translate_symbol(const Symbol symbol) const {
    for (const auto& [symbol_name, symbol_val]: symbol_map_) {
        if (symbol_val == symbol) {
            return symbol_name;
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
    const auto [it, inserted] = symbol_map_.insert({str, next_symbol_value_});
    if (inserted) {
        return next_symbol_value_++;
    }
    return it->second;

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

OnTheFlyAlphabet::InsertionResult mata::OnTheFlyAlphabet::add_new_symbol(const std::string& key, const Symbol value) {
    InsertionResult insertion_result{ try_add_new_symbol(key, value) };
    if (!insertion_result.second) { // If the insertion of key-value pair failed.
        throw std::runtime_error("multiple occurrences of the same symbol");
    }
    update_next_symbol_value(value);
    return insertion_result;
}

void mata::OnTheFlyAlphabet::update_next_symbol_value(const Symbol value) {
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

void mata::EnumAlphabet::add_new_symbol(const Symbol symbol) {
    symbols_.insert(symbol);
    update_next_symbol_value(symbol);
}

void mata::EnumAlphabet::update_next_symbol_value(const Symbol value) {
    if (next_symbol_value_ <= value) {
        next_symbol_value_ = value + 1;
    }
}

size_t mata::EnumAlphabet::erase(const Symbol symbol) {
    const size_t num_of_erased{ symbols_.erase(symbol) };
    if (symbol == next_symbol_value_ - 1) {
        --next_symbol_value_;
    }
    return num_of_erased;
}

size_t mata::OnTheFlyAlphabet::erase(const Symbol symbol) {
    const auto symbol_map_end = symbol_map_.end();
    for (auto it = symbol_map_.begin(); it != symbol_map_end; ++it) {
        if (it->second == symbol) {
            if (symbol == next_symbol_value_ - 1) {
                --next_symbol_value_;
            }
            symbol_map_.erase(it);
            return 1;
        }
    }
    return 0;
}

size_t mata::OnTheFlyAlphabet::erase(const std::string& symbol_name) {
    if (const auto found_it{ symbol_map_.find(symbol_name) }; found_it != symbol_map_.end()) {
        if (found_it->second == next_symbol_value_ - 1) { --next_symbol_value_; }
        symbol_map_.erase(found_it);
        return 1;
    }
    return 0;
}

mata::Word mata::encode_word_utf8(const mata::Word& word) {
    mata::Word utf8_encoded_word;
    for (const Symbol symbol: word) {
        if (symbol < 0x80) {
            // U+0000   to U+007F  : 0xxxxxxx
            utf8_encoded_word.push_back(symbol);
        } else if (symbol < 0x800) {
            // U+0080   to U+07FF  : 110xxxxx 10xxxxxx
            utf8_encoded_word.push_back(0xC0 | (symbol >> 6));
            utf8_encoded_word.push_back(0x80 | (symbol & 0x3F));
        } else if (symbol < 0x10000) {
            // U+0800   to U+FFFF  : 1110xxxx 10xxxxxx 10xxxxxx
            utf8_encoded_word.push_back(0xE0 | (symbol >> 12));
            utf8_encoded_word.push_back(0x80 | ((symbol >> 6) & 0x3F));
            utf8_encoded_word.push_back(0x80 | (symbol & 0x3F));
        } else if (symbol < 0x110000) {
            // U+010000 to U+10FFFF: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
            utf8_encoded_word.push_back(0xF0 | (symbol >> 18));
            utf8_encoded_word.push_back(0x80 | ((symbol >> 12) & 0x3F));
            utf8_encoded_word.push_back(0x80 | ((symbol >> 6) & 0x3F));
            utf8_encoded_word.push_back(0x80 | (symbol & 0x3F));
        } else {
            throw std::runtime_error("Symbol " + std::to_string(symbol) + " is out of range for UTF-8 encoding.");
        }
    }
    return utf8_encoded_word;
}

mata::Word mata::decode_word_utf8(const mata::Word& word) {
    mata::Word decoded_word;
    for (size_t i = 0; i < word.size(); i++) {
        if (Symbol symbol = word[i]; (symbol & 0x80) == 0) {
            // U+0000   to U+007F  : 0xxxxxxx
            decoded_word.push_back(symbol);
        } else if ((symbol & 0xE0) == 0xC0) {
            // U+0080   to U+07FF  : 110xxxxx 10xxxxxx
            assert(i + 1 < word.size());
            decoded_word.push_back(((symbol & 0x1F) << 6) | (word[i+1] & 0x3F));
            i += 1;
        } else if ((symbol & 0xF0) == 0xE0) {
            // U+0800   to U+FFFF  : 1110xxxx 10xxxxxx 10xxxxxx
            assert(i + 2 < word.size());
            decoded_word.push_back(((symbol & 0x0F) << 12) | ((word[i+1] & 0x3F) << 6) | (word[i+2] & 0x3F));
            i += 2;
        } else if ((symbol & 0xF8) == 0xF0 && symbol < 0xF5) {
            // U+010000 to U+10FFFF: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
            assert(i + 3 < word.size());
            decoded_word.push_back(((symbol & 0x07) << 18) | ((word[i+1] & 0x3F) << 12) | ((word[i+2] & 0x3F) << 6) | (word[i+3] & 0x3F));
            i += 3;
        } else {
            throw std::runtime_error("Invalid UTF-8 encoding.");
        }
    }
    return decoded_word;
}
