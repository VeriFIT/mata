/* alphabet.cc -- File containing alphabets for automata
 */

#include <mata/alphabet.hh>
#include <mata/nfa/types.hh>
#include <mata/nft/types.hh>
#include <mata/utils/assert.hh>

using mata::AlphabetLevels;
using mata::OnTheFlyAlphabet;
using mata::Symbol;

OnTheFlyAlphabet::OnTheFlyAlphabet(const OnTheFlyAlphabet& alphabet)
	: Alphabet(alphabet),
	  symbol_map_(alphabet.symbol_map_),
	  dense_names_(alphabet.dense_names_),
	  sparse_names_(alphabet.sparse_names_),
	  num_of_dense_symbols_(alphabet.num_of_dense_symbols_),
	  next_symbol_value_(alphabet.next_symbol_value_) {
	redirect_copied_index();
}

OnTheFlyAlphabet::OnTheFlyAlphabet(StringToSymbolMap str_sym_map) : symbol_map_(std::move(str_sym_map)) {
	for (auto it{symbol_map_.cbegin()}, end{symbol_map_.cend()}; it != end; ++it) { index_name(it); }
}

OnTheFlyAlphabet& OnTheFlyAlphabet::operator=(const OnTheFlyAlphabet& rhs) {
	if (this != &rhs) {
		Alphabet::operator=(rhs);
		symbol_map_ = rhs.symbol_map_;
		dense_names_ = rhs.dense_names_;
		sparse_names_ = rhs.sparse_names_;
		num_of_dense_symbols_ = rhs.num_of_dense_symbols_;
		next_symbol_value_ = rhs.next_symbol_value_;
		redirect_copied_index();
	}
	return *this;
}

void OnTheFlyAlphabet::redirect_copied_index() {
	// The copied pointers still point into the source map, whose names equal ours.
	for (const auto& [name, symbol] : symbol_map_) {
		auto& names{*const_cast<SymbolNames*>(find_names(symbol))};
		if (*names.name == name) { names.name = &name; }
	}
}

const OnTheFlyAlphabet::SymbolNames* OnTheFlyAlphabet::find_names(const Symbol symbol) const {
	if (symbol < dense_names_.size()) {
		const SymbolNames& names{dense_names_[symbol]};
		return names.num_of_names == 0 ? nullptr : &names;
	}
	const auto names_it{sparse_names_.find(symbol)};
	return names_it == sparse_names_.end() ? nullptr : &names_it->second;
}

void OnTheFlyAlphabet::index_name(const StringToSymbolMap::const_iterator name_it) {
	const Symbol symbol{name_it->second};
	// Grow the dense index while it stays proportional to the number of names; a few huge symbols go to the sparse one.
	if (symbol >= dense_names_.size() && symbol < 2 * symbol_map_.size() + 64) {
		Symbol covered{static_cast<Symbol>(dense_names_.size())};
		dense_names_.resize(symbol + 1);
		// Move the sparse symbols now covered by the dense index. Looking up the newly covered symbols rather than
		//  scanning the sparse index keeps the cost proportional to the growth.
		for (; covered <= symbol && !sparse_names_.empty(); ++covered) {
			if (const auto names_it{sparse_names_.find(covered)}; names_it != sparse_names_.end()) {
				dense_names_[covered] = names_it->second;
				sparse_names_.erase(names_it);
				++num_of_dense_symbols_;
			}
		}
	}
	const bool dense{symbol < dense_names_.size()};
	SymbolNames& names{dense ? dense_names_[symbol] : sparse_names_[symbol]};
	if (names.num_of_names++ == 0) {
		names.name = &name_it->first;
		if (dense) { ++num_of_dense_symbols_; }
	}
}

void OnTheFlyAlphabet::unindex_name(const StringToSymbolMap::const_iterator name_it) {
	const Symbol symbol{name_it->second};
	SymbolNames& names{*const_cast<SymbolNames*>(find_names(symbol))};
	if (--names.num_of_names == 0) {
		if (symbol < dense_names_.size()) {
			names.name = nullptr;
			--num_of_dense_symbols_;
		} else {
			sparse_names_.erase(symbol);
		}
		return;
	}
	if (names.name != &name_it->first) { return; }
	// The reported name goes away; report another name of the same symbol. Only shared symbols pay for this scan.
	for (auto it{symbol_map_.cbegin()}, end{symbol_map_.cend()}; it != end; ++it) {
		if (it->second == symbol && it != name_it) {
			names.name = &it->first;
			return;
		}
	}
}

