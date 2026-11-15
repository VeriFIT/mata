/** @file
 * @brief Methods for NFTs.
 */

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <limits>
#include <list>
#include <map>
#include <optional>
#include <queue>
#include <ranges>
#include <span>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "../mata-writer.hh"
#include "mata/alphabet.hh"
#include "mata/nfa/builder.hh"
#include "mata/nft/nft.hh"
#include "mata/utils/assert.hh"
#include "mata/utils/sparse-set.hh"
#include "nft-internal.hh"

using namespace mata::utils;
using namespace mata::nft;
using mata::BoolVector;
using mata::Symbol;
using mata::Word;

using StateBoolArray = std::vector<bool>; ///< Bool array for states in the automaton.

const std::string mata::nft::TYPE_NFT = "NFT";

Levels::Levels(const size_t num_of_levels, const size_t count, const Level value)
	: super(count, value),
	  num_of_levels{num_of_levels} {
	MATA_ASSERT(value < num_of_levels, "Levels::Levels: level is out of range of the set number of levels.");
}

Levels::Levels(const size_t num_of_levels, const std::initializer_list<Level> levels)
	: super{levels},
	  num_of_levels{num_of_levels} {
	MATA_ASSERT(check_levels_in_range_(), "Levels::Levels: level is out of range of the set number of levels.");
}

Levels::Levels(const size_t num_of_levels, const iterator first, const iterator last)
	: super{first, last},
	  num_of_levels{num_of_levels} {
	MATA_ASSERT(check_levels_in_range_(), "Levels::Levels: level is out of range of the set number of levels.");
}

Levels& Levels::operator=(const std::initializer_list<Level> other) {
	super::operator=(other);
	num_of_levels = num_of_levels_used().value_or(DEFAULT_NUM_OF_LEVELS);
	return *this;
}

Levels& Levels::operator=(const std::vector<Level>& levels) {
	if (this != &levels) {
		super::operator=(levels);
		num_of_levels = num_of_levels_used().value_or(DEFAULT_NUM_OF_LEVELS);
	}
	return *this;
}

Levels& Levels::operator=(std::vector<Level>&& levels) {
	if (this != &levels) {
		super::operator=(std::move(levels));
		num_of_levels = num_of_levels_used().value_or(DEFAULT_NUM_OF_LEVELS);
	}
	return *this;
}

Levels& Levels::set(const State state, const Level level) {
	MATA_ASSERT(level < num_of_levels, "Levels::set: level is out of range of the set number of levels.");
	if (size() <= state) { resize(nfa::num_of_states_for(state), DEFAULT_LEVEL); }
	(*this)[state] = level;
	return *this;
}

Levels& Levels::set(const std::vector<Level>& levels) {
	assign(levels.begin(), levels.end());
	MATA_ASSERT(check_levels_in_range_(), "Levels::set: level is out of range of the set number of levels.");
	return *this;
}

Levels& Levels::set(std::vector<Level>&& levels) {
	const auto num_of_levels_orig{num_of_levels};
	*this = std::move(levels);
	num_of_levels = num_of_levels_orig;
	MATA_ASSERT(check_levels_in_range_(), "Levels::set: level is out of range of the set number of levels.");
	return *this;
}

void Levels::append(const Levels& levels_vector) {
	reserve(size() + levels_vector.size());
	std::ranges::copy(levels_vector, std::back_inserter(*this));
	MATA_ASSERT(check_levels_in_range_(), "Levels::append: level is out of range of the set number of levels.");
}

std::vector<Level> Levels::get_levels_of(const SparseSet<State>& states) const {
	std::vector<Level> result;
	result.reserve(states.size());
	for (const State state : states) { result.push_back((*this)[state]); }
	return result;
}

std::vector<Level> Levels::get_levels_of(const StateSet& states) const {
	std::vector<Level> result;
	result.reserve(states.size());
	for (const State state : states) { result.push_back((*this)[state]); }
	return result;
}

std::optional<Level> Levels::get_minimal_level_of(const StateSet& states, Ordering::Compare levels_ordering) const {
	if (states.empty()) { return std::nullopt; }
	auto levels = states | std::views::transform([&](const State& state) { return (*this)[state]; });
	return std::ranges::min(levels, std::move(levels_ordering));
}

std::optional<Level> Levels::get_minimal_next_level_of(const StateSet& states) const {
	return get_minimal_level_of(states, Ordering::Next);
}

bool Levels::can_follow(const Level source_level, const Level target_level) {
	return source_level < target_level || target_level == 0;
}

bool Levels::can_follow_for_states(const State source, const State target) const {
	return can_follow((*this)[source], (*this)[target]);
}

size_t Nft::num_of_states_with_level(const Level level) const { return levels.count(level); }

Nft& Nft::trim(StateRenaming* state_renaming) {
#ifdef _STATIC_STRUCTURES_
	BoolVector useful_states{get_useful_states()};
	useful_states.clear();
	useful_states = get_useful_states();
#else
	const BoolVector useful_states{get_useful_states()};
#endif

	// Drop the levels of the states that are about to be removed.
	// Independent of the structural renaming below, which does not touch `levels`.
	// `levels` may hold more entries than `useful_states` has states, so the bounds check is load-bearing.
	auto is_state_useful = [&](const State q) { return q < useful_states.size() && useful_states[q]; };
	State move_index{0};
	levels.erase(
		std::ranges::remove_if(
			levels,
			[&](Level&) -> bool {
				const State prev{move_index};
				++move_index;
				return not is_state_useful(prev);
			}
		).begin(),
		levels.end()
	);

	return trim_impl(useful_states, state_renaming);
}

void Nft::remove_epsilon(const Symbol epsilon) { *this = nft::remove_epsilon(*this, epsilon); }

std::string Nft::print_to_dot(
	const bool decode_ascii_chars, const bool use_intervals, const int max_label_length, const Alphabet* alphabet
) const {
	std::stringstream output;
	print_to_dot(output, decode_ascii_chars, use_intervals, max_label_length, alphabet);
	return output.str();
}

void Nft::print_to_dot(
	std::ostream& output,
	const bool decode_ascii_chars,
	const bool use_intervals,
	const int max_label_length,
	const Alphabet* alphabet
) const {
	auto to_ascii = [&](const Symbol symbol) -> std::string {
		// Translate only printable ASCII characters.
		if (symbol < 33 || symbol >= 127) { return "<" + std::to_string(symbol) + ">"; }
		switch (symbol) {
			case '"':
				return "\\\"";
			case '\\':
				return "\\\\";
			default:
				// Not a braced initializer list: '{1, c}' would build the two-character string "\x01c".
				// NOLINTNEXTLINE(modernize-return-braced-init-list)
				return std::string(1, static_cast<char>(symbol));
		}
	};

	std::map<std::pair<Symbol, Level>, std::string> symbol_names{};
	auto translate_symbol = [&](const Symbol symbol, const Level level) -> std::string {
		switch (symbol) {
			case EPSILON:
				return "<eps>";
			case DONT_CARE:
				return "<dntcr>";
			default:
				break;
		}
		if (decode_ascii_chars) { return to_ascii(symbol); }
		// Reverse lookups may scan the whole alphabet; translate each (symbol, level) once.
		if (alphabet != nullptr || this->alphabets != nullptr) {
			const auto key{std::pair{symbol, alphabet != nullptr ? Level{0} : level}};
			if (const auto it{symbol_names.find(key)}; it != symbol_names.end()) { return it->second; }
			return symbol_names
				.emplace(
					key, alphabet != nullptr ? alphabet->reverse_translate_symbol(symbol)
											 : this->alphabets->reverse_translate_symbol(symbol, level)
				)
				.first->second;
		}
		return std::to_string(symbol);
	};

	auto vec_of_symbols_to_string = [&](const OrdVector<Symbol>& symbols, const Level level) {
		std::string result;
		for (const Symbol& symbol : symbols) { result += translate_symbol(symbol, level) + ","; }
		result.pop_back(); // Remove last comma
		return result;
	};

	auto vec_of_symbols_to_string_with_intervals = [&](const OrdVector<Symbol>& symbols, const Level level) {
		std::string result;

		const auto intervals{[&]() {
			std::vector<std::pair<Symbol, Symbol>> intervals_val;
			auto symbols_it = symbols.begin();
			std::pair<Symbol, Symbol> interval{*symbols_it, *symbols_it};
			++symbols_it;
			for (; symbols_it != symbols.end(); ++symbols_it) {
				if (*symbols_it == interval.second + 1) {
					interval.second = *symbols_it;
				} else {
					intervals_val.push_back(interval);
					interval = {*symbols_it, *symbols_it};
				}
			}
			intervals_val.push_back(interval);
			return intervals_val;
		}()};

		for (const auto& [symbol_from, symbol_to] : intervals) {
			if (const size_t interval_size{symbol_to - symbol_from + 1}; interval_size == 1) {
				result += translate_symbol(symbol_from, level) + ",";
			} else if (interval_size == 2) {
				result += translate_symbol(symbol_from, level) + "," + translate_symbol(symbol_to, level) + ",";
			} else {
				result += "[" + translate_symbol(symbol_from, level) + "-" + translate_symbol(symbol_to, level) + "],";
			}
		}

		result.pop_back(); // Remove last comma
		return result;
	};

	BoolVector is_state_drawn(num_of_states(), false);
	output << "digraph finiteAutomaton {\n" << "node [shape=circle];\n";

	// Double circle for final states
	for (const State final_state : final) {
		is_state_drawn[final_state] = true;
		output << final_state << " [shape=doublecircle];\n";
	}

	// Print transitions
	const size_t delta_size = delta.num_of_states();
	// Reused across sources to keep the hash table allocated.
	std::unordered_map<State, OrdVector<Symbol>> tgt_symbols_map;
	for (State source = 0; source != delta_size; ++source) {
		tgt_symbols_map.clear();
		for (const SymbolPost& move : delta[source]) {
			is_state_drawn[source] = true;
			for (const State target : move.targets) {
				is_state_drawn[target] = true;
				tgt_symbols_map[target].insert(move.symbol);
			}
		}
		for (const auto& [target, symbols] : tgt_symbols_map) {
			if (max_label_length == 0) {
				output << source << " -> " << target << ";\n";
				continue;
			}

			const Level source_level = source < levels.size() ? levels[source] : 0;
			std::string label = use_intervals ? vec_of_symbols_to_string_with_intervals(symbols, source_level)
											  : vec_of_symbols_to_string(symbols, source_level);
			const std::string on_hover_label = replace_all(replace_all(label, "<", "&lt;"), ">", "&gt;");
			bool is_shortened = false;
			if (max_label_length > 0 && label.length() > static_cast<size_t>(max_label_length)) {
				label.replace(static_cast<size_t>(max_label_length), std::string::npos, "...");
				is_shortened = true;
			}

			if (is_shortened) {
				output << source << " -> " << target << " [label=\"" << label << "\", tooltip=\"" << on_hover_label
					   << "\"];\n";
			} else {
				output << source << " -> " << target << " [label=\"" << label << "\"];\n";
			}
		}
	}

	// Circle for isolated states with no transitions
	for (State state{0}; state < is_state_drawn.size(); ++state) {
		if (!is_state_drawn[state]) { output << state << " [shape=circle];\n"; }
	}

	// Levels for each state
	output << "node [shape=none, label=\"\"];\n";
	output << "forcelabels=true;\n";
	for (State s{0}; s < levels.size(); s++) { output << s << " [label=\"" << s << ":" << levels[s] << "\"];\n"; }

	// Arrow for initial states
	for (const State init_state : initial) { output << "i" << init_state << " -> " << init_state << ";\n"; }

	// Lines end with '\n', not std::endl: flushing per line costs one write() per line. Flush once so the caller's
	//  stream holds the whole transducer when this returns.
	output << "}\n" << std::flush;
}

