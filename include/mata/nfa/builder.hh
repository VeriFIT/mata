/** @file
 * @brief Functions to build predefined types of NFAs, to create them from regular expressions, and to load them from
 *  files.
 */

#ifndef LIBMATA_BUILDER_HH
#define LIBMATA_BUILDER_HH

#include "mata/parser/inter-aut.hh"
#include "mata/parser/re2parser.hh"
#include "nfa.hh"

#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

/**
 * Namespace providing options to build NFAs.
 */
namespace mata::nfa::builder {

using namespace mata::nfa;

/// Finds a @c std::string_view name without copying it.
using NameStateMap = utils::StringMap<State>;

/**
 * Create an automaton accepting only a single @p word.
 */
Nfa create_single_word_nfa(const std::vector<Symbol>& word);

/**
 * Create an automaton accepting only a single @p word.
 *
 * @param word Word to accept.
 * @param alphabet Alphabet to use in NFA for translating word into symbols. If specified, the alphabet has to contain
 *  translations for all of the word symbols. If left empty, a new alphabet with only the symbols of the word will be
 *  created.
 */
Nfa create_single_word_nfa(const std::vector<std::string>& word, std::shared_ptr<Alphabet> alphabet = nullptr);

/**
 * Create automaton accepting only epsilon string.
 */
Nfa create_empty_string_nfa();

/**
 * Create automaton accepting sigma star over the passed alphabet.
 *
 * @param[in] alphabet Alphabet to construct sigma star automaton with. When alphabet is left empty, the default empty
 *  alphabet is used, creating an automaton accepting only the empty string.
 */
Nfa create_sigma_star_nfa(const std::shared_ptr<Alphabet>& alphabet = std::make_shared<OnTheFlyAlphabet>());

/**
 * Creates Tabakov-Vardi random NFA.
 * The implementation is based on the paper "Experimental Evaluation of Classical Automata Constructions" by Tabakov and
 * Vardi.
 *
 * @param num_of_states Number of states in the automaton.
 * @param alphabet_size Size of the alphabet.
 * @param states_transitions_ratio_per_symbol Ratio between number of transitions and number of states for each symbol.
 *  The value must be in range [0, num_of_states]. A value of 1 means that there will be num_of_states transitions for
 * each symbol. A value of num_of_states means that there will be a transition between every pair of states for each
 * symbol.
 * @param final_state_density Density of final states in the automaton. The value must be in range [0, 1]. The state 0
 * is always final. If the density is 1, every state will be final.
 * @param seed Seed for the PRNG used. If no seed is given, the algorithm chooses one uniformly at random.
 */
Nfa create_random_nfa_tabakov_vardi(
	size_t num_of_states,
	size_t alphabet_size,
	double states_transitions_ratio_per_symbol,
	double final_state_density,
	const std::optional<unsigned int>& seed = std::nullopt
);

/**
 * Loads an automaton from a single parsed section of the mata format.
 *
 * Unlike the @c IntermediateAut overload below, this one reads @p parsec verbatim: `%Initial` and `%Final` are taken
 *  as plain lists of state names and a body line is always a `source symbol target` triple. State names are therefore
 *  arbitrary strings, not subject to the `q`/`a`/`n` naming markers or to the Boolean formulas that
 *  @c IntermediateAut parses; symbol names are still resolved through @p alphabet, which decides which of them are
 *  valid. The two overloads consequently accept different inputs and cannot be merged: routing this one through
 *  @c IntermediateAut would subject the state names to the naming mode of the section (with the default `%States`
 *  marked naming rejecting every name not starting with `q`) and would renumber states, as the @c IntermediateAut
 *  path collects names from an unordered set.
 */
Nfa construct(const mata::parser::ParsedSection& parsec, Alphabet* alphabet, NameStateMap* state_map = nullptr);

/** Loads an automaton from Parsed object */
Nfa construct(const mata::IntermediateAut& inter_aut, Alphabet* alphabet, NameStateMap* state_map = nullptr);
/** Loads an automaton from Parsed object; version for python binding */
void construct(
	Nfa* result, const mata::IntermediateAut& inter_aut, Alphabet* alphabet, NameStateMap* state_map = nullptr
);

template <class ParsedObject>
Nfa construct(const ParsedObject& parsed, Alphabet* alphabet = nullptr, NameStateMap* state_map = nullptr) {
	OnTheFlyAlphabet tmp_alphabet{};
	if (!alphabet) { alphabet = &tmp_alphabet; }
	return construct(parsed, alphabet, state_map);
} // construct().

/**
 * Reads an NFA from the `.mata` format in @p nfa_in_mata with the fast reader of explicit alphabets.
 *
 * The reader skips the intermediate representations of the general reader (@c mata::parser::parse_mf() and
 *  @c IntermediateAut), and reads only the inputs whose transitions are `source symbol target` triples and whose
 *  `%Initial` and `%Final` lines list states. For those, it returns the NFA @c parse_from_mata() has always returned,
 *  except that the initial states are numbered in the order of the input.
 *
 * @param nfa_in_mata Input with a single `@NFA` or `@NFA-explicit` section.
 * @param alphabet Alphabet translating the symbol names; @c nullptr reads them as numbers (@c IntAlphabet).
 * @param state_map If not @c nullptr, receives the map from state names to states, see @c construct().
 * @param keep_quotes Whether quoted names of body lines keep their quotes, see @c mata::parser::parse_mf().
 * @return The NFA, or @c std::nullopt when @p nfa_in_mata needs the general reader, as do Boolean formulas, bitvector
 *  alphabets, enumerated namings, and malformed inputs. @p state_map is then left unchanged, while @p alphabet may have
 *  learned symbols of the lines read before.
 */
std::optional<Nfa> parse_explicit_from_mata(
	std::string_view nfa_in_mata,
	Alphabet* alphabet = nullptr,
	NameStateMap* state_map = nullptr,
	bool keep_quotes = false
);

/**
 * Reads NFAs with bitvector alphabets (`@NFA-bits`) over one shared set of minterms, as the general reader followed by
 *  @c Mintermization::mintermize() of all of them together does, but without the intermediate representations.
 *
 * Transitions whose formulas are the usual Boolean combinations of symbols are read directly into BDDs. Any other
 *  `@NFA-bits` input goes through the general reader inside @c add(), so that all added automata still share one
 *  partition. The NFAs are the ones @c parse_from_mata() has always returned, except that the initial and the listed
 *  final states are numbered in the order of the input.
 */
class BitvectorNfaReader {
  public:
	BitvectorNfaReader();
	BitvectorNfaReader(BitvectorNfaReader&&) noexcept;
	BitvectorNfaReader& operator=(BitvectorNfaReader&&) noexcept;
	~BitvectorNfaReader();

