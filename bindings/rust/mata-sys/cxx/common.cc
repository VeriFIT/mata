#include "mata-sys/cxx/common.hh"

#include <algorithm>
#include <mutex>
#include <optional>
#include <utility>

#include "mata/nft/types.hh"
#include "mata/parser/mintermization.hh"
#include "mata/parser/parser.hh"

namespace mata_sys::detail {

NameTable::NameTable(const rust::Slice<const rust::String> names, const rust::Slice<const Symbol> symbols) {
	if (names.size() != symbols.size()) { throw std::invalid_argument("names and symbols differ in length"); }
	for (std::size_t i = 0; i < names.size(); ++i) {
		auto name = std::string(names[i]);
		by_symbol_.emplace(symbols[i], name);
		by_name_.emplace(std::move(name), symbols[i]);
	}
}

Symbol NameTable::translate_symb(const std::string& name) {
	const auto found = by_name_.find(name);
	if (found == by_name_.end()) { throw std::runtime_error("unknown symbol name '" + name + "'"); }
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

std::vector<mata::IntermediateAut> parse_sections(const rust::Str text) {
	auto sections = mata::IntermediateAut::parse_from_mf(mata::parser::parse_mf(std::string(text)));

	auto bitvector_sections = std::vector<const mata::IntermediateAut*>();
	for (const auto& section : sections) {
		if (section.is_bitvector()) { bitvector_sections.push_back(&section); }
	}
	if (!bitvector_sections.empty()) {
		auto mintermized = mintermize(bitvector_sections);
		auto next_mintermized = mintermized.begin();
		for (auto& section : sections) {
			if (section.is_bitvector()) { section = std::move(*next_mintermized++); }
		}
	}
	return sections;
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

Symbol NumberingAlphabet::translate_symb(const std::string& str) {
	// More than 10 digits is more than any symbol, and too long for stoull. A number is written without
	// leading zeros, so `01` is a name, as it is to Mata.
	if (str.empty() || str.size() > 10 || (str.size() > 1 && str[0] == '0') ||
		!std::ranges::all_of(str, [](const char c) { return c >= '0' && c <= '9'; }) ||
		get_symbol_map().contains(str)) {
		return *taken_.insert(OnTheFlyAlphabet::translate_symb(str)).first;
	}
	const auto number{std::stoull(str)};
	if (number >= mata::nft::DONT_CARE || taken_.contains(static_cast<Symbol>(number))) {
		return *taken_.insert(OnTheFlyAlphabet::translate_symb(str)).first;
	}
	bind(str, static_cast<Symbol>(number));
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

} // namespace mata_sys::detail