void Nft::print_to_dot(
	const std::string& filename,
	const bool decode_ascii_chars,
	const bool use_intervals,
	const int max_label_length,
	const Alphabet* alphabet
) const {
	std::ofstream output(filename);
	if (!output) { throw std::ios_base::failure("Failed to open file: " + filename); }
	print_to_dot(output, decode_ascii_chars, use_intervals, max_label_length, alphabet);
}

namespace {
/// Writes @p nft as @c Nft::print_to_mata() prints it.
void write_mata(const Nft& nft, mata::parser::MataWriter& writer) {
	writer << "@NFT-explicit\n" << "%Alphabet-auto\n";
	// TODO should be this, but we cannot parse %Alphabet-numbers yet
	//<< "%Alphabet-numbers\n";

	if (!nft.initial.empty()) {
		writer << "%Initial";
		for (const State init_state : nft.initial) { writer << " q" << init_state; }
		writer.end_line();
	}

	if (!nft.final.empty()) {
		writer << "%Final";
		for (const State final_state : nft.final) { writer << " q" << final_state; }
		writer.end_line();
	}

	if (!nft.levels.empty()) {
		const size_t num_of_states{nft.num_of_states()};
		BoolVector live_states(num_of_states, false);
		for (const State& s : nft.initial) { live_states[s] = true; }
		for (const State& s : nft.final) { live_states[s] = true; }
		for (const Transition& trans : nft.delta.transitions()) {
			live_states[trans.source] = true;
			live_states[trans.target] = true;
		}
		writer << "%Levels";
		for (State s{0}; s < num_of_states; s++) {
			if (live_states[s]) { writer << " q" << s << ':' << nft.levels[s]; }
		}
		writer.end_line();
	}
	writer << "%LevelsNum " << nft.levels.num_of_levels;
	writer.end_line();

	// Reverse lookups may scan the whole alphabet; translate each (symbol, level) once.
	std::map<std::pair<Symbol, Level>, std::string> symbol_names{};
	for (const Transition& trans : nft.delta.transitions()) {
		writer << "q" << trans.source << " ";
		// EPSILON and DONT_CARE belong to no alphabet; print the names the parser maps back.
		if (trans.symbol == EPSILON) {
			writer << mata::EPSILON_NAME;
		} else if (trans.symbol == DONT_CARE) {
			writer << mata::DONT_CARE_NAME;
		} else if (nft.alphabets != nullptr) {
			const auto key{std::pair{trans.symbol, nft.levels[trans.source]}};
			auto it{symbol_names.find(key)};
			if (it == symbol_names.end()) {
				it =
					symbol_names
						.emplace(
							key, mata::quote_symbol_name(nft.alphabets->reverse_translate_symbol(key.first, key.second))
						)
						.first;
			}
			writer << it->second;
		} else {
			writer << trans.symbol;
		}
		writer << " q" << trans.target;
		writer.end_line();
	}
}
} // namespace

std::string Nft::print_to_mata() const {
	parser::MataWriter writer;
	write_mata(*this, writer);
	return writer.take();
}

void Nft::print_to_mata(std::ostream& output) const {
	parser::MataWriter writer{output};
	write_mata(*this, writer);
	writer.flush();
}

void Nft::print_to_mata(const std::string& filename) const {
	std::ofstream output(filename);
	if (!output) { throw std::ios_base::failure("Failed to open file: " + filename); }
	print_to_mata(output);
}

Nft Nft::get_one_letter_aut(const std::set<Level>& levels_to_keep, const Symbol abstract_symbol) const {
	Nft one_symbol_transducer{with_levels(levels, num_of_states(), initial, final)};
	for (nfa::State source_state = 0; source_state < delta.num_of_states(); ++source_state) {
		nfa::StatePost& state_post_of_new_source_state = one_symbol_transducer.delta.mutable_state_post(source_state);
		if (levels_to_keep.contains(levels[source_state])) {
			state_post_of_new_source_state = delta[source_state];
		} else {
			nfa::SymbolPost new_transition{abstract_symbol};
			for (const nfa::SymbolPost& symbol_post : delta[source_state]) {
				if (symbol_post.symbol == EPSILON) {
					state_post_of_new_source_state.insert(symbol_post);
				} else {
					new_transition.targets.insert(symbol_post.targets);
				}
			}
			if (!new_transition.targets.empty()) { state_post_of_new_source_state.insert(new_transition); }
		}
	}
	return one_symbol_transducer;
}

void Nft::get_one_letter_aut(Nft& result) const { result = get_one_letter_aut(); }

// TODO(nft): Implement for NFTs.
StateSet Nft::post(const StateSet& states, const Symbol symbol, const EpsilonClosureOpt epsilon_closure_opt) const {
	StateSet res{};

	// If the symbol is EPSILON, we can stay in the same state.
	if (symbol == EPSILON && epsilon_closure_opt != EpsilonClosureOpt::None) { res = states; }

	if (delta.empty()) { return res; }

	StateSet from_states = states;
	if (epsilon_closure_opt == EpsilonClosureOpt::Before) {
		// Before making the step using the symbol, we compute the epsilon closure.
		from_states = mk_epsilon_closure(states);
	}

	// Now, we can make the step using the symbol.
	for (const State state : from_states) {
		if (symbol == DONT_CARE) {
			for (const SymbolPost& symbol_post : delta[state]) {
				if (symbol_post.symbol == EPSILON) { continue; }
				res.insert(symbol_post.targets);
			}
			continue;
		}

		const StatePost& state_post{delta[state]};
		if (const auto move_it{state_post.find(symbol)}; move_it != state_post.end()) { res.insert(move_it->targets); }

		if (symbol == EPSILON) { continue; }
		if (const auto move_it{state_post.find(DONT_CARE)}; move_it != state_post.end()) {
			res.insert(move_it->targets);
		}
	}

	if (epsilon_closure_opt == EpsilonClosureOpt::After) {
		// We need to compute the epsilon closure of the resulting states.
		res = mk_epsilon_closure(res);
	}

	return res;
}

void mata::nft::internal::validate_nft_levels(const Nft& nft, const char* const operation) {
	// num_of_states() counts only states used in delta, initial and final; the level vector may legitimately
	//  be longer (e.g. sized by construction or before trimming). Missing levels for used states are the hazard.
	if (nft.levels.size() < nft.num_of_states()) {
		throw std::invalid_argument(
			std::string{operation} + ": the level vector has " + std::to_string(nft.levels.size()) + " entries for " +
			std::to_string(nft.num_of_states()) + " states"
		);
	}
	for (State state{0}; state < nft.levels.size(); ++state) {
		if (nft.levels[state] >= nft.levels.num_of_levels) {
			throw std::invalid_argument(
				std::string{operation} + ": state " + std::to_string(state) + " has level " +
				std::to_string(nft.levels[state]) + " outside [0, " + std::to_string(nft.levels.num_of_levels) + ")"
			);
		}
	}
}

