//TODO: rename to intermediate_afa / afa-intermediate, or something like that.
/*
 * inter-aut.hh -- intermediate representation of automata.
 *
 * It represents automaton after parsing and before translation to particular automaton.
 */

#ifndef MATA_INTER_AUT_HH
#define MATA_INTER_AUT_HH

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <utility>
#include <vector>

#include "parser.hh"

namespace mata {

/**
 * A node of graph representing transition formula. A node could be operator (!,&,|) or operand (symbol, state, node).
 * Each node has a name (in case of marking naming, an initial character defining type of node is removed and stored in
 * name), raw (name including potential type marker), and information about its type.
 */
struct FormulaNode {
public:
    enum class OperandType {
        SYMBOL,
        STATE,
        NODE,
        TRUE,
        FALSE,
        NOT_OPERAND
    };

    enum class OperatorType {
        NEG,
        AND,
        OR,
        NOT_OPERATOR
    };

    enum class Type {
        OPERAND,
        OPERATOR,
        LEFT_PARENTHESIS,
        RIGHT_PARENTHESIS,
        UNKNOWN
    };

    /// Define whether a node is operand or operator
    Type type;
    /// Raw name of node as it was specified in input text, i.e., including type marker.
    std::string raw;
    /// Parsed name, i.e., a potential type marker (first character) is removed.
    std::string name; // Parsed name. When type marking is used, markers are removed.
    /// if a node is operator, it defines which one
    OperatorType operator_type;
    /// if a node is operand, it defines which one
    OperandType operand_type;

    bool is_operand() const { return type == Type::OPERAND; }

    bool is_operator() const { return type == Type::OPERATOR; }

    bool is_rightpar() const { return type == Type::RIGHT_PARENTHESIS; }

    bool is_leftpar() const { return type == Type::LEFT_PARENTHESIS; }

    bool is_state() const { return operand_type == OperandType::STATE; }

    bool is_symbol() const { return operand_type == OperandType::SYMBOL; }

    bool is_and() const { return type == Type::OPERATOR && operator_type == OperatorType::AND; }

    bool is_neg() const { return type == Type::OPERATOR && operator_type == OperatorType::NEG; }

    // TODO: should constant be its own operand type?
    bool is_constant() const { return is_true() || is_false(); }
    bool is_true() const { return is_operand() && operand_type == OperandType::TRUE; }
    bool is_false() const { return is_operand() && operand_type == OperandType::FALSE; }

    FormulaNode()
        : type(Type::UNKNOWN), raw(), name(), operator_type(OperatorType::NOT_OPERATOR),
          operand_type(OperandType::NOT_OPERAND) {}

    FormulaNode(Type t, std::string raw, std::string name, OperatorType operator_t)
        : type(t), raw(std::move(raw)), name(std::move(name)), operator_type(operator_t), operand_type(OperandType::NOT_OPERAND) {}

    FormulaNode(Type t, std::string raw, std::string name, OperandType operand)
        : type(t), raw(std::move(raw)), name(std::move(name)), operator_type(OperatorType::NOT_OPERATOR), operand_type(operand) {}

    FormulaNode(Type t, const std::string& raw)
        : type(t), raw(raw), name(raw), operator_type(OperatorType::NOT_OPERATOR), operand_type(OperandType::NOT_OPERAND) {}

    FormulaNode(const FormulaNode& n)
        : type(n.type), raw(n.raw), name(n.name), operator_type(n.operator_type), operand_type(n.operand_type) {}

    FormulaNode(FormulaNode&&) noexcept = default;

    FormulaNode& operator=(const FormulaNode& other) = default;
    FormulaNode& operator=(FormulaNode&& other) noexcept = default;
};

/**
 * Structure representing a transition formula using a graph.
 * A node of graph consists of node itself and set of children nodes.
 * Nodes are operators and operands of the formula.
 * E.g., a formula q1 & s1 will be transformed to a tree with & as a root node
 * and q1 and s2 being children nodes of the root.
 */
class FormulaGraph {
private:
    /// Maximal number of @c FormulaNode children in any @c FormulaGraph node (for AND, OR nodes). Only one (NOT node)
    ///  or zero (@c FormulaNode node) children may be used. Used as an optimalization by preallocating @c children at
    ///  node initialization.
    static constexpr size_t MAX_NUM_OF_CHILDREN{ 2 };
public:
    FormulaNode node{};
    std::vector<FormulaGraph> children{};

    FormulaGraph() = default;
    explicit FormulaGraph(const FormulaNode& n) : node(n), children() { children.reserve(MAX_NUM_OF_CHILDREN); }
    explicit FormulaGraph(FormulaNode&& n) : node(std::move(n)), children() { children.reserve(MAX_NUM_OF_CHILDREN); }
    FormulaGraph(const FormulaGraph& g) : node(g.node), children(g.children) {}
    FormulaGraph(FormulaGraph&& g) noexcept : node(std::move(g.node)), children(std::move(g.children)) {}

    FormulaGraph& operator=(const mata::FormulaGraph&) = default;
    FormulaGraph& operator=(mata::FormulaGraph&&) noexcept = default;

