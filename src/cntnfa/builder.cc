// TODO: Insert header file.

#include "mata/cntnfa/builder.hh"
#include "mata/cntnfa/annotations.hh"
#include "mata/cntnfa/types.hh"

#include <fstream>

using namespace mata::cntnfa;
using mata::cntnfa::Cntnfa;
using mata::Symbol;

Cntnfa builder::construct_counter_nfa(const mata::parser::ParsedSection& parsec, Alphabet* alphabet, NameStateMap* state_map)
{
    Cntnfa aut;
    assert(alphabet != nullptr);

    // A lambda for translating state names to identifiers
    // TODO: Currently supports only explicit CNTNFA
    if (parsec.type != "CNTNFA-explicit") {
        throw std::runtime_error(std::string(__FUNCTION__) + ": expecting type \"CNTNFA-explicit\"");
    }

    bool remove_state_map = false;
    if (state_map == nullptr) {
        state_map = new NameStateMap();
        remove_state_map = true;
    }

    // A lambda for translating state names to identifiers
    auto get_state = [&aut, &state_map](const std::string& name) -> State {
        auto it = state_map->find(name);
        if (it == state_map->end()) {
            State s;
            if (name.size() >= 2 && name[0] == 'q' && std::isdigit(name[1])) {
                s = aut.add_state(static_cast<State>(std::stoi(name.substr(1))));
            } else {
                s = aut.add_state();
            }
            (*state_map)[name] = s;
            return s;
        }
        return it->second;
    };

    // A lambda for cleanup
    auto clean_up = [&]() {
        if (remove_state_map) delete state_map;
    };

    // Parse the initial states
    if (parsec.haskey("Initial")) {
        for (const auto& name : parsec["Initial"])
            aut.initial.insert(get_state(name));
    }

    // Parse the final states
    if (parsec.haskey("Final")) {
        for (const auto& name : parsec["Final"])
            aut.final.insert(get_state(name));
    }

    // Parse the registers
    if (parsec.haskey("Registers")) {
        for (const auto& name : parsec["Registers"]) {
            aut.counter_set.insert(name);
        }
    }

    // Parse the transitions
    for (const auto& line : parsec.body) {
        if (line.size() < 3) {
            clean_up();
            throw std::runtime_error("Invalid transition: too few tokens");
        }

        State source = get_state(line[0]);
        Symbol symbol = alphabet->translate_symb(line[1]);
        State target = get_state(line.back());

        // Allocate annotation group
        // size_t annotations_id = aut.annotation_collection.size();
        // aut.annotation_collection.allocate(annotation_id + 1);

        // Default: no annotations
        size_t annotations_id = UNDEFINED_ANNOTATIONS;

        // Parse all annotation groups between symbol and target
        for (size_t i = 2; i + 1 < line.size(); ++i) {
            if (line[i] == "(") {
                std::vector<std::string> group;
                ++i;
                while (i < line.size() && line[i] != ")") {
                    group.push_back(line[i]);
                    ++i;
                }

                if (group.size() != 3) {
                    clean_up();
                    throw std::runtime_error("Annotation must have 3 parts: (type name value)");
                }

                const std::string& type = group[0];
                const std::string& name = group[1];
                int value = std::stoi(group[2]);

                if (!aut.counter_set.has(name)) {
                    clean_up();
                    throw std::runtime_error("Unknown counter in annotation: " + name);
                }

                if (annotations_id == UNDEFINED_ANNOTATIONS) {
                    annotations_id = aut.create_annotation_set();
                }

                size_t counter_id = aut.counter_set.get_index(name);

                if (type == ":=") {
                    aut.annotation_collection.insert(
                        std::make_shared<CounterAssign>(counter_id, value), annotations_id);
                } else if (type == "+") {
                    aut.annotation_collection.insert(
                        std::make_shared<CounterIncrement>(counter_id, value), annotations_id);
                } else if (type == "=") {
                    aut.annotation_collection.insert(
                        std::make_shared<CounterEqual>(counter_id, value), annotations_id);
                } else if (type == "!=") {
                    aut.annotation_collection.insert(
                        std::make_shared<CounterNotEqual>(counter_id, value), annotations_id);
                } else if (type == ">") {
                    aut.annotation_collection.insert(
                        std::make_shared<CounterGreater>(counter_id, value), annotations_id);
                } else if (type == "<") {
                    aut.annotation_collection.insert(
                        std::make_shared<CounterLess>(counter_id, value), annotations_id);
                } else if (type == ">=") {
                    aut.annotation_collection.insert(
                        std::make_shared<CounterGreaterEqual>(counter_id, value), annotations_id);
                } else if (type == "<=") {
                    aut.annotation_collection.insert(
                        std::make_shared<CounterLessEqual>(counter_id, value), annotations_id);
                } else {
                    clean_up();
                    throw std::runtime_error("Unsupported annotation type: " + type);
                }
            }
        }

        // Create transition with annotation ID
        aut.delta.add(source, symbol, AnnotationState{target, annotations_id});
    }

    clean_up();
    return aut;
} // construct_counter_nfa().

