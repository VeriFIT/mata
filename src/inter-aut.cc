/*
 * inter-aut.hh -- intermediate representation of automata.
 * It represents automaton after parsing and before translation to particular automaton.
 *
 * Copyright (c) 2022 Martin Hruska <hruskamartin25@gmail.com>
 *
 * This file is a part of libmata.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <mata/inter-aut.hh>
#include <mata/util.hh>

namespace
{
    bool has_atmost_one_auto_naming(const Mata::IntermediateAut& aut)
    {
        return !(!(aut.node_naming == Mata::IntermediateAut::Naming::AUTO &&
              aut.symbol_naming == Mata::IntermediateAut::Naming::AUTO) &&
                 (aut.state_naming == Mata::IntermediateAut::AUTO));
    }

    bool is_logical_operator(char ch)
    {
        return (Mata::Util::haskey(std::set<char>{'&', '|', '!'}, ch));
    }

    Mata::IntermediateAut::Naming get_naming_type(const std::string &key)
    {
        const size_t found = key.find('-');
        assert(found != std::string::npos);
        const std::string& type = key.substr(found+1, std::string::npos);

        if (type == "auto")
            return Mata::IntermediateAut::Naming::AUTO;
        else if (type == "enum")
            return Mata::IntermediateAut::Naming::ENUM;
        else if (type == "marked")
            return Mata::IntermediateAut::Naming::MARKED;
        else if (type == "chars")
            return Mata::IntermediateAut::Naming::CHARS;
        else if (type == "utf")
            return Mata::IntermediateAut::Naming::UTF;

        assert(false && "Unknown naming type - a naming type should be always defined correctly otherwise it is"
                        "impossible to parse automaton correctly");
    }

    Mata::IntermediateAut::AlphabetType get_alphabet_type(const std::string &type)
    {
        assert(type.find('-') != std::string::npos);
        const std::string& alph_type = type.substr(type.find('-')+1, std::string::npos);

        if (alph_type == "bits") {
            return Mata::IntermediateAut::AlphabetType::BITVECTOR;
        } else if (alph_type == "explicit") {
            return Mata::IntermediateAut::AlphabetType::EXPLICIT;
        } else if (alph_type == "intervals") {
            return Mata::IntermediateAut::AlphabetType::INTERVALS;
        }

        assert(false && "Unknown alphabet type - an alphabet type should be always defined correctly otherwise it is"
                        "impossible to parse automaton correctly");
    }

    bool is_naming_marker(const Mata::IntermediateAut::Naming naming)
    {
        return naming == Mata::IntermediateAut::Naming::MARKED;
    }

    bool is_naming_enum(const Mata::IntermediateAut::Naming naming)
    {
        return naming == Mata::IntermediateAut::Naming::ENUM;
    }

    bool is_naming_auto(const Mata::IntermediateAut::Naming naming)
    {
        return naming == Mata::IntermediateAut::Naming::AUTO;
    }

    bool contains(const std::vector<std::string> &vec, const std::string &item)
    {
        return std::find(vec.begin(), vec.end(), item) != vec.end();
    }

    bool no_operators(const std::vector<Mata::FormulaNode> nodes)
    {
        for (const auto& node : nodes)
            if (node.is_operator())
                return false;

        return true;
    }

    std::string serialize_graph(const Mata::FormulaGraph& graph)
    {
        if (graph.node.is_operand())
            return graph.node.raw;

        if (graph.children.size() == 1) { // unary operator
            const auto& child = graph.children.front();
            std::string child_name = child.node.is_operand() ? child.node.raw :
                    "(" + serialize_graph(child.node) + ")";
            return graph.node.raw + child_name;
        }

        assert(graph.node.is_operator() && graph.children.size() == 2);
        const auto& left_child = graph.children.front();
        std::string lhs = (left_child.node.is_operand()) ? left_child.node.raw :
                serialize_graph(left_child);
        if (left_child.children.size() == 2)
            lhs = "(" + lhs + ")";
        const auto& right_child = graph.children[1];
        std::string rhs = (right_child.node.is_operand()) ? right_child.node.raw :
                serialize_graph(right_child);
        if (right_child.children.size() == 2)
            rhs = "(" + rhs + ")";

        return lhs + " " + graph.node.raw + " " + rhs;
    }

    Mata::FormulaNode create_node(const Mata::IntermediateAut &mata, const std::string &token)
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
        } else if (token == "true") {
            return Mata::FormulaNode{Mata::FormulaNode::Type::OPERAND, token, token,
                                     Mata::FormulaNode::OperandType::SYMBOL};
        } else if (token == "false") {
            return Mata::FormulaNode{Mata::FormulaNode::Type::OPERAND, token, token,
                                     Mata::FormulaNode::OperandType::SYMBOL};
        } else if (is_naming_enum(mata.state_naming) && contains(mata.states_names, token)) {
            return Mata::FormulaNode{Mata::FormulaNode::Type::OPERAND, token, token,
                                     Mata::FormulaNode::OperandType::STATE};
        } else if (is_naming_enum(mata.node_naming) && contains(mata.nodes_names, token)) {
            return Mata::FormulaNode{Mata::FormulaNode::Type::OPERAND, token, token,
                                     Mata::FormulaNode::OperandType::NODE};
        } else if (is_naming_enum(mata.symbol_naming) && contains(mata.symbols_names, token)) {
            return Mata::FormulaNode{Mata::FormulaNode::Type::OPERAND, token, token,
                                     Mata::FormulaNode::OperandType::SYMBOL};
        } else if (is_naming_marker(mata.state_naming) && token[0] == 'q') {
            return Mata::FormulaNode{Mata::FormulaNode::Type::OPERAND, token, token.substr(1),
                                     Mata::FormulaNode::OperandType::STATE};
        } else if (is_naming_marker(mata.node_naming) && token[0] == 'n') {
            return Mata::FormulaNode{Mata::FormulaNode::Type::OPERAND, token, token.substr(1),
                                     Mata::FormulaNode::OperandType::NODE};
        } else if (is_naming_marker(mata.symbol_naming) && token[0] == 'a') {
            return Mata::FormulaNode{Mata::FormulaNode::Type::OPERAND, token, token.substr(1),
                                     Mata::FormulaNode::OperandType::SYMBOL};
        } else if (is_naming_auto(mata.state_naming)) {
            return Mata::FormulaNode{Mata::FormulaNode::Type::OPERAND, token, token,
                                     Mata::FormulaNode::OperandType::STATE};
        } else if (is_naming_auto(mata.node_naming)) {
            return Mata::FormulaNode{Mata::FormulaNode::Type::OPERAND, token, token,
                                     Mata::FormulaNode::OperandType::NODE};
        } else if (is_naming_auto(mata.symbol_naming)) {
            return Mata::FormulaNode{Mata::FormulaNode::Type::OPERAND, token, token,
                                     Mata::FormulaNode::OperandType::SYMBOL};
        }

        throw std::runtime_error("Unknown token " + token);
        assert(false);
    }

    /**
     * Checks if @p op1 has lower precedence than @p op2. Precedence from the lowest to highest is: | < & < !
     */
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
    std::vector<Mata::FormulaNode> infix_to_postfix(
            const Mata::IntermediateAut &aut, const std::vector<std::string> &tokens) {
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

        for (const auto& node : output) {
            assert(node.is_operator() || (node.name != "!" && node.name != "&" && node.name != "|"));
            assert(node.is_leftpar() || node.name != "(");
            assert(node.is_rightpar() || node.name != ")");
        }

        return output;
    }

    /**
     * Create a graph for a postfix representation of transition formula
     * @param postfix A postfix representation of transition formula
     * @return A parsed graph
     */
    Mata::FormulaGraph postfix_to_graph(const std::vector<Mata::FormulaNode> &postfix)
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
                default: assert(false && "Unknown type of node");
            }
        }

        assert(opstack.size() == 1);

        return opstack.back();
    }

    /**
     * Function adds disjunction operators to a postfix form when there are no operators at all.
     * This is currently case of initial and final states of NFA where a user usually doesn't want to write
     * initial or final states as a formula
     * @param postfix Postfix to which operators are eventually added
     * @return A postfix with eventually added operators
     */
    std::vector<Mata::FormulaNode> add_disjunction_implicitly(const std::vector<Mata::FormulaNode> &postfix)
    {
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
     * The wrapping function for parsing one section of input to IntermediateAut.
     * It parses basic information about type of automaton and naming of its component.
     * Then it parses initial and final formula and finally it creates graphs for transition formula.
     * @param section A section of input MATA format
     * @return Parsed InterAutomata representing an automaton from input.
     */
    Mata::IntermediateAut mf_to_aut(const Mata::Parser::ParsedSection &section)
    {
        Mata::IntermediateAut aut;

        if (section.type.find("NFA") != std::string::npos) {
            aut.automaton_type = Mata::IntermediateAut::AutomatonType::NFA;
        } else if (section.type.find("AFA") != std::string::npos) {
            aut.automaton_type = Mata::IntermediateAut::AutomatonType::AFA;
        }
        aut.alphabet_type = get_alphabet_type(section.type);

        for (const auto& keypair : section.dict) {
            const std::string& key = keypair.first;
            if (key.find("Alphabet") != std::string::npos) {
                aut.symbol_naming = get_naming_type(key);
                if (aut.are_symbols_enum_type())
                    aut.symbols_names.insert(
                            aut.symbols_names.end(), keypair.second.begin(), keypair.second.end());
            } else if (key.find("States") != std::string::npos) {
                aut.state_naming = get_naming_type(key);
                if (aut.are_states_enum_type())
                    aut.states_names.insert(
                            aut.states_names.end(), keypair.second.begin(), keypair.second.end());
            } else if (key.find("Nodes") != std::string::npos) {
                aut.node_naming = get_naming_type(key);
                if (aut.are_nodes_enum_type())
                    aut.nodes_names.insert(
                            aut.nodes_names.end(), keypair.second.begin(), keypair.second.end());
            }
        }

        // Once we know what states and nodes are we can parse initial and final formula
        for (const auto& keypair : section.dict) {
            const std::string &key = keypair.first;

            if (key.find("Initial") != std::string::npos) {
                auto postfix = infix_to_postfix(aut, keypair.second);
                if (no_operators(postfix)) {
                    aut.initial_enumerated = true;
                    postfix = add_disjunction_implicitly(postfix);
                }
                aut.initial_formula = postfix_to_graph(postfix);
            } else if (key.find("Final") != std::string::npos) {
                auto postfix = infix_to_postfix(aut, keypair.second);
                if (no_operators(postfix)) {
                    postfix = add_disjunction_implicitly(postfix);
                    aut.final_enumerated = true;
                }
                aut.final_formula = postfix_to_graph(postfix);
            }
        }

        if (!has_atmost_one_auto_naming(aut)) {
            throw std::runtime_error("Only one of nodes, symbols and states could be specified automatically");
        }

        for (const auto& trans : section.body) {
            Mata::IntermediateAut::parse_transition(aut,trans);
        }

        return aut;
    }
} // anonymous

