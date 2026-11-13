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
 * The states the readers give to the names they meet first, given the names the caller mapped to states beforehand.
 *  A mapped name gets its state allocated when used. A new name gets the lowest state that the automaton does not have
 *  yet and no mapped name has, so that it never shares the state of a mapped name, used later or not at all.
 */
class NewStates {
  public:
	using State = nfa::State;

	/// @param mapped The names the caller mapped.
	explicit NewStates(const utils::StringMap<State>& mapped);

	/// The state of a new name in an automaton of @p num_of_states states; @p num_of_states never decreases between
	///  calls, as the caller allocates each state returned.
	State next(size_t num_of_states);

  private:
	/// The states of the mapped names, ascending.
	std::vector<State> mapped_{};
	/// The number of states of @c mapped_ not above the last state returned.
	size_t passed_{0};
};

/**
 * The map from state names to states of the readers, which behaves as the @c std::unordered_map it is built from and
 *  turns into, but finds the names of printed automata, a fixed prefix followed by a number, in a vector.
 */
class StateNameMap {
  public:
	using State = nfa::State;
	using Map = utils::StringMap<State>;

	/// @param names The initial names. @param max_number The largest number kept in the vector, which bounds its size.
	StateNameMap(const Map& names, size_t max_number);

	/// The state of a new name in an automaton of @p num_of_states states, see @c NewStates.
	State new_state(const size_t num_of_states) { return new_states_.next(num_of_states); }

	/// The state named @p name, if any.
	std::optional<State> find(std::string_view name);

	/// Maps @p name, which has no state yet, to @p state.
	void insert(std::string_view name, State state);

	/// All the names and their states.
	Map to_map() const;

	/// Calls @p function with every name and its state.
	template <class Function> void for_each(Function&& function) const {
		for (const auto& [name, state] : others_) { function(std::string_view{name}, state); }
		std::string name{prefix_.value_or("")};
		const size_t prefix_size{name.size()};
		for (size_t number{0}; number < numbered_.size(); ++number) {
			if (numbered_[number] == NO_STATE) { continue; }
			name.resize(prefix_size);
			name += std::to_string(number);
			function(std::string_view{name}, numbered_[number]);
		}
	}

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
	NewStates new_states_;
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
 * The reader covers only lines that are lists of names, and in the formula mode also the operators and parentheses of
 *  transition formulas (`&`, `|`, `!`, `(`, `)`), which it splits off as tokens of their own, as @c parse_mf() does.
 *  It gives up on anything else @c parse_mf() would turn into an operator, on a second section, on a line continuation
 *  left open at the end of the input, and on every input @c parse_mf() rejects. Giving up is reported as
 *  @c Line::Unsupported, so that the caller can hand the input to the general reader, which then either reads it or
 *  reports the error.
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

	/// @p input has to outlive the reader and the tokens it returns. @p formulas Whether to read the operators and
	///  parentheses of transition formulas.
	explicit ExplicitSectionReader(std::string_view input, const bool formulas = false)
		: input_{input},
		  formulas_{formulas} {}

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
	bool formulas_;
	size_t position_{0};
	std::string_view key_{};
	std::vector<ExplicitToken> tokens_{};
	/// Storage of the quoted tokens whose escapes had to be resolved; a deque keeps the views into it valid.
	std::deque<std::string> unescaped_{};
};

} // namespace mata::parser

#endif // MATA_EXPLICIT_SECTION_READER_HH_
