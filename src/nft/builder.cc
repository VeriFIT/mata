// TODO: Insert header file.

#include "mata/utils/utils.hh"
#include "mata/utils/sparse-set.hh"
#include "mata/nft/builder.hh"

#include <fstream>

using namespace mata::nft;
using mata::nft::Nft;
using mata::Symbol;

Nft builder::construct(const mata::parser::ParsedSection& parsec, mata::Alphabet* alphabet, NameStateMap* state_map) {
    Nft aut;
    assert(nullptr != alphabet);

    // HACK - it should be only "parsec.type != TYPE_NFA" without the conjunction
    if (parsec.type != TYPE_NFT && parsec.type != TYPE_NFT + "-explicit") {
        throw std::runtime_error(std::string(__FUNCTION__) + ": expecting type \"" +
                                 TYPE_NFT + "\"");
    }

    bool remove_state_map = false;
    if (nullptr == state_map) {
        state_map = new NameStateMap();
        remove_state_map = true;
    }

    // a lambda for translating state names to identifiers
    auto get_state_name = [&](const std::string& str) {
        if (!state_map->contains(str)) {
            State state = aut.add_state();
            state_map->insert({str, state});
            return state;
        } else {
            return (*state_map)[str];
        }
    };

    // a lambda for cleanup
    auto clean_up = [&]() {
        if (remove_state_map) { delete state_map; }
    };


    auto it = parsec.dict.find("Initial");
    if (parsec.dict.end() != it)
    {
        for (const auto& str : it->second)
        {
            State state = get_state_name(str);
            aut.initial.insert(state);
        }
    }

    it = parsec.dict.find("Final");
    if (parsec.dict.end() != it) {
        for (const auto& str : it->second) {
            const State state = get_state_name(str);
            aut.final.insert(state);
        }
    }

    it = parsec.dict.find("Levels");
    if (parsec.dict.end() != it) {
        for (const auto &str : it->second) {
            std::stringstream ss(str);
            std::string level_str;
            try {
                std::string state_name;
                std::getline(ss, state_name, ':');
                std::getline(ss, level_str, ':');
                if (!ss.eof()) {
                    throw std::runtime_error("Bad format of levels: too many colons in " + str);
                }

                const State state = get_state_name(state_name);
                const long level = std::stol(level_str);
                if (level < 0) {
                    throw std::runtime_error("Bad format of levels: level " + level_str + " is out of range.");
                }
                aut.levels[state] = static_cast<Level>(level);

            } catch (const std::invalid_argument&) {
                throw std::runtime_error("Bad format of levels: unsupported level " + level_str);
            } catch (const std::out_of_range&) {
                throw std::runtime_error("Bad format of levels: level " + level_str + " is out of range.");
            } catch (...) {
                throw std::runtime_error("Bad format of levels.");
            }
        }
    }

    it = parsec.dict.find("LevelsNum");
    if (parsec.dict.end() != it) {
        if (it->second.empty()) {
            throw std::runtime_error("LevelsNum has to be specified.");
        }
        if (it->second.size() > 1) {
            throw std::runtime_error("Only one LevelsNum can be specified.");
        }
        try {
            const long level = std::stol(it->second[0]);
            if (level < 0) {
                throw std::runtime_error("Bad format of levels: level " + it->second[0] + " is out of range.");
            }
            aut.num_of_levels = static_cast<Level>(level);
        } catch (const std::invalid_argument&) {
            throw std::runtime_error("Bad format of levels: unsupported level " + it->second[0]);
        } catch (const std::out_of_range&) {
            throw std::runtime_error("Bad format of levels: level " + it->second[0] + " is out of range.");
        }
    }

    for (const auto& body_line : parsec.body) {
        if (body_line.size() != 3) {
            // clean up
            clean_up();

            if (body_line.size() == 2) {
                throw std::runtime_error("Epsilon transitions not supported: " +
                                         std::to_string(body_line));
            } else {
                throw std::runtime_error("Invalid transition: " +
                                         std::to_string(body_line));
            }
        }

        const State source = get_state_name(body_line[0]);
        const Symbol symbol = alphabet->translate_symb(body_line[1]);
        const State target = get_state_name(body_line[2]);
        aut.delta.add(source, symbol, target);
    }

    // do the dishes and take out garbage
    clean_up();

    return aut;
} // construct().