void Nft::unwind_jumps_inplace(const OrdVector<Symbol>& dont_care_symbol_replacements, const JumpMode jump_mode) {
	internal::validate_nft_levels(*this, "unwind_jumps_inplace");
	const bool dont_care_for_dont_care = dont_care_symbol_replacements == utils::OrdVector<Symbol>({DONT_CARE});
	const size_t num_of_delta_states{delta.num_of_states()};
	const auto num_of_levels{static_cast<Level>(levels.num_of_levels)};
	// The unsigned span of a transition: a target level below the source level wraps around.
	const auto span = [&](const Level src_lvl, const Level trg_lvl) -> Level {
		return (trg_lvl == 0) ? (num_of_levels - src_lvl) : trg_lvl - src_lvl;
	};

	// Fresh states are numbered from num_of_states() in the order of the transitions they unwind, as add_state()
	//  would; a jump over the levels in between gets one per inner level, a wrapping one (0 < target < source level)
	//  a single one. A source keeps its post unless it has a jump or a DONT_CARE over one level to spell out.
	size_t num_of_fresh_states{0};
	std::vector<State> rewritten_sources{};
	for (State source{0}; source < num_of_delta_states; ++source) {
		const Level src_lvl{levels[source]};
		bool rewritten{false};
		for (const SymbolPost& symbol_post : delta[source]) {
			const bool spelled{symbol_post.symbol == DONT_CARE && !dont_care_for_dont_care};
			for (const State target : symbol_post.targets) {
				const Level trg_lvl{levels[target]};
				if (const Level diff_lvl{span(src_lvl, trg_lvl)}; diff_lvl > 1) {
					num_of_fresh_states += (trg_lvl == 0 || trg_lvl > src_lvl) ? diff_lvl - 1 : 1;
					rewritten = true;
				} else if (diff_lvl == 1 && spelled) {
					rewritten = true;
				}
			}
		}
		if (rewritten) { rewritten_sources.push_back(source); }
	}
	const size_t first_fresh{num_of_states()};
	if (num_of_fresh_states > 0) {
		delta.allocate(first_fresh + num_of_fresh_states);
		if (levels.size() < first_fresh + num_of_fresh_states) {
			levels.resize(first_fresh + num_of_fresh_states, DEFAULT_LEVEL);
		}
	}
	auto next_fresh{static_cast<State>(first_fresh)};

	// The one link of a chain from the fresh state @p fresh, over every replacement for DONT_CARE.
	const auto set_fresh_link = [&](const State fresh, const Symbol symbol, const State target) {
		StatePost& fresh_post{delta.mutable_state_post(fresh)};
		if (symbol == DONT_CARE && !dont_care_for_dont_care) {
			for (const Symbol replace_symbol : dont_care_symbol_replacements) {
				fresh_post.push_back(SymbolPost{replace_symbol, target});
			}
		} else {
			fresh_post.push_back(SymbolPost{symbol, target});
		}
	};

	// The moves of a spelled-out DONT_CARE post, merged into the rest of the post once it is complete.
	std::vector<std::pair<Symbol, State>> spelled_moves{};
	std::vector<State> fresh_targets{};
	for (const State source : rewritten_sources) {
		const Level src_lvl{levels[source]};
		StatePost state_post{};
		spelled_moves.clear();
		for (const SymbolPost& symbol_post : delta[source]) {
			const Symbol symbol{symbol_post.symbol};
			const bool spelled{symbol == DONT_CARE && !dont_care_for_dont_care};
			// Targets kept, then the fresh ones, all above the kept ones and numbered in increasing order.
			SymbolPost kept{symbol};
			fresh_targets.clear();
			for (const State target : symbol_post.targets) {
				const Level trg_lvl{levels[target]};
				const Level diff_lvl{span(src_lvl, trg_lvl)};
				if (diff_lvl == 1 && spelled) {
					// The transition is replaced, so a DONT_CARE among the replacements does not stay either.
					for (const Symbol replace_symbol : dont_care_symbol_replacements) {
						if (replace_symbol != DONT_CARE) { spelled_moves.emplace_back(replace_symbol, target); }
					}
				} else if (diff_lvl > 1) {
					// The first link connecting the original source state with an inner state.
					State inner_src{next_fresh++};
					levels[inner_src] = src_lvl + 1;
					if (spelled) {
						for (const Symbol replace_symbol : dont_care_symbol_replacements) {
							spelled_moves.emplace_back(replace_symbol, inner_src);
						}
					} else {
						fresh_targets.push_back(inner_src);
					}

					// Links 1 to n-1 connecting inner states.
					const Symbol inner_symbol{jump_mode == JumpMode::AppendDontCares ? DONT_CARE : symbol};
					for (Level inner_trg_lvl = src_lvl + 2,
							   pre_trg_lvl = (trg_lvl == 0) ? (num_of_levels - 1) : (trg_lvl - 1);
						 inner_trg_lvl - 1 < pre_trg_lvl; ++inner_trg_lvl) {
						const State inner_trg{next_fresh++};
						levels[inner_trg] = inner_trg_lvl;
						set_fresh_link(inner_src, inner_symbol, inner_trg);
						inner_src = inner_trg;
					}

					// The last link connecting the last inner state with the original target state.
					set_fresh_link(inner_src, inner_symbol, target);
				} else if (spelled) {
					spelled_moves.emplace_back(DONT_CARE, target);
				} else {
					kept.push_back(target);
				}
			}
			for (const State fresh : fresh_targets) { kept.push_back(fresh); }
			if (!kept.empty()) { state_post.push_back(std::move(kept)); }
		}

		if (!spelled_moves.empty()) {
			std::ranges::sort(spelled_moves);
			spelled_moves.erase(std::ranges::unique(spelled_moves).begin(), spelled_moves.end());
			StatePost merged{};
			auto post{state_post.begin()};
			for (auto move{spelled_moves.begin()}; move != spelled_moves.end();) {
				const Symbol symbol{move->first};
				for (; post != state_post.end() && post->symbol < symbol; ++post) {
					merged.push_back(std::move(*post));
				}
				SymbolPost spelled_post{symbol};
				for (; move != spelled_moves.end() && move->first == symbol; ++move) {
					spelled_post.push_back(move->second);
				}
				if (post != state_post.end() && post->symbol == symbol) {
					spelled_post.insert(post->target_span());
					++post;
				}
				merged.push_back(std::move(spelled_post));
			}
			for (; post != state_post.end(); ++post) { merged.push_back(std::move(*post)); }
			state_post = std::move(merged);
		}
		delta.mutable_state_post(source) = std::move(state_post);
	}
}

Nft Nft::unwind_jumps(const OrdVector<Symbol>& dont_care_symbol_replacements, const JumpMode jump_mode) const {
	internal::validate_nft_levels(*this, "unwind_jumps");
	Nft result{*this};
	result.unwind_jumps_inplace(dont_care_symbol_replacements, jump_mode);
	return result;
}

void Nft::unwind_jumps(
	Nft& result, const OrdVector<Symbol>& dont_care_symbol_replacements, const JumpMode jump_mode
) const {
	result = unwind_jumps(dont_care_symbol_replacements, jump_mode);
}

StateSet Nft::post(
	const StateSet& states,
	const Symbol symbol,
	const Level symbol_level,
	const EpsilonClosureOpt epsilon_closure_opt,
	const JumpMode jump_mode
) const {
	MATA_ASSERT(std::ranges::all_of(states, [&](const State state) { return levels[state] == 0; }));

	// Computes the epsilon closure of a set of states.
	auto get_epsilon_closure = [&](const StateSet& states) {
		StateSet closure{states};
		std::queue<State> worklist;
		for (const State state : states) { worklist.push(state); }
		while (!worklist.empty()) {
			const State state = worklist.front();
			worklist.pop();
			for (const State target : post(state, EPSILON, symbol_level, EpsilonClosureOpt::None, jump_mode)) {
				if (!closure.contains(target)) {
					closure.insert(target);
					worklist.push(target);
				}
			}
		}
		return closure;
	};

	StateSet result{};
	// If we want to compute the epsilon cosure and the symbol is EPSILON, we can stay in the same zero states.
	if (symbol == EPSILON && epsilon_closure_opt != EpsilonClosureOpt::None) { result = states; }
	if (delta.empty()) { return result; }

	StateSet from_states = states;
	if (epsilon_closure_opt == EpsilonClosureOpt::Before) {
		// Before making the step using the symbol, we compute the epsilon closure.
		from_states = get_epsilon_closure(states);
	}

	StateSet used_nonzero_states; // To avoid cycling.
	std::stack<State> stack;
	for (const State state : from_states) { stack.push(state); }

	// Adds states (with zero level) to the result or enqueues them for further processing.
	auto add_to_result_or_enqueue = [&](const SmallStateSet& targets) {
		for (const State target : targets) {
			if (levels[target] == 0) {
				result.insert(target);
			} else if (!used_nonzero_states.contains(target)) {
				used_nonzero_states.insert(target);
				stack.push(target);
			}
		}
	};

	// The main loop.
	while (!stack.empty()) {
		const State current_state = stack.top();
		const Level current_level = levels[current_state];
		stack.pop();

		for (const SymbolPost& symbol_post : delta[current_state]) {
			// DONT_CARE does not match EPSILON.
			const Symbol transition_symbol = symbol_post.symbol;
			const bool is_one_of_symbols_epsilon = (symbol == EPSILON || transition_symbol == EPSILON);
			const bool is_one_of_symbols_dont_care = (symbol == DONT_CARE || transition_symbol == DONT_CARE);
			const bool is_symbol_match =
				(transition_symbol == symbol || (is_one_of_symbols_dont_care && !is_one_of_symbols_epsilon));
			if (current_level > symbol_level) {
				// The transition is behind us. Just go forward until next zero level state.
				add_to_result_or_enqueue(symbol_post.targets);
			} else if (current_level == symbol_level && is_symbol_match) {
				// We are exactly at the symbol_level and the symbol matches. Proceed.
				add_to_result_or_enqueue(symbol_post.targets);
			} else if (current_level < symbol_level) {
				// We are before the symbol_level. We need to check if we are jumping over the symbol_level or not.
				for (const State target : symbol_post.targets) {
					const Level target_level = levels[target];
					const bool are_we_jumping_over_the_symbol_level =
						(target_level > symbol_level || target_level == 0);
					if (are_we_jumping_over_the_symbol_level) {
						// We need to match the symbol based on the jump_mode.
						// DONT_CARE does not match EPSILON.
						if ((jump_mode == JumpMode::AppendDontCares && symbol != EPSILON) ||
							(jump_mode == JumpMode::RepeatSymbol && is_symbol_match)) {
							if (target_level == 0) {
								result.insert(target);
							} else if (!used_nonzero_states.contains(target)) {
								used_nonzero_states.insert(target);
								stack.push(target);
							}
						}
					} else {
						// We are not jumping over the symbol_level, just go forward.
						if (!used_nonzero_states.contains(target)) {
							used_nonzero_states.insert(target);
							stack.push(target);
						}
					}
				}
			}
		}
	}

	if (epsilon_closure_opt == EpsilonClosureOpt::After) {
		// We need to compute the epsilon closure of the resulting states.
		return get_epsilon_closure(result);
	}

	return result;
}

