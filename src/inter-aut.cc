/*
 * inter-aut.hh -- intermediate representation of automata.
 *
 * It represents automaton after parsing and before translation to particular automaton.
 */

#include <cassert>

#include "mata/parser/inter-aut.hh"
#include "mata/utils/utils.hh"

namespace {

bool has_at_most_one_auto_naming(const mata::IntermediateAut& aut) {
  return !(!(aut.node_naming == mata::IntermediateAut::Naming::Auto &&
             aut.symbol_naming == mata::IntermediateAut::Naming::Auto) &&
           (aut.state_naming == mata::IntermediateAut::Naming::Auto));
    }

    bool is_logical_operator(const char ch) { return (mata::utils::haskey(std::set<char>{ '&', '|', '!'}, ch)); }

    mata::IntermediateAut::Naming get_naming_type(const std::string &key)
    {
        const size_t found = key.find('-');
        assert(found != std::string::npos);

        if (const std::string& type = key.substr(found + 1, std::string::npos); type == "auto")
            return mata::IntermediateAut::Naming::Auto;
        else if (type == "enum")
            return mata::IntermediateAut::Naming::Enum;
        else if (type == "marked")
            return mata::IntermediateAut::Naming::Marked;
        else if (type == "chars")
            return mata::IntermediateAut::Naming::Chars;
        else if (type == "utf")
            return mata::IntermediateAut::Naming::Utf;

        assert(false && "Unknown naming type - a naming type should be always defined correctly otherwise it is"
                        "impossible to parse automaton correctly");
        return {};
    }

    mata::IntermediateAut::AlphabetType get_alphabet_type(const std::string &type)
    {
        assert(type.find('-') != std::string::npos);

        if (const std::string& alph_type = type.substr(type.find('-') + 1, std::string::npos); alph_type == "bits") {
            return mata::IntermediateAut::AlphabetType::Bitvector;
        } else if (alph_type == "explicit") {
            return mata::IntermediateAut::AlphabetType::Explicit;
        } else if (alph_type == "intervals") {
            return mata::IntermediateAut::AlphabetType::Intervals;
        }

        assert(false && "Unknown alphabet type - an alphabet type should be always defined correctly otherwise it is"
                        "impossible to parse automaton correctly");
        return {};
    }

    bool is_naming_marker(const mata::IntermediateAut::Naming naming)
    {
        return naming == mata::IntermediateAut::Naming::Marked;
    }

    bool is_naming_enum(const mata::IntermediateAut::Naming naming)
    {
        return naming == mata::IntermediateAut::Naming::Enum;
    }

    bool is_naming_auto(const mata::IntermediateAut::Naming naming)
    {
        return naming == mata::IntermediateAut::Naming::Auto;
    }

    bool contains(const std::vector<std::string> &vec, const std::string &item) {
        return std::ranges::find(vec, item) != vec.end();
    }

    bool no_operators(const std::vector<mata::FormulaNode>& nodes) {
        return std::ranges::all_of(nodes, [](const mata::FormulaNode& node) { return !node.is_operator(); });
    }