Nft builder::construct(const mata::IntermediateAut& inter_aut, mata::Alphabet* alphabet, NameStateMap* state_map) {
    Nft aut;
    assert(nullptr != alphabet);

    if (!inter_aut.is_nft()) {
        throw std::runtime_error(std::string(__FUNCTION__) + ": expecting type \"" +
                                 TYPE_NFT + "\"");
    }

    NameStateMap tmp_state_map;
    if (nullptr == state_map) {
        state_map = &tmp_state_map;
    }

    // a lambda for translating state names to identifiers
    auto get_state_name = [&state_map, &aut](const std::string& str) {
        if (!state_map->contains(str)) {
            State state = aut.add_state();
            state_map->insert({str, state});
            return state;
        } else {
            return (*state_map)[str];
        }
    };

    for (const auto& str : inter_aut.initial_formula.collect_node_names())
    {
        const State state = get_state_name(str);
        aut.initial.insert(state);
    }

    for (const auto& [formula_node, formula_graph] : inter_aut.transitions) {
        if (formula_graph.children.size() != 2) {
            if (formula_graph.children.size() == 1) {
                throw std::runtime_error("Epsilon transitions not supported");
            } else {
                throw std::runtime_error("Invalid transition");
            }
        }

        const State source = get_state_name(formula_node.name);
        const Symbol symbol = alphabet->translate_symb(formula_graph.children[0].node.name);
        const State target = get_state_name(formula_graph.children[1].node.name);

        aut.delta.add(source, symbol, target);
    }

    std::unordered_set<std::string> final_formula_nodes;
    if (!(inter_aut.final_formula.node.is_constant())) {
        // we do not want to parse true/false (constant) as a state so we do not collect it
        final_formula_nodes = inter_aut.final_formula.collect_node_names();
    }

    // for constant true, we will pretend that final nodes are negated with empty final_formula_nodes
    if (inter_aut.final_formula.node.is_true() || inter_aut.are_final_states_conjunction_of_negation()) {
        // Final nodes are negated.
        // We add all states NOT in final_formula_nodes to final states.
        for (const auto & [state_name, state] : *state_map) {
            if (!final_formula_nodes.contains(state_name)) {
                aut.final.insert(state);
            }
        }
    } else {
        // we add all states in final_formula_nodes to final states
        for (const auto& str : final_formula_nodes) { aut.final.insert(get_state_name(str)); }
    }

    return aut;
} // construct().

void builder::construct(
        mata::nft::Nft *result,
        const mata::IntermediateAut &inter_aut,
        mata::Alphabet *alphabet,
        mata::nft::builder::NameStateMap *state_map
) {
    *result = construct(inter_aut, alphabet, state_map);
}

Nft builder::create_single_word_nft(const std::vector<Symbol>& word) {
    return Nft(mata::nfa::builder::create_single_word_nfa(word));
}

Nft builder::create_single_word_nft(const std::vector<std::string>& word, mata::Alphabet *alphabet) {
    return Nft(mata::nfa::builder::create_single_word_nfa(word, alphabet));
}

Nft builder::create_empty_string_nft(const size_t num_of_levels) {
    Nft nft{ nfa::builder::create_empty_string_nfa() };
    nft.num_of_levels = num_of_levels;
    return nft;
}

Nft builder::create_sigma_star_nft(const size_t num_of_levels) {
    Nft nft{ 1, { 0 }, { 0 }, { 0 }, num_of_levels };
    nft.delta.add(0, DONT_CARE, 0);
    return nft;
}

Nft builder::create_sigma_star_nft(const mata::Alphabet* alphabet, const size_t num_of_levels) {
    Nft nft{1, { 0 }, { 0 }, { 0 }, num_of_levels};
    nft.insert_identity(0, alphabet->get_alphabet_symbols().to_vector());
    return nft;
}

Nft builder::parse_from_mata(std::istream& nft_stream) {
    const std::string nft_str = "NFT";
    const parser::Parsed parsed{ parser::parse_mf(nft_stream) };
    if (parsed.size() != 1) {
        throw std::runtime_error("The number of sections in the input file is '" + std::to_string(parsed.size())
            + "'. Required is '1'.\n");
    }
    if (const std::string automaton_type{ parsed[0].type }; automaton_type.compare(0, nft_str.length(), nft_str) != 0) {
        throw std::runtime_error("The type of input automaton is '" + automaton_type + "'. Required is 'NFT'\n");
    }
    IntAlphabet alphabet;
    // return construct(IntermediateAut::parse_from_mf(parsed)[0], &alphabet);
    return construct(parsed[0], &alphabet);
}