namespace {

/// When the multiword search of Nft::post() halts early.
struct PostStop {
	/// The public predicate of Nft::post(), called with the reading-head positions of all levels; null if unset.
	const std::function<bool(State, const std::vector<size_t>&)>* is_early_exit_config{nullptr};
	/// Halt at a final zero-level state with all input consumed, or with any input if @c match_prefix. Membership
	/// only needs this, and it compares position ids, so no configuration is decoded for it.
	bool at_final{false};
	bool match_prefix{false};
};

/// A multiword Nft::post() query. A configuration keeps the reading-head positions of the used levels only:
/// projected-out levels never move.
struct PostQuery {
	const StateSet& states;
	StateSet* visited_zero_level_states;
	bool epsilon_closure_after;
	JumpMode jump_mode;
	PostStop stop;
	size_t num_of_levels;
	std::vector<Level> used_levels; ///< Ascending.
	std::vector<const Word*> used_words; ///< The word of each used level.
	std::vector<size_t> num_of_used_below; ///< For each level up to and including num_of_levels.
	/// The end is never reached when a projected-out level has a word: its reading head never moves.
	bool end_reachable;
};

/// Reading-head positions of the used levels as one mixed-radix rank below 2^32: used level i contributes
/// position * stride[i], so advancing the used levels [first, last) adds a difference of stride prefix sums.
/// @p WIDTH is the number of used levels, or std::dynamic_extent. A fixed width unrolls the decoding: with one used
/// level the rank is the position itself, two used levels take one division.
template <size_t WIDTH> class RankedPositions {
  public:
	static constexpr bool ranked{true};
	using Tuple = std::conditional_t<WIDTH == std::dynamic_extent, std::vector<uint32_t>, std::array<uint32_t, WIDTH>>;

	/// @p radices: word length + 1 for each used level, with a product @p num_of_ids that fits in 32 bits.
	RankedPositions(const std::vector<uint64_t>& radices, const uint64_t num_of_ids) : num_of_ids_{num_of_ids} {
		MATA_ASSERT(WIDTH == std::dynamic_extent || WIDTH == radices.size());
		MATA_ASSERT(num_of_ids <= std::numeric_limits<uint32_t>::max());
		if constexpr (WIDTH == std::dynamic_extent) {
			radices_.resize(radices.size());
			stride_sums_.resize(radices.size() + 1);
		}
		uint32_t stride{1};
		for (size_t used{0}; used < radices.size(); ++used) {
			radices_[used] = static_cast<uint32_t>(radices[used]);
			stride_sums_[used + 1] = stride_sums_[used] + stride;
			end_id_ += (radices_[used] - 1) * stride;
			stride *= radices_[used];
		}
	}

	static uint32_t start_id() { return 0; }
	uint32_t end_id() const { return end_id_; }
	uint64_t num_of_ids() const { return num_of_ids_; }

	void decode(uint32_t id, Tuple& tuple) const {
		if constexpr (WIDTH == std::dynamic_extent) {
			tuple.resize(radices_.size());
			if (tuple.empty()) { return; }
		}
		const size_t last{tuple.size() - 1};
		for (size_t used{0}; used < last; ++used) {
			tuple[used] = id % radices_[used];
			id /= radices_[used];
		}
		tuple[last] = id; // The rank is below the product of the radices.
	}

	/// The id of configuration @p id, whose positions are @p tuple, with the used levels [first, last) advanced.
	uint32_t advance(const uint32_t id, const Tuple& /*tuple*/, const size_t first, const size_t last) const {
		return id + (stride_sums_[last] - stride_sums_[first]);
	}

  private:
	Tuple radices_{};
	/// Prefix sums of the strides: stride_sums_[i] is the sum of the strides of the used levels below i.
	std::conditional_t<WIDTH == std::dynamic_extent, std::vector<uint32_t>, std::array<uint32_t, WIDTH + 1>>
		stride_sums_{};
	uint32_t end_id_{0};
	uint64_t num_of_ids_;
};

/// Reading-head positions of the used levels hash-consed into fixed-width blocks of one append-only arena, for when
/// the ranks would not fit in 32 bits: an id indexes its block. A successor is built in reused scratch storage and
/// looked up there, so a tuple is copied only when it is new.
class InternedPositions {
  public:
	// Read through the template parameter as 'Positions::ranked', which cppcheck does not follow.
	// cppcheck-suppress unusedStructMember
	static constexpr bool ranked{false};
	using Tuple = std::vector<size_t>;

	/// @p ends: the word length of each used level.
	explicit InternedPositions(const std::vector<size_t>& ends)
		: width_{ends.size()},
		  slots_(64, 0),
		  scratch_(width_, 0),
		  start_id_{intern(scratch_)},
		  end_id_{intern(ends)} {}

	uint32_t start_id() const { return start_id_; }
	uint32_t end_id() const { return end_id_; }
	uint64_t num_of_ids() const { return hashes_.size(); }

	void decode(const uint32_t id, Tuple& tuple) const {
		const auto block{tuples_.begin() + static_cast<std::ptrdiff_t>(id * width_)};
		tuple.assign(block, block + static_cast<std::ptrdiff_t>(width_));
	}

	/// The id of configuration @p id, whose positions are @p tuple, with the used levels [first, last) advanced.
	uint32_t advance(const uint32_t /*id*/, const Tuple& tuple, const size_t first, const size_t last) {
		scratch_.assign(tuple.begin(), tuple.end());
		for (size_t used{first}; used < last; ++used) { ++scratch_[used]; }
		return intern(scratch_);
	}

  private:
	/// Open addressing with linear probing over the ids, at most half full; the hash of each tuple is kept.
	uint32_t intern(const Tuple& tuple) {
		const size_t hash{hash_range(tuple.begin(), tuple.end())};
		size_t slot{hash & (slots_.size() - 1)};
		for (; slots_[slot] != 0; slot = (slot + 1) & (slots_.size() - 1)) {
			const uint32_t id{slots_[slot] - 1};
			if (hashes_[id] == hash &&
				std::equal(tuple.begin(), tuple.end(), tuples_.begin() + static_cast<std::ptrdiff_t>(id * width_))) {
				return id;
			}
		}
		const auto id{static_cast<uint32_t>(hashes_.size())};
		slots_[slot] = id + 1;
		hashes_.push_back(hash);
		tuples_.insert(tuples_.end(), tuple.begin(), tuple.end());
		if (2 * hashes_.size() > slots_.size()) { grow(); }
		return id;
	}

	void grow() {
		slots_.assign(2 * slots_.size(), 0);
		for (uint32_t id{0}; id < hashes_.size(); ++id) {
			size_t slot{hashes_[id] & (slots_.size() - 1)};
			while (slots_[slot] != 0) { slot = (slot + 1) & (slots_.size() - 1); }
			slots_[slot] = id + 1;
		}
	}

