// TODO: Insert header file.

#include "mata/lvlfa/builder.hh"
#include "mata/parser/mintermization.hh"

#include <fstream>

using namespace mata::lvlfa;
using mata::lvlfa::Lvlfa;
using mata::Symbol;

Lvlfa builder::construct(const mata::parser::ParsedSection& parsec, mata::Alphabet* alphabet, NameStateMap* state_map) {
    Lvlfa aut;
    assert(nullptr != alphabet);

    if (parsec.type != TYPE_NFA) {
        throw std::runtime_error(std::string(__FUNCTION__) + ": expecting type \"" +
                                 TYPE_NFA + "\"");
    }

    bool remove_state_map = false;
    if (nullptr == state_map) {
        state_map = new NameStateMap();
        remove_state_map = true;
    }

    // a lambda for translating state names to identifiers
    auto get_state_name = [&state_map, &aut](const std::string& str) {
        if (!state_map->count(str)) {
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
    if (parsec.dict.end() != it)
    {
        for (const auto& str : it->second)
        {
            State state = get_state_name(str);
            aut.final.insert(state);
        }
    }

    aut.levels.clear();
    it = parsec.dict.find("Levels");
    if (parsec.dict.end() != it)
    {
        aut.levels.resize(it->second.size(), 0);
        for (const auto &str : it->second)
        {
            std::stringstream ss(str);
            std::string state_name, level_str;
            try {
                std::getline(ss, state_name, ':');
                std::getline(ss, level_str, ':');
                if (!ss.eof()) {
                    throw std::runtime_error("Bad format of levels: too many colons in " + str);
                }

                State state = get_state_name(state_name);
                long level = std::stol(level_str);
                if (level < 0) {
                    throw std::runtime_error("Bad format of levels: level " + level_str + " is out of range.");
                }
                aut.levels[state] = static_cast<Level>(level);

            } catch (const std::invalid_argument &ex) {
                throw std::runtime_error("Bad format of levels: unsupported level " + level_str);
            } catch (const std::out_of_range &ex) {
                throw std::runtime_error("Bad format of levels: level " + level_str + " is out of range.");
            } catch (...) {
                throw std::runtime_error("Bad format of levels.");
            }
        }
    }

    it = parsec.dict.find("MaxLevel");
    if (parsec.dict.end() != it) {
        if (it->second.size() == 0) {
            throw std::runtime_error("MaxLevel has to be specified.");
        }
        if (it->second.size() > 1) {
            throw std::runtime_error("Only one MexLevel can be specified.");
        }
        try {
            long level = std::stol(it->second[0]);
            if (level < 0) {
                throw std::runtime_error("Bad format of levels: level " + it->second[0] + " is out of range.");
            }
            aut.max_level = static_cast<Level>(level);
        } catch (const std::invalid_argument &ex) {
            throw std::runtime_error("Bad format of levels: unsupported level " + it->second[0]);
        } catch (const std::out_of_range &ex) {
            throw std::runtime_error("Bad format of levels: level " + it->second[0] + " is out of range.");
        }
    }

    for (const auto& body_line : parsec.body)
    {
        if (body_line.size() != 3)
        {
            // clean up
            clean_up();

            if (body_line.size() == 2)
            {
                throw std::runtime_error("Epsilon transitions not supported: " +
                                         std::to_string(body_line));
            }
            else
            {
                throw std::runtime_error("Invalid transition: " +
                                         std::to_string(body_line));
            }
        }

        State src_state = get_state_name(body_line[0]);
        Symbol symbol = alphabet->translate_symb(body_line[1]);
        State tgt_state = get_state_name(body_line[2]);

        aut.delta.add(src_state, symbol, tgt_state);
    }

    // do the dishes and take out garbage
    clean_up();

    return aut;
} // construct().

Lvlfa builder::construct(const mata::IntermediateAut& inter_aut, mata::Alphabet* alphabet, NameStateMap* state_map) {
    // throw std::runtime_error("Constructor via IntermediateAut is not implemented for LVLFA.");
    Lvlfa aut;
    assert(nullptr != alphabet);

    if (!inter_aut.is_lvlfa()) {
        throw std::runtime_error(std::string(__FUNCTION__) + ": expecting type \"" +
                                 TYPE_NFA + "\"");
    }

    NameStateMap tmp_state_map;
    if (nullptr == state_map) {
        state_map = &tmp_state_map;
    }

    // a lambda for translating state names to identifiers
    auto get_state_name = [&state_map, &aut](const std::string& str) {
        if (!state_map->count(str)) {
            State state = aut.add_state();
            state_map->insert({str, state});
            return state;
        } else {
            return (*state_map)[str];
        }
    };

    for (const auto& str : inter_aut.initial_formula.collect_node_names())
    {
        State state = get_state_name(str);
        aut.initial.insert(state);
    }

    for (const auto& trans : inter_aut.transitions)
    {
        if (trans.second.children.size() != 2)
        {
            if (trans.second.children.size() == 1)
            {
                throw std::runtime_error("Epsilon transitions not supported");
            }
            else
            {
                throw std::runtime_error("Invalid transition");
            }
        }

        State src_state = get_state_name(trans.first.name);
        Symbol symbol = alphabet->translate_symb(trans.second.children[0].node.name);
        State tgt_state = get_state_name(trans.second.children[1].node.name);

        aut.delta.add(src_state, symbol, tgt_state);
    }

    std::unordered_set<std::string> final_formula_nodes;
    if (!(inter_aut.final_formula.node.is_constant())) {
        // we do not want to parse true/false (constant) as a state so we do not collect it
        final_formula_nodes = inter_aut.final_formula.collect_node_names();
    }
    // for constant true, we will pretend that final nodes are negated with empty final_formula_nodes
    bool final_nodes_are_negated = (inter_aut.final_formula.node.is_true() || inter_aut.are_final_states_conjunction_of_negation());

    if (final_nodes_are_negated) {
        // we add all states NOT in final_formula_nodes to final states
        for (const auto &state_name_and_id : *state_map) {
            if (!final_formula_nodes.count(state_name_and_id.first)) {
                aut.final.insert(state_name_and_id.second);
            }
        }
    } else {
        // we add all states in final_formula_nodes to final states
        for (const auto& str : final_formula_nodes)
        {
            State state = get_state_name(str);
            aut.final.insert(state);
        }
    }

    return aut;
} // construct().

void builder::construct(
        mata::lvlfa::Lvlfa *result,
        const mata::IntermediateAut &inter_aut,
        mata::Alphabet *alphabet,
        mata::lvlfa::builder::NameStateMap *state_map
) {
    *result = construct(inter_aut, alphabet, state_map);
}

Lvlfa builder::create_single_word_lvlfa(const std::vector<Symbol>& word) {
    const size_t word_size{ word.size() };
    Lvlfa lvlfa{ word_size + 1, { 0 }, { word_size } };

    for (State state{ 0 }; state < word_size; ++state) {
        lvlfa.delta.add(state, word[state], state + 1);
    }
    return lvlfa;
}

Lvlfa builder::create_single_word_lvlfa(const std::vector<std::string>& word, mata::Alphabet *alphabet) {
    if (!alphabet) {
        alphabet = new OnTheFlyAlphabet{ word };
    }
    const size_t word_size{ word.size() };
    Lvlfa lvlfa{ word_size + 1, { 0 }, { word_size }, std::vector<Level>(word_size + 1, 0), 0, alphabet };

    for (State state{ 0 }; state < word_size; ++state) {
        lvlfa.delta.add(state, alphabet->translate_symb(word[state]), state + 1);
    }
    return lvlfa;
}

Lvlfa builder::create_empty_string_lvlfa() {
    return Lvlfa{ 1, StateSet{ 0 }, StateSet{ 0 } };
}

Lvlfa builder::create_sigma_star_lvlfa(mata::Alphabet* alphabet) {
    Lvlfa lvlfa{ 1, StateSet{ 0 }, StateSet{ 0 }, { 0 }, 0, alphabet };
    for (const mata::Symbol& symbol : alphabet->get_alphabet_symbols()) {
        lvlfa.delta.add(0, symbol, 0);
    }
    return lvlfa;
}

Lvlfa builder::parse_from_mata(std::istream& lvlfa_stream) {
    const std::string lvlfa_str = "LVLFA";
    parser::Parsed parsed{ parser::parse_mf(lvlfa_stream) };
    if (parsed.size() != 1) {
        throw std::runtime_error("The number of sections in the input file is '" + std::to_string(parsed.size())
            + "'. Required is '1'.\n");
    }
    const std::string automaton_type{ parsed[0].type };
    if (automaton_type.compare(0, lvlfa_str.length(), lvlfa_str) != 0) {
        throw std::runtime_error("The type of input automaton is '" + automaton_type + "'. Required is 'LVLFA'\n");
    }
    IntAlphabet alphabet;
    return construct(IntermediateAut::parse_from_mf(parsed)[0], &alphabet);
    // return construct(parsed, &alphabet);
}

Lvlfa builder::parse_from_mata(const std::filesystem::path& lvlfa_file) {
    std::ifstream file_stream{ lvlfa_file };
    if (!file_stream) {
        throw std::runtime_error("Could not open file \'" + lvlfa_file.string() + "'\n");
    }

    Lvlfa lvlfa;
    try {
        lvlfa = parse_from_mata(file_stream);
    } catch (const std::exception& ex) {
        file_stream.close();
        throw;
    }
    return lvlfa;
}

Lvlfa builder::parse_from_mata(const std::string& lvlfa_in_mata) {
    std::istringstream lvlfa_stream(lvlfa_in_mata);
    return parse_from_mata(lvlfa_stream);
}