size_t Mata::IntermediateAut::get_number_of_disjuncts() const
{
    size_t res = 0;

    for (const auto& trans : this->transitions) {
        size_t trans_disjuncts = 0;
        std::vector<const FormulaGraph *> stack;
        stack.push_back(&trans.second);

        while (!stack.empty()) {
            const FormulaGraph *gr = stack.back();
            stack.pop_back();
            if (gr->node.is_operator() && gr->node.operator_type == FormulaNode::OR)
                trans_disjuncts++;
            for (const auto &ch: gr->children)
                stack.push_back(&ch);
        }
        res += std::max(trans_disjuncts, (size_t) 1);
    }

    return res;
}

/**
 * Parses a transition by firstly transforming transition formula to postfix form and then creating
 * a tree representing the formula from postfix.
 * @param aut Automaton to which transition will be added.
 * @param tokens Series of tokens representing transition formula
 */
void Mata::IntermediateAut::parse_transition(Mata::IntermediateAut &aut, const std::vector<std::string> &tokens)
{
    assert(tokens.size() > 1); // transition formula has at least two items
    const Mata::FormulaNode lhs = create_node(aut, tokens[0]);
    const std::vector<std::string> rhs(tokens.begin()+1, tokens.end());

    std::vector<Mata::FormulaNode> postfix;

    // add implicit conjunction to NFA explicit states, i.e. p a q -> p a & q
    if (aut.automaton_type == Mata::IntermediateAut::NFA && tokens[tokens.size()-2] != "&") {
        // we need to take care about this case manually since user does not need to determine
        // symbol and state naming and put conjunction to transition
        if (aut.alphabet_type != Mata::IntermediateAut::BITVECTOR) {
            assert(rhs.size() == 2);
            postfix.push_back(Mata::FormulaNode{Mata::FormulaNode::Type::OPERAND, rhs[0], rhs[0],
                                                Mata::FormulaNode::OperandType::SYMBOL});
            postfix.push_back(create_node(aut,rhs[1]));
        } else if (aut.alphabet_type == Mata::IntermediateAut::BITVECTOR) {
            // this is a case where rhs state not separated by conjunction from rest of trans
            postfix = infix_to_postfix(aut, std::vector<std::string>(rhs.begin(), rhs.end()-1));
            postfix.push_back(create_node(aut,rhs.back()));
        } else
            assert(false && "Unknown NFA type");

        postfix.push_back(Mata::FormulaNode(
                Mata::FormulaNode::OPERATOR, "&", "&", Mata::FormulaNode::AND));
    } else
        postfix = infix_to_postfix(aut, rhs);

    for (const auto& node : postfix) {
        assert(node.is_operator() || (node.name != "!" && node.name != "&" && node.name != "|"));
        assert(node.is_leftpar() || node.name != "(");
        assert(node.is_rightpar() || node.name != ")");
    }
    const Mata::FormulaGraph graph = postfix_to_graph(postfix);

    aut.transitions.push_back(std::pair<Mata::FormulaNode,Mata::FormulaGraph>(lhs, graph));
}

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

        if (g->node.is_operand()) {
            res.insert(g->node.name);
        }

        for (const auto& child : g->children) {
            stack.push_back(&child);
        }
    }

    return res;
}