	/// How @c add() took an input.
	enum class Read : std::uint8_t {
		/// Not a single NFA section with a bitvector alphabet; nothing of it is kept.
		Rejected,
		/// Read directly into BDDs.
		Directly,
		/// Read by the general reader, as the formulas leave the shapes read directly.
		ByGeneralReader,
	};

	/**
	 * Reads @p nfa_in_mata, an input with a single NFA section with a bitvector alphabet.
	 * @param keep_quotes Whether quoted names of body lines keep their quotes, see @c mata::parser::parse_mf().
	 * @param state_map If not @c nullptr, the map from state names to states of this automaton, see @c construct().
	 *  @c build() reads and fills it, so it has to outlive the call of @c build().
	 * @throws std::runtime_error A bitvector NFA the general reader rejects; nothing of it is kept.
	 */
	Read add(std::string_view nfa_in_mata, bool keep_quotes = false, NameStateMap* state_map = nullptr);

	/**
	 * Builds the NFAs of all the inputs @c add() kept, in the order they were added. Their symbols are the minterms of
	 *  the predicates of all transitions of all of them, named by their numbers and translated by @p alphabet.
	 * @param alphabet Alphabet translating the minterm names; @c nullptr reads them as numbers (@c IntAlphabet).
	 */
	std::vector<Nfa> build(Alphabet* alphabet = nullptr);