Cntnfa builder::construct(const mata::parser::ParsedSection& parsec, mata::Alphabet* alphabet, NameStateMap* state_map) {
    Cntnfa aut;
    assert(nullptr != alphabet);

    if (parsec.type != TYPE_CNTNFA) {
        throw std::runtime_error(std::string(__FUNCTION__) + ": expecting type \"" +
                                 TYPE_CNTNFA + "\"");
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
        Target tgt_state = get_state_name(body_line[2]);

        aut.delta.add(src_state, symbol, tgt_state);
    }

    // do the dishes and take out garbage
    clean_up();

    return aut;
} // construct().

Cntnfa builder::construct(const mata::IntermediateAut& inter_aut, mata::Alphabet* alphabet, NameStateMap* state_map) {
    Cntnfa aut;
    assert(nullptr != alphabet);

    if (!inter_aut.is_nfa()) {
        throw std::runtime_error(std::string(__FUNCTION__) + ": expecting type \"" +
                                 TYPE_CNTNFA + "\"");
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
        Target tgt_state = get_state_name(trans.second.children[1].node.name);

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
        mata::cntnfa::Cntnfa *result,
        const mata::IntermediateAut &inter_aut,
        mata::Alphabet *alphabet,
        mata::cntnfa::builder::NameStateMap *state_map
) {
    *result = construct(inter_aut, alphabet, state_map);
}

Cntnfa builder::create_single_word_nfa(const std::vector<Symbol>& word) {
    const size_t word_size{ word.size() };
    Cntnfa nfa{ word_size + 1, { 0 }, { word_size } };

    for (State state{ 0 }; state < word_size; ++state) {
        nfa.delta.add(state, word[state], Target(state + 1));
    }
    return nfa;
}

Cntnfa builder::create_single_word_nfa(const std::vector<std::string>& word, mata::Alphabet *alphabet) {
    if (!alphabet) {
        alphabet = new OnTheFlyAlphabet{ word };
    }
    const size_t word_size{ word.size() };
    Cntnfa nfa{ word_size + 1, { 0 }, { word_size }, {}, {}, alphabet };

    for (State state{ 0 }; state < word_size; ++state) {
        nfa.delta.add(state, alphabet->translate_symb(word[state]), Target(state + 1));
    }
    return nfa;
}

Cntnfa builder::create_empty_string_nfa() {
    return Cntnfa{ 1, StateSet{ 0 }, StateSet{ 0 } };
}

Cntnfa builder::create_sigma_star_nfa(mata::Alphabet* alphabet) {
    Cntnfa nfa{ 1, StateSet{ 0 }, StateSet{ 0 }, {}, {}, alphabet };
    for (const mata::Symbol& symbol : alphabet->get_alphabet_symbols()) {
        nfa.delta.add(0, symbol, Target(0));
    }
    return nfa;
}

Cntnfa builder::parse_from_mata(std::istream& nfa_stream) {
    const std::string nfa_str = "NFA";
    parser::Parsed parsed{ parser::parse_mf(nfa_stream) };
    if (parsed.size() != 1) {
        throw std::runtime_error("The number of sections in the input file is '" + std::to_string(parsed.size())
            + "'. Required is '1'.\n");
    }
    const std::string automaton_type{ parsed[0].type };
    if (automaton_type.compare(0, nfa_str.length(), nfa_str) != 0) {
        throw std::runtime_error("The type of input automaton is '" + automaton_type + "'. Required is 'NFA'\n");
    }
    IntAlphabet alphabet;
    return construct(IntermediateAut::parse_from_mf(parsed)[0], &alphabet);
}

Cntnfa builder::parse_from_mata(const std::filesystem::path& nfa_file) {
    std::ifstream file_stream{ nfa_file };
    if (!file_stream) {
        throw std::runtime_error("Could not open file \'" + nfa_file.string() + "'\n");
    }

    Cntnfa nfa;
    try {
        nfa = parse_from_mata(file_stream);
    } catch (const std::exception& ex) {
        file_stream.close();
        throw;
    }
    return nfa;
}

Cntnfa builder::parse_from_mata(const std::string& nfa_in_mata) {
    std::istringstream nfa_stream(nfa_in_mata);
    return parse_from_mata(nfa_stream);
}
