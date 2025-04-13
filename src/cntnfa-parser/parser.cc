/* parser.cc -- implementation of MATA format parser (for CNTNFA) */

#include <fstream>

#include "mata/cntnfa-parser/parser.hh"

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
            line = line.substr(0, commentPos); // Remove comment part
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
            nfa.sectionType = line.substr(1); // @NFA-explicit
        } else if (line[0] == '%') {
            std::string key;
            iss >> key;
            key = key.substr(1); // Remove '%'
            if (key == "States-enum") { // %States-enum q0 q1 q2 ...
                parseLineIntoSet(iss, nfa.states);
            } else if (key == "Alphabet-enum") { // %Alphabet-enum a b c ...
                parseLineIntoSet(iss, nfa.alphabet);
            } else if (key == "Initial") { // %Initial q0 q2 ...
                parseLineIntoSet(iss, nfa.initialStates);
            } else if (key == "Final") { // %Final q3 ...
                parseLineIntoSet(iss, nfa.finalStates);
            } else if (key == "Registers") { // %Registers c0 c1 ...
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

ParsedNfa parseNfaFromString(const std::string& content) {
    ParsedNfa nfa;
    std::istringstream iss(content);
    std::string line, buffer;

    while (std::getline(iss, line)) {
        size_t commentPos = line.find('#');
        if (commentPos != std::string::npos) {
            line = line.substr(0, commentPos); // Remove comment part
        }

        while (!line.empty() && line.back() == '\\') {
            buffer += line.substr(0, line.length() - 1);
            std::getline(iss, line);
        }
        line = buffer + line;
        buffer.clear();

        if (line.empty()) {
            continue;
        }

        std::istringstream line_iss(line);

        if (line[0] == '@') {
            nfa.sectionType = line.substr(1); // @NFA-explicit
        } else if (line[0] == '%') {
            std::string key;
            line_iss >> key;
            key = key.substr(1); // Remove '%'
            if (key == "States-enum") { // %States-enum q0 q1 q2 ...
                parseLineIntoSet(line_iss, nfa.states);
            } else if (key == "Alphabet-enum") { // %Alphabet-enum a b c ...
                parseLineIntoSet(line_iss, nfa.alphabet);
            } else if (key == "Initial") { // %Initial q0 q2 ...
                parseLineIntoSet(line_iss, nfa.initialStates);
            } else if (key == "Final") { // %Final q3 ...
                parseLineIntoSet(line_iss, nfa.finalStates);
            } else if (key == "Registers") { // %Registers c0 c1 ...
                parseLineIntoSet(line_iss, nfa.registers);
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

Nfa convertParsedNfaToNfa(const ParsedNfa& parsedNfa) {
    Nfa nfa;

    // Prepare state mapping
    std::unordered_map<std::string, State> state_mapping;
    State next_state_id = 0;

    // Helper function to get or insert state
    // Returns the state ID for the given name, inserting it if it doesn't exist
    auto get_or_insert_state = [&](const std::string& name) {
        if (state_mapping.contains(name))
            return state_mapping[name];
        State id = next_state_id++;
        state_mapping[name] = id;
        nfa.add_state(id);
        return id;
    };

    // Insert all states
    for (const auto& state : parsedNfa.states) {
        get_or_insert_state(state);
    }

    // Insert initial and final states
    for (const auto& state : parsedNfa.initialStates)
        nfa.initial.insert(get_or_insert_state(state));

    for (const auto& state : parsedNfa.finalStates)
        nfa.final.insert(get_or_insert_state(state));

    // Insert counters (registers)
    size_t counter_id = 0;
    for (const auto& reg_name : parsedNfa.registers) {
        (void)reg_name;
        nfa.counter_set.insert_counter(CounterRegister(0), counter_id++);
    }

    // Setup alphabet
    auto* alphabet = new OnTheFlyAlphabet();
    for (const auto& sym : parsedNfa.alphabet) {
        alphabet->translate_symb(sym);
    }
    nfa.alphabet = alphabet;

    // Process transitions
    for (const auto& t : parsedNfa.transitions) {
        State src = get_or_insert_state(t.source);
        State tgt = get_or_insert_state(t.target);
        Symbol symbol = nfa.alphabet->translate_symb(t.symbol);

        size_t annotation_id = nfa.annotation_collection.size();
        nfa.annotation_collection.allocate(annotation_id + 1);

        for (const auto& ann_str : t.annotations) {
            if (ann_str.starts_with("t(")) {
                size_t eq_pos = ann_str.find("==");
                size_t plus_pos = ann_str.find("+=");
    
                // Check for counter increment or test
                if (plus_pos != std::string::npos) {
                    size_t cid = std::stoul(ann_str.substr(2, plus_pos - 2));

                    if (cid >= nfa.counter_set.size()) {
                        throw std::runtime_error("Counter ID out of range in annotation: " + ann_str);
                    }

                    int inc_val = std::stoi(ann_str.substr(plus_pos + 2, ann_str.size() - plus_pos - 3));
                    nfa.annotation_collection.insert_annotation(CounterIncrement(cid, inc_val), annotation_id);
                } else if (eq_pos != std::string::npos) {
                    size_t cid = std::stoul(ann_str.substr(2, eq_pos - 2));

                    if (cid >= nfa.counter_set.size()) {
                        throw std::runtime_error("Counter ID out of range in annotation: " + ann_str);
                    }

                    int expected_val = std::stoi(ann_str.substr(eq_pos + 1, ann_str.size() - eq_pos - 2));
                    nfa.annotation_collection.insert_annotation(CounterTest(cid, expected_val), annotation_id);
                } else {
                    throw std::runtime_error("Unknown annotation format: " + ann_str);
                }
            }
        }

        // Create transition with annotation state
        TargetSet target_set;
        target_set.push_back(Target(tgt, annotation_id));

        nfa.delta.add(src, symbol, target_set);
    }

    return nfa;
}

}; // namespace mata::cntnfa
