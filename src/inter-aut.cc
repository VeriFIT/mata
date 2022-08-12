//
// Created by Martin Hruska on 10.08.2022.
//

#include <mata/inter-aut.hh>

namespace
{
    Mata::InterAutomaton::Naming get_naming_type(const std::string &key)
    {
        assert(key.find("-") != std::string::npos);
        const std::string& type = key.substr(key.find("-")+1, std::string::npos);

        if (type == "auto")
            return Mata::InterAutomaton::Naming::AUTO;
        else if (type == "enum")
            return Mata::InterAutomaton::Naming::ENUM;
        else if (type == "marked")
            return Mata::InterAutomaton::Naming::MARKER;

        assert(false);
    }

    Mata::InterAutomaton::AlphabetType get_alphabet_type(const std::string &type)
    {
        assert(type.find("-") != std::string::npos);
        const std::string& alph_type = type.substr(type.find("-")+1, std::string::npos);

        if (alph_type == "bits") {
            return Mata::InterAutomaton::AlphabetType::BITVECTOR;
        } else if (alph_type == "explicit") {
            return Mata::InterAutomaton::AlphabetType::EXPLICIT;
        } else if (alph_type == "intervals") {
            return Mata::InterAutomaton::AlphabetType::INTERVALS;
        }
    }

    Mata::FormulaGraph parse_transition(Mata::InterAutomaton& aut, const std::vector<std::string> &tokens)
    {
    }

    Mata::InterAutomaton mf_to_aut(const Mata::Parser::ParsedSection &section)
    {
        Mata::InterAutomaton aut;

        if (section.type.find("NFA")) {
            aut.automaton_type = Mata::InterAutomaton::AutomatonType::NFA;
        } else if (section.type.find("AFA")) {
            aut.automaton_type = Mata::InterAutomaton::AutomatonType::AFA;
        }
        aut.alphabet_type = get_alphabet_type(section.type);

        for (const std::pair<std::string, std::vector<std::string>>& keypair : section.dict) {
            const std::string& key = keypair.first;
            if (key.find("Alphabet") != std::string::npos) {
                aut.symbol_naming = get_naming_type(key);
                if (aut.symbols_enumerated())
                    aut.symbols_names.insert(
                            aut.symbols_names.end(), keypair.second.begin(), keypair.second.end());
            } else if (key.find("States") != std::string::npos) {
                aut.state_naming = get_naming_type(key);
                if (aut.states_enumerated())
                    aut.states_names.insert(
                            aut.states_names.end(), keypair.second.begin(), keypair.second.end());
            } else if (key.find("Nodes") != std::string::npos) {
                aut.node_naming = get_naming_type(key);
                if (aut.nodes_enumerated())
                    aut.nodes_names.insert(
                            aut.nodes_names.end(), keypair.second.begin(), keypair.second.end());
            }
            // TODO: parse initial formula
            // TODO: parse final formula
        }

        for (const auto& trans : section.body) {
            aut.transitions.push_back(parse_transition(aut,trans));
        }

        return aut;
    }
} // anonymous

std::vector<Mata::InterAutomaton> Mata::InterAutomaton::parse_from_mf(const Mata::Parser::Parsed &parsed)
{
    std::vector<Mata::InterAutomaton> result;

    for (const auto& parsed_section : parsed) {
        result.push_back(mf_to_aut(parsed_section));
    }

    return result;
}