/** @file
 * @brief Operations for NFA.
 */

#include <algorithm>
#include <fstream>
#include <iterator>
#include <list>
#include <optional>
#include <queue>
#include <string>

#include "mata/alphabet.hh"
#include "mata/nfa/nfa.hh"
#include "mata/utils/sparse-set.hh"
#include <mata/simlib/explicit_lts.hh>
#include <mata/utils/assert.hh>

using namespace mata::utils;
using namespace mata::nfa;
using mata::BoolVector;
using mata::Symbol;
using mata::Word;

using StateBoolArray = std::vector<bool>; ///< Bool array for states in the automaton.

const std::string mata::nfa::TYPE_NFA = "NFA";

void Nfa::remove_epsilon(const Symbol epsilon) { *this = mata::nfa::remove_epsilon(*this, epsilon); }

Run Nfa::get_shortest_accepting_run_from_state(State state, const std::vector<State>& distances_to_final) const {
	Run result{.word = {}, .path = {state}};
	while (!final[state]) {
		for (auto [symbol, target] : delta[state].moves()) {
			if (distances_to_final[target] < distances_to_final[state]) {
				result.word.push_back(symbol);
				result.path.push_back(target);
				state = target;
				break;
			}
		}
	}
	return result;
}

Nfa mata::nfa::trim(
	const Nfa& nfa,
	StateRenaming* state_renaming,
	std::optional<std::reference_wrapper<const SparseSet<State>>> initial_states,
	std::optional<std::reference_wrapper<const SparseSet<State>>> final_states
) {
	Nfa nfa_trimmed{nfa};
	// Inline the custom initial/final override logic before trimming in place.
	if (initial_states) { nfa_trimmed.initial = initial_states->get(); }
	if (final_states) { nfa_trimmed.final = final_states->get(); }
	nfa_trimmed.trim(state_renaming);
	return nfa_trimmed;
}

bool Nfa::is_flat() const {
	bool flat = true;

	mata::nfa::Nfa::TarjanDiscoverCallback callback{};
	callback.scc_discover = [&](const std::vector<mata::nfa::State>& scc,
								const std::vector<mata::nfa::State>& tarjan_stack) -> bool {
		(void) tarjan_stack;

		for (const mata::nfa::State& st : scc) {
			bool one_input_visited = false;
			for (const mata::nfa::SymbolPost& sp : this->delta[st]) {
				for (const mata::nfa::State& tgt : scc) {
					if (sp.targets.contains(tgt)) {
						if (one_input_visited) {
							flat = false;
							return true;
						}
						one_input_visited = true;
					}
				}
			}
		}
		return false;
	};

	tarjan_scc_discover(callback);
	return flat;
}

std::string Nfa::print_to_dot(
	const bool decode_ascii_chars, const bool use_intervals, const int max_label_length, const Alphabet* alphabet
) const {
	std::stringstream output;
	print_to_dot(output, decode_ascii_chars, use_intervals, max_label_length, alphabet);
	return output.str();
}

