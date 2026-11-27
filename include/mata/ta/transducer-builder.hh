/** @file
 * @brief Reader for explicit structure-preserving ranked tree transducers.
 */

#ifndef MATA_TA_TRANSDUCER_BUILDER_HH
#define MATA_TA_TRANSDUCER_BUILDER_HH

#include "mata/parser/parser.hh"
#include "mata/ta/transducer.hh"

#include <filesystem>
#include <istream>
#include <string>

namespace mata::ta::transducer_builder {

using NameStateMap = utils::StringMap<State>;

/**
 * Builds a transducer from one @c \@TT-explicit section. Rules have the form @c a->b:q or
 * @c f(q1,q2)->g:p. @c %Final declares final root states; @c %Initial is accepted for compatibility with the format
 * and its names are registered as states. An empty @c %Alphabet-auto directive selects on-the-fly symbol naming.
 * The same alphabet and state-name conventions as @c ta::builder are used.
 */
TreeTransducer
	construct(const parser::ParsedSection& section, Alphabet* alphabet = nullptr, NameStateMap* state_map = nullptr);
TreeTransducer
	parse_from_mata(const std::string& input, Alphabet* alphabet = nullptr, NameStateMap* state_map = nullptr);
inline TreeTransducer
	parse_from_mata(const char* input, Alphabet* alphabet = nullptr, NameStateMap* state_map = nullptr) {
	return parse_from_mata(std::string{input}, alphabet, state_map);
}
TreeTransducer parse_from_mata(std::istream& input, Alphabet* alphabet = nullptr, NameStateMap* state_map = nullptr);
TreeTransducer parse_from_mata(
	const std::filesystem::path& input, Alphabet* alphabet = nullptr, NameStateMap* state_map = nullptr
);

} // namespace mata::ta::transducer_builder

#endif // MATA_TA_TRANSDUCER_BUILDER_HH
