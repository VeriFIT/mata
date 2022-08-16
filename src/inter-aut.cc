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
        } else if (token == "(") {
            return Mata::FormulaNode{Mata::FormulaNode::Type::LEFT_PARENTHESIS, token};
        } else if (token == ")") {
            return Mata::FormulaNode{Mata::FormulaNode::Type::RIGHT_PARENTHESIS, token};
        }

        assert(false);
    }

    bool lower_precedens(Mata::FormulaNode::OperatorType op1, Mata::FormulaNode::OperatorType op2) {
        if (op1 == Mata::FormulaNode::NEG) {
            return false;
        } else if (op1 == Mata::FormulaNode::AND && op2 != Mata::FormulaNode::NEG) {
            return false;
        }

        return true;
    }

    std::vector<Mata::FormulaNode> infix2postfix(Mata::InterAutomaton &aut, const std::vector<std::string> &tokens) {
        std::vector<Mata::FormulaNode> opstack;
        std::vector<Mata::FormulaNode> output;

        // starting from 1 - skipping lhs of transition
        for (size_t i = 1; i < tokens.size(); ++i) {
            Mata::FormulaNode node = create_node(aut, tokens[i]);
            switch (node.type) {
                case Mata::FormulaNode::OPERAND:
                    output.push_back(node);
                    break;
                case Mata::FormulaNode::LEFT_PARENTHESIS:
                    opstack.push_back(node);
                    break;
                case Mata::FormulaNode::RIGHT_PARENTHESIS:
                    while (!opstack.back().is_leftpar()) {
                        output.push_back(opstack.back());
                        output.pop_back();
                    }
                    output.pop_back();
                    break;
                case Mata::FormulaNode::OPERATOR:
                    for (int j = opstack.size()-1; j >= 0; --j) {
                        assert(!opstack[j].is_operand());
                        if (lower_precedens(node.operator_type, opstack[j].operator_type)) {
                            output.push_back(opstack[j]);
                            opstack.erase(opstack.begin()+j);
                        }
                    }
                    opstack.push_back(node);
                    break;
                default: assert(false);
            }
        }

        while (!opstack.empty()) {
            output.push_back(opstack.back());
            opstack.pop_back();
        }

        return output;
    }

    Mata::FormulaGraph postfix2graph(const std::vector<Mata::FormulaNode> &postfix)
    {
        std::vector<Mata::FormulaGraph> opstack;

        for (const auto& node : postfix) {
            Mata::FormulaGraph gr(node);
            switch (node.type) {
                case Mata::FormulaNode::OPERAND:
                    opstack.push_back(gr);
                    break;
                case Mata::FormulaNode::OPERATOR:
                    switch (node.operator_type) {
                        case Mata::FormulaNode::NEG:
                            gr.children.push_back(opstack.back());
                            opstack.pop_back();
                            opstack.push_back(gr);
                            break;
                        default:
                            gr.children.push_back(opstack.back());
                            opstack.pop_back();
                            gr.children.insert(gr.children.begin(), opstack.back());
                            opstack.pop_back();
                            opstack.push_back(gr);
                    }
                    break;
                default: assert(false);
            }
        }

        assert(opstack.size() == 1);

        return opstack.back();
    }

    void parse_transition(Mata::InterAutomaton &aut, const std::vector<std::string> &tokens)
    {
        assert(tokens.size() > 1); // transition formula has at least two items
        const Mata::FormulaNode lhs = create_node(aut, tokens[0]);
        const std::vector<Mata::FormulaNode> postfix = infix2postfix(aut, tokens);
        const Mata::FormulaGraph graph = postfix2graph(postfix);

        aut.transitions.push_back(std::pair<Mata::FormulaNode,Mata::FormulaGraph>(lhs, graph));
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

        for (const auto& keypair : section.dict) {
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