    std::string serialize_graph(const mata::FormulaGraph& graph)
    {
        if (graph.node.is_operand())
            return graph.node.raw;

        if (graph.children.size() == 1) { // unary operator
            const auto& child = graph.children.front();
            const std::string child_name = child.node.is_operand() ? child.node.raw :
                    "(" + serialize_graph(mata::FormulaGraph{ child.node }) + ")";
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

    mata::FormulaNode create_node(const mata::IntermediateAut &mata, const std::string& token) {
        if (is_logical_operator(token[0])) {
            switch (token[0]) {
                case '&':
                    return { mata::FormulaNode::Type::Operator, token, token,
                    mata::FormulaNode::OperatorType::And };
                case '|':
                    return { mata::FormulaNode::Type::Operator, token, token,
                    mata::FormulaNode::OperatorType::Or };
                case '!':
                    return { mata::FormulaNode::Type::Operator, token, token,
                    mata::FormulaNode::OperatorType::Neg };
                default:
                    assert(false);
            }
        } else if (token == "(") {
            return { mata::FormulaNode::Type::LeftParenthesis, token};
        } else if (token == ")") {
            return { mata::FormulaNode::Type::RightParenthesis, token};
        } else if (token == "\\true") {
            return { mata::FormulaNode::Type::Operand, token, token,
                                      mata::FormulaNode::OperandType::True};
        } else if (token == "\\false") {
            return { mata::FormulaNode::Type::Operand, token, token,
                                      mata::FormulaNode::OperandType::False};
        } else if (is_naming_enum(mata.state_naming) && contains(mata.states_names, token)) {
            return { mata::FormulaNode::Type::Operand, token, token,
                                      mata::FormulaNode::OperandType::State};
        } else if (is_naming_enum(mata.node_naming) && contains(mata.nodes_names, token)) {
            return { mata::FormulaNode::Type::Operand, token, token,
                                      mata::FormulaNode::OperandType::Node};
        } else if (is_naming_enum(mata.symbol_naming) && contains(mata.symbols_names, token)) {
            return { mata::FormulaNode::Type::Operand, token, token,
                                      mata::FormulaNode::OperandType::Symbol};
        } else if (is_naming_marker(mata.state_naming) && token[0] == 'q') {
            return { mata::FormulaNode::Type::Operand, token, token.substr(1),
                                      mata::FormulaNode::OperandType::State};
        } else if (is_naming_marker(mata.node_naming) && token[0] == 'n') {
            return { mata::FormulaNode::Type::Operand, token, token.substr(1),
                                      mata::FormulaNode::OperandType::Node};
        } else if (is_naming_marker(mata.symbol_naming) && token[0] == 'a') {
            return { mata::FormulaNode::Type::Operand, token, token.substr(1),
                                      mata::FormulaNode::OperandType::Symbol};
        } else if (is_naming_auto(mata.state_naming)) {
            return { mata::FormulaNode::Type::Operand, token, token,
                                      mata::FormulaNode::OperandType::State};
        } else if (is_naming_auto(mata.node_naming)) {
            return { mata::FormulaNode::Type::Operand, token, token,
                                      mata::FormulaNode::OperandType::Node};
        } else if (is_naming_auto(mata.symbol_naming)) {
            return { mata::FormulaNode::Type::Operand, token, token,
                                      mata::FormulaNode::OperandType::Symbol};
        }

        throw std::runtime_error("Unknown token " + token);
    }

    /**
     * Checks if @p op1 has lower precedence than @p op2. Precedence from the lowest to highest is: | < & < !
     */
    bool lower_precedence(const mata::FormulaNode::OperatorType op1, const mata::FormulaNode::OperatorType op2) {
        if (op1 == mata::FormulaNode::OperatorType::Neg ||
            (op1 == mata::FormulaNode::OperatorType::And && op2 != mata::FormulaNode::OperatorType::Neg)
        ) { return false; }
        return true;
    }

    /**
     * Transforms a series of tokes to postfix notation.
     * @param aut Automaton for which transition formula is parsed
     * @param tokens Series of tokens representing transition formula parsed from the input text
     * @return A postfix notation for input
     */
    std::vector<mata::FormulaNode> infix_to_postfix(const mata::IntermediateAut &aut,
                                                    const std::vector<std::string>& tokens) {
        std::vector<mata::FormulaNode> opstack;
        std::vector<mata::FormulaNode> output;
        output.reserve(tokens.size());
        opstack.reserve(32);

        for (const std::string& token: tokens) {
            switch (mata::FormulaNode node = create_node(aut, token); node.type) {
                case mata::FormulaNode::Type::Operand:
                    output.emplace_back(std::move(node));
                    break;
                case mata::FormulaNode::Type::LeftParenthesis:
                    opstack.emplace_back(std::move(node));
                    break;
                case mata::FormulaNode::Type::RightParenthesis:
                    while (!opstack.back().is_left_parenthesis()) {
                        output.emplace_back(std::move(opstack.back()));
                        opstack.pop_back();
                    }
                    opstack.pop_back();
                    break;
                case mata::FormulaNode::Type::Operator:
                    for (int j = static_cast<int>(opstack.size())-1; j >= 0; --j) {
                        auto formula_node_opstack_it{ opstack.begin() + j };
                        assert(!formula_node_opstack_it->is_operand());
                        if (formula_node_opstack_it->is_left_parenthesis()) {
                            break;
                        }
                        if (lower_precedence(node.operator_type, formula_node_opstack_it->operator_type)) {
                            output.emplace_back(std::move(*formula_node_opstack_it));
                            opstack.erase(formula_node_opstack_it);
                        }
                    }
                    opstack.emplace_back(std::move(node));
                    break;
                default: assert(false);
            }
        }

        while (!opstack.empty()) {
            output.push_back(std::move(opstack.back()));
            opstack.pop_back();
        }

        #ifndef NDEBUG
        for (const auto& node : output) {
            assert(node.is_operator() || (node.name != "!" && node.name != "&" && node.name != "|"));
            assert(node.is_left_parenthesis() || node.name != "(");
            assert(node.is_right_parenthesis() || node.name != ")");
        }
        #endif // #ifndef NDEBUG.

        return output;
    }

    /**
     * Create a graph for a postfix representation of transition formula
     * @param postfix A postfix representation of transition formula
     * @return A parsed graph
     */
    mata::FormulaGraph postfix_to_graph(std::vector<mata::FormulaNode> postfix) {
            std::vector<mata::FormulaGraph> opstack{};
            opstack.reserve(4);
            for (mata::FormulaNode& node: postfix) {
                switch (node.type) {
                case mata::FormulaNode::Type::Operand:
                    opstack.emplace_back(std::move(node));
                    break;
                case mata::FormulaNode::Type::Operator: {
                    switch (node.operator_type) {
                        case mata::FormulaNode::OperatorType::Neg: { // 1 child: graph will be a NEG node.
                            assert(!opstack.empty());
                            mata::FormulaGraph child{ std::move(opstack.back()) };
                            opstack.pop_back();
                            mata::FormulaGraph& gr{ opstack.emplace_back(std::move(node)) };
                            gr.children.emplace_back(std::move(child));
                            break;
                        }
                        default: { // 2 children: Graph will be either an AND node, or an OR node.
                            assert(opstack.size() > 1);
                            mata::FormulaGraph second_child{ std::move(opstack.back()) };
                            opstack.pop_back();
                            mata::FormulaGraph first_child{ std::move(opstack.back()) };
                            opstack.pop_back();
                            mata::FormulaGraph& gr{ opstack.emplace_back(std::move(node)) };
                            gr.children.emplace_back(std::move(first_child));
                            gr.children.emplace_back(std::move(second_child));
                            break;
                        }
                    }
                    break;
                }
                default:
                    assert(false && "Unknown type of node");
            }
        }

        // In case of no transition
        if (opstack.size() != 1) {
            return {};
        }

        assert(opstack.size() == 1);
        return std::move(*opstack.begin());
}

    /**
     * Function adds disjunction operators to a postfix form when there are no operators at all.
     * This is currently case of initial and final states of NFA where a user usually doesn't want to write
     * initial or final states as a formula
     * @param postfix Postfix to which operators are eventually added
     * @return A postfix with eventually added operators
     */
    std::vector<mata::FormulaNode> add_disjunction_implicitly(std::vector<mata::FormulaNode> postfix) {
        const size_t postfix_size{ postfix.size() };
        if (postfix_size == 1) // no need to add operators
            return postfix;

        for (const auto& op : postfix) {
            if (op.is_operator()) // operators provided by user, return the original postfix
                return postfix;
        }

        std::vector<mata::FormulaNode> res;
        if (postfix_size >= 2) {
            res.push_back(std::move(postfix[0]));
            res.push_back(std::move(postfix[1]));
            res.emplace_back(
                    mata::FormulaNode::Type::Operator, "|", "|", mata::FormulaNode::OperatorType::Or);
        }

        for (size_t i{ 2 }; i < postfix_size; ++i) {
            res.push_back(std::move(postfix[i]));
            res.emplace_back(
                    mata::FormulaNode::Type::Operator, "|", "|", mata::FormulaNode::OperatorType::Or);
        }

        return res;
    }

    /**
     * The wrapping function for parsing one section of input to IntermediateAut.
     * It parses basic information about type of automaton and naming of its component.
     * Then it parses initial and final formula, and finally it creates graphs for transition formula.
     * @param section A section of input MATA format
     * @return Parsed InterAutomata representing an automaton from input.
     */
    mata::IntermediateAut mf_to_aut(const mata::parser::ParsedSection& section) {
        mata::IntermediateAut aut;

        if (section.type.find("NFA") != std::string::npos) {
            aut.automaton_type = mata::IntermediateAut::AutomatonType::Nfa;
        } else if (section.type.find("AFA") != std::string::npos) {
            aut.automaton_type = mata::IntermediateAut::AutomatonType::Afa;
        } else if (section.type.find("NFT") != std::string::npos) {
            aut.automaton_type = mata::IntermediateAut::AutomatonType::Nft;
        }
        aut.alphabet_type = get_alphabet_type(section.type);

        for (const auto& [key, symbol_names] : section.dict) {
            if (key.find("Alphabet") != std::string::npos) {
                aut.symbol_naming = get_naming_type(key);
                if (aut.are_symbols_enum_type())
                    aut.symbols_names.insert(
                        aut.symbols_names.end(), symbol_names.begin(), symbol_names.end()
                    );
            } else if (key.find("States") != std::string::npos) {
                aut.state_naming = get_naming_type(key);
                if (aut.are_states_enum_type())
                    aut.states_names.insert(
                        aut.states_names.end(), symbol_names.begin(), symbol_names.end()
                    );
            } else if (key.find("Nodes") != std::string::npos) {
                aut.node_naming = get_naming_type(key);
                if (aut.are_nodes_enum_type())
                    aut.nodes_names.insert(
                        aut.nodes_names.end(), symbol_names.begin(), symbol_names.end()
                    );
            }
        }

        // Once we know what states and nodes are we can parse initial and final formula
        for (const auto& [key, symbol_names] : section.dict) {
            if (key.starts_with("Initial")) {
                auto postfix = infix_to_postfix(aut, symbol_names);
                if (no_operators(postfix)) {
                    aut.initial_enumerated = true;
                    postfix = add_disjunction_implicitly(std::move(postfix));
                }
                aut.initial_formula = postfix_to_graph(std::move(postfix));
            } else if (key.starts_with("Final")) {
                auto postfix = infix_to_postfix(aut, symbol_names);
                if (no_operators(postfix)) {
                    postfix = add_disjunction_implicitly(std::move(postfix));
                    aut.final_enumerated = true;
                }
                aut.final_formula = postfix_to_graph(std::move(postfix));;
            }
        }

        if (!has_at_most_one_auto_naming(aut)) {
            throw std::runtime_error("Only one of nodes, symbols and states could be specified automatically");
        }

        for (const auto& trans : section.body) {
            mata::IntermediateAut::parse_transition(aut, trans);
        }

        return aut;
    }
} // Anonymous namespace.

size_t mata::IntermediateAut::get_number_of_disjuncts() const
{
    size_t res = 0;

    for (const auto& formula_graph : this->transitions | std::views::values) {
        size_t trans_disjuncts = 0;
        std::vector<const FormulaGraph *> stack;
        stack.push_back(&formula_graph);

        while (!stack.empty()) {
            const FormulaGraph *gr = stack.back();
            stack.pop_back();
            if (gr->node.is_operator() && gr->node.operator_type == FormulaNode::OperatorType::Or)
                trans_disjuncts++;
            for (const auto &ch: gr->children)
                stack.push_back(&ch);
        }
        res += std::max(trans_disjuncts, 1ul);
    }

    return res;
}

/**
 * Parses a transition by firstly transforming transition formula to postfix form and then creating
 * a tree representing the formula from postfix.
 * @param aut Automaton to which transition will be added.
 * @param tokens Series of tokens representing transition formula
 */
void mata::IntermediateAut::parse_transition(mata::IntermediateAut &aut, const std::vector<std::string>& tokens)
{
    assert(tokens.size() > 1); // transition formula has at least two items
    mata::FormulaNode lhs = create_node(aut, tokens[0]);
    std::vector<std::string> rhs(tokens.begin()+1, tokens.end());

    std::vector<mata::FormulaNode> postfix;

    // add implicit conjunction to NFA explicit states, i.e. p a q -> p a & q
    if (aut.automaton_type == mata::IntermediateAut::AutomatonType::Nfa && tokens[tokens.size() - 2] != "&") {
        // we need to take care about this case manually since user does not need to determine
        // symbol and state naming and put conjunction to transition
        if (aut.alphabet_type != mata::IntermediateAut::AlphabetType::Bitvector) {
            assert(rhs.size() == 2);
            postfix.emplace_back(mata::FormulaNode::Type::Operand, rhs[0], rhs[0], mata::FormulaNode::OperandType::Symbol);
            postfix.emplace_back(create_node(aut, rhs[1]));
        } else if (aut.alphabet_type == mata::IntermediateAut::AlphabetType::Bitvector) {
            // This is a case where rhs state is not separated by a conjunction from the rest of the transitions.
            const std::string last_token{ rhs.back() };
            rhs.pop_back();
            postfix = infix_to_postfix(aut, rhs);
            postfix.emplace_back(create_node(aut, last_token));
        } else
            assert(false && "Unknown NFA type");

        postfix.emplace_back(mata::FormulaNode::Type::Operator, "&", "&", mata::FormulaNode::OperatorType::And);
    } else if (aut.automaton_type == mata::IntermediateAut::AutomatonType::Nft && tokens[tokens.size() - 2] != "&") {
        // we need to take care about this case manually since user does not need to determine
        // symbol and state naming and put conjunction to transition
        if (aut.alphabet_type != mata::IntermediateAut::AlphabetType::Bitvector) {
            assert(rhs.size() == 2);
            postfix.emplace_back(mata::FormulaNode::Type::Operand, rhs[0], rhs[0], mata::FormulaNode::OperandType::Symbol);
            postfix.emplace_back(create_node(aut, rhs[1]));
        } else if (aut.alphabet_type == mata::IntermediateAut::AlphabetType::Bitvector) {
            // This is a case where rhs state is not separated by a conjunction from the rest of the transitions.
            const std::string last_token{ rhs.back() };
            rhs.pop_back();
            postfix = infix_to_postfix(aut, rhs);
            postfix.emplace_back(create_node(aut, last_token));
        } else
            assert(false && "Unknown NFT type");

        postfix.emplace_back(mata::FormulaNode::Type::Operator, "&", "&", mata::FormulaNode::OperatorType::And);
    } else
        postfix = infix_to_postfix(aut, rhs);

    #ifndef NDEBUG
    for (const auto& node: postfix) {
        assert(node.is_operator() || (node.name != "!" && node.name != "&" && node.name != "|"));
        assert(node.is_left_parenthesis() || node.name != "(");
        assert(node.is_right_parenthesis() || node.name != ")");
    }
    #endif // #ifndef NDEBUG.
    aut.transitions.emplace_back(std::move(lhs), postfix_to_graph(std::move(postfix)));
}

std::unordered_set<std::string> mata::FormulaGraph::collect_node_names() const
{
    std::unordered_set<std::string> res;
    std::vector<const mata::FormulaGraph *> stack;

    stack.emplace_back(reinterpret_cast<const FormulaGraph*>(&(this->node)));
    while (!stack.empty()) {
        const FormulaGraph* g = stack.back();
        assert(g != nullptr);
        stack.pop_back();

        if (g->node.type == FormulaNode::Type::Unknown)
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

void mata::FormulaGraph::print_tree(std::ostream& os) const
{
    std::vector<const FormulaGraph*> next_level;

    next_level.push_back(this);
    while (!next_level.empty())
    {
        std::vector<const FormulaGraph*> this_level = next_level;
        next_level.clear();
        for (const auto& graph : this_level) {
            for (const auto& child : graph->children) { next_level.push_back(&child); }
            os << graph->node.raw << "    ";
        }
        os << "\n";
    }
}

std::vector<mata::IntermediateAut> mata::IntermediateAut::parse_from_mf(const mata::parser::Parsed &parsed)
{
    std::vector<mata::IntermediateAut> result;
    result.reserve(parsed.size());

    for (const parser::ParsedSection& parsed_section: parsed) {
        if (parsed_section.type.find("FA") == std::string::npos && parsed_section.type.find("FT") == std::string::npos) {
            continue;
        }
        result.push_back(mf_to_aut(parsed_section));
    }

    return result;
}

const mata::FormulaGraph& mata::IntermediateAut::get_symbol_part_of_transition(
        const std::pair<FormulaNode, FormulaGraph>& transition) const
{
    if (!this->is_nfa()) {
        throw std::runtime_error("We currently support symbol extraction only for NFA");
    }
    assert(transition.first.is_operand() && transition.first.operand_type == FormulaNode::OperandType::State);
    assert(transition.second.node.is_operator()); // conjunction with rhs state
    assert(transition.second.children[1].node.is_operand()); // rhs state
    return transition.second.children[0];
}

void mata::IntermediateAut::add_transition(const FormulaNode& lhs, const FormulaNode& symbol, const FormulaGraph& rhs)
{
    const FormulaNode conjunction(FormulaNode::Type::Operator, "&", "&", FormulaNode::OperatorType::And);
    FormulaGraph graph(conjunction);
    graph.children.emplace_back(symbol);
    graph.children.push_back(rhs);
    this->transitions.emplace_back(lhs, std::move(graph));
}

void mata::IntermediateAut::add_transition(const FormulaNode& lhs, const FormulaNode& rhs)
{
    assert(rhs.is_operand());
    FormulaGraph graph(rhs);
    this->transitions.emplace_back(lhs, std::move(graph));
}

void mata::IntermediateAut::print_transitions_trees(std::ostream& os) const
{
    for (const auto& [formula_node, formula_graph] : transitions) {
        os << formula_node.raw << " -> ";
        formula_graph.print_tree(os);
    }
}

std::unordered_set<std::string> mata::IntermediateAut::get_positive_finals() const
{
    if (!is_graph_conjunction_of_negations(this->final_formula))
        throw (std::runtime_error("Final formula is not a conjunction of negations"));

    std::unordered_set<std::string> all = initial_formula.collect_node_names();
    for (const auto& [formula_node, formula_graph] : this->transitions) {
        all.insert(formula_node.name);
        // get names from state part of transition
        const auto node_names = formula_graph.children[1].collect_node_names();
        all.insert(node_names.begin(), node_names.end());
    }

    for (const std::string& nonfinal : final_formula.collect_node_names())
        all.erase(nonfinal);

    return all;
}

bool mata::IntermediateAut::is_graph_conjunction_of_negations(const mata::FormulaGraph &graph) {
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

std::ostream& std::operator<<(std::ostream& os, const mata::IntermediateAut& inter_aut)
{
    const std::string type = inter_aut.is_nfa() ? "NFA" : (inter_aut.is_afa() ? "AFA" : "Unknown");
    os << "Intermediate automaton type " << type << '\n';
    os << "Naming - state: " << static_cast<size_t>(inter_aut.state_naming) << " symbol: "
       << static_cast<size_t>(inter_aut.symbol_naming) << " node: " << static_cast<size_t>(inter_aut.node_naming) << '\n';
    os << "Alphabet " << static_cast<size_t>(inter_aut.alphabet_type) << '\n';

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
    for (const auto& [formula_node, formula_graph] : inter_aut.transitions) {
        os << formula_node.raw << " -> ";
        os << serialize_graph(formula_graph);
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
