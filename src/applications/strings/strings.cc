/* nfa-strings.cc -- Operations on NFAs for string solving.
 */

#include "mata/applications/strings.hh"
#include "mata/nfa/builder.hh"

#include <optional>

using namespace mata::nfa;
using namespace mata::applications::strings;

std::set<mata::Word> mata::applications::strings::get_shortest_words(const Nfa& nfa) {
    // Map mapping states to a set of the shortest words accepted by the automaton from the mapped state.
    // Get the shortest words for all initial states accepted by the whole automaton (not just a part of the automaton).
    return ShortestWordsMap{ nfa }.get_shortest_words_from(StateSet{ nfa.initial });
}

std::set<mata::Word> ShortestWordsMap::get_shortest_words_from(const StateSet& states) const {
    std::set<Word> result{};

    if (!shortest_words_map_.empty()) {
        WordLength shortest_words_length{ -1 };
        for (const State state : states) {
            const auto& current_shortest_words_map{shortest_words_map_.find(state)};
            if (current_shortest_words_map == shortest_words_map_.end()) { continue; }

            if (const auto& [length, shortest_words]{ current_shortest_words_map->second };
                result.empty() || length < shortest_words_length) {
                // Find a new set of the shortest words.
                result = shortest_words;
                shortest_words_length = length;
            } else if (length == shortest_words_length) {
                // Append the shortest words from other state of the same length to the already found set of the shortest words.
                result.insert(
                    shortest_words.begin(),
                    shortest_words.end()
                );
            }
        }

    }

    return result;
}

std::set<mata::Word> ShortestWordsMap::get_shortest_words_from(const State state) const {
    return get_shortest_words_from(StateSet{ state });
}

void ShortestWordsMap::insert_initial_lengths() {
    if (const auto initial_states{ reversed_automaton_.initial }; !initial_states.empty()) {
        for (const State state : initial_states) {
            shortest_words_map_.insert(
                std::make_pair(
                    state, std::make_pair(
                        0,
                        std::set<Word>{ std::vector<Symbol>{} }
                    )
                )
            );
        }

        const auto initial_states_begin{ initial_states.begin() };
        const auto initial_states_end{ initial_states.end() };
        processed_.insert(initial_states_begin, initial_states_end);
        fifo_queue_.insert(
            fifo_queue_.end(), initial_states_begin,
            initial_states_end
        );
    }
}

void ShortestWordsMap::compute() {
    while (!fifo_queue_.empty()) {
        const State state{ fifo_queue_.front() };
        fifo_queue_.pop_front();
        // Compute the shortest words for the current state.
        compute_for_state(state);
    }
}

void ShortestWordsMap::compute_for_state(const State state) {
    const LengthWordsPair& dst{ map_default_shortest_words(state) };
    const WordLength dst_length_plus_one{ dst.first + 1 };

    for (const SymbolPost& transition : reversed_automaton_.delta.state_post(state)) {
        for (const State state_to : transition.targets) {
            const LengthWordsPair& orig{ map_default_shortest_words(state_to) };
            LengthWordsPair act{ orig };

            if ((act.first == -1) || (dst_length_plus_one < act.first)) {
                // Found new shortest words after appending transition symbols.
                act.second.clear();
                update_current_words(act, dst, transition.symbol);
            } else if (dst_length_plus_one == act.first) {
                // Append transition symbol to increase length of the shortest words.
                update_current_words(act, dst, transition.symbol);
            }

            if (orig.second != act.second) { shortest_words_map_[state_to] = act; }

            if (!processed_.contains(state_to)) {
                processed_.insert(state_to);
                fifo_queue_.push_back(state_to);
            }
        }
    }
}

void ShortestWordsMap::update_current_words(LengthWordsPair& act, const LengthWordsPair& dst, const Symbol symbol) {
    for (Word word: dst.second) {
        word.insert(word.begin(), symbol);
        act.second.insert(word);
    }
    act.first = dst.first + 1;
}

std::set<mata::Symbol> mata::applications::strings::get_accepted_symbols(const Nfa& nfa) {
    std::set<mata::Symbol> accepted_symbols;
    for (const State init : nfa.initial) {
        for (const SymbolPost& symbol_post_init : nfa.delta[init]) {
            mata::Symbol sym = symbol_post_init.symbol;
            if (auto symbol_it = accepted_symbols.lower_bound(sym);
                (symbol_it == accepted_symbols.end() || *symbol_it != sym)
                && nfa.final.intersects_with(symbol_post_init.targets)) { accepted_symbols.insert(symbol_it, sym); }
        }
    }
    return accepted_symbols;
}

