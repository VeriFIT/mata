#include "mata-sys/cxx/shim.hh"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

#include "mata-sys/cxx/common.hh"
#include "mata-sys/src/lib.rs.h"
#include "mata/nfa/builder.hh"

namespace mata_sys {

namespace {

using detail::assign;
using detail::check_state;
using detail::slice_of;
using detail::SymbolSet;
using detail::to_symbol_set;
using detail::to_word;
using mata::Word;
using mata::nfa::ParameterMap;
using mata::nfa::Run;
using mata::nfa::StateSet;

/// Wraps an automaton for Rust, without its alphabet.
std::unique_ptr<Nfa> boxed(Nfa&& nfa) {
	nfa.alphabet = nullptr;
	return std::make_unique<Nfa>(std::move(nfa));
}

/// `nfa`, or an epsilon-free copy in `storage` (Mata reads epsilon as a letter in the language operations).
const Nfa& without_epsilon(const Nfa& nfa, std::optional<Nfa>& storage) {
	if (!has_epsilon_transitions(nfa)) { return nfa; }
	storage.emplace(mata::nfa::remove_epsilon(nfa));
	return *storage;
}

/// Brzozowski's minimal DFA, trimmed, with Mata's single-state automaton for the empty language.
Nfa trimmed_minimal(Nfa minimal) {
	minimal.trim();
	if (minimal.initial.empty()) { minimal = Nfa(std::size_t{1}, {0}, {}); }
	return minimal;
}

/// Drops the transitions over symbols outside `symbols`, as `complement_brzozowski()` does.
void restrict_to_symbols(Nfa& nfa, const SymbolSet& symbols) {
	for (State state = 0; state < nfa.num_of_states(); ++state) {
		auto& state_post = nfa.delta.mutable_state_post(state);
		state_post.erase(
			std::remove_if(
				state_post.begin(), state_post.end(),
				[&](const mata::nfa::SymbolPost& symbol_post) { return !symbols.contains(symbol_post.symbol); }
			),
			state_post.end()
		);
	}
}

/// Whether `nfa` stays within `limits`.
bool fits(const Nfa& nfa, const Limits& limits) { return nfa.num_of_states() <= limits.max_states; }

/// `determinize()` that gives up once the DFA does not fit `limits`.
std::optional<Nfa> limited_determinize(const Nfa& nfa, const Limits& limits, const SymbolSet* symbols = nullptr) {
	auto stopped = false;
	auto dfa = mata::nfa::determinize(
		nfa, nullptr,
		[&](const Nfa& partial, State, const StateSet&) {
			stopped = !fits(partial, limits);
			return !stopped;
		},
		symbols
	);
	// The callback never sees an automaton without transitions.
	if (stopped || !fits(dfa, limits)) { return std::nullopt; }
	return dfa;
}

/// `minimize_brzozowski()` (untrimmed) with each determinization within `limits`.
std::optional<Nfa> limited_brzozowski(const Nfa& nfa, const Limits& limits) {
	const auto backward = limited_determinize(mata::nfa::revert(nfa), limits);
	if (!backward.has_value()) { return std::nullopt; }
	return limited_determinize(mata::nfa::revert(*backward), limits);
}

/// `nfa` boxed for Rust, or null when its construction stopped or it does not fit `limits`.
std::unique_ptr<Nfa> boxed_within(std::optional<Nfa>&& nfa, const Limits& limits) {
	if (!nfa.has_value() || !fits(*nfa, limits)) { return nullptr; }
	return boxed(std::move(*nfa));
}

ParameterMap params_of(const CheckAlgorithm algorithm) {
	switch (algorithm) {
		case CheckAlgorithm::Naive:
			return {{"algorithm", "naive"}};
		case CheckAlgorithm::Antichains:
			return {{"algorithm", "antichains"}};
	}
	throw std::invalid_argument("unknown CheckAlgorithm");
}

ParameterMap params_of(const Determinization algorithm) {
	switch (algorithm) {
		case Determinization::FourRussians:
			return {{"algorithm", "four_russians"}};
		case Determinization::Classical:
			return {{"algorithm", "classical"}};
	}
	throw std::invalid_argument("unknown Determinization");
}

/// The symbol post at `index` of `source`; throws past the end.
const mata::nfa::SymbolPost&
	symbol_post_at(const mata::Automaton& automaton, const State source, const std::size_t index) {
	const auto& state_post = automaton.delta.state_post(source);
	if (index >= state_post.size()) { throw std::out_of_range("symbol post index out of range"); }
	return *(state_post.begin() + static_cast<std::ptrdiff_t>(index));
}

} // namespace

// ParsedNfas and WordStream.

std::unique_ptr<Nfa> ParsedNfas::take(const std::size_t index) {
	if (index >= automata_.size()) { throw std::out_of_range("parsed automaton index out of range"); }
	if (taken_[index]) { throw std::runtime_error("parsed automaton " + std::to_string(index) + " was already taken"); }
	taken_[index] = true;
	return boxed(std::move(automata_[index]));
}

#if MATA_HAS_GENERATOR_SUPPORT

WordStream::WordStream(const Nfa& nfa, const std::size_t max_length)
	: source_(has_epsilon_transitions(nfa) ? mata::nfa::remove_epsilon(nfa) : nfa),
	  generator_(source_.get_words_lazy(max_length)) {}

bool WordStream::next(rust::Vec<Symbol>& word) {
	if (!position_.has_value()) {
		position_.emplace(generator_.begin());
	} else if (*position_ != std::default_sentinel) {
		++*position_;
	}
	if (*position_ == std::default_sentinel) { return false; }
	assign(word, **position_);
	return true;
}

#else

WordStream::WordStream(const Nfa& nfa, const std::size_t max_length)
	: source_(has_epsilon_transitions(nfa) ? mata::nfa::remove_epsilon(nfa) : nfa) {
	const auto words = source_.get_words(max_length);
	words_.assign(words.begin(), words.end());
}

bool WordStream::next(rust::Vec<Symbol>& word) {
	if (position_ == words_.size()) { return false; }
	assign(word, words_[position_++]);
	return true;
}

#endif

// Construction.

std::unique_ptr<Nfa> new_nfa(const std::size_t num_states) { return boxed(Nfa(num_states)); }

std::unique_ptr<Nfa> clone_nfa(const Nfa& nfa) { return boxed(Nfa(nfa)); }

std::unique_ptr<Nfa> empty_word_nfa() { return boxed(mata::nfa::builder::create_empty_string_nfa()); }

std::unique_ptr<Nfa> word_nfa(const rust::Slice<const Symbol> word) {
	return boxed(mata::nfa::builder::create_single_word_nfa(to_word(word)));
}

std::unique_ptr<Nfa> sigma_star_nfa(const rust::Slice<const Symbol> symbols) {
	auto nfa = Nfa(1);
	nfa.initial.insert(0);
	nfa.final.insert(0);
	for (const Symbol symbol : to_symbol_set(symbols)) { nfa.delta.add(0, symbol, 0); }
	return boxed(std::move(nfa));
}

std::unique_ptr<Nfa> regex_nfa(const rust::Str pattern) {
	return boxed(mata::nfa::builder::create_from_regex(std::string(pattern)));
}

// Parsing.

std::unique_ptr<ParsedNfas> parse_mata(
	const rust::Str text,
	const bool numeric_symbols,
	const rust::Slice<const rust::String> names,
	const rust::Slice<const Symbol> named_symbols,
	const Symbol next_symbol,
	rust::Vec<rust::String>& new_names,
	rust::Vec<Symbol>& new_symbols
) {
	auto alphabet = detail::ParseAlphabet(numeric_symbols, names, named_symbols, next_symbol);
	const auto sections = detail::parse_sections(text);
	auto automata = std::vector<Nfa>();
	automata.reserve(sections.size());
	for (std::size_t i = 0; i < sections.size(); ++i) {
		if (!sections[i].is_nfa()) {
			throw std::runtime_error("section " + std::to_string(i + 1) + " does not describe an NFA");
		}
		automata.push_back(mata::nfa::builder::construct(sections[i], alphabet.get()));
	}
	alphabet.report_new_names(new_names, new_symbols);
	return std::make_unique<ParsedNfas>(std::move(automata));
}

std::size_t parsed_len(const ParsedNfas& parsed) { return parsed.size(); }

std::unique_ptr<Nfa> parsed_take(ParsedNfas& parsed, const std::size_t index) { return parsed.take(index); }

// Read-only views.

const mata::Automaton& nfa_automaton(const Nfa& nfa) { return nfa; }

std::size_t num_states(const mata::Automaton& automaton) { return automaton.num_of_states(); }

bool is_initial(const mata::Automaton& automaton, const State state) { return automaton.initial.contains(state); }

rust::Slice<const State> initial_states(const mata::Automaton& automaton) {
	return slice_of(std::to_address(automaton.initial.begin()), automaton.initial.size());
}

bool is_final(const mata::Automaton& automaton, const State state) { return automaton.final.contains(state); }

rust::Slice<const State> final_states(const mata::Automaton& automaton) {
	return slice_of(std::to_address(automaton.final.begin()), automaton.final.size());
}

bool contains_transition(
	const mata::Automaton& automaton, const State source, const Symbol symbol, const State target
) {
	return automaton.delta.contains(source, symbol, target);
}

std::size_t num_transitions(const mata::Automaton& automaton) { return automaton.delta.num_of_transitions(); }

std::size_t num_symbol_posts(const mata::Automaton& automaton, const State source) {
	return automaton.delta.state_post(source).size();
}

Symbol symbol_post_symbol(const mata::Automaton& automaton, const State source, const std::size_t index) {
	return symbol_post_at(automaton, source, index).symbol;
}

rust::Slice<const State>
	symbol_post_targets(const mata::Automaton& automaton, const State source, const std::size_t index) {
	const auto& targets = symbol_post_at(automaton, source, index).targets;
	return slice_of(targets.data(), targets.size());
}

rust::Slice<const State> successors(const mata::Automaton& automaton, const State source, const Symbol symbol) {
	const auto& targets = automaton.delta.get_successors(source, symbol);
	return slice_of(targets.data(), targets.size());
}

rust::Vec<Symbol> used_symbols(const mata::Automaton& automaton) {
	auto symbols = rust::Vec<Symbol>();
	assign(symbols, automaton.delta.get_used_symbols());
	return symbols;
}

bool has_epsilon_transitions(const mata::Automaton& automaton) {
	// EPSILON is the largest symbol, so only the last symbol post can hold it.
	return std::ranges::any_of(automaton.delta, [](const mata::nfa::StatePost& state_post) {
		return !state_post.empty() && state_post.back().symbol == mata::nfa::EPSILON;
	});
}

rust::Vec<State> reachable_states(const mata::Automaton& automaton) {
	auto states = rust::Vec<State>();
	assign(states, automaton.get_reachable_states());
	return states;
}

rust::Vec<State> useful_states(const mata::Automaton& automaton) {
	const auto useful = automaton.get_useful_states();
	auto states = rust::Vec<State>();
	for (State state = 0; state < useful.size(); ++state) {
		if (useful[state] != 0) { states.push_back(state); }
	}
	return states;
}

bool is_acyclic(const mata::Automaton& automaton) { return automaton.is_acyclic(); }

bool is_lang_empty(const mata::Automaton& automaton) { return automaton.has_no_accepting_path(); }

// States.

State add_state(Nfa& nfa) { return nfa.add_state(); }

void add_initial(Nfa& nfa, const State state) {
	check_state(nfa, state);
	nfa.initial.insert(state);
}

bool remove_initial(Nfa& nfa, const State state) {
	if (!nfa.initial.contains(state)) { return false; }
	nfa.initial.erase(state);
	return true;
}

void add_final(Nfa& nfa, const State state) {
	check_state(nfa, state);
	nfa.final.insert(state);
}

bool remove_final(Nfa& nfa, const State state) {
	if (!nfa.final.contains(state)) { return false; }
	nfa.final.erase(state);
	return true;
}

// Transitions.

void add_transition(Nfa& nfa, const State source, const Symbol symbol, const State target) {
	check_state(nfa, source);
	check_state(nfa, target);
	nfa.delta.add(source, symbol, target);
}

bool remove_transition(Nfa& nfa, const State source, const Symbol symbol, const State target) {
	if (!nfa.delta.contains(source, symbol, target)) { return false; }
	nfa.delta.remove(source, symbol, target);
	return true;
}

// Structural queries.

bool is_deterministic(const Nfa& nfa) {
	// Mata asks for exactly one initial state, which rules out the empty DFA of the empty language.
	if (nfa.initial.size() > 1) { return false; }
	return std::ranges::all_of(nfa.delta, [](const mata::nfa::StatePost& state_post) {
		return std::ranges::all_of(state_post, [](const mata::nfa::SymbolPost& symbol_post) {
			return symbol_post.symbol != mata::nfa::EPSILON && symbol_post.num_of_targets() <= 1;
		});
	});
}

// With epsilon transitions, an automaton is no DFA (Mata reads epsilon as a letter here).
bool is_minimal(const Nfa& nfa) { return !has_epsilon_transitions(nfa) && nfa.is_minimal(); }

bool is_complete(const Nfa& nfa, const rust::Slice<const Symbol> symbols) {
	// Mata's own check throws on symbols outside `symbols`, and so does this one. Epsilon is a move,
	// not a letter (as in every other language operation), so it is never outside.
	const auto wanted = to_symbol_set(symbols);
	for (const State state : nfa.get_reachable_states()) {
		const auto& state_post = nfa.delta.state_post(state);
		auto post = state_post.begin();
		auto symbol = wanted.begin();
		bool is_symbol_missing = false;
		while (post != state_post.end()) {
			if (post->symbol == mata::nfa::EPSILON) {
				++post;
				continue;
			}
			if (symbol == wanted.end() || post->symbol < *symbol) {
				throw std::runtime_error(
					"state " + std::to_string(state) + " has a transition over symbol " + std::to_string(post->symbol) +
					", which is outside the given symbols"
				);
			}
			if (*symbol < post->symbol) {
				is_symbol_missing = true;
				++symbol;
				continue;
			}
			++post;
			++symbol;
		}
		if (is_symbol_missing || symbol != wanted.end()) { return false; }
	}
	return true;
}

// Mata's is_identical() ignores the number of states.
bool is_identical(const Nfa& lhs, const Nfa& rhs) {
	return lhs.num_of_states() == rhs.num_of_states() && lhs.is_identical(rhs);
}

void adopt(Nfa& nfa) {
	nfa.alphabet = nullptr;
	detail::check_delta(nfa);
}

bool accepts(const Nfa& nfa, const rust::Slice<const Symbol> word) {
	return nfa.is_in_lang(to_word(word), /*use_epsilon=*/true);
}

// In-place operations.

void trim(Nfa& nfa) { nfa.trim(); }

void remove_epsilon(Nfa& nfa) { nfa.remove_epsilon(); }

bool make_complete(Nfa& nfa, const rust::Slice<const Symbol> symbols) {
	return nfa.make_complete(to_symbol_set(symbols));
}

void unite_with(Nfa& nfa, const Nfa& other) { nfa.unite_nondet_with(other); }

// Operations that build a new automaton.

std::unique_ptr<Nfa> union_nondet(const Nfa& lhs, const Nfa& rhs) { return boxed(mata::nfa::union_nondet(lhs, rhs)); }

std::unique_ptr<Nfa> intersection(const Nfa& lhs, const Nfa& rhs) { return boxed(mata::nfa::intersection(lhs, rhs)); }

std::unique_ptr<Nfa> concatenate(const Nfa& lhs, const Nfa& rhs) { return boxed(mata::nfa::concatenate(lhs, rhs)); }

std::unique_ptr<Nfa> difference(const Nfa& lhs, const Nfa& rhs) {
	auto lhs_storage = std::optional<Nfa>();
	auto rhs_storage = std::optional<Nfa>();
	return boxed(mata::nfa::lang_difference(without_epsilon(lhs, lhs_storage), without_epsilon(rhs, rhs_storage)));
}

std::unique_ptr<Nfa>
	complement(const Nfa& nfa, const rust::Slice<const Symbol> symbols, const Complementation algorithm) {
	auto params = ParameterMap();
	switch (algorithm) {
		case Complementation::Classical:
			params = {{"algorithm", "classical"}};
			break;
		case Complementation::Brzozowski:
			params = {{"algorithm", "brzozowski"}};
			break;
		default:
			throw std::invalid_argument("unknown Complementation");
	}
	auto storage = std::optional<Nfa>();
	return boxed(mata::nfa::complement(without_epsilon(nfa, storage), to_symbol_set(symbols), params));
}

std::unique_ptr<Nfa> determinize(const Nfa& nfa, const Determinization algorithm) {
	auto storage = std::optional<Nfa>();
	return boxed(mata::nfa::determinize(without_epsilon(nfa, storage), params_of(algorithm)));
}

std::unique_ptr<Nfa> minimize(const Nfa& nfa, const Minimization algorithm) {
	auto storage = std::optional<Nfa>();
	const Nfa& input = without_epsilon(nfa, storage);
	switch (algorithm) {
		case Minimization::Brzozowski:
			return boxed(trimmed_minimal(mata::nfa::minimize(input, {{"algorithm", "brzozowski"}})));
		case Minimization::Hopcroft:
			// "auto" determinizes and trims first: Hopcroft's algorithm needs a trimmed DFA.
			return boxed(mata::nfa::minimize(input, {{"algorithm", "auto"}}));
	}
	throw std::invalid_argument("unknown Minimization");
}

std::unique_ptr<Nfa> reduce(
	const Nfa& nfa, const Reduction algorithm, const ResidualConstruction construction, const Direction direction
) {
	switch (algorithm) {
		case Reduction::Simulation:
			return boxed(mata::nfa::reduce(nfa, nullptr, {{"algorithm", "simulation"}}));
		case Reduction::Residual: {
			auto params = ParameterMap{{"algorithm", "residual"}};
			switch (construction) {
				case ResidualConstruction::After:
					params["type"] = "after";
					break;
				case ResidualConstruction::With:
					params["type"] = "with";
					break;
				default:
					throw std::invalid_argument("unknown ResidualConstruction");
			}
			switch (direction) {
				case Direction::Forward:
					params["direction"] = "forward";
					break;
				case Direction::Backward:
					params["direction"] = "backward";
					break;
				default:
					throw std::invalid_argument("unknown Direction");
			}
			auto storage = std::optional<Nfa>();
			return boxed(mata::nfa::reduce(without_epsilon(nfa, storage), nullptr, params));
		}
	}
	throw std::invalid_argument("unknown Reduction");
}

std::unique_ptr<Nfa> revert(const Nfa& nfa) { return boxed(mata::nfa::revert(nfa)); }

// The same operations within limits, each following the Mata function it names.

std::unique_ptr<Nfa> difference_within(const Nfa& lhs, const Nfa& rhs, const Limits& limits) {
	auto lhs_storage = std::optional<Nfa>();
	auto rhs_storage = std::optional<Nfa>();
	auto stopped = false;
	auto difference = mata::nfa::lang_difference(
		without_epsilon(lhs, lhs_storage), without_epsilon(rhs, rhs_storage),
		[&](const Nfa&, const Nfa&, const StateSet&, const StateSet&, State, const Nfa& partial) {
			stopped = !fits(partial, limits);
			return !stopped;
		}
	);
	if (stopped) { return nullptr; }
	return boxed_within(std::move(difference), limits);
}

std::unique_ptr<Nfa> complement_within(
	const Nfa& nfa, const rust::Slice<const Symbol> symbols, const Complementation algorithm, const Limits& limits
) {
	auto storage = std::optional<Nfa>();
	const Nfa& input = without_epsilon(nfa, storage);
	const auto symbol_set = to_symbol_set(symbols);
	switch (algorithm) {
		case Complementation::Classical: {
			// complement_classical(), which does not determinize a DFA.
			if (input.is_deterministic()) {
				return boxed_within(mata::nfa::complement(input, symbol_set, {{"algorithm", "classical"}}), limits);
			}
			auto result = limited_determinize(input, limits, &symbol_set);
			if (result.has_value()) {
				result->trim();
				result->complement_deterministic(symbol_set);
			}
			return boxed_within(std::move(result), limits);
		}
		case Complementation::Brzozowski: {
			// complement_brzozowski().
			auto result = limited_brzozowski(input, limits);
			if (result.has_value()) {
				restrict_to_symbols(*result, symbol_set);
				if (result->final.empty() && !result->initial.empty()) {
					result->complement_deterministic(symbol_set, *result->initial.begin());
				} else {
					result->complement_deterministic(symbol_set);
				}
			}
			return boxed_within(std::move(result), limits);
		}
	}
	throw std::invalid_argument("unknown Complementation");
}

std::unique_ptr<Nfa> determinize_within(const Nfa& nfa, const Limits& limits) {
	auto storage = std::optional<Nfa>();
	return boxed_within(limited_determinize(without_epsilon(nfa, storage), limits), limits);
}

std::unique_ptr<Nfa> minimize_within(const Nfa& nfa, const Minimization algorithm, const Limits& limits) {
	auto storage = std::optional<Nfa>();
	const Nfa& input = without_epsilon(nfa, storage);
	switch (algorithm) {
		case Minimization::Brzozowski:
			return boxed_within(limited_brzozowski(input, limits).transform(trimmed_minimal), limits);
		case Minimization::Hopcroft: {
			// minimize_auto(), which does not determinize a DFA.
			const auto auto_params = ParameterMap{{"algorithm", "auto"}};
			if (input.is_deterministic()) { return boxed_within(mata::nfa::minimize(input, auto_params), limits); }
			return boxed_within(
				limited_determinize(input, limits).transform([&](const Nfa& dfa) {
					return mata::nfa::minimize(dfa, auto_params);
				}),
				limits
			);
		}
	}
	throw std::invalid_argument("unknown Minimization");
}

// Decision procedures.

bool accepting_run(const Nfa& nfa, rust::Vec<Symbol>& word, rust::Vec<State>& path) {
	auto run = Run();
	if (nfa.is_lang_empty(&run)) { return false; }
	assign(word, run.word);
	assign(path, run.path);
	return true;
}

bool some_word(const Nfa& nfa, rust::Vec<Symbol>& word) {
	const auto found = nfa.get_word();
	if (!found.has_value()) { return false; }
	assign(word, *found);
	return true;
}

bool shortest_word(const Nfa& nfa, rust::Vec<Symbol>& word) {
	// Mata's breadth-first search counts epsilon transitions as steps, so remove them first.
	auto storage = std::optional<Nfa>();
	const auto found = without_epsilon(nfa, storage).get_shortest_word();
	if (!found.has_value()) { return false; }
	assign(word, *found);
	return true;
}

namespace {

/// Mata's universality check; a null `run` asks for no counterexample, which saves its bookkeeping.
bool universal(
	const Nfa& nfa, const rust::Slice<const Symbol> symbols, const CheckAlgorithm algorithm, Run* const run
) {
	auto storage = std::optional<Nfa>();
	const auto alphabet = mata::EnumAlphabet(to_symbol_set(symbols));
	return without_epsilon(nfa, storage).is_universal(alphabet, run, params_of(algorithm));
}

/// Mata's inclusion check; a null `run` asks for no counterexample, which saves its bookkeeping.
bool included(const Nfa& smaller, const Nfa& bigger, const CheckAlgorithm algorithm, Run* const run) {
	auto smaller_storage = std::optional<Nfa>();
	auto bigger_storage = std::optional<Nfa>();
	const mata::Alphabet* const no_alphabet = nullptr;
	return mata::nfa::is_included(
		without_epsilon(smaller, smaller_storage), without_epsilon(bigger, bigger_storage), run, no_alphabet,
		params_of(algorithm)
	);
}

} // namespace

bool is_universal(
	const Nfa& nfa,
	const rust::Slice<const Symbol> symbols,
	const CheckAlgorithm algorithm,
	rust::Vec<Symbol>& counterexample
) {
	auto run = Run();
	if (universal(nfa, symbols, algorithm, &run)) { return true; }
	assign(counterexample, run.word);
	return false;
}

bool decide_universal(const Nfa& nfa, const rust::Slice<const Symbol> symbols, const CheckAlgorithm algorithm) {
	return universal(nfa, symbols, algorithm, nullptr);
}

bool is_included(
	const Nfa& smaller, const Nfa& bigger, const CheckAlgorithm algorithm, rust::Vec<Symbol>& counterexample
) {
	auto run = Run();
	if (included(smaller, bigger, algorithm, &run)) { return true; }
	assign(counterexample, run.word);
	return false;
}

bool decide_included(const Nfa& smaller, const Nfa& bigger, const CheckAlgorithm algorithm) {
	return included(smaller, bigger, algorithm, nullptr);
}

bool is_disjoint(const Nfa& lhs, const Nfa& rhs, rust::Vec<Symbol>& common_word) {
	// Epsilon transitions move one automaton only and are left out of the witness.
	auto witness = Run();
	if (mata::nfa::is_intersection_empty(lhs, rhs, &witness)) { return true; }
	assign(common_word, witness.word);
	return false;
}

bool decide_disjoint(const Nfa& lhs, const Nfa& rhs) { return mata::nfa::is_intersection_empty(lhs, rhs); }

// Words.

std::unique_ptr<WordStream> words(const Nfa& nfa, const std::size_t max_length) {
	return std::make_unique<WordStream>(nfa, max_length);
}

bool next_word(WordStream& stream, rust::Vec<Symbol>& word) { return stream.next(word); }

// Printing.

rust::String
	print_mata(const Nfa& nfa, const rust::Slice<const rust::String> names, const rust::Slice<const Symbol> symbols) {
	const auto table = detail::NameTable(names, symbols);
	return rust::String(nfa.print_to_mata(&table));
}

rust::String print_dot(
	const Nfa& nfa,
	const bool ascii_labels,
	const bool intervals,
	const std::int32_t max_label_length,
	const rust::Slice<const rust::String> names,
	const rust::Slice<const Symbol> symbols
) {
	const auto table = detail::NameTable(names, symbols);
	const mata::Alphabet* const alphabet = table.empty() ? nullptr : &table;
	return rust::String(nfa.print_to_dot(ascii_labels, intervals, max_label_length, alphabet));
}

} // namespace mata_sys
