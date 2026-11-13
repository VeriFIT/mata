#include "mata-sys/cxx/common.hh"

#include <algorithm>
#include <charconv>
#include <mutex>
#include <optional>
#include <utility>

#include "mata/nft/types.hh"
#include "mata/parser/mintermization.hh"

namespace mata_sys::detail {

NameTable::NameTable(const rust::Slice<const rust::String> names, const rust::Slice<const Symbol> symbols) {
	if (names.size() != symbols.size()) { throw std::invalid_argument("names and symbols differ in length"); }
	for (std::size_t i = 0; i < names.size(); ++i) {
		auto name = std::string(names[i]);
		by_symbol_.emplace(symbols[i], name);
		by_name_.emplace(std::move(name), symbols[i]);
	}
}

Symbol NameTable::translate_symb(const std::string_view name) {
	const auto found = by_name_.find(name);
	if (found == by_name_.end()) { throw std::runtime_error("unknown symbol name '" + std::string(name) + "'"); }
	return found->second;
}

std::string NameTable::reverse_translate_symbol(const Symbol symbol) const {
	const auto found = by_symbol_.find(symbol);
	return found == by_symbol_.end() ? std::to_string(symbol) : found->second;
}

SymbolSet NameTable::get_alphabet_symbols() const {
	auto symbols = std::vector<Symbol>();
	symbols.reserve(by_symbol_.size());
	for (const auto& [symbol, name] : by_symbol_) { symbols.push_back(symbol); }
	return SymbolSet(std::move(symbols));
}

std::vector<mata::IntermediateAut> mintermize(const std::vector<const mata::IntermediateAut*>& automata) {
	// CUDD swaps a process-wide out-of-memory handler, so one thread at a time.
	static auto cudd_mutex = std::mutex();
	const auto cudd_lock = std::lock_guard(cudd_mutex);
	auto mintermization = mata::Mintermization();
	return mintermization.mintermize(automata);
}
void check_delta(const mata::Automaton& automaton) {
	const State num_states = static_cast<State>(automaton.num_of_states());
	for (State source = 0; source < automaton.delta.num_of_states(); ++source) {
		const auto& state_post = automaton.delta[source];
		auto previous = std::optional<Symbol>();
		for (const auto& symbol_post : state_post) {
			if (previous.has_value() && symbol_post.symbol <= *previous) {
				throw std::runtime_error(
					"the transitions of state " + std::to_string(source) + " are not sorted by symbol"
				);
			}
			previous = symbol_post.symbol;
			if (symbol_post.targets.empty()) {
				throw std::runtime_error("state " + std::to_string(source) + " has a symbol without targets");
			}
			if (!std::ranges::is_sorted(symbol_post.targets) ||
				std::ranges::adjacent_find(symbol_post.targets) != symbol_post.targets.end()) {
				throw std::runtime_error(
					"the targets of state " + std::to_string(source) + " are not sorted and unique"
				);
			}
			if (symbol_post.targets.back() >= num_states) {
				throw std::runtime_error("state " + std::to_string(source) + " has a target that does not exist");
			}
		}
	}
}

Symbol NumberingAlphabet::translate_symb(const std::string_view str) {
	// More than 10 digits is more than any symbol. A number is written without
	// leading zeros, so `01` is a name, as it is to Mata.
	if (str.empty() || str.size() > 10 || (str.size() > 1 && str[0] == '0') ||
		!std::ranges::all_of(str, [](const char c) { return c >= '0' && c <= '9'; }) ||
		get_symbol_map().contains(str)) {
		return *taken_.insert(OnTheFlyAlphabet::translate_symb(str)).first;
	}
	unsigned long long number{};
	std::from_chars(str.data(), str.data() + str.size(), number);
	if (number >= mata::nft::DONT_CARE || taken_.contains(static_cast<Symbol>(number))) {
		return *taken_.insert(OnTheFlyAlphabet::translate_symb(str)).first;
	}
	bind(std::string{str}, static_cast<Symbol>(number));
	return static_cast<Symbol>(number);
}

void NumberingAlphabet::bind(const std::string& name, const Symbol symbol) {
	add_new_symbol(name, symbol);
	taken_.insert(symbol);
}

ParseAlphabet::ParseAlphabet(
	const bool numeric_symbols,
	const rust::Slice<const rust::String> names,
	const rust::Slice<const Symbol> named_symbols,
	const Symbol next_symbol
)
	: numeric_(numeric_symbols),
	  first_new_symbol_(next_symbol),
	  on_the_fly_(next_symbol) {
	if (names.size() != named_symbols.size()) { throw std::invalid_argument("names and symbols differ in length"); }
	for (std::size_t i = 0; i < names.size(); ++i) {
		auto name = std::string(names[i]);
		on_the_fly_.bind(name, named_symbols[i]);
		known_names_.insert(std::move(name));
	}
}

void ParseAlphabet::report_new_names(rust::Vec<rust::String>& new_names, rust::Vec<Symbol>& new_symbols) const {
	new_names.clear();
	new_symbols.clear();
	if (numeric_) { return; }
	auto added = std::vector<std::pair<Symbol, std::string>>();
	for (const auto& [name, symbol] : on_the_fly_.get_symbol_map()) {
		if (known_names_.contains(name)) { continue; }
		// A number that is its own symbol may lie below the first new symbol, in a gap of the alphabet.
		if ((symbol < first_new_symbol_ && name != std::to_string(symbol)) || symbol >= mata::nft::DONT_CARE) {
			throw std::runtime_error("the alphabet has no symbol left for the name '" + name + "'");
		}
		added.emplace_back(symbol, name);
	}
	std::ranges::sort(added);
	for (const auto& [symbol, name] : added) {
		new_symbols.push_back(symbol);
		new_names.push_back(rust::String(name));
	}
}

std::vector<Section> split_sections(const std::string_view text) {
	const auto is_space = [](const char c) { return c == ' ' || (c >= '\t' && c <= '\r'); };
	auto sections = std::vector<Section>();
	std::size_t pos = 0;
	while (pos < text.size()) {
		const std::size_t start = pos;
		auto type = std::optional<std::string_view>();
		while (true) {
			while (pos < text.size() && is_space(text[pos])) { ++pos; }
			if (pos == text.size() || (type.has_value() && text[pos] == '@')) { break; }
			const std::size_t line_end = std::min(text.find('\n', pos), text.size());
			const auto line = text.substr(pos, line_end - pos);
			pos = line_end;
			if (!type.has_value() && line[0] != '#') {
				// The type line; errors in it are left to the readers.
				type =
					line[0] == '@' ? line.substr(1, line.find_first_of("\"()#%@\\ \t\r", 1) - 1) : std::string_view();
			}
		}
		if (type.has_value()) { sections.push_back({text.substr(start, pos - start), *type}); }
	}
	return sections;
}

} // namespace mata_sys::detail