void Mata::FormulaGraph::print_tree(std::ostream& os) const
{
    std::vector<const FormulaGraph*> next_level;
    std::vector<const FormulaGraph*> this_level;

    next_level.push_back(this);
    while (!next_level.empty())
    {
        this_level = next_level;
        next_level.clear();
        for (const auto& graph : this_level) {
            for (const auto& child : graph->children) {
                next_level.push_back(&child);
            }
            os << graph->node.raw << "    ";
        }
        os << "\n";
    }
}

std::vector<Mata::IntermediateAut> Mata::IntermediateAut::parse_from_mf(const Mata::Parser::Parsed &parsed)
{
    std::vector<Mata::IntermediateAut> result;
    result.reserve(parsed.size());

    for (const auto& parsed_section : parsed) {
        if (parsed_section.type.find("FA") == std::string::npos) {
            continue;
        }
        result.push_back(mf_to_aut(parsed_section));
    }

    return result;
}

const Mata::FormulaGraph& Mata::IntermediateAut::get_symbol_part_of_transition(
        const std::pair<FormulaNode, FormulaGraph>& trans) const
{
    if (!this->is_nfa()) {
        throw std::runtime_error("We currently support symbol extraction only for NFA");
    }
    assert(trans.first.is_operand() && trans.first.operand_type == FormulaNode::STATE);
    assert(trans.second.node.is_operator()); // conjunction with rhs state
    assert(trans.second.children[1].node.is_operand()); // rhs state
    return trans.second.children[0];
}

