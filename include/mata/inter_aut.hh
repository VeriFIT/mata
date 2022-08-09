//
// Created by Martin Hruska on 09.08.2022.
//

#ifndef LIBMATA_INTER_AUT_HH
#define LIBMATA_INTER_AUT_HH

#include <string>
#include <vector>

namespace Mata
{

struct FormulaNode
{
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
};

struct FormulaGraph
{
    FormulaNode node;
    std::vector<FormulaNode> children;
};

struct InterAutomaton
{
private:
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
        CLASS
    };

    Naming state_naming;
    Naming symbol_naming;
    Naming enum_naming;
    AlphabetType alphabet_type;

public:
    struct FormulaGraph transition_formula;
};

} /* Mata */
#endif //LIBMATA_INTER_AUT_HH