std::set<std::pair<int, int>> mata::applications::strings::get_word_lengths(const Nfa& aut) {
    Nfa one_letter;
    /// if we are interested in lengths of words, it suffices to forget the different symbols on transitions.
    /// The lengths of @p aut are hence equivalent to lengths of the NFA taken from @p aut where all symbols on
    /// transitions are renamed to a single symbol (e.g., `a`).
    aut.get_one_letter_aut(one_letter);
    one_letter = determinize(one_letter).trim();
    if(one_letter.num_of_states() == 0) {
        return {};
    }

    std::set<std::pair<int, int>> ret;
    std::vector<int> handles(one_letter.num_of_states(), 0); // initialized to 0
    assert(one_letter.initial.size() == 1);
    std::optional<nfa::State> curr_state = *one_letter.initial.begin();
    std::set<nfa::State> visited;
    int cnt = 0; // handle counter
    int loop_size = 0; // loop size
    int loop_start = -1; // cnt where the loop starts

    while(curr_state.has_value()) {
        visited.insert(curr_state.value());
        handles[curr_state.value()] = cnt++;
        nfa::StatePost post = one_letter.delta[curr_state.value()];

        curr_state.reset();
        assert(post.size() <= 1);
        for(const SymbolPost& move : post) {
            assert(move.targets.size() == 1);
            if (nfa::State target = *move.targets.begin(); !visited.contains(target)) { curr_state = target; } else {
                curr_state.reset();
                loop_start = handles[target];
                loop_size = cnt - handles[target];
            }
        }
    }
    for(const nfa::State& fin : one_letter.final) {
        if(handles[fin] >= loop_start) {
            ret.insert({handles[fin], loop_size});
        } else {
            ret.insert({handles[fin], 0});
        }
    }

    return ret;
}

bool mata::applications::strings::is_lang_eps(const Nfa& aut) {
    Nfa tr_aut = Nfa{ aut }.trim();
    if(tr_aut.initial.size() == 0)
        return false;
    for(const auto& ini : tr_aut.initial) {
        if(!tr_aut.final[ini])
            return false;
        if(tr_aut.delta[ini].size() > 0)
            return false;
    }
    return true;
}

std::optional<std::vector<mata::Word>> mata::applications::strings::get_words_of_lengths(const Nft& nft, std::vector<unsigned> lengths) {
    assert(nft.levels.num_of_levels == lengths.size());
    assert(!nft.contains_jump_transitions());
    if (nft.initial.empty() || nft.final.empty()) { return std::nullopt; }
    if (nft.initial.intersects_with(nft.final) && std::ranges::all_of(lengths, [](const int x) { return x == 0; })) {
        return std::vector<mata::Word>(nft.levels.num_of_levels, mata::Word());
    }

    for (const State initial_state: nft.initial) {
        /// Current state, its state post iterator, its end iterator, and iterator in the current symbol post to target states.
        std::vector<std::tuple<State, StatePost::const_iterator, StatePost::const_iterator, StateSet::const_iterator>> worklist{};
        std::vector<mata::Word> result(lengths.size());
        auto is_result_correct = [&result, &lengths]() {
            for (size_t i = 0; i < lengths.size(); ++i) {
                if (result[i].size() != lengths[i]) {
                    return false;
                }
            }
            return true;
        };

        const StatePost& initial_state_post{ nft.delta[initial_state] };
        auto initial_symbol_post_it{ initial_state_post.cbegin() };
        auto initial_symbol_post_end{ initial_state_post.cend() };

        if (initial_symbol_post_it == initial_symbol_post_end) { continue; }

        worklist.emplace_back(
            initial_state, initial_symbol_post_it, initial_symbol_post_end, initial_symbol_post_it->targets.cbegin()
        );

        while (!worklist.empty()) {
            // Using references to iterators to be able to increment the top-most element in the worklist in place.
            if (auto& [cur_state, state_post_it, state_post_end, targets_it]{ worklist.back() };
                state_post_it != state_post_end) {
                Symbol cur_symbol = state_post_it->symbol;
                if (const nft::Level cur_level = nft.levels[cur_state];
                    targets_it == state_post_it->targets.cend() || (
                        cur_symbol != EPSILON && lengths[cur_level] == result[cur_level].size())) {
                    ++state_post_it;
                    if (state_post_it != state_post_end) { targets_it = state_post_it->cbegin(); }
                } else {
                    if (cur_symbol != EPSILON) { result[cur_level].push_back(cur_symbol); }
                    if (nft.final.contains(*targets_it) && is_result_correct()) {
                        return result;
                    }
                    if (const StatePost& state_post{ nft.delta[*targets_it] }; !state_post.empty()) {
                        auto new_state_post_it{ state_post.cbegin() };
                        auto new_targets_it{ new_state_post_it->cbegin() };
                        worklist.emplace_back(*targets_it, new_state_post_it, state_post.cend(), new_targets_it);
                    } else {
                        if (cur_symbol != EPSILON) { result[cur_level].pop_back(); }
                        ++targets_it;
                    }
                }
            } else { // state_post_it == state_post_end.
                worklist.pop_back();
                if (!worklist.empty()) {
                    auto& [prev_state, prev_state_post_it, prev_state_post_end, prev_targets_it]{ worklist.back() };
                    assert(prev_state_post_it != prev_state_post_end);
                    const Symbol prev_symbol = prev_state_post_it->symbol;
                    const nft::Level prev_level = nft.levels[prev_state];
                    if (prev_symbol != EPSILON) {
                        assert(!result[prev_level].empty() && result[prev_level].back() == prev_symbol);
                        result[prev_level].pop_back();
                    }
                    ++prev_targets_it;
                }
            }
        }
    }

    return std::nullopt;
}