void Mata::IntermediateAut::add_transition(const FormulaNode& lhs, const FormulaNode& symbol, const FormulaGraph& rhs)
{
    FormulaNode conjunction(FormulaNode::OPERATOR, "&", "&", FormulaNode::AND);
    FormulaGraph graph(conjunction);
    graph.children.push_back(FormulaGraph(symbol));
    graph.children.push_back(rhs);
    this->transitions.push_back(std::pair<FormulaNode, FormulaGraph>(lhs, graph));
}

void Mata::IntermediateAut::add_transition(const FormulaNode& lhs, const FormulaNode& rhs)
{
    assert(rhs.is_operand());
    FormulaGraph graph(rhs);
    this->transitions.push_back(std::pair<FormulaNode, FormulaGraph>(lhs, graph));
}

void Mata::IntermediateAut::print_transitions_trees(std::ostream& os) const
{
    for (const auto& trans : transitions) {
        os << trans.first.raw << " -> ";
        trans.second.print_tree(os);
    }
}

std::unordered_set<std::string> Mata::IntermediateAut::get_positive_finals() const
{
    if (!is_graph_conjunction_of_negations(this->final_formula))
        throw (std::runtime_error("Final formula is not a conjunction of negations"));

    std::unordered_set<std::string> all = initial_formula.collect_node_names();
    for (const auto& trans : this->transitions) {
        all.insert(trans.first.name);
        // get names from state part of transition
        const auto node_names = trans.second.children[1].collect_node_names();
        all.insert(node_names.begin(), node_names.end());
    }

    for (const std::string& nonfinal : final_formula.collect_node_names())
        all.erase(nonfinal);

    return all;
}

bool Mata::IntermediateAut::is_graph_conjunction_of_negations(const Mata::FormulaGraph &graph) {
    const FormulaGraph *act_graph = &graph;

    while (act_graph->children.size() == 2) {
        // this node is conjunction and the left son is negation, otherwise returns false
        if (act_graph->node.is_operator() && act_graph->node.is_and() &&
            act_graph->children[0].node.is_operator() && act_graph->children[0].node.is_neg())
            act_graph = &act_graph->children[1];
        else
            return false;
    }

    // the last child needs to be negation
    return (act_graph->node.is_operator() && act_graph->node.is_neg());
}

std::ostream& std::operator<<(std::ostream& os, const Mata::IntermediateAut& inter_aut)
{
    std::string type = inter_aut.is_nfa() ? "NFA" : (inter_aut.is_afa() ? "AFA" : "Unknown");
    os << "Intermediate automaton type " << type << '\n';
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
        os << trans.first.raw << " -> ";
        os << serialize_graph(trans.second);
        /*
        for (const auto& rhs : trans.second.collect_node_names()) {
            os << rhs << ' ';
        }
         */
        os << '\n';
    }
    os << "\n";

    return os;
}
