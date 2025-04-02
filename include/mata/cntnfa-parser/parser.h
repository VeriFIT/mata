#ifndef MATA_CNTNFA_PARSER_HH
#define MATA_CNTNFA_PARSER_HH

#include <sstream>
#include <unordered_set>
#include <string>

#include "mata/utils/ord-vector.hh"

namespace mata::cntnfa {

struct ParsedTransition {
    std::string source;
    std::string symbol;
    std::string target;
    utils::OrdVector<std::string> annotations;

    ParsedTransition()
        : source(), symbol(), target(), annotations() {}

    bool operator<(const ParsedTransition& other) const {
        if (source != other.source) return source < other.source;
        if (symbol != other.symbol) return symbol < other.symbol;
        if (target != other.target) return target < other.target;
        return annotations < other.annotations;
    }

    bool operator==(const ParsedTransition& other) const {
        return source == other.source &&
               symbol == other.symbol &&
               target == other.target &&
               annotations == other.annotations;
    }

    bool operator!=(const ParsedTransition& other) const {
        return !(*this == other);
    }
};

struct ParsedNfa {
    std::string sectionType;
    std::unordered_set<std::string> states;
    std::unordered_set<std::string> alphabet;
    std::unordered_set<std::string> initialStates;
    std::unordered_set<std::string> finalStates;
    std::unordered_set<std::string> registers;
    utils::OrdVector<ParsedTransition> transitions;

    ParsedNfa()
        : sectionType(),
          states(),
          alphabet(),
          initialStates(),
          finalStates(),
          registers(),
          transitions() {}
};

void parseLineIntoSet(std::istringstream& iss, std::unordered_set<std::string>& targetSet);

ParsedTransition parseFlexibleTransition(const std::string& line);

ParsedNfa parseNfaFromFile(const std::string& filename);

}; // namespace mata::cntnfa

#endif /* MATA_CNTNFA_PARSER_HH */
