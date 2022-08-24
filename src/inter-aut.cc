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
                case '!':
                    return Mata::FormulaNode{Mata::FormulaNode::Type::OPERATOR, token, token,
                                             Mata::FormulaNode::OperatorType::NEG};
                default:
                    assert(false);
            }
        } else if (token == "(") {
            return Mata::FormulaNode{Mata::FormulaNode::Type::LEFT_PARENTHESIS, token};
        } else if (token == ")") {
            return Mata::FormulaNode{Mata::FormulaNode::Type::RIGHT_PARENTHESIS, token};
        } else if (is_naming_enum(mata.state_naming) && contains(mata.states_names, token)) {
            return Mata::FormulaNode{Mata::FormulaNode::Type::OPERAND, token, token,
                                     Mata::FormulaNode::OperandType::STATE};
        } else if (is_naming_enum(mata.node_naming) && contains(mata.nodes_names, token)) {
            return Mata::FormulaNode{Mata::FormulaNode::Type::OPERAND, token, token,
                                     Mata::FormulaNode::OperandType::NODE};
        } else if (is_naming_enum(mata.symbol_naming) && contains(mata.symbols_names, token)) {
            return Mata::FormulaNode{Mata::FormulaNode::Type::OPERAND, token, token,
                                     Mata::FormulaNode::OperandType::SYM};
        } else if (is_naming_marker(mata.state_naming) && token[0] == 'q') {
            return Mata::FormulaNode{Mata::FormulaNode::Type::OPERAND, token, token.substr(1),
                                     Mata::FormulaNode::OperandType::STATE};
        } else if (is_naming_marker(mata.node_naming) && token[0] == 'n') {
            return Mata::FormulaNode{Mata::FormulaNode::Type::OPERAND, token, token.substr(1),
                                     Mata::FormulaNode::OperandType::NODE};
        } else if (is_naming_marker(mata.symbol_naming) && token[0] == 'a') {
            return Mata::FormulaNode{Mata::FormulaNode::Type::OPERAND, token, token.substr(1),
                                     Mata::FormulaNode::OperandType::SYM};
        } else if (is_naming_auto(mata.state_naming)) {
            return Mata::FormulaNode{Mata::FormulaNode::Type::OPERAND, token, token,
                                     Mata::FormulaNode::OperandType::STATE};
        } else if (is_naming_auto(mata.node_naming)) {
            return Mata::FormulaNode{Mata::FormulaNode::Type::OPERAND, token, token,
                                     Mata::FormulaNode::OperandType::NODE};
        } else if (is_naming_auto(mata.symbol_naming)) {
            return Mata::FormulaNode{Mata::FormulaNode::Type::OPERAND, token, token,
                                     Mata::FormulaNode::OperandType::SYM};
        }

        throw std::runtime_error("Unknown token " + token);
        assert(false);
    }

    bool lower_precedence(Mata::FormulaNode::OperatorType op1, Mata::FormulaNode::OperatorType op2) {
        if (op1 == Mata::FormulaNode::NEG) {
            return false;
        } else if (op1 == Mata::FormulaNode::AND && op2 != Mata::FormulaNode::NEG) {
            return false;
        }

        return true;
    }

    /**
     * Transforms a series of tokes to postfix notation.
     * @param aut Automaton for which transition formula is parsed
     * @param tokens Series of tokens representing transition formula parsed from the input text
     * @return A postfix notation for input
     */
    std::vector<Mata::FormulaNode> infix2postfix(Mata::InterAutomaton &aut, const std::vector<std::string> &tokens) {
        std::vector<Mata::FormulaNode> opstack;
        std::vector<Mata::FormulaNode> output;

        for (const auto& token : tokens) {
            Mata::FormulaNode node = create_node(aut, token);
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
                        opstack.pop_back();
                        break;
                    }
                    opstack.pop_back();
                    break;
                case Mata::FormulaNode::OPERATOR:
                    for (int j = opstack.size()-1; j >= 0; --j) {
                        assert(!opstack[j].is_operand());
                        if (opstack[j].is_leftpar())
                            break;
                        if (lower_precedence(node.operator_type, opstack[j].operator_type)) {
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

    /**
     * Create a graph for a postfix representation of transition formula
     * @param postfix A postfix representation of transition formula
     * @return A parsed graph
     */
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
                            assert(!opstack.empty());
                            gr.children.push_back(opstack.back());
                            opstack.pop_back();
                            opstack.push_back(gr);
                            break;
                        default:
                            assert(opstack.size() > 1);
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

    std::vector<Mata::FormulaNode> add_operators_implicitly(const Mata::InterAutomaton& aut,
                                                            const std::vector<Mata::FormulaNode>& postfix)
    {
        assert(aut.is_nfa());
        if (postfix.size() == 1) // no need to add operators
            return postfix;

        for (const auto& op : postfix) {
            if (op.is_operator()) // operators provided by user, return the original postfix
                return postfix;
        }

        std::vector<Mata::FormulaNode> res;
        if (postfix.size() >= 2) {
            res.push_back(postfix[0]);
            res.push_back(postfix[1]);
            res.push_back(Mata::FormulaNode(
                    Mata::FormulaNode::OPERATOR, "|", "|", Mata::FormulaNode::OR));
        }

        for (size_t i = 2; i < postfix.size(); ++i) {
            res.push_back(postfix[i]);
            res.push_back(Mata::FormulaNode(
                    Mata::FormulaNode::OPERATOR, "|", "|", Mata::FormulaNode::OR));
        }

        return res;
    }

    /**
     * Parses a transition by firstly transforming transition formulat to postfix form and then creating
     * a tree representing the formulat from postfix.
     * @param aut Automaton to which transition will be added.
     * @param tokens Series of tokens representing transition formula
     */
    void parse_transition(Mata::InterAutomaton &aut, const std::vector<std::string> &tokens)
    {
        assert(tokens.size() > 1); // transition formula has at least two items
        const Mata::FormulaNode lhs = create_node(aut, tokens[0]);
        const std::vector<std::string> rhs(tokens.begin()+1, tokens.end());

        std::vector<Mata::FormulaNode> postfix = infix2postfix(aut, rhs);
        // add implicit conjunction to NFA explicit states, i.e. p a q -> p a & q
        if (aut.automaton_type == Mata::InterAutomaton::NFA && aut.alphabet_type == Mata::InterAutomaton::EXPLICIT
            && postfix.size() == 2) {
            postfix.push_back(Mata::FormulaNode(
                    Mata::FormulaNode::OPERATOR, "&", "&", Mata::FormulaNode::AND));
        }

        const Mata::FormulaGraph graph = postfix2graph(postfix);

        aut.transitions.push_back(std::pair<Mata::FormulaNode,Mata::FormulaGraph>(lhs, graph));
    }

    /**
     * The wrapping function for parsing one section of input to InterAutomaton.
     * It parses basic information about type of automaton and naming of its component.
     * Then it parses initial and final formula and finally it creates graphs for transition formula.
     * @param section A section of input MATA format
     * @return Parsed InterAutomata representing an automaton from input.
     */
    Mata::InterAutomaton mf_to_aut(const Mata::Parser::ParsedSection &section)
    {
        Mata::InterAutomaton aut;

        if (section.type.find("NFA") != std::string::npos) {
            aut.automaton_type = Mata::InterAutomaton::AutomatonType::NFA;
        } else if (section.type.find("AFA") != std::string::npos) {
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
        }

        // Once we know what states and nodes are we can parser initial and final formula
        for (const auto& keypair : section.dict) {
            const std::string &key = keypair.first;

            if (key.find("Initial") != std::string::npos) {
                auto postfix = infix2postfix(aut, keypair.second);
                if (aut.is_nfa() && aut.alphabet_type == Mata::InterAutomaton::EXPLICIT)
                    postfix = add_operators_implicitly(aut, postfix);
                aut.initial_formula = postfix2graph(postfix);
            } else if (key.find("Final") != std::string::npos) {
                auto postfix = infix2postfix(aut, keypair.second);
                if (aut.is_nfa() && aut.alphabet_type == Mata::InterAutomaton::EXPLICIT)
                    postfix = add_operators_implicitly(aut, postfix);
                aut.final_formula = postfix2graph(postfix);
            }
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

std::unordered_set<std::string> Mata::FormulaGraph::collect_node_names() const
{
    std::unordered_set<std::string> res;
    std::vector<const Mata::FormulaGraph *> stack;

    stack.push_back(reinterpret_cast<const FormulaGraph *const>(&(this->node)));
    while (!stack.empty()) {
        const FormulaGraph* g = stack.back();
        assert(g != nullptr);
        stack.pop_back();

        if (g->node.type == FormulaNode::UNKNOWN)
           continue; // skip undefined nodes

        if (g->node.is_operand())
            res.insert(g->node.name);

        for (const auto& child : g->children) {
            stack.push_back(&child);
        }
    }

    return res;
}

std::vector<Mata::InterAutomaton> Mata::InterAutomaton::parse_from_mf(const Mata::Parser::Parsed &parsed)
{
    std::vector<Mata::InterAutomaton> result;
    result.reserve(parsed.size());

    for (const auto& parsed_section : parsed) {
        result.push_back(mf_to_aut(parsed_section));
    }

    return result;
}

std::ostream& std::operator<<(std::ostream& os, const Mata::InterAutomaton& inter_aut)
{
    os << "Intermediate automaton type " << inter_aut.automaton_type << '\n';
    os << "Naming - state: " << inter_aut.state_naming << " symbol: " << inter_aut.symbol_naming << " node: "
        << inter_aut.node_naming << '\n';
    os << "Alphabet " << inter_aut.alphabet_type << '\n';

    os << "Initial states: ";
    for (const auto& state : inter_aut.initial_formula.collect_node_names()) {
        os << state << ' ';
    }
    os << '\n';

    os << "Final states: ";
    for (const auto& state : inter_aut.final_formula.collect_node_names()) {
        os << state << ' ';
    }
    os << '\n';

    os << "Transitions: \n";
    for (const auto& trans : inter_aut.transitions) {
        os << trans.first.name << " -> ";
        for (const auto& rhs : trans.second.collect_node_names()) {
            os << rhs << ' ';
        }
        os << '\n';
    }

    return os;
}