  private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

/**
 * Parse NFA from the mata format in a string.
 *
 * Reads the input with @c parse_explicit_from_mata() when it can, and with the general reader otherwise. A bitvector
 *  alphabet is mintermized.
 *
 * @param nfa_in_mata String containing NFA in mata format.
 * @param alphabet Alphabet translating the symbol names; @c nullptr reads them as numbers (@c IntAlphabet).
 * @param state_map If not @c nullptr, receives the map from state names to states, see @c construct().
 * @param keep_quotes Whether quoted names of body lines keep their quotes, see @c mata::parser::parse_mf().
 * @throws std::runtime_error Parsing of NFA fails.
 */
Nfa parse_from_mata(
	const std::string& nfa_in_mata,
	Alphabet* alphabet = nullptr,
	NameStateMap* state_map = nullptr,
	bool keep_quotes = false
);

/**
 * Parse NFA from the mata format in an input stream.
 *
 * @param nfa_stream Input stream containing NFA in mata format.
 * @see parse_from_mata(const std::string&, Alphabet*, NameStateMap*, bool) for the other parameters.
 * @throws std::runtime_error Parsing of NFA fails.
 */
Nfa parse_from_mata(
	std::istream& nfa_stream, Alphabet* alphabet = nullptr, NameStateMap* state_map = nullptr, bool keep_quotes = false
);

/**
 * Parse NFA from the mata format in a file.
 *
 * @param nfa_file Path to the file containing NFA in mata format.
 * @see parse_from_mata(const std::string&, Alphabet*, NameStateMap*, bool) for the other parameters.
 * @throws std::runtime_error @p nfa_file does not exist.
 * @throws std::runtime_error Parsing of NFA fails.
 */
Nfa parse_from_mata(
	const std::filesystem::path& nfa_file,
	Alphabet* alphabet = nullptr,
	NameStateMap* state_map = nullptr,
	bool keep_quotes = false
);

/**
 * @brief Create NFA from @p regex
 *
 * The created NFA does not contain epsilons and is trimmed; with @p reduce, it is also reduced by simulation reduction.
 * It uses the parser from RE2, see mata/parser/re2praser.hh for more
 * details and options.
 *
 * At https://github.com/google/re2/wiki/Syntax, you can find the syntax
 * of @p regex with following further limitations:
 *  1) The allowed characters are the first 256 characters of Unicode,
 *     i.e., Latin1 encoding (ASCII + 128 more characters). For the
 *     full Unicode, check mata/parser/re2praser.hh.
 *  2) The created automaton represents the language of the regex and
 *     is not expected to be used in regex matching. Therefore, stuff
 *     like ^, $, \b, etc. are ignored in the regex. For example, the
 *     regular expressions a*b and ^a*b will both result in the same
 *     NFA accepting the language of multiple 'a's followed by one 'b'.
 *     See also issue #494.
 *
 * @sa mata::parser::create_nfa()
 *
 * @param regex regular expression
 * @param reduce whether to reduce the NFA using simulation reduction; costly for large regexes
 */
Nfa create_from_regex(const std::string& regex, bool reduce = true);

/**
 * @brief Create NFA from @p regex with the conversion and post-processing steps chosen by @p options
 *
 * @sa create_from_regex(const std::string&, bool) for the syntax and mata::parser::create_nfa() for the options.
 *
 * @param regex regular expression
 * @param options conversion and post-processing options
 */
Nfa create_from_regex(const std::string& regex, const parser::Re2Options& options);

} // namespace mata::nfa::builder.

#endif // LIBMATA_BUILDER_HH