void Nfa::print_to_dot(
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
				return std::string(1, static_cast<char>(symbol));
		}
	};

	auto translate_symbol = [&](const Symbol symbol) -> std::string {
		switch (symbol) {
			case EPSILON:
				return "<eps>";
			default:
				break;
		}
		if (decode_ascii_chars) {
			return to_ascii(symbol);
		} else if (alphabet != nullptr) {
			return alphabet->reverse_translate_symbol(symbol);
		} else if (this->alphabet != nullptr) {
			return this->alphabet->reverse_translate_symbol(symbol);
		} else {
			return std::to_string(symbol);
		}
	};

	auto vec_of_symbols_to_string = [&](const OrdVector<Symbol>& symbols) {
		std::string result;
		for (const Symbol& symbol : symbols) { result += translate_symbol(symbol) + ","; }
		result.pop_back(); // Remove last comma
		return result;
	};

	auto vec_of_symbols_to_string_with_intervals = [&](const OrdVector<Symbol>& symbols) {
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
				result += translate_symbol(symbol_from) + ",";
			} else if (interval_size == 2) {
				result += translate_symbol(symbol_from) + "," + translate_symbol(symbol_to) + ",";
			} else {
				result += "[" + translate_symbol(symbol_from) + "-" + translate_symbol(symbol_to) + "],";
			}
		}

		result.pop_back(); // Remove last comma
		return result;
	};

	BoolVector is_state_drawn(num_of_states(), false);
	output << "digraph finiteAutomaton {" << std::endl << "node [shape=circle];" << std::endl;

	// Double circle for final states
	for (const State final_state : final) {
		is_state_drawn[final_state] = true;
		output << final_state << " [shape=doublecircle];" << std::endl;
	}

	// Print transitions
	const size_t delta_size = delta.num_of_states();
	for (State source = 0; source != delta_size; ++source) {
		std::unordered_map<State, OrdVector<Symbol>> tgt_symbols_map;
		for (const SymbolPost& move : delta[source]) {
			is_state_drawn[source] = true;
			for (State target : move.targets) {
				is_state_drawn[target] = true;
				tgt_symbols_map[target].insert(move.symbol);
			}
		}
		for (const auto& [target, symbols] : tgt_symbols_map) {
			if (max_label_length == 0) {
				output << source << " -> " << target << ";" << std::endl;
				continue;
			}

			std::string label =
				(use_intervals) ? vec_of_symbols_to_string_with_intervals(symbols) : vec_of_symbols_to_string(symbols);
			std::string on_hover_label = utils::replace_all(utils::replace_all(label, "<", "&lt;"), ">", "&gt;");
			bool is_shortened = false;
			if (max_label_length > 0 && label.length() > static_cast<size_t>(max_label_length)) {
				label.replace(static_cast<size_t>(max_label_length), std::string::npos, "...");
				is_shortened = true;
			}

			if (is_shortened) {
				output << source << " -> " << target << " [label=\"" << label << "\", tooltip=\"" << on_hover_label
					   << "\"];" << std::endl;
			} else {
				output << source << " -> " << target << " [label=\"" << label << "\"];" << std::endl;
			}
		}
	}

	// Circle for isolated states with no transitions
	for (State state{0}; state < is_state_drawn.size(); ++state) {
		if (!is_state_drawn[state]) { output << state << " [shape=circle];" << std::endl; }
	}

	// Arrow for initial states
	output << "node [shape=none, label=\"\"];" << std::endl;
	for (const State init_state : initial) { output << "i" << init_state << " -> " << init_state << ";" << std::endl; }

	output << "}" << std::endl;
}

