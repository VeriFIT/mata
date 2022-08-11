//
// Created by Martin Hruska on 09.08.2022.
//

#ifndef LIBMATA_INTER_AUT_HH
#define LIBMATA_INTER_AUT_HH

#include <mata/parser.hh>

#include <string>
#include <vector>

namespace Mata
{

struct FormulaNode {
public:
    enum OperandType {
        SYM,
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
    };

    Type type;
    std::string raw;
    std::string name; // parsed name. When type marking is used, markers are removed.
    OperatorType operator_type;
    OperandType operand_type;

    bool is_operand() { return type == Type::OPERAND; }

    bool is_operator() { return type == Type::OPERATOR; }

    bool is_righpar() { return type == Type::RIGHT_PARENTHESIS; }

    bool is_leftpar() { return type == Type::LEFT_PARENTHESIS; }

    FormulaNode(Type t, std::string raw, std::string name,
                OperatorType oprtor) : type(t), raw(raw), name(name), operator_type(oprtor),
                                       operand_type(NOT_OPERAND) {}

    FormulaNode(Type t, std::string raw, std::string name,
                OperandType operand) : type(t), raw(raw), name(name), operator_type(NOT_OPERATOR),
                                       operand_type(operand) {};

    FormulaNode(Type t, std::string raw) : type(t), raw(raw), name(raw), operator_type(NOT_OPERATOR),
                                           operand_type(NOT_OPERAND) {};
};

struct FormulaGraph
{
    FormulaNode node;
    std::vector<FormulaGraph> children;

    FormulaGraph(FormulaNode n) : node(n), children() {}
};

struct InterAutomaton
{
    enum AutomatonType
    {
        NFA,
        AFA
    };

    enum Naming
    {
        AUTO,
        MARKER,
        ENUM
    };

    enum AlphabetType
    {
        EXPLICIT,
        BITVECTOR,
        CLASS,
        INTERVALS
    };

public:
    Naming state_naming;
    Naming symbol_naming;
    Naming node_naming;
    AlphabetType alphabet_type;
    AutomatonType automaton_type;

    // For case of enumeration
    std::vector<std::string> states_names;
    std::vector<std::string> symbols_names;
    std::vector<std::string> nodes_names;

    struct std::vector<std::pair<FormulaNode, FormulaGraph>> transitions;

    static std::vector<InterAutomaton> parse_from_mf(const Mata::Parser::Parsed& parsed);

    bool states_enumerated() {return state_naming == Naming::ENUM;}
    bool symbols_enumerated() {return state_naming == Naming::ENUM;}
    bool nodes_enumerated() {return state_naming == Naming::ENUM;}
};

} /* Mata */
#endif //LIBMATA_INTER_AUT_HH