    std::unordered_set<std::string> collect_node_names() const;
    void print_tree(std::ostream& os) const;
};

/**
 * Structure for a general intermediate representation of parsed automaton.
 * It contains information about type of automata, type of naming of nodes, symbols and states
 * and type of alphabet. It contains also the transitions formula and formula for initial and final
 * states. The formulas are represented as tree where nodes are either operands or operators.
 */
class IntermediateAut {
public:
    /**
     * Type of automaton. So far we support nondeterministic finite automata (NFA) and
     * alternating finite automata (AFA)
     */
    enum class AutomatonType {
        NFA,
        AFA
    };

    /**
     * The possible kinds of naming of items in sets of states, nodes, or symbols.
     * It implies how the given set is defined.
     * Naming could be automatic (all things in formula not belonging to other sets will be assigned to a
     * set with automatic naming), marker based (everything beginning with `q` is a state, with `s` is a symbol,
     * with `n` is a node), or enumerated (the given set is defined by enumeration).
     * There are two special cases used for alphabet - symbols could be any character (CHARS) or anything from utf (UTF).
     */
     enum class Naming {
        AUTO,
        MARKED,
        ENUM,
        CHARS,
        UTF
    };

    /**
     * Type of alphabet. We can have explicit alphabet (containing explicit symbols), or bitvector, or class of symbols
     * (e.g., alphabet is everything in utf), or intervals.
     * So far, only explicit representation is supported.
     */
    enum class AlphabetType {
        EXPLICIT,
        BITVECTOR,
        CLASS,
        INTERVALS
    };

    Naming state_naming = Naming::MARKED;
    Naming symbol_naming = Naming::MARKED;
    Naming node_naming = Naming::MARKED;
    AlphabetType alphabet_type{};
    AutomatonType automaton_type{};

    // The vectors represent the given sets when enumeration is used.
    std::vector<std::string> states_names{};
    std::vector<std::string> symbols_names{};
    std::vector<std::string> nodes_names{};

    FormulaGraph initial_formula{};
    FormulaGraph final_formula{};

    bool initial_enumerated = false;
    bool final_enumerated = false;

    /**
     * Transitions are pairs where the first member is left-hand side of transition (i.e., a state)
     * and the second item is a graph representing transition formula (which can contain symbols, nodes, and states).
     */
    struct std::vector<std::pair<FormulaNode, FormulaGraph>> transitions{};

    /**
     * Returns symbolic part of transition. That may be just a symbol or bitvector formula.
     * This function is supported only for NFA where transitions have a rhs state at the end of right
     * handed side of transition.
     * @param transition Transition from which symbol is returned
     * @return Graph representing symbol. It maybe just an explicit symbol or graph representing bitvector formula
     */
    const FormulaGraph& get_symbol_part_of_transition(const std::pair<FormulaNode, FormulaGraph>& trans) const;

    /**
     * @brief A method for building a vector of IntermediateAut for a parsed input.
     *
     * For each section in input is created one IntermediateAut.
     * It parses basic information about type of automata, its naming conventions etc.
     * Then it parses input and final formula of automaton.
     * Finally, transition formulas are transformed to graph representation by
     * turning an input stream of tokens to postfix notation and then
     * a tree representing the formula is built from it.
     * @param parsed Parsed input in MATA format.
     * @return A vector of InterAutomata from each section in parsed input.
     */
    static std::vector<IntermediateAut> parse_from_mf(const mata::parser::Parsed& parsed);

    bool are_states_enum_type() const {return state_naming == Naming::ENUM;}
    bool are_symbols_enum_type() const {return symbol_naming == Naming::ENUM;}
    bool are_nodes_enum_type() const {return node_naming == Naming::ENUM;}

    bool is_bitvector() const {return alphabet_type == AlphabetType::BITVECTOR;}
    bool is_nfa() const {return automaton_type == AutomatonType::NFA;}
    bool is_afa() const {return automaton_type == AutomatonType::AFA;}

    std::unordered_set<std::string> get_enumerated_initials() const {return initial_formula.collect_node_names();}
    std::unordered_set<std::string> get_enumerated_finals() const {return final_formula.collect_node_names();}

    bool are_final_states_conjunction_of_negation() const {
        return is_graph_conjunction_of_negations(final_formula);
    }

    static bool is_graph_conjunction_of_negations(const FormulaGraph& graph);

    /**
     * Method returns a set of final states in the case that they were entered as a conjunction
     * of negated states. It collects all negated states and subtracts them from set of all states.
     */
    std::unordered_set<std::string> get_positive_finals() const;

    size_t get_number_of_disjuncts() const;

    static void parse_transition(mata::IntermediateAut &aut, const std::vector<std::string> &tokens);
    void add_transition(const FormulaNode& lhs, const FormulaNode& symbol, const FormulaGraph& rhs);
    void add_transition(const FormulaNode& lhs, const FormulaNode& rhs);
    void print_transitions_trees(std::ostream&) const;
}; // class IntermediateAut.

} // namespace mata.

namespace std {
    std::ostream& operator<<(std::ostream& os, const mata::IntermediateAut& inter_aut);
}

#endif //MATA_INTER_AUT_HH
