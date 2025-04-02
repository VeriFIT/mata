/* parser.cc -- implementation of MATA format parser (for CNTNFA) */

#include <fstream>

#include "mata/cntnfa-parser/parser.h"

namespace mata::cntnfa {

void parseLineIntoSet(std::istringstream& iss, std::unordered_set<std::string>& targetSet) {
    std::string token;
    while (iss >> token) {
        targetSet.insert(token);
    }
}

ParsedTransition parseFlexibleTransition(const std::string& line) {
    std::istringstream iss(line);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }

    ParsedTransition transition;
    if (tokens.size() < 3) {
        throw std::runtime_error("Invalid transition format: " + line);
    }

    transition.source = tokens.front();
    transition.target = tokens.back();

    // Everything in between is considered a symbol or annotation
    for (size_t i = 1; i < tokens.size() - 1; ++i) {
        if (tokens[i].rfind("t(", 0) == 0) {
            transition.annotations.push_back(tokens[i]);
        } else {
            if (!transition.symbol.empty()) {
                throw std::runtime_error("Multiple symbols in one transition are not allowed: " + line);
            }
            transition.symbol = tokens[i];
        }
    }

    return transition;
}

ParsedNfa parseNfaFromFile(const std::string& filename) {
    ParsedNfa nfa;
    std::ifstream file(filename);
    std::string line, buffer;

    while (std::getline(file, line)) {
        size_t commentPos = line.find('#');
        if (commentPos != std::string::npos) {
            line = line.substr(0, commentPos);
        }

        while (!line.empty() && line.back() == '\\') {
            buffer += line.substr(0, line.length() - 1);
            std::getline(file, line);
        }
        line = buffer + line;
        buffer.clear();

        if (line.empty()) {
            continue;
        }

        std::istringstream iss(line);

        if (line[0] == '@') {
            nfa.sectionType = line.substr(1);
        } else if (line[0] == '%') {
            std::string key;
            iss >> key;
            key = key.substr(1);
            if (key == "States-enum") {
                parseLineIntoSet(iss, nfa.states);
            } else if (key == "Alphabet-enum") {
                parseLineIntoSet(iss, nfa.alphabet);
            } else if (key == "Initial") {
                parseLineIntoSet(iss, nfa.initialStates);
            } else if (key == "Final") {
                parseLineIntoSet(iss, nfa.finalStates);
            } else if (key == "Registers") {
                parseLineIntoSet(iss, nfa.registers);
            }
        } else {
            try {
                nfa.transitions.push_back(parseFlexibleTransition(line));
            } catch (const std::exception& e) {
                std::cerr << "Error parsing transition: " << e.what() << "\n";
            }
        }
    }

    return nfa;
}

}; // namespace mata::cntnfa
