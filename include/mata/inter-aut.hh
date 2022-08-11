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

struct FormulaNode
{
private:
    enum OperandType
    {
        SYM,
        STATE,
        NODE
    };

    enum OperatorType
    {
        NEG,
        AND,
        OR
    };

    enum Type
    {
        OPERAND,
        OPERATOR
    };

    std::string raw;
    std::string name; // parsed name. When type marking is used, markers are removed.
    Type type;
    OperandType operand_type;
    OperatorType operator_type;

public:
    bool is_operand() { return type == Type::OPERAND;}
    bool is_operator() { return type == Type::OPERATOR;}
};

struct FormulaGraph
{
    FormulaNode node;
    std::vector<FormulaNode> children;
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

    struct std::vector<FormulaGraph> transitions;

    static std::vector<InterAutomaton> parse_from_mf(const Mata::Parser::Parsed& parsed);

    bool states_enumerated() {return state_naming == Naming::ENUM;}
    bool symbols_enumerated() {return state_naming == Naming::ENUM;}
    bool nodes_enumerated() {return state_naming == Naming::ENUM;}
};

} /* Mata */
#endif //LIBMATA_INTER_AUT_HH
