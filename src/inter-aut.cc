//
// Created by Martin Hruska on 10.08.2022.
//

#include <mata/inter-aut.hh>
#include <mata/util.hh>

namespace
{
    bool is_logical_operator(char ch)
    {
        return (Mata::util::haskey(std::set<char>{'&', '|', '!'}, ch));
    }

    Mata::InterAutomaton::Naming get_naming_type(const std::string &key)
    {
        assert(key.find('-') != std::string::npos);
        const std::string& type = key.substr(key.find('-')+1, std::string::npos);

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
        assert(type.find('-') != std::string::npos);
        const std::string& alph_type = type.substr(type.find('-')+1, std::string::npos);

        if (alph_type == "bits") {
            return Mata::InterAutomaton::AlphabetType::BITVECTOR;
        } else if (alph_type == "explicit") {
            return Mata::InterAutomaton::AlphabetType::EXPLICIT;
        } else if (alph_type == "intervals") {
            return Mata::InterAutomaton::AlphabetType::INTERVALS;
        }

        assert(false);
    }

    bool is_naming_marker(const Mata::InterAutomaton::Naming naming)
    {
        return naming == Mata::InterAutomaton::Naming::MARKER;
    }

    bool is_naming_enum(const Mata::InterAutomaton::Naming naming)
    {
        return naming == Mata::InterAutomaton::Naming::ENUM;
    }

    bool is_naming_auto(const Mata::InterAutomaton::Naming naming)
    {
        return naming == Mata::InterAutomaton::Naming::AUTO;
    }

    bool contains(const std::vector<std::string> &vec, const std::string &item)
    {
        return std::find(vec.begin(), vec.end(), item) != vec.end();
    }

    Mata::FormulaNode create_node(const Mata::InterAutomaton &mata, const std::string &token)
    {
        if (is_logical_operator(token[0])) {
            switch (token[0]) {
                case '&':
                    return Mata::FormulaNode{Mata::FormulaNode::Type::OPERATOR, token, token,
                                         Mata::FormulaNode::OperatorType::AND};
                case '|':
                    return Mata::FormulaNode{Mata::FormulaNode::Type::OPERATOR, token, token,
                                             Mata::FormulaNode::OperatorType::OR};
                case '^':
                    return Mata::FormulaNode{Mata::FormulaNode::Type::OPERATOR, token, token,
                                             Mata::FormulaNode::OperatorType::NEG};
                default:
                    assert(false);
            }
        } else if (is_naming_enum(mata.state_naming) && contains(mata.states_names, token)) {
            return Mata::FormulaNode{Mata::FormulaNode::Type::OPERAND, token, token,
                                     Mata::FormulaNode::OperandType::STATE};
        } else if (is_naming_enum(mata.node_naming) && contains(mata.nodes_names, token)) {
            return Mata::FormulaNode{Mata::FormulaNode::Type::OPERAND, token, token,
                                     Mata::FormulaNode::OperandType::NODE};
        } else if (is_naming_enum(mata.symbol_naming) && contains(mata.nodes_names, token)) {
            return Mata::FormulaNode{Mata::FormulaNode::Type::OPERAND, token, token,
                                     Mata::FormulaNode::OperandType::SYM};
        } else if (is_naming_marker(mata.state_naming) && token[0] == 'q') {
            return Mata::FormulaNode{Mata::FormulaNode::Type::OPERAND, token, token.substr(1),
                                     Mata::FormulaNode::OperandType::STATE};
        } else if (is_naming_marker(mata.node_naming) && token[0] == 'n') {
            return Mata::FormulaNode{Mata::FormulaNode::Type::OPERAND, token, token.substr(1),
                                     Mata::FormulaNode::OperandType::NODE};
        } else if (is_naming_enum(mata.symbol_naming) && token[0] == 'a') {
            return Mata::FormulaNode{Mata::FormulaNode::Type::OPERAND, token, token.substr(1),
                                     Mata::FormulaNode::OperandType::SYM};
        } else if (is_naming_auto(mata.state_naming)) {
            return Mata::FormulaNode{Mata::FormulaNode::Type::OPERAND, token, token,
                                     Mata::FormulaNode::OperandType::STATE};
        } else if (is_naming_auto(mata.node_naming)) {
            return Mata::FormulaNode{Mata::FormulaNode::Type::OPERAND, token, token,
                                     Mata::FormulaNode::OperandType::NODE};
        }

        assert(false);
    }

    void parse_transition(Mata::InterAutomaton &aut, const std::vector<std::string> &tokens)
    {
        assert(!tokens.empty());
        const Mata::FormulaNode lhs = create_node(aut, tokens[0]);
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

        if (!(!(!(aut.node_naming == Mata::InterAutomaton::Naming::AUTO &&
            aut.symbol_naming == Mata::InterAutomaton::Naming::AUTO) &&
            (aut.state_naming == Mata::InterAutomaton::AUTO)))) {
            throw std::runtime_error("Only one of nodes, symbols and states could be specified automatically");
        }

        for (const auto& trans : section.body) {
            parse_transition(aut,trans);
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