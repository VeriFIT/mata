/** @file
 * @brief Fast reader of the `.mata` sections whose lines are plain lists of names.
 *
 * The general reader, @c parse_mf() followed by @c IntermediateAut::parse_from_mf(), handles the whole format, and
 *  pays for it with a Boolean formula tree per transition. The automata Mata supports mostly come as plain
 *  `source symbol target` lines, which this reader tokenizes in place, without any intermediate representation.
 */

#ifndef MATA_EXPLICIT_SECTION_READER_HH_
#define MATA_EXPLICIT_SECTION_READER_HH_

#include "mata/nfa/types.hh"

#include <cstdint>
#include <deque>
#include <istream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mata::parser {

/// Reads the rest of @p input, which the readers of the `.mata` format take as a whole.
std::string read_whole_stream(std::istream& input);

/**
 * The map from state names to states of the readers, which behaves as the @c std::unordered_map it is built from and
 *  turns into, but finds the names of printed automata, a fixed prefix followed by a number, in a vector.
 */
class StateNameMap {
  public:
	using State = nfa::State;
	using Map = std::unordered_map<std::string, State>;

	/// @param names The initial names. @param max_number The largest number kept in the vector, which bounds its size.
	StateNameMap(const Map& names, size_t max_number);

	/// The state named @p name, if any.
	std::optional<State> find(std::string_view name);

	/// Maps @p name, which has no state yet, to @p state.
	void insert(std::string_view name, State state);

	/// All the names and their states.
	Map to_map() const;

  private:
	/// The number in @p name if @p name is the prefix of the numbered names followed by a number without leading zeros
	///  and not larger than @c max_number_. The first such name with any prefix fixes the prefix.
	std::optional<size_t> number_of(std::string_view name);

	static constexpr State NO_STATE{static_cast<State>(-1)};
	size_t max_number_;
	std::optional<std::string> prefix_{};
	/// The state of each numbered name, or @c NO_STATE.
	std::vector<State> numbered_{};
	Map others_{};
	std::string buffer_{};
};

/// One token of a line, as @c parse_mf() reads it.
struct ExplicitToken {
	/// The token without the enclosing quotes, with escapes resolved.
	std::string_view text{};
	/// Whether the token was quoted.
	bool quoted{false};
};

/**
 * Reads the single section of a `.mata` input line by line, tokenizing every line as @c parse_mf() does.
 *
 * The reader covers only lines that are lists of names: it gives up on anything @c parse_mf() would turn into an
 *  operator of a transition formula (`&`, `|`, `!`, `(`, `)`), on a second section, on a line continuation left open at
 *  the end of the input, and on every input @c parse_mf() rejects. Giving up is reported as @c Line::Unsupported, so
 *  that the caller can hand the input to the general reader, which then either reads it or reports the error.
 */
class ExplicitSectionReader {
  public:
	enum class Line : std::uint8_t {
		/// A `%KEY values...` line; see @c key() and @c tokens().
		Key,
		/// A body line; see @c tokens().
		Body,
		/// The end of the input.
		End,
		/// The input leaves the subset this reader covers.
		Unsupported,
	};

	/// @p input has to outlive the reader and the tokens it returns.
	explicit ExplicitSectionReader(std::string_view input) : input_{input} {}

	/// Reads the `@TYPE` line, skipping the comment lines before it.
	/// @return The section type, or @c std::nullopt when the input does not start with a well-formed type line.
	std::optional<std::string_view> read_type();

	/// Reads the next line, joining the lines continued by a trailing `\`.
	Line read_line();

	/// The key of the last @c Line::Key line, without the leading `%`.
	std::string_view key() const { return key_; }

	/// The values of the last @c Line::Key line, or the tokens of the last @c Line::Body line. Valid until the next
	///  call of @c read_line().
	const std::vector<ExplicitToken>& tokens() const { return tokens_; }

  private:
	/// Appends the tokens of one physical line to @c tokens_. @return Whether the line stays in the subset.
	bool tokenize(std::string_view line);

	std::string_view input_;
	size_t position_{0};
	std::string_view key_{};
	std::vector<ExplicitToken> tokens_{};
	/// Storage of the quoted tokens whose escapes had to be resolved; a deque keeps the views into it valid.
	std::deque<std::string> unescaped_{};
};

} // namespace mata::parser

#endif // MATA_EXPLICIT_SECTION_READER_HH_
