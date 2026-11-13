/* parser.hh -- Mata Format (MF) parser.
 */

#ifndef MATA_PARSER_HH_
#define MATA_PARSER_HH_

#include <cstddef>
#include <cstring>
#include <list>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace mata {

/**
 * An append-only storage of names: the tokens of a parsed section and the formula nodes of an intermediate automaton
 *  keep views into it instead of strings of their own.
 * The bytes handed out are never moved, so a view stays valid for as long as the storage lives. Copies of
 *  @c parser::ParsedSection and of @c IntermediateAut share the storage, which keeps the views of the copies valid.
 */
class NameStore {
  public:
	/**
	 * @brief Copies @p name into the storage.
	 * @return A view of the copy, valid for as long as the storage lives.
	 */
	std::string_view store(const std::string_view name) {
		if (chunks_.empty() || chunks_.back().capacity - used_ < name.size()) {
			const std::size_t capacity{name.size() < chunk_capacity_ ? chunk_capacity_ : name.size()};
			chunks_.push_back(Chunk{.data = std::make_unique_for_overwrite<char[]>(capacity), .capacity = capacity});
			used_ = 0;
		}
		char* const stored{chunks_.back().data.get() + used_};
		std::memcpy(stored, name.data(), name.size());
		used_ += name.size();
		return {stored, name.size()};
	}

  private:
	/// One block of the storage; the bytes of the blocks handed out earlier stay where they are.
	struct Chunk {
		std::unique_ptr<char[]> data{};
		std::size_t capacity{};
	};

	static constexpr std::size_t chunk_capacity_ = 16 * 1'024;

	std::vector<Chunk> chunks_{};
	/// The number of bytes used in the last chunk.
	std::size_t used_{};
};

} // namespace mata

/**
 * Parser from `.mata` format to automata (currently `Nfa` and `Nft` are supported).
 *
 * This includes parsing either from files or from other streams (strings, etc.).
 */
namespace mata::parser {

using KeyListStore = std::map<std::string, std::vector<std::string>>;

/**
 * One token of a body line, with the flag telling whether the reader read it in quotes.
 * @c create_node() lets a quoted token be an operand even when it starts with @c & @c | or @c !, which the
 * unquoted reader would take for an operator of the transition formula.
 * The token is a view: @c parse_mf() points it into the name storage of its section, other code has to keep the
 *  bytes alive for as long as the token is used.
 */
struct BodyToken {
	std::string_view token{};
	bool quoted{false};

	BodyToken() = default;
	BodyToken(const std::string_view token, const bool quoted) : token{token}, quoted{quoted} {}
	/// An unquoted token, so that a body line reads as a list of plain names in tests and call sites.
	BodyToken(const char* token) : token{token} {}
	// A view of a temporary string would dangle.
	BodyToken(std::string&&, bool) = delete;
	bool operator==(const BodyToken& rhs) const = default;
};

inline std::ostream& operator<<(std::ostream& os, const BodyToken& token) { return os << token.token; }

using BodyLine = std::vector<BodyToken>;

/** Parsed data (single section) */
struct ParsedSection {
	std::string type{};
	KeyListStore dict{};
	std::vector<BodyLine> body{};
	/// The storage the tokens of @c body point into; copies of the section share it.
	std::shared_ptr<NameStore> name_store{std::make_shared<NameStore>()};

	ParsedSection() = default;

	/// Is the section empty?
	bool empty() const { return type.empty() && dict.empty() && body.empty(); }

	/// Equality operator
	bool operator==(const ParsedSection& rhs) const;

	/// subscript operator for the key-value store
	const std::vector<std::string>& operator[](const std::string& key) const;

	/// check whether the key-value store contains a key
	bool haskey(const std::string& key) const { return dict.contains(key); }
};

/** Parsed data */
using Parsed = std::vector<ParsedSection>;

/** Parses a string into an intermediary structure */
Parsed parse_mf(const std::string& input, bool keep_quotes = false);

/** Parses a stream into an intermediary structure */
Parsed parse_mf(std::istream& input, bool keep_quotes = false);

/** Parses one section from a stream into an intermediary structure */
ParsedSection parse_mf_section(std::istream& input, bool keep_quotes = false);

/** Parses one section from a string into an intermediary structure */
ParsedSection parse_mf_section(const std::string& input, bool keep_quotes = false);

/** Parses one section from the beginning of @p input into an intermediary structure, advancing @p input past it. */
ParsedSection parse_mf_section_view(std::string_view& input, bool keep_quotes = false);

/// registers dispatcher
void init();

} // namespace mata::parser

namespace std {
/// Output stream operator
std::ostream& operator<<(std::ostream& os, const mata::parser::ParsedSection& parsec);
} // namespace std

#endif /* MATA_PARSER_HH_ */