mata::utils::OrdVector<Symbol> OnTheFlyAlphabet::get_alphabet_symbols() const {
	utils::OrdVector<Symbol> symbols{};
	symbols.reserve(num_of_dense_symbols_ + sparse_names_.size());
	// Erased symbols leave their slots behind; stop at the last named one.
	for (Symbol symbol{0}; symbols.size() < num_of_dense_symbols_ && symbol < dense_names_.size(); ++symbol) {
		if (dense_names_[symbol].num_of_names != 0) { symbols.push_back(symbol); }
	}
	// Sparse symbols all lie above the dense ones.
	std::vector<Symbol> sparse_symbols{};
	sparse_symbols.reserve(sparse_names_.size());
	for (const Symbol symbol : sparse_names_ | std::views::keys) { sparse_symbols.push_back(symbol); }
	std::ranges::sort(sparse_symbols);
	for (const Symbol symbol : sparse_symbols) { symbols.push_back(symbol); }
	return symbols;
} // OnTheFlyAlphabet::get_alphabet_symbols.

mata::utils::OrdVector<Symbol> OnTheFlyAlphabet::get_complement(const mata::utils::OrdVector<Symbol>& symbols) const {
	return get_alphabet_symbols().difference(symbols);
}

void OnTheFlyAlphabet::add_symbols_from(const StringToSymbolMap& new_symbol_map) {
	for (const auto& [symbol_name, symbol] : new_symbol_map) {
		update_next_symbol_value(symbol);
		try_add_new_symbol(symbol_name, symbol);
	}
}

std::string mata::OnTheFlyAlphabet::reverse_translate_symbol(const Symbol symbol) const {
	if (const SymbolNames* const names{find_names(symbol)}; names != nullptr) { return *names->name; }
	throw std::runtime_error("symbol '" + std::to_string(symbol) + "' is out of range of enumeration");
}

void mata::OnTheFlyAlphabet::add_symbols_from(const std::vector<std::string>& symbol_names) {
	for (const std::string& symbol_name : symbol_names) { add_new_symbol(symbol_name); }
}

Symbol mata::OnTheFlyAlphabet::translate_symb(const std::string& str) {
	// The reserved special-symbol names translate straight to the special symbols and are never registered as ordinary
	// symbols, so a printed name always parses back to the same symbol.
	if (str == EPSILON_NAME) { return nfa::EPSILON; }
	if (str == DONT_CARE_NAME) { return nft::DONT_CARE; }
	const auto [it, inserted] = symbol_map_.try_emplace(str, next_symbol_value_);
	if (inserted) {
		index_name(it);
		return next_symbol_value_++;
	}
	return it->second;

	// TODO: How can the user specify to throw exceptions when we encounter an unknown symbol? How to specify that
	//  the alphabet should have only the previously fixed symbols?
	// auto it = symbol_map.find(str);
	// if (symbol_map.end() == it)
	//{
	//    throw std::runtime_error("unknown symbol \'" + str + "\'");
	//}

	// return it->second;
}

mata::Word mata::OnTheFlyAlphabet::translate_word(const mata::WordName& word_name) const {
	const size_t word_size{word_name.size()};
	Word word;
	word.reserve(word_size);
	for (size_t i{0}; i < word_size; ++i) {
		// The reserved special-symbol names are never registered and translate to the special symbols.
		if (word_name[i] == EPSILON_NAME) {
			word.push_back(nfa::EPSILON);
			continue;
		}
		if (word_name[i] == DONT_CARE_NAME) {
			word.push_back(nft::DONT_CARE);
			continue;
		}
		const auto symbol_mapping_it = symbol_map_.find(word_name[i]);
		if (symbol_mapping_it == symbol_map_.end()) {
			throw std::runtime_error("Unknown symbol \'" + word_name[i] + "\'");
		}
		word.push_back(symbol_mapping_it->second);
	}
	return word;
}

OnTheFlyAlphabet::InsertionResult mata::OnTheFlyAlphabet::add_new_symbol(const std::string& key) {
	InsertionResult insertion_result{try_add_new_symbol(key, next_symbol_value_)};
	if (!insertion_result.second) { // If the insertion of key-value pair failed.
		throw std::runtime_error("multiple occurrences of the same symbol");
	}
	++next_symbol_value_;
	return insertion_result;
}

