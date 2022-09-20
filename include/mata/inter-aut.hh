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


#ifndef _MATA_INTER_AUT_HH
#define _MATA_INTER_AUT_HH

#include <unordered_map>
#include <unordered_set>

#include <mata/parser.hh>

#include <string>
#include <vector>

namespace Mata
{

/**
 * A node of graph representing transition formula. A node could be operator (!,&,|) or operand (symbol, state, node).
 * Each node has a name (in case of marking naming, an initial character defining type of node is removed and stored in
 * name), raw (name including potential type marker), and information about its type.
 */
struct FormulaNode
{
public:
    enum OperandType {
        SYMBOL,
        STATE,
        NODE,
        NOT_OPERAND
    };

    enum OperatorType {
        NEG,
        AND,
        OR,
        NOT_OPERATOR
    };

    enum Type {
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
    std::string name; // parsed name. When type marking is used, markers are removed.
    /// if a node is operator, it defines which one
    OperatorType operator_type;
    /// if a node is operand, it defines which one
    OperandType operand_type;

    bool is_operand() const { return type == Type::OPERAND; }

    bool is_operator() const { return type == Type::OPERATOR; }

    bool is_rightpar() const { return type == Type::RIGHT_PARENTHESIS; }

    bool is_leftpar() const { return type == Type::LEFT_PARENTHESIS; }

    FormulaNode() : type(UNKNOWN), raw(""), name(""), operator_type(NOT_OPERATOR), operand_type(NOT_OPERAND) {}

    FormulaNode(Type t, std::string raw, std::string name,
                OperatorType operator_t) : type(t), raw(raw), name(name), operator_type(operator_t),
                                           operand_type(NOT_OPERAND) {}

    FormulaNode(Type t, std::string raw, std::string name,
                OperandType operand) : type(t), raw(raw), name(name), operator_type(NOT_OPERATOR),
                                       operand_type(operand) {}

    FormulaNode(Type t, std::string raw) : type(t), raw(raw), name(raw), operator_type(NOT_OPERATOR),
                                           operand_type(NOT_OPERAND) {}
};

/**
 * Structure representing a transition formula using a graph.
 * A node of graph consists of node itself and set of children nodes.
 * Nodes are operators and operands of the formula.
 * E.g., a formula q1 & s1 will be transformed to a tree with & as a root node
 * and q1 and s2 being children nodes of the root.
 */
struct FormulaGraph
{
    FormulaNode node;
    std::vector<FormulaGraph> children;

    FormulaGraph() : node(), children() {}
    FormulaGraph(FormulaNode n) : node(n), children() {}

    std::unordered_set<std::string> collect_node_names() const;
};

/**
 * Structure for a general intermediate representation of parsed automaton.
 * It contains information about type of automata, type of naming of nodes, symbols and states
 * and type of alphabet. It contains also the transitions formula and formula for initial and final
 * states. The formulas are represented as tree where nodes are either operands or operators.
 */
struct IntermediateAut
{
    /**
     * Type of automaton. So far we support nondeterministic finite automata (NFA) and
     * alternating finite automata (AFA)
     */
    enum AutomatonType
    {
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
    enum Naming
    {
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
    enum AlphabetType
    {
        EXPLICIT,
        BITVECTOR,
        CLASS,
        INTERVALS
    };

public:
    Naming state_naming = MARKED;
    Naming symbol_naming = MARKED;
    Naming node_naming = MARKED;
    AlphabetType alphabet_type;
    AutomatonType automaton_type;

    // The vectors represent the given sets when enumeration is used.
    std::vector<std::string> states_names;
    std::vector<std::string> symbols_names;
    std::vector<std::string> nodes_names;

    FormulaGraph initial_formula;
    FormulaGraph final_formula;

    bool initial_enumerated = false;
    bool final_enumerated = false;

    /**
     * Transitions are pairs where the first member is left-hand side of transition (i.e., a state)
     * and the second item is a graph representing transition formula (which can contain symbols, nodes, and states).
     */
    struct std::vector<std::pair<FormulaNode, FormulaGraph>> transitions;

    /**
     * A method for building a vector of IntermediateAut for a parsed input.
     * For each section in input is created one IntermediateAut.
     * It parses basic information about type of automata, its naming conventions etc.
     * Then it parses input and final formula of automaton.
     * Finally, transition formulas are transformed to graph representation by
     * turning an input stream of tokens to postfix notation and then
     * a tree representing the formula is built from it.
     * @param parsed Parsed input in MATA format.
     * @return A vector of InterAutomata from each section in parsed input.
     */
    static std::vector<IntermediateAut> parse_from_mf(const Mata::Parser::Parsed& parsed);

    bool are_states_enum_type() const {return state_naming == Naming::ENUM;}
    bool are_symbols_enum_type() const {return symbol_naming == Naming::ENUM;}
    bool are_nodes_enum_type() const {return node_naming == Naming::ENUM;}

    bool is_nfa() const {return automaton_type == AutomatonType::NFA;}

    std::unordered_set<std::string> get_enumerated_initials() const {return initial_formula.collect_node_names();}
    std::unordered_set<std::string> get_enumerated_finals() const {return final_formula.collect_node_names();}
};

} /* Mata */

namespace std
{
    std::ostream& operator<<(std::ostream& os, const Mata::IntermediateAut& inter_aut);
}

#endif //_MATA_INTER_AUT_HH
