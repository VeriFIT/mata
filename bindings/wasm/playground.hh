/** @file
 * @brief The engine of the Mata playground on the website: it builds automata from regular expressions and from the
 *  .mata format, applies an operation or a decision procedure to them, and describes the outcome in JSON.
 */

#ifndef MATA_PLAYGROUND_HH
#define MATA_PLAYGROUND_HH

#include <string>

namespace mata::playground {

/**
 * @brief A request of the playground.
 *
 * Every member is text, so that the request crosses over from JavaScript as a plain object.
 */
struct Request {
	std::string a_format; ///< Format of automaton A: "regex" or "mata".
	std::string a_text; ///< Automaton A.
	std::string b_format; ///< Format of automaton B: "regex" or "mata".
	std::string b_text; ///< Automaton B; ignored by the operations on A alone.
	/// One of "show", "determinize", "minimize", "reduce", "trim", "complement", "reverse", "remove_epsilon",
	///  "intersection", "union", "concatenation", "difference", "is_empty", "is_universal", "is_included",
	///  "are_equivalent", and "is_in_lang".
	std::string operation;
	std::string word; ///< The word for "is_in_lang": the names of its symbols, each on its own line.
	/// The alphabet of "complement" and "is_universal": "used" for the symbols occurring in A, "bytes" for all 256
	/// bytes
	///  and the symbols occurring in A.
	std::string alphabet;
};

/**
 * @brief Evaluate @p request.
 *
 * A byte b is the symbol b. Its name is the character itself for the printable ASCII characters that can stand
 *  unquoted in the .mata format, and \\xHH otherwise. Automata built from regular expressions are over bytes, and a
 *  name of a byte in the .mata format means that byte, so the two formats can be combined.
 *
 * @return A JSON object. On success, it has the members
 *  - "ok": true,
 *  - "symbols": the names of the symbols, which the automata and words refer to by their indices,
 *  - "automata": the automata "a", "b" (when the operation uses it), and "result" (for an operation that builds an
 *     automaton), each with the members "states" (the number of states), "initial", "final", "transitionCount",
 *     "transitions" (a list of [source, symbol index, target], where -1 is ε; left out for large automata),
 *     "deterministic" (false for an automaton with ε-transitions), "stateNames" (for an input in the .mata format),
 * "mata", and "mataTruncated",
 *  - "check" (for a decision procedure): "holds", and "word", a word that shows why the answer is no, or null,
 *  - "witnessInA" (for "are_equivalent" that does not hold): whether the word is in A, rather than in B,
 *  - "epsilonRemoved": the names of the automata ("a", "b") whose ε-transitions were removed before the operation,
 *  - "milliseconds": the time the operation took.
 *  On failure, it has the members "ok": false, "error" (the message), and "source": "a", "b", "word", or "operation".
 */
std::string evaluate(const Request& request);

/// The Git revision Mata was built from.
std::string revision();

} // namespace mata::playground

#endif // MATA_PLAYGROUND_HH