OnTheFlyAlphabet::InsertionResult mata::OnTheFlyAlphabet::add_new_symbol(const std::string& key, const Symbol value) {
	InsertionResult insertion_result{try_add_new_symbol(key, value)};
	if (!insertion_result.second) { // If the insertion of key-value pair failed.
		throw std::runtime_error("multiple occurrences of the same symbol");
	}
	update_next_symbol_value(value);
	return insertion_result;
}

void mata::OnTheFlyAlphabet::update_next_symbol_value(const Symbol value) {
	if (next_symbol_value_ <= value) { next_symbol_value_ = value + 1; }
}

std::ostream& mata::OnTheFlyAlphabet::print(std::ostream& os) const {
	// symbol_map_ is unordered and names may share a value; sort by (value, name) so that the output is deterministic.
	std::vector<std::pair<std::string, Symbol>> pairs{symbol_map_.begin(), symbol_map_.end()};
	std::sort(pairs.begin(), pairs.end(), [](const auto& lhs, const auto& rhs) {
		return std::tie(lhs.second, lhs.first) < std::tie(rhs.second, rhs.first);
	});
	os << '{';
	bool first = true;
	for (const auto& [name, symbol] : pairs) {
		if (!first) { os << ", "; }
		os << name << "->" << symbol;
		first = false;
	}
	return os << '}';
}

std::ostream& mata::EnumAlphabet::print(std::ostream& os) const {
	os << '{';
	bool first = true;
	for (const Symbol symbol : symbols_) {
		if (!first) { os << ", "; }
		os << symbol;
		first = false;
	}
	return os << '}';
}

std::ostream& mata::IntAlphabet::print(std::ostream& os) const { return os << "IntAlphabet"; }

std::ostream& std::operator<<(std::ostream& os, const mata::Alphabet& alphabet) { return alphabet.print(os); }

