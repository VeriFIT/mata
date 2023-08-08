// TODO: Insert file header.

#ifndef MATA_TESTS_PERFORMANCE_UTILS_HH
#define MATA_TESTS_PERFORMANCE_UTILS_HH

#include "mata/parser/inter-aut.hh"
#include "mata/nfa/nfa.hh"
#include "mata/nfa/plumbing.hh"
#include "mata/nfa/algorithms.hh"
#include "mata/parser/mintermization.hh"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <string>
#include <cstring>

using namespace Mata::Nfa;

/**
 * @brief Load automaton from file at @p filename into @p aut, using @p alphabet for symbols on transitions.
 *
 * @param[in] filename Path to the file with automaton to load.
 * @param[out] aut Automaton instance to load into.
 * @param[out] alphabet Alphabet to use for symbols on transitions.
 * @param[in] skip_mintermization Whether to skip mirmization of the loaded automaton.
 * @param[in] aut_name Automaton name to print in logging outputs.
 * @return 0 if loading the automaton succeeded. Otherwise value != 0 if loading failed.
 */ 
int load_automaton(const std::string& filename, Nfa& aut, Mata::OnTheFlyAlphabet& alphabet,
                   const bool skip_mintermization = false, const std::string& aut_name = "unnamed");

#endif // MATA_TESTS_PERFORMANCE_UTILS_HH.