void Nfa::print_to_dot(
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

std::string Nfa::print_to_mata(const Alphabet* alphabet) const {
	std::stringstream output;
	print_to_mata(output, alphabet);
	return output.str();
}

void Nfa::print_to_mata(std::ostream& output, const Alphabet* alphabet) const {
	output << "@NFA-explicit" << std::endl << "%Alphabet-auto" << std::endl;
	// TODO should be this, but we cannot parse %Alphabet-numbers yet
	//<< "%Alphabet-numbers" << std::endl;

	if (!initial.empty()) {
		output << "%Initial";
		for (const State init_state : initial) { output << " q" << init_state; }
		output << std::endl;
	}

	if (!final.empty()) {
		output << "%Final";
		for (const State final_state : final) { output << " q" << final_state; }
		output << std::endl;
	}

	for (const Transition& trans : delta.transitions()) {
		output << "q" << trans.source << " "
			   << ((alphabet != nullptr)
					   ? alphabet->reverse_translate_symbol(trans.symbol)
					   : ((this->alphabet != nullptr) ? this->alphabet->reverse_translate_symbol(trans.symbol)
													  : std::to_string(trans.symbol)))
			   << " q" << trans.target << std::endl;
	}
}

void Nfa::print_to_mata(const std::string& filename, const Alphabet* alphabet) const {
	std::ofstream output(filename);
	if (!output) { throw std::ios_base::failure("Failed to open file: " + filename); }
	print_to_mata(output, alphabet);
}

Nfa Nfa::get_one_letter_aut(const Symbol abstract_symbol) const {
	Nfa digraph{num_of_states(), initial, final};
	// Add directed transitions for digraph.
	for (const Transition& transition : delta.transitions()) {
		// Directly try to add the transition. Finding out whether the transition is already in the digraph
		//  only iterates through transition relation again.
		digraph.delta.add(transition.source, abstract_symbol, transition.target);
	}
	return digraph;
}

void Nfa::get_one_letter_aut(Nfa& result) const { result = get_one_letter_aut(); }

StateSet Nfa::post(const StateSet& states, const Symbol symbol, const EpsilonClosureOpt epsilon_closure_opt) const {
	PostWorkspace workspace{};
	return delta.post(states, symbol, epsilon_closure_opt, {EPSILON}, workspace);
}

Nfa& Nfa::unify_initial(const bool force_new_state) {
	if (!force_new_state && (initial.empty() || initial.size() == 1)) { return *this; }

	const bool unified_state_is_final{initial.intersects_with(final)};
	// Allocates the delta for the new state, hence 'mutable_state_post()' below cannot reallocate the delta.
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

Nfa& Nfa::unify_final(const bool force_new_state) {
	if (!force_new_state && (final.empty() || final.size() == 1)) { return *this; }

	const bool unified_state_is_initial{final.intersects_with(initial)};
	// Allocates the delta for the new state, hence 'mutable_state_post()' below cannot reallocate the delta.
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

Nfa& Nfa::operator=(Nfa&& other) noexcept {
	if (this != &other) {
		Automaton::operator=(std::move(other));
		alphabet = std::move(other.alphabet);
		attributes = std::move(other.attributes);
	}
	return *this;
}

void Nfa::add_transition(
	const State source, const std::string& symbol_name, const State target, Alphabet* const alphabet
) {
	Alphabet* const resolved_alphabet{alphabet != nullptr ? alphabet : this->alphabet.get()};
	if (resolved_alphabet == nullptr) {
		throw std::runtime_error(
			"Nfa::add_transition(): no alphabet available to translate symbol '" + symbol_name + "'"
		);
	}
	delta.add(source, resolved_alphabet->translate_symb(symbol_name), target);
}

State Nfa::insert_word(const State source, const Word& word, const State target) {
	MATA_ASSERT(!word.empty());
	MATA_ASSERT(source < num_of_states());
	MATA_ASSERT(target < num_of_states());

	const size_t word_len = word.size();
	if (word_len == 1) {
		delta.add(source, word[0], target);
		return target;
	}

	// Add transition source --> inner_state.
	State inner_state = add_state();
	delta.add(source, word[0], inner_state);

	// Add transitions inner_state --> inner_state
	State prev_state = inner_state;
	for (size_t idx{1}; idx < word_len - 1; idx++) {
		inner_state = add_state();
		delta.add(prev_state, word[idx], inner_state);
		prev_state = inner_state;
	}

	// Add transition inner_state --> target
	delta.add(prev_state, word[word_len - 1], target);
	return target;
}

State Nfa::insert_word(const State source, const Word& word) { return insert_word(source, word, add_state()); }

Nfa& Nfa::complement_deterministic(const OrdVector<Symbol>& symbols, const std::optional<State> sink_state) {
	const State sink{sink_state.value_or(num_of_states())};
	if (initial.empty()) { // The automaton has no reachable states (accepting an empty language).
		// Insert a single initial sink state.
		initial.insert(sink);
	}
	make_complete(symbols, sink);
	swap_final_nonfinal();
	return *this;
}

Nfa& Nfa::unite_nondet_with(const mata::nfa::Nfa& nfa) {
	if (this == &nfa) { return *this; }
	if (final.empty() || initial.empty()) {
		*this = nfa;
		return *this;
	}
	if (nfa.final.empty() || nfa.initial.empty()) { return *this; }

	const size_t num_of_states_this{this->num_of_states()};
	const size_t num_of_states_aut{nfa.num_of_states()};
	const size_t num_of_states_result{num_of_states_this + num_of_states_aut};

	this->delta.reserve(num_of_states_result);
	// Allocate space for initial and final states from 'this' which might be missing in Delta.
	// This ensures that the next state appended to Delta will have the correct index for the first state of 'aut'.
	this->delta.allocate(num_of_states_this);

	auto renumber_states{[&](const State state) { return num_of_states_this + state; }};
	this->delta.append(nfa.delta.renumber_targets(renumber_states));

	// Set accepting states.
	this->final.reserve(num_of_states_result);
	for (const State& aut_fin : nfa.final) { this->final.insert(renumber_states(aut_fin)); }
	// Set initial states.
	this->initial.reserve(num_of_states_result);
	for (const State& aut_ini : nfa.initial) { this->initial.insert(renumber_states(aut_ini)); }

	return *this;
}


Nfa& Nfa::unite_nondet_with(Nfa&& nfa) {
	if (this == &nfa) {
		// Alias: take a snapshot to avoid reading moved-from state.
		Nfa snapshot = nfa;
		nfa.clear();
		return unite_nondet_with(snapshot);
	}

	if (final.empty() || initial.empty()) {
		*this = std::move(nfa);
		return *this;
	}
	if (nfa.final.empty() || nfa.initial.empty()) { return *this; }

	const size_t num_of_states_this{this->num_of_states()};
	const size_t num_of_states_aut{nfa.num_of_states()};

	this->delta.reserve(num_of_states_this + num_of_states_aut);
	this->delta.allocate(num_of_states_this);

	// Use append_shifted to move the rhs state posts without copying.
	this->delta.append_shifted(std::move(nfa.delta), num_of_states_this);

	// Set accepting states.
	this->final.reserve(num_of_states_this + num_of_states_aut);
	for (const State& aut_fin : nfa.final) {
		this->final.insert(num_of_states_this + aut_fin);
	}
	// Set initial states.
	this->initial.reserve(num_of_states_this + num_of_states_aut);
	for (const State& aut_ini : nfa.initial) {
		this->initial.insert(num_of_states_this + aut_ini);
	}

	// Clear nfa to leave it valid but empty.
	nfa.clear();
	return *this;
}


Nfa Nfa::decode_utf8() const {
	Nfa result{num_of_states(), {initial}, {final}};
	BoolVector used(num_of_states(), false);
	std::stack<State> worklist;

	// Pushes a set of states to the worklist and marks them as used.
	auto push_state_set = [&](const StateSet& set) {
		for (State state : set) {
			if (used[state]) { continue; }
			worklist.push(state);
			used[state] = true;
		}
	};

	// Adds a symbol_post to the state_post.
	// If the transition sequence is deterministic, we can use emplace_back
	// because symbols are discovered in ascending order. However, in cases
	// of nondeterministic sequences, we must use insert to ensure proper ordering.
	// For example, consider the sequences 0xC8 0x80 and 0xC8 0x88.
	// Based solely on the first byte (0xC8), we cannot determine which sequence
	// will result in the higher number.
	auto add_to_state_post = [&](StatePost& state_post, const SymbolPost& symbol_post, const bool is_nondet) {
		if (is_nondet) {
			if (auto [it, inserted] = state_post.insert(symbol_post); !inserted) {
				it->insert(symbol_post.targets);
			}
		} else {
			state_post.emplace_back(symbol_post);
		}
	};

	// UTF-8 Byte Patterns:
	// U+0000   to U+007F  : 0xxxxxxx
	// U+0080   to U+07FF  : 110xxxxx 10xxxxxx
	// U+0800   to U+FFFF  : 1110xxxx 10xxxxxx 10xxxxxx
	// U+010000 to U+10FFFF: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
	// NOTE: Due to the nature of RE2, the automaton language can contain unexpected (invalid)
	//       UTF-8 sequences, such as 11000000 10000000 (U+0300). Because of that,
	//       we need to check if the decoded symbol is within the valid range of Unicode code points.
	push_state_set(StateSet{this->initial});
	while (!worklist.empty()) {
		const State q1 = worklist.top();
		StatePost& q1_state_post = result.delta.mutable_state_post(q1);
		worklist.pop();
		// 1st Byte
		for (const SymbolPost& sp1 : this->delta[q1]) {
			const Symbol s1 = sp1.symbol;
			if ((s1 & 0x80) == 0x00) {
				q1_state_post.emplace_back(SymbolPost{s1, sp1.targets});
				;
				push_state_set(sp1.targets);
				continue;
			}
			// 2nd Byte
			const bool is_nondet1 = sp1.targets.size() > 1;
			for (const State q2 : sp1.targets) {
				for (const SymbolPost& sp2 : this->delta[q2]) {
					const Symbol s2 = sp2.symbol;
					if ((s1 & 0xE0) == 0xC0) {
						MATA_ASSERT((s2 & 0xC0) == 0x80);
						const Symbol symbol = ((s1 & 0x1F) << 6) | (s2 & 0x3F);
						if (symbol < 0x80) {
							continue; // Invalid UTF-8 sequence
						}
						MATA_ASSERT(symbol <= 0x7'FF);
						add_to_state_post(q1_state_post, SymbolPost{symbol, sp2.targets}, is_nondet1);
						push_state_set(sp2.targets);
						continue;
					}
					// 3rd Byte
					const bool is_nondet2 = is_nondet1 || sp2.targets.size() > 1;
					for (const State q3 : sp2.targets) {
						for (const SymbolPost& sp3 : this->delta[q3]) {
							const Symbol s3 = sp3.symbol;
							if ((s1 & 0xF0) == 0xE0) {
								MATA_ASSERT((s3 & 0xC0) == 0x80);
								const Symbol symbol = ((s1 & 0x0F) << 12) | ((s2 & 0x3F) << 6) | (s3 & 0x3F);
								if (symbol < 0x8'00) {
									continue; // Invalid UTF-8 sequence
								}
								MATA_ASSERT(symbol <= 0xFF'FF);
								add_to_state_post(q1_state_post, SymbolPost{symbol, sp3.targets}, is_nondet2);
								push_state_set(sp3.targets);
								continue;
							}
							// 4th Byte
							const bool is_nondet3 = is_nondet2 || sp3.targets.size() > 1;
							for (const State q4 : sp3.targets) {
								for (const SymbolPost& sp4 : this->delta[q4]) {
									const Symbol s4 = sp4.symbol;
									MATA_ASSERT((s1 & 0xF8) == 0xF0);
									MATA_ASSERT((s4 & 0xC0) == 0x80);
									const Symbol symbol =
										((s1 & 0x07) << 18) | ((s2 & 0x3F) << 12) | ((s3 & 0x3F) << 6) | (s4 & 0x3F);
									if (symbol < 0x1'00'00 || symbol > 0x10'FF'FF) {
										continue; // Invalid UTF-8 sequence
									}
									add_to_state_post(q1_state_post, SymbolPost{symbol, sp4.targets}, is_nondet3);
									push_state_set(sp4.targets);
								}
							}
						}
					}
				}
			}
		}
	}

	return result;
}

StateSet Nfa::mk_epsilon_closure(const StateSet& source_states, const std::vector<Symbol>& epsilons) const {
	PostWorkspace workspace{};
	return delta.epsilon_closure(source_states, epsilons, workspace);
}