Symbol mata::IntAlphabet::translate_symb(const std::string& symb) {
	Symbol symbol;
	std::istringstream stream{symb};
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
	std::istringstream stream{str};
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
	const size_t word_size{word_name.size()};
	mata::Word word;
	Symbol symbol;
	std::stringstream stream;
	word.reserve(word_size);
	for (const auto& str_symbol : word_name) {
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
	std::istringstream str_stream{symbol};
	Symbol converted_symbol;
	str_stream >> converted_symbol;
	add_new_symbol(converted_symbol);
}

void mata::EnumAlphabet::add_new_symbol(const Symbol symbol) {
	symbols_.insert(symbol);
	update_next_symbol_value(symbol);
}

void mata::EnumAlphabet::update_next_symbol_value(const Symbol value) {
	if (next_symbol_value_ <= value) { next_symbol_value_ = value + 1; }
}

size_t mata::EnumAlphabet::erase(const Symbol symbol) {
	const size_t num_of_erased{symbols_.erase(symbol)};
	if (symbol == next_symbol_value_ - 1) { --next_symbol_value_; }
	return num_of_erased;
}

size_t mata::OnTheFlyAlphabet::erase(const Symbol symbol) {
	const SymbolNames* const names{find_names(symbol)};
	if (names == nullptr) { return 0; }
	if (symbol == next_symbol_value_ - 1) { --next_symbol_value_; }
	const auto name_it{symbol_map_.find(*names->name)};
	unindex_name(name_it);
	symbol_map_.erase(name_it);
	return 1;
}

size_t mata::OnTheFlyAlphabet::erase(const std::string& symbol_name) {
	if (const auto found_it{symbol_map_.find(symbol_name)}; found_it != symbol_map_.end()) {
		if (found_it->second == next_symbol_value_ - 1) { --next_symbol_value_; }
		unindex_name(found_it);
		symbol_map_.erase(found_it);
		return 1;
	}
	return 0;
}

Symbol AlphabetLevels::translate_symb(const std::string& symb, const std::optional<mata::Level> level) {
	return for_level(level)->translate_symb(symb);
}

std::string
	AlphabetLevels::reverse_translate_symbol(const Symbol symbol, const std::optional<mata::Level> level) const {
	return for_level(level)->reverse_translate_symbol(symbol);
}

mata::utils::OrdVector<Symbol> AlphabetLevels::get_alphabet_symbols(const std::optional<mata::Level> level) const {
	return for_level(level)->get_alphabet_symbols();
}

mata::utils::OrdVector<Symbol> AlphabetLevels::get_complement(
	const mata::utils::OrdVector<Symbol>& symbols, const std::optional<mata::Level> level
) const {
	return for_level(level)->get_complement(symbols);
}

bool AlphabetLevels::empty(const std::optional<mata::Level> level) const {
	if (mode_ == Mode::MultiLevel && !level.has_value()) {
		// MultiLevel + nullopt: empty iff every underlying alphabet is empty.
		for (const auto& alphabet : alphabets_) {
			if (alphabet != nullptr && !alphabet->empty()) { return false; }
		}
		return true;
	}
	return for_level(level)->empty();
}

void AlphabetLevels::clear(const std::optional<mata::Level> level) {
	if (mode_ == Mode::MultiLevel && !level.has_value()) {
		// MultiLevel + nullopt: clear every underlying alphabet.
		for (const auto& alphabet : alphabets_) {
			if (alphabet != nullptr) { alphabet->clear(); }
		}
		return;
	}
	for_level(level)->clear();
}

const std::shared_ptr<mata::Alphabet>& AlphabetLevels::for_level(const std::optional<mata::Level> level) const {
	// Handle Mode::Global.
	if (mode_ == Mode::Global) {
		if (alphabets_.empty() || alphabets_[0] == nullptr) {
			throw std::runtime_error("AlphabetLevels (Global) has no underlying alphabet.");
		}
		return alphabets_[0];
	}

	// Handle Mode::MultiLevel.

	if (not level.has_value()) { throw std::runtime_error("AlphabetLevels (MultiLevel) requires an explicit level."); }

	if (*level >= alphabets_.size()) {
		throw std::runtime_error(
			"AlphabetLevels has no alphabet for level " + std::to_string(*level) + " (out of range)."
		);
	}

	if (alphabets_[*level] == nullptr) {
		throw std::runtime_error(
			"AlphabetLevels has no alphabet for level " + std::to_string(*level) + " (entry is null)."
		);
	}

	return alphabets_[*level];
}

std::shared_ptr<mata::Alphabet>& AlphabetLevels::for_level(const std::optional<mata::Level> level) {
	return const_cast<std::shared_ptr<Alphabet>&>(std::as_const(*this).for_level(level));
}

mata::Word mata::encode_word_utf8(const mata::Word& word) {
	mata::Word utf8_encoded_word;
	for (const Symbol symbol : word) {
		if (symbol < 0x80) {
			// U+0000   to U+007F  : 0xxxxxxx
			utf8_encoded_word.push_back(symbol);
		} else if (symbol < 0x8'00) {
			// U+0080   to U+07FF  : 110xxxxx 10xxxxxx
			utf8_encoded_word.push_back(0xC0 | (symbol >> 6));
			utf8_encoded_word.push_back(0x80 | (symbol & 0x3F));
		} else if (symbol < 0x1'00'00) {
			// U+0800   to U+FFFF  : 1110xxxx 10xxxxxx 10xxxxxx
			utf8_encoded_word.push_back(0xE0 | (symbol >> 12));
			utf8_encoded_word.push_back(0x80 | ((symbol >> 6) & 0x3F));
			utf8_encoded_word.push_back(0x80 | (symbol & 0x3F));
		} else if (symbol < 0x11'00'00) {
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
			MATA_ASSERT(i + 1 < word.size());
			decoded_word.push_back(((symbol & 0x1F) << 6) | (word[i + 1] & 0x3F));
			i += 1;
		} else if ((symbol & 0xF0) == 0xE0) {
			// U+0800   to U+FFFF  : 1110xxxx 10xxxxxx 10xxxxxx
			MATA_ASSERT(i + 2 < word.size());
			decoded_word.push_back(((symbol & 0x0F) << 12) | ((word[i + 1] & 0x3F) << 6) | (word[i + 2] & 0x3F));
			i += 2;
		} else if ((symbol & 0xF8) == 0xF0 && symbol < 0xF5) {
			// U+010000 to U+10FFFF: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
			MATA_ASSERT(i + 3 < word.size());
			decoded_word.push_back(
				((symbol & 0x07) << 18) | ((word[i + 1] & 0x3F) << 12) | ((word[i + 2] & 0x3F) << 6) |
				(word[i + 3] & 0x3F)
			);
			i += 3;
		} else {
			throw std::runtime_error("Invalid UTF-8 encoding.");
		}
	}
	return decoded_word;
}