	size_t width_;
	std::vector<size_t> tuples_{}; ///< The arena: the tuple of id i at [i * width_, (i + 1) * width_).
	std::vector<size_t> hashes_{}; ///< The hash of each tuple, by id.
	std::vector<uint32_t> slots_; ///< id + 1, or 0 for an empty slot.
	Tuple scratch_{};
	uint32_t start_id_{0};
	uint32_t end_id_{0};
};

/// The search of the multiword Nft::post() over configurations (positions id, state), with @p positions for the
/// reading-head positions. Sets @p stopped when @c query.stop halted it.
template <class Positions>
StateSet search_by_words(const Nft& nft, const PostQuery& query, Positions& positions, bool& stopped) {
	StateSet result{};
	const uint32_t end_id{positions.end_id()};
	const size_t num_of_states{nft.num_of_states()};

	// `visited` deduplicates configurations (so the search terminates even on cyclic inputs), keyed on the packed
	// (positions_id, state) pair. With 32-bit states the pair packs losslessly into one uint64; a 64-bit build
	// checks that the states fit (in post_by_words()). When the ids are ranks, a search that keeps going moves to a
	// bitmap over all (rank, state) pairs: no hashing and no allocation per configuration. It moves once the set
	// holds 1/256 of the bitmap's bits, so a search that ends after a few configurations never pays for clearing the
	// bitmap.
	const auto pack = [](const uint32_t positions_id, const State state) -> uint64_t {
		return (static_cast<uint64_t>(positions_id) << 32U) | static_cast<uint32_t>(state);
	};
	constexpr uint64_t max_visited_bits{uint64_t{1} << 27U}; // 16 MiB.
	const bool bitmap_fits{
		Positions::ranked && positions.num_of_ids() <= max_visited_bits / std::max<size_t>(num_of_states, 1)
	};
	const size_t bitmap_bits{bitmap_fits ? static_cast<size_t>(positions.num_of_ids() * num_of_states) : 0};
	std::unordered_set<uint64_t> visited;
	std::vector<bool> visited_bitmap{};
	auto mark_visited = [&](const uint32_t positions_id, const State state) {
		if (visited_bitmap.empty()) {
			if (!visited.insert(pack(positions_id, state)).second) { return false; }
			if (!bitmap_fits || visited.size() < bitmap_bits / 256) { return true; }
			visited_bitmap.resize(bitmap_bits);
			for (const uint64_t packed : visited) {
				visited_bitmap[((packed >> 32U) * num_of_states) + (packed & 0xFF'FF'FF'FFU)] = true;
			}
			visited = {};
			return true;
		}
		auto bit{visited_bitmap[(static_cast<size_t>(positions_id) * num_of_states) + state]};
		if (bit) { return false; }
		bit = true;
		return true;
	};

	const bool best_first{query.stop.at_final || query.stop.is_early_exit_config != nullptr};
	struct QueueItem {
		size_t consumed; // total input symbols read so far; higher = closer to a full (accepting) match
		uint32_t positions_id;
		State state;
		bool operator<(const QueueItem& other) const { return consumed < other.consumed; } // max-heap on consumed
	};
	std::priority_queue<QueueItem> best_first_queue; // used when best_first
	std::vector<std::pair<uint32_t, State>> stack; // used otherwise

	// Positions of the used levels of the configuration being expanded, and of a discovered one together with the
	// positions of all levels when the public early-exit predicate needs them.
	typename Positions::Tuple current_positions{};
	typename Positions::Tuple discovered_positions{};
	std::vector<size_t> discovered_level_positions{};

	// Registers a freshly reached configuration: deduplicates it, tests the stop condition (which may halt the whole
	// search, matching a final state as soon as it is generated rather than when it is later expanded), and
	// otherwise queues it for expansion. @p consumed counts the input symbols read to reach it.
	auto discover = [&](const uint32_t positions_id, const State target, const size_t consumed) {
		if (stopped || !mark_visited(positions_id, target)) { return; }
		if (query.stop.at_final) {
			if (nft.levels[target] == 0 &&
				(query.stop.match_prefix || (query.end_reachable && positions_id == end_id)) &&
				nft.final.contains(target)) {
				stopped = true;
				return;
			}
		} else if (query.stop.is_early_exit_config != nullptr) {
			positions.decode(positions_id, discovered_positions);
			discovered_level_positions.assign(query.num_of_levels, 0);
			for (size_t used{0}; used < query.used_levels.size(); ++used) {
				discovered_level_positions[query.used_levels[used]] = discovered_positions[used];
			}
			if ((*query.stop.is_early_exit_config)(target, discovered_level_positions)) {
				stopped = true;
				return;
			}
		}
		if (best_first) {
			best_first_queue.push({consumed, positions_id, target});
		} else {
			stack.emplace_back(positions_id, target);
		}
	};

	for (const State state : query.states) { discover(positions.start_id(), state, 0); }

	// Two symbols match iff they are equal, or one is DONT_CARE and neither is EPSILON.
	// EPSILON matches only EPSILON. (The same relation as nft::symbols_match.)
	auto symbols_match = [](const Symbol a, const Symbol b) {
		return (a == EPSILON || b == EPSILON) ? (a == EPSILON && b == EPSILON)
											  : (a == b || a == DONT_CARE || b == DONT_CARE);
	};

	while (!stopped && (best_first ? !best_first_queue.empty() : !stack.empty())) {
		// Best-first (most input consumed) when short-circuiting, depth-first otherwise (see the worklist comment).
		uint32_t current_positions_id;
		State current_state;
		size_t current_consumed{0};
		if (best_first) {
			const QueueItem top{best_first_queue.top()};
			best_first_queue.pop();
			current_positions_id = top.positions_id;
			current_state = top.state;
			current_consumed = top.consumed;
		} else {
			const std::pair<uint32_t, State> top{stack.back()};
			stack.pop_back();
			current_positions_id = top.first;
			current_state = top.second;
		}
		positions.decode(current_positions_id, current_positions);
		const Level current_level{nft.levels[current_state]};

		if (current_level == 0) {
			if (query.visited_zero_level_states != nullptr) {
				// Keep track of visited zero level states. This is useful for the is_prefix_in_lang function.
				query.visited_zero_level_states->insert(current_state);
			}
			if (query.end_reachable && current_positions_id == end_id) {
				// A zero-level state reached with all tape symbols consumed.
				result.insert(current_state);
				if (!query.epsilon_closure_after) {
					continue; // No need to explore further unless we are computing the epsilon closure.
				}
			}
		}

		// Expand one outgoing transition (possibly a jump) of the current state.
		const StatePost& state_post{nft.delta[current_state]};
		auto expand = [&](const SymbolPost& symbol_post) {
			const Symbol transition_symbol{symbol_post.symbol};
			for (const State target : symbol_post.targets) {
				const Level target_level{nft.levels[target]};
				// The (possibly jumping) transition reads one symbol on each level it spans: [current_level,
				// stop_level). On a jump the symbol is repeated, or replaced by DONT_CARE for the trailing levels in
				// JumpMode::AppendDontCares. Projected-out levels of the span are taken without reading anything, so
				// only the used levels [first_used, last_used) of the span are read.
				const size_t stop_level{target_level == 0 ? nft.levels.num_of_levels : target_level};
				const size_t first_used{query.num_of_used_below[current_level]};
				const size_t last_used{query.num_of_used_below[stop_level]};

				// Consuming step: check that one input symbol can be read on each used level of the span. A level
				// with no input left cannot be read, so the step fails. Because exhaustion is detected by the
				// reading-head position (not an EPSILON sentinel), a literal EPSILON symbol in an input word is
				// matched and consumed here by an EPSILON transition. The successor id is computed only once the
				// whole span is known to be readable, so a mismatching transition costs nothing more.
				bool consumed{true};
				for (size_t used{first_used}; used < last_used; ++used) {
					const Symbol expected =
						(query.used_levels[used] == current_level || query.jump_mode != JumpMode::AppendDontCares)
							? transition_symbol
							: DONT_CARE;
					const Word& word{*query.used_words[used]};
					if (current_positions[used] >= word.size() ||
						!symbols_match(expected, word[current_positions[used]])) {
						consumed = false;
						break;
					}
				}
				if (consumed) {
					discover(
						positions.advance(current_positions_id, current_positions, first_used, last_used), target,
						current_consumed + (last_used - first_used)
					);
					if (stopped) { return; }
				}

				// Non-consuming step: an EPSILON transition may change state without reading any input. This is what
				// lets it be taken when a tape is exhausted; kept separate from the consuming step above so that a
				// literal EPSILON in an input word can still be read there. Positions are unchanged, so the source
				// id is reused directly.
				if (transition_symbol == EPSILON) {
					discover(current_positions_id, target, current_consumed);
					if (stopped) { return; }
				}
			}
		};

		// Pick only the outgoing transitions that can possibly fire, instead of scanning them all.
		const size_t current_used{query.num_of_used_below[current_level]};
		const bool current_is_used{current_used < query.num_of_used_below[current_level + 1]};
		const bool current_has_symbol{
			current_is_used && current_positions[current_used] < query.used_words[current_used]->size()
		};
		const Symbol current_symbol{
			current_has_symbol ? (*query.used_words[current_used])[current_positions[current_used]] : EPSILON
		};
		if (!current_is_used || current_symbol == DONT_CARE) {
			for (const SymbolPost& symbol_post : state_post) {
				expand(symbol_post);
				if (stopped) { break; }
			}
		} else {
			if (const auto it = state_post.find(EPSILON); it != state_post.end()) { expand(*it); }
			if (!stopped && current_has_symbol && current_symbol != EPSILON) {
				if (const auto it = state_post.find(current_symbol); it != state_post.end()) { expand(*it); }
				if (!stopped) {
					if (const auto it = state_post.find(DONT_CARE); it != state_post.end()) { expand(*it); }
				}
			}
		}
	}

	return result;
}

/// The multiword Nft::post() (see there), halting as @p stop says. Sets @p stopped when it did.
StateSet post_by_words(
	const Nft& nft,
	const StateSet& states,
	const std::vector<Word>& tape_symbols,
	const BoolVector& use_tape,
	StateSet* const visited_zero_level_states,
	const bool epsilon_closure_after,
	const JumpMode jump_mode,
	const PostStop& stop,
	bool& stopped
) {
	MATA_ASSERT(std::ranges::all_of(states, [&](const State state) { return nft.levels[state] == 0; }));
	// With no transitions the only reachable configurations are the initial states with their reading heads still at
	// the start, so the input is fully consumed iff every word is empty.
	const bool all_input_consumed{std::ranges::all_of(tape_symbols, [](const Word& word) { return word.empty(); })};
	if (nft.delta.empty()) {
		// Honor the stop condition here too.
		if (stop.at_final) {
			for (const State state : states) {
				if ((stop.match_prefix || all_input_consumed) && nft.final.contains(state)) {
					stopped = true;
					return StateSet{state};
				}
			}
		} else if (stop.is_early_exit_config != nullptr) {
			const std::vector<size_t> start_positions(tape_symbols.size(), 0);
			for (const State state : states) {
				if ((*stop.is_early_exit_config)(state, start_positions)) {
					stopped = true;
					return StateSet{state};
				}
			}
		}
		if (visited_zero_level_states != nullptr) {
			// Keep track of visited zero level states.
			// This is useful for is_prefix_in_lang function.
			visited_zero_level_states->insert(states);
		}
		// If all tape symbols are empty (all words are epsilon), we can return existing zero level states.
		return all_input_consumed ? states : StateSet{};
	}
	if constexpr (sizeof(State) > sizeof(uint32_t)) {
		if (nft.num_of_states() > std::numeric_limits<uint32_t>::max()) {
			throw std::overflow_error("Nft::post: the NFT has more states than fit into 32 bits");
		}
	}

	PostQuery query{
		.states = states,
		.visited_zero_level_states = visited_zero_level_states,
		.epsilon_closure_after = epsilon_closure_after,
		.jump_mode = jump_mode,
		.stop = stop,
		.num_of_levels = tape_symbols.size(),
		.used_levels = {},
		.used_words = {},
		.num_of_used_below = {},
		.end_reachable = true,
	};
	std::vector<size_t> ends{};
	for (Level level{0}; level < tape_symbols.size(); ++level) {
		query.num_of_used_below.push_back(query.used_levels.size());
		if (!use_tape[level]) {
			if (!tape_symbols[level].empty()) { query.end_reachable = false; }
			continue;
		}
		query.used_levels.push_back(level);
		query.used_words.push_back(&tape_symbols[level]);
		ends.push_back(tape_symbols[level].size());
	}
	query.num_of_used_below.push_back(query.used_levels.size());

	// A configuration's reading-head positions get a 32-bit id. When the number of position tuples, the product of
	// (word length + 1) over the used levels, fits in 32 bits, the id is the mixed-radix rank of the tuple, and no
	// position tuple is copied, hashed or stored. One and two used levels, the common cases, get their own
	// instantiation. Larger inputs intern the position tuples instead, and the id indexes them.
	std::vector<uint64_t> radices{};
	uint64_t num_of_tuples{1};
	for (const size_t end : ends) {
		const uint64_t radix{uint64_t{end} + 1};
		if (radix > std::numeric_limits<uint32_t>::max() / num_of_tuples) {
			InternedPositions positions{ends};
			return search_by_words(nft, query, positions, stopped);
		}
		radices.push_back(radix);
		num_of_tuples *= radix;
	}
	switch (radices.size()) {
		case 1: {
			RankedPositions<1> positions{radices, num_of_tuples};
			return search_by_words(nft, query, positions, stopped);
		}
		case 2: {
			RankedPositions<2> positions{radices, num_of_tuples};
			return search_by_words(nft, query, positions, stopped);
		}
		default: {
			RankedPositions<std::dynamic_extent> positions{radices, num_of_tuples};
			return search_by_words(nft, query, positions, stopped);
		}
	}
}

} // Anonymous namespace.

StateSet Nft::post(
	const StateSet& states,
	const std::vector<Word>& level_words,
	const BoolVector& use_level,
	StateSet* const visited_zero_level_states,
	const bool epsilon_closure_after,
	const JumpMode jump_mode,
	const std::function<bool(State, const std::vector<size_t>&)>& is_early_exit_config
) const {
	bool stopped{false};
	return post_by_words(
		*this, states, level_words, use_level, visited_zero_level_states, epsilon_closure_after, jump_mode,
		PostStop{.is_early_exit_config = is_early_exit_config ? &is_early_exit_config : nullptr}, stopped
	);
}

bool Nft::reaches_final_by_levels_(
	const std::vector<Word>& level_words, const bool match_prefix, const JumpMode jump_mode
) const {
	bool stopped{false};
	post_by_words(
		*this, StateSet{initial.begin(), initial.end()}, level_words, BoolVector(level_words.size(), true), nullptr,
		true, jump_mode, PostStop{.at_final = true, .match_prefix = match_prefix}, stopped
	);
	return stopped;
}

StateSet Nft::post(
	const StateSet& states,
	const std::vector<Word>& level_words,
	const std::vector<Level>& word_levels,
	StateSet* visited_zero_level_states,
	const bool epsilon_closure_after,
	JumpMode jump_mode
) const {
	MATA_ASSERT(level_words.size() == word_levels.size());
	std::vector<Word> word_per_level(levels.num_of_levels);
	BoolVector use_level(levels.num_of_levels, false);
	for (size_t i = 0; i < level_words.size(); ++i) {
		use_level[word_levels[i]] = true;
		word_per_level[word_levels[i]] = level_words[i];
	}
	return post(states, word_per_level, use_level, visited_zero_level_states, epsilon_closure_after, jump_mode);
}

Nft& Nft::operator=(Nft&& other) noexcept {
	if (this != &other) {
		Automaton::operator=(std::move(other));
		// Only the 'Automaton' base of 'other' has been moved from; its own members are intact.
		// NOLINTNEXTLINE(bugprone-use-after-move)
		levels = std::move(other.levels);
		alphabets = std::exchange(other.alphabets, nullptr);
	}
	return *this;
}

State Nft::add_state() {
	const State state{Automaton::add_state()};
	levels.set(state);
	return state;
}

State Nft::add_state(const State state) {
	// Do not reset the level of a state that is already in this NFT; only newly added states get the default level.
	if (state >= levels.size()) { levels.resize(nfa::num_of_states_for(state), DEFAULT_LEVEL); }
	return Automaton::add_state(state);
}

State Nft::add_state_with_level(const Level level) {
	const State state{Automaton::add_state()};
	levels.set(state, level);
	return state;
}

State Nft::add_state_with_level(const State state, const Level level) {
	levels.set(state, level);
	return Automaton::add_state(state);
}

State Nft::insert_word_impl_(const State source, const Word& word, const State target) {
	MATA_ASSERT(!word.empty());
	MATA_ASSERT(source < num_of_states());
	MATA_ASSERT(target < num_of_states());

	const size_t word_len = word.size();
	if (word_len == 1) {
		delta.add(source, word[0], target);
		return target;
	}

	// Add transition source --> inner_state.
	State inner_state = Automaton::add_state();
	delta.add(source, word[0], inner_state);

	// Add transitions inner_state --> inner_state
	State prev_state = inner_state;
	for (size_t idx{1}; idx < word_len - 1; idx++) {
		inner_state = Automaton::add_state();
		delta.add(prev_state, word[idx], inner_state);
		prev_state = inner_state;
	}

	// Add transition inner_state --> target
	delta.add(prev_state, word[word_len - 1], target);
	return target;
}

State Nft::insert_word(const State source, const Word& word, const State target) {
	// Ensure both states exist in the NFT.
	add_state(std::max(source, target));
	if (levels[source] != levels[target]) {
		throw std::invalid_argument{"Inserting word between source and target states with different levels."};
	}

	const auto first_new_state{static_cast<State>(num_of_states())};
	const State word_target{insert_word_impl_(source, word, target)};
	const size_t num_of_states_after{num_of_states()};
	const Level source_level{levels[source]};
	Level lvl{levels.next_level_after(source_level)};
	for (State state{first_new_state}; state < num_of_states_after; ++state, lvl = levels.next_level_after(lvl)) {
		add_state_with_level(state, lvl);
	}

	MATA_ASSERT(levels[word_target] == 0 || levels[num_of_states_after - 1] < levels[word_target]);

	return word_target;
}

State Nft::insert_word(const State source, const Word& word) {
	if (num_of_states() <= source) { add_state(source); }
	return insert_word(source, word, add_state_with_level(levels[source]));
}

State Nft::insert_word_by_levels(
	const State source, const std::vector<Word>& word_parts_on_levels, const State target
) {
	MATA_ASSERT(word_parts_on_levels.size() == levels.num_of_levels);
	MATA_ASSERT(source < num_of_states());
	MATA_ASSERT(target < num_of_states());
	MATA_ASSERT(source < levels.size());
	MATA_ASSERT(target < levels.size());
	MATA_ASSERT(levels[source] == levels[target]);
	const Level from_to_level{levels[source]};

	if (levels.num_of_levels == 1) { return insert_word(source, word_parts_on_levels[0], target); }

	std::vector<Word::const_iterator> word_part_it_v(levels.num_of_levels);
	for (Level lvl{from_to_level}, i{0}; i < static_cast<Level>(levels.num_of_levels); ++i) {
		word_part_it_v[lvl] = word_parts_on_levels[lvl].begin();
		lvl = levels.next_level_after(lvl);
	}

	// This function retrieves the next symbol from a word part at a specified level and advances the corresponding
	// iterator. Returns EPSILON when the iterator reaches the end of the word part.
	auto get_next_symbol = [&](const Level lvl) {
		if (word_part_it_v[lvl] == word_parts_on_levels[lvl].end()) { return EPSILON; }
		return *(word_part_it_v[lvl]++);
	};

	// Add transition source --> inner_state.
	Level inner_lvl = levels.next_level_after(from_to_level);
	State inner_state = add_state_with_level(inner_lvl);
	delta.add(source, get_next_symbol(from_to_level), inner_state);

	// Add transition inner_state --> inner_state
	State prev_state = inner_state;
	Level prev_lvl = inner_lvl;
	const size_t max_word_part_len{std::ranges::max_element(word_parts_on_levels, [](const Word& a, const Word& b) {
									   return a.size() < b.size();
								   })->size()};
	if (const size_t word_total_len = levels.num_of_levels * max_word_part_len; word_total_len != 0) {
		for (size_t symbol_idx{1}; symbol_idx < word_total_len - 1; symbol_idx++) {
			inner_lvl = levels.next_level_after(prev_lvl);
			inner_state = add_state_with_level(inner_lvl);
			delta.add(prev_state, get_next_symbol(prev_lvl), inner_state);
			prev_state = inner_state;
			prev_lvl = inner_lvl;
		}
	}
	// Add transition inner_state --> target.
	delta.add(prev_state, get_next_symbol(prev_lvl), target);
	return target;
}

State Nft::insert_word_by_levels(const State source, const std::vector<Word>& word_parts_on_levels) {
	MATA_ASSERT(source < levels.size());
	return insert_word_by_levels(source, word_parts_on_levels, add_state_with_level(levels[source]));
}

State Nft::add_transition_by_levels(const State source, const std::vector<Symbol>& symbols, const State target) {
	return insert_word(source, symbols, target);
}

State Nft::add_transition_by_levels(const State source, const std::vector<Symbol>& symbols) {
	return insert_word(source, symbols);
}

State Nft::add_transition_with_length(
	const State source, const Symbol symbol, const size_t length, const JumpMode jump_mode
) {
	MATA_ASSERT(source < num_of_states());

	if (length == 0) { return source; }

	MATA_ASSERT(levels[source] + length <= levels.num_of_levels);
	const auto target_level = static_cast<Level>((levels[source] + length) % levels.num_of_levels);
	const State target = add_state_with_level(target_level);

	if (length == 1 || jump_mode == JumpMode::RepeatSymbol) {
		delta.add(source, symbol, target);
		return target;
	}

	State inner_src = source;
	Level inner_level = levels[inner_src] + 1;
	for (size_t i = 0; i < length - 1; ++i, ++inner_level) {
		MATA_ASSERT(inner_level < levels.num_of_levels);
		const State inner_target = add_state_with_level(inner_level);
		delta.add(inner_src, symbol, inner_target);
		inner_src = inner_target;
	}
	MATA_ASSERT(inner_level == levels[source] + length);
	delta.add(inner_src, symbol, target);

	return target;
}

void Nft::add_transition_with_same_level_targets(
	State source, Symbol symbol, const StateSet& targets, JumpMode jump_mode
) {
	MATA_ASSERT(not targets.empty());
	MATA_ASSERT(source < num_of_states());
	MATA_ASSERT(std::ranges::all_of(targets, [&](const State target) { return target < num_of_states(); }));

	const Level target_level = levels[targets.front()];
	const size_t trans_len = (target_level == 0 ? levels.num_of_levels : target_level) - levels[source];
	MATA_ASSERT(std::ranges::all_of(targets, [&](const State target) { return levels[target] == target_level; }));
	MATA_ASSERT(target_level == 0 || target_level > levels[source]);
	MATA_ASSERT(trans_len > 0);

	if (trans_len == 1 || jump_mode == JumpMode::RepeatSymbol) {
		StatePost& mutable_state_post = delta.mutable_state_post(source);
		auto mutable_symbol_post_it = mutable_state_post.find(symbol);
		if (mutable_symbol_post_it == mutable_state_post.end()) {
			if (mutable_state_post.empty() || mutable_state_post.back().symbol < symbol) {
				// Optimization: If the new symbol is greater than the last one, we can simply push it back.
				mutable_state_post.push_back({symbol, targets});
			} else {
				// Otherwise, we need to insert it in the correct position to keep the order.
				mutable_state_post.insert({symbol, targets});
			}
		} else {
			mutable_symbol_post_it->targets.insert(targets);
		}
		return;
	}

	State inner_src = source;
	Level inner_level = levels[inner_src] + 1;
	for (size_t i = 0; i < trans_len - 1; ++i, ++inner_level) {
		MATA_ASSERT(inner_level < levels.num_of_levels);
		const State inner_target = add_state_with_level(inner_level);
		delta.add(inner_src, symbol, inner_target);
		inner_src = inner_target;
	}
	MATA_ASSERT(inner_level == levels[source] + trans_len);
	StatePost& mutable_inner_state_post = delta.mutable_state_post(inner_src);
	MATA_ASSERT(mutable_inner_state_post.find(symbol) == mutable_inner_state_post.end());
	mutable_inner_state_post.insert(SymbolPost(symbol, targets));
}

void Nft::add_transition(
	const State source, const std::string& symbol_name, const State target, Alphabet* const alphabet
) {
	MATA_ASSERT(source < num_of_states());
	MATA_ASSERT(target < num_of_states());

	const Level source_level{levels[source]};
	const Level target_level{levels[target]};
	const size_t trans_len{(target_level == 0 ? levels.num_of_levels : target_level) - source_level};
	MATA_ASSERT(target_level == 0 || target_level > source_level);
	MATA_ASSERT(trans_len > 0);

	// Resolve the alphabet for each level the transition spans (source_level, next_level_after(source_level), ...)
	//  and translate symbol_name separately for each of them, since per-level alphabets (this->alphabets in
	//  AlphabetLevels::Mode::MultiLevel) may translate the same name to a different Symbol on different levels.
	auto resolve_level_alphabet{[&](const Level level) -> Alphabet& {
		if (alphabet != nullptr) { return *alphabet; }
		if (this->alphabets != nullptr) { return *this->alphabets->for_level(level); }
		throw std::runtime_error(
			"Nft::add_transition(): no alphabet available to translate symbol '" + symbol_name + "' for level " +
			std::to_string(level)
		);
	}};

	std::vector<Symbol> level_symbols;
	level_symbols.reserve(trans_len);
	Level lvl{source_level};
	for (size_t i{0}; i < trans_len; ++i, lvl = levels.next_level_after(lvl)) {
		level_symbols.push_back(resolve_level_alphabet(lvl).translate_symb(symbol_name));
	}

	if (std::ranges::all_of(level_symbols, [&](const Symbol symb) { return symb == level_symbols.front(); })) {
		// The affected levels agree on the translation of symbol_name: a single jump transition suffices.
		delta.add(source, level_symbols.front(), target);
		return;
	}

	// The affected levels disagree: unwind the jump into a sequence of ordinary, single-level transitions, each
	//  carrying its own level's translation of symbol_name.
	State inner_src{source};
	lvl = levels.next_level_after(source_level);
	for (size_t i{0}; i < trans_len - 1; ++i, lvl = levels.next_level_after(lvl)) {
		const State inner_target{add_state_with_level(lvl)};
		delta.add(inner_src, level_symbols[i], inner_target);
		inner_src = inner_target;
	}
	delta.add(inner_src, level_symbols.back(), target);
}

Nft& Nft::insert_identity(const State state, const std::vector<Symbol>& symbols, const JumpMode jump_mode) {
	for (const Symbol symbol : symbols) { insert_identity(state, symbol, jump_mode); }
	return *this;
}

Nft& Nft::insert_identity(const State state, const Alphabet* alphabet, const JumpMode jump_mode) {
	for (const Symbol symbol : get_symbols_to_work_with(alphabet, levels[state])) {
		insert_identity(state, symbol, jump_mode);
	}
	return *this;
}

Nft& Nft::insert_identity(const State state, const Symbol symbol, const JumpMode jump_mode) {
	add_state(state);
	// A full-span jump must start and end on level 0. AppendDontCares would pad it with DONT_CARE instead of
	//  repeating the symbol, which is no identity, so it keeps the explicit loop like NoJump.
	if (jump_mode == JumpMode::RepeatSymbol && levels[state] == 0) {
		delta.add(state, symbol, state);
	} else {
		insert_word(state, Word(levels.num_of_levels, symbol), state);
	}
	return *this;
}

std::shared_ptr<const mata::Alphabet>
	Nft::resolve_alphabet(const Alphabet* const alphabet, const std::optional<Level> level) const {
	if (alphabet != nullptr) {
		return {alphabet, [](const Alphabet*) {}};
	}
	if (this->alphabets != nullptr) { return this->alphabets->for_level(level); }
	return {std::make_shared<mata::EnumAlphabet>(EnumAlphabet{delta.get_used_symbols()})};
}

OrdVector<Symbol>
	Nft::get_symbols_to_work_with(const Alphabet* const alphabet, const std::optional<Level> level) const {
	return mata::nft::get_symbols_to_work_with(*this, alphabet, level);
}

bool Nft::contains_jump_transitions() const {
	if (levels.num_of_levels == 1) { return false; }

	for (const Transition& transition : delta.transitions()) {
		const Level src_level{levels[transition.source]};
		Level tgt_level{levels[transition.target]};
		if (tgt_level == 0) {
			// we want to check if the difference between src and tgt levels is at most 1 modulo levels.num_of_levels
			tgt_level = tgt_level + static_cast<Level>(levels.num_of_levels);
		}
		if (tgt_level - src_level != 1) { return true; }
	}
	return false;
}

void Nft::clear() {
	Automaton::clear();
	levels.clear();
}

Nft& Nft::unify_initial(const bool force_new_state) {
	if (!force_new_state && (initial.empty() || initial.size() == 1)) { return *this; }

	const bool unified_state_is_final{initial.intersects_with(final)};
	// Nft::add_state() puts the new state on level 0 and grows `levels` along with `delta`. It also allocates the
	//  delta for the new state, hence 'mutable_state_post()' below cannot reallocate the delta.
	const State new_initial_state{add_state()};

	// Merge the state posts of all the original initial states symbol by symbol. The synchronized iterator yields
	//  the symbols in an increasing order, therefore the new state post stays sorted when appending to it.
	SynchronizedExistentialSymbolPostIterator sync_iterator{};
	sync_iterator.reserve(initial.size());
	for (const State orig_initial_state : initial) {
		const StatePost& state_post{delta.state_post(orig_initial_state)};
		sync_iterator.push_back(state_post.cbegin(), state_post.cend());
	}
	StatePost& new_state_post{delta.mutable_state_post(new_initial_state)};
	while (sync_iterator.advance()) {
		new_state_post.push_back(
			SymbolPost{sync_iterator.get_current_minimum()->symbol, sync_iterator.unify_targets()}
		);
	}

	if (unified_state_is_final) { final.insert(new_initial_state); }
	initial.clear();
	initial.insert(new_initial_state);
	return *this;
}

Nft& Nft::unify_final(const bool force_new_state) {
	if (!force_new_state && (final.empty() || final.size() == 1)) { return *this; }

	const bool unified_state_is_initial{final.intersects_with(initial)};
	// Nft::add_state() puts the new state on level 0 and grows `levels` along with `delta`. It also allocates the
	//  delta for the new state, hence 'mutable_state_post()' below cannot reallocate the delta.
	const State new_final_state{add_state()};

	// A single pass over the delta: redirect every transition leading to a final state to the new final state, too.
	for (State source{0}; source < delta.num_of_states(); ++source) {
		for (SymbolPost& symbol_post : delta.mutable_state_post(source)) {
			if (std::ranges::any_of(symbol_post.targets, [&](const State target) { return final[target]; })) {
				// The new state is greater than any state in the delta, hence the targets remain sorted.
				symbol_post.targets.push_back(new_final_state);
			}
		}
	}

	if (unified_state_is_initial) { initial.insert(new_final_state); }
	final.clear();
	final.insert(new_final_state);
	return *this;
}

bool Nft::is_deterministic() const {
	if (initial.size() != 1) { return false; }
	if (delta.empty()) { return true; }

	const size_t aut_size{num_of_states()};
	for (State i = 0; i < aut_size; ++i) {
		const StatePost& state_post{delta[i]};
		bool has_dont_care{false};
		State dont_care_target{};
		for (const SymbolPost& symbol_post : state_post) {
			if (symbol_post.num_of_targets() != 1) { return false; }
			if (symbol_post.symbol == DONT_CARE) {
				has_dont_care = true;
				dont_care_target = *symbol_post.targets.begin();
			}
		}

		if (!has_dont_care) { continue; }
		for (const SymbolPost& symbol_post : state_post) {
			// DONT_CARE overlaps every non-epsilon concrete symbol.
			// Overlapping transitions are still deterministic when they lead to the same state.
			if (symbol_post.symbol != DONT_CARE && symbol_post.symbol != EPSILON &&
				*symbol_post.targets.begin() != dont_care_target) {
				return false;
			}
		}
	}
	return true;
}

bool Nft::is_complete(const OrdVector<Symbol>& symbols) const {
	// TODO: make a general function for traversal over reachable states that can be shared by other functions?
	std::list<State> worklist(initial.begin(), initial.end());
	std::unordered_set<State> processed(initial.begin(), initial.end());
	const bool epsilon_is_in_alphabet{!symbols.empty() && symbols.back() == EPSILON};

	while (!worklist.empty()) {
		const State state = *worklist.begin();
		worklist.pop_front();

		size_t num_of_exact_symbols{0};
		bool has_dont_care{false};
		bool has_epsilon{false};
		for (const SymbolPost& symbol_post : delta[state]) {
			if (symbol_post.symbol < DONT_CARE) {
				if (!haskey(symbols, symbol_post.symbol)) {
					throw std::runtime_error(
						std::to_string(__func__) + ": encountered a symbol that is not in the provided alphabet"
					);
				}
				++num_of_exact_symbols;
			} else if (symbol_post.symbol == DONT_CARE) {
				has_dont_care = true;
			} else {
				MATA_ASSERT(symbol_post.symbol == EPSILON, "Nft::is_complete: unexpected special symbol.");
				has_epsilon = true;
				if (epsilon_is_in_alphabet) { ++num_of_exact_symbols; }
			}

			for (const State target_state : symbol_post.targets) {
				const bool inserted{processed.insert(target_state).second};
				if (inserted) { worklist.push_back(target_state); }
			}
		}

		// DONT_CARE covers every non-epsilon symbol. EPSILON only matches itself.
		if (has_dont_care) {
			if (epsilon_is_in_alphabet && !has_epsilon) { return false; }
		} else if (num_of_exact_symbols != symbols.size()) {
			return false;
		}
	}

	return true;
}

StateSet Nft::mk_epsilon_closure(const StateSet& source_states, const std::vector<Symbol>& epsilons) const {
	StateSet closure{source_states};
	std::queue<State> worklist;
	for (const State state : source_states) { worklist.push(state); }
	while (!worklist.empty()) {
		const State state = worklist.front();
		worklist.pop();
		for (const Symbol epsilon : epsilons) {
			if (auto move_it{delta[state].find(epsilon)}; move_it != delta[state].end()) {
				for (const State target : move_it->targets) {
					if (closure.insert(target).second) { worklist.push(target); }
				}
			}
		}
	}
	return closure;
}

void Nft::fill_alphabet(OnTheFlyAlphabet& alphabet_to_fill) const {
	for (const StatePost& state_post : delta) {
		for (const SymbolPost& symbol_post : state_post) {
			alphabet_to_fill.update_next_symbol_value(symbol_post.symbol);
			alphabet_to_fill.try_add_new_symbol(std::to_string(symbol_post.symbol), symbol_post.symbol);
		}
	}
}

bool Nft::is_identical(const Nft& aut) const {
	// Nft's own is_identical() hides Automaton::is_identical() by name, so an unqualified call here
	//  would recurse into this very function instead of reaching the structural comparison.
	return levels.num_of_levels == aut.levels.num_of_levels && levels == aut.levels && Automaton::is_identical(aut);
}

bool mata::nft::internal::unwinding_changes(const Nft& nft, const OrdVector<Symbol>& dont_care_symbol_replacements) {
	if (nft.contains_jump_transitions()) { return true; }
	if (dont_care_symbol_replacements == OrdVector<Symbol>{DONT_CARE}) { return false; }
	return std::ranges::any_of(nft.delta, [](const StatePost& state_post) {
		return state_post.find(DONT_CARE) != state_post.end();
	});
}

mata::nfa::Nfa
	Nft::to_nfa_update_copy(const OrdVector<Symbol>& dont_care_symbol_replacements, const JumpMode jump_mode) const {
	internal::validate_nft_levels(*this, "to_nfa_update_copy");
	if (!internal::unwinding_changes(*this, dont_care_symbol_replacements)) { return to_nfa_copy(); }
	// Unwind a private copy and move it out instead of copying the unwound transducer once more.
	Nft unwound{*this};
	unwound.unwind_jumps_inplace(dont_care_symbol_replacements, jump_mode);
	return unwound.to_nfa_move();
}

mata::nfa::Nfa
	Nft::to_nfa_update_move(const OrdVector<Symbol>& dont_care_symbol_replacements, const JumpMode jump_mode) {
	unwind_jumps_inplace(dont_care_symbol_replacements, jump_mode);
	return to_nfa_move();
}

Nft Nft::apply(
	const mata::nfa::Nfa& nfa,
	const Level level_to_apply_on,
	const bool project_out_applied_level,
	const JumpMode jump_mode
) const {
	// A projected image is trimmed anyway; asking for it explicitly lets the composer race both directions, which
	//  matters for applying languages, where the forward composition can build orders of magnitude more dead states.
	return compose(
		Nft{nfa}, *this, 0, level_to_apply_on, project_out_applied_level, jump_mode, CompositionMode::Auto,
		project_out_applied_level ? CompositionNormalization::Trim : CompositionNormalization::Auto
	);
}

Nft Nft::apply(
	const Word& word, const Level level_to_apply_on, const bool project_out_applied_level, const JumpMode jump_mode
) const {
	return apply(nfa::builder::create_single_word_nfa(word), level_to_apply_on, project_out_applied_level, jump_mode);
}

bool Nft::make_complete(const Alphabet* const alphabet, const std::optional<std::vector<State>>& sink_states) {
	return make_complete(get_symbols_to_work_with(alphabet), sink_states);
}

bool Nft::make_complete(const OrdVector<Symbol>& symbols, const std::optional<std::vector<State>>& sink_states) {
	const size_t num_of_states_orig{this->num_of_states()};
	if (num_of_states_orig == 0) { return false; }

	const std::vector<State> sinks{[&] {
		auto sinks_val{sink_states.value_or(std::vector<State>{})};
		if (sinks_val.empty()) {
			for (Level level{0}; level < levels.num_of_levels; ++level) {
				sinks_val.push_back(add_state_with_level(level));
			}
		}
		MATA_ASSERT(
			sinks_val.size() == levels.num_of_levels,
			"Nft::make_complete: sink_states size must be equal to num_of_levels."
		);

		MATA_ASSERT( // NOLINT(*-assert-side-effect)
			[&] {
				for (Level level{0}; level < levels.num_of_levels; ++level) {
					const State sink_state{sinks_val[level]};
					if (sink_state >= this->num_of_states() || levels[sink_state] != level) { return false; }
				}
				return true;
			}(),
			"Nft::make_complete: sink_states must have correct levels and exist in the NFT.");
		return sinks_val;
	}()};

	auto next_level = [&](const Level level) {
		return (levels.num_of_levels == 1) ? level : (level + 1) % static_cast<Level>(levels.num_of_levels);
	};

	// Add missing transitions from original states to sink states. Both a state post and @p symbols are sorted, so one
	//  merge finds the missing symbols, and the completed post is built by appending, not by a sorted insert per
	//  missing symbol. A post may hold symbols outside @p symbols (e.g., EPSILON), which complete nothing.
	bool transition_added{false};
	const auto symbols_begin{symbols.begin()};
	const auto symbols_end{symbols.end()};
	StatePost completed_post{}; // Scratch buffer, swapped with each rebuilt state post.
	for (State state{0}; state < num_of_states_orig; ++state) {
		// The constant accessor first: 'mutable_state_post()' allocates the posts up to 'state', which a complete
		//  state must not cause.
		const StatePost& post{delta[state]};
		auto symbols_it{symbols_begin};
		auto post_it{post.begin()};
		const auto post_end{post.end()};
		while (symbols_it != symbols_end) {
			while (post_it != post_end && post_it->symbol < *symbols_it) { ++post_it; }
			if (post_it == post_end || post_it->symbol != *symbols_it) { break; } // '*symbols_it' is missing.
			++post_it;
			++symbols_it;
		}
		if (symbols_it == symbols_end) { continue; }
		transition_added = true;
		const size_t num_of_missing_at_most{static_cast<size_t>(symbols_end - symbols_it)};

		const State sink{sinks[next_level(levels[state])]};
		StatePost& state_post{delta.mutable_state_post(state)}; // 'post' dangles from here.
		completed_post.clear();
		completed_post.reserve(state_post.size() + num_of_missing_at_most);
		symbols_it = symbols_begin;
		for (SymbolPost& symbol_post : state_post) {
			while (symbols_it != symbols_end && *symbols_it < symbol_post.symbol) {
				completed_post.emplace_back(*symbols_it, sink);
				++symbols_it;
			}
			if (symbols_it != symbols_end && *symbols_it == symbol_post.symbol) { ++symbols_it; }
			completed_post.emplace_back(std::move(symbol_post));
		}
		for (; symbols_it != symbols_end; ++symbols_it) { completed_post.emplace_back(*symbols_it, sink); }
		std::swap(state_post, completed_post);
	}

	// Add loops on sink states (from sink to a sink of the next level). They were created above, after every original
	//  state, so their posts are empty and the sorted @p symbols are appended in order.
	if (transition_added && num_of_states_orig < this->num_of_states()) {
		for (Level level{0}; level < levels.num_of_levels; ++level) {
			StatePost& sink_post{delta.mutable_state_post(sinks[level])};
			MATA_ASSERT(sink_post.empty(), "Nft::make_complete: the post of a fresh sink state must be empty.");
			sink_post.reserve(symbols.size());
			for (const Symbol symbol : symbols) { sink_post.emplace_back(symbol, sinks[next_level(level)]); }
		}
	}

	return transition_added;
}