Nft builder::parse_from_mata(const std::filesystem::path& nft_file) {
    std::ifstream file_stream{ nft_file };
    if (!file_stream) {
        throw std::runtime_error("Could not open file \'" + nft_file.string() + "'\n");
    }

    Nft nft;
    try {
        nft = parse_from_mata(file_stream);
    } catch (const std::exception&) {
        file_stream.close();
        throw;
    }
    return nft;
}

Nft builder::parse_from_mata(const std::string& nft_in_mata) {
    std::istringstream nft_stream(nft_in_mata);
    return parse_from_mata(nft_stream);
}

Nft builder::from_nfa_with_levels_zero(const mata::nfa::Nfa& nfa, const size_t num_of_levels, const bool explicit_transitions, std::optional<Symbol> next_levels_symbol) {
    if (num_of_levels == 1 || (!explicit_transitions && !next_levels_symbol.has_value())) {
        return Nft(nfa, num_of_levels, 0);
    }

    const size_t nfa_num_of_states{ nfa.num_of_states() };
    Nft nft{ nfa_num_of_states, nfa.initial, nfa.final, {}, num_of_levels };

    State first_level_state;
    State inner_src;
    State inner_tgt;
    Symbol inner_symbol;
    for(State src{ 0 }; src < nfa_num_of_states; ++src) {
        for (const SymbolPost& symbol_post: nfa.delta[src]) {
            if (symbol_post.symbol == EPSILON && (!next_levels_symbol.has_value() || next_levels_symbol.value() == EPSILON)) {
                nft.delta.add(src, symbol_post.symbol, symbol_post.targets);
                continue;
            }

            first_level_state = nft.add_state_with_level(1);
            inner_symbol = next_levels_symbol.has_value() ? next_levels_symbol.value() : symbol_post.symbol;
            for (const State tgt: symbol_post.targets) {
                // Transition from level 0 to level 1
                nft.delta.add(src, symbol_post.symbol, first_level_state);
                inner_src = first_level_state;

                // Transition from level 1 to level num_of_levels-1
                if (explicit_transitions) {
                    for (Level tgt_level{ 2 }; tgt_level < num_of_levels; ++tgt_level) {
                        inner_tgt = nft.add_state_with_level(tgt_level);
                        nft.delta.add(inner_src, inner_symbol, inner_tgt);
                        inner_src = inner_tgt;
                    }
                }

                // Transition from level num_of_levels-1 to level num_of_levels (which is the target state of level 0 = num_of_levels % num_of_levels)
                nft.delta.add(inner_src, inner_symbol, tgt);
            }
        }
    }

    return nft;
}

Nft builder::from_nfa_with_levels_advancing(mata::nfa::Nfa nfa, size_t num_of_levels) {
    Nft result{ std::move(nfa) };
    result.levels = Levels(result.num_of_states(), static_cast<Level>(num_of_levels)); // num_of_levels represents that the state does not have level assigned yet
    result.num_of_levels = num_of_levels;

    // We apply simple DFS
    StateSet worklist; // worklist for DFS
    for (State initial_state : result.initial) {
        // start with initial states, which should be level 0
        result.levels[initial_state] = 0;
        worklist.insert(initial_state);
    }

    while (!worklist.empty()) {
        State state_to_process = worklist.back();
        worklist.pop_back();
        for (State tgt : result.delta[state_to_process].get_successors()) {
            Level next_level = result.levels[state_to_process] + 1;
            if (next_level == num_of_levels) {
                next_level = 0;
            }
            if (result.levels[tgt] == num_of_levels) { // tgt does not have a level yet
                if (next_level != 0 && result.final.contains(tgt)) {
                    throw std::runtime_error("Creating Nft from Nfa that does not represent a valid Nft (final state is not at level 0) in mata::nft::Nft::from_nfa_with_levels_advancing()");
                }
                result.levels[tgt] = next_level;
                worklist.insert(tgt);
            } else if (result.levels[tgt] != next_level) {
                throw std::runtime_error("Creating Nft from Nfa that does not represent a valid Nft (a state has more possible levels) in mata::nft::Nft::from_nfa_with_levels_advancing()");
            }
        }
    }

    return result;
}
