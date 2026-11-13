/** @file
 * @brief Printing an NFA as a regular expression.
 *
 * The automaton is converted by state elimination (Brzozowski and McCluskey): its transitions get labelled by regexes
 *  and its states are removed one by one, each time relabelling the transitions around the removed state, until one
 *  transition from a new initial state to a new final state is left. The size of the result depends on the order of the
 *  removals; the order follows the weights of Delgado and Morais, which estimate by how much the removal of a state
 *  makes the labels longer.
 */

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
#include <optional>
#include <ostream>
#include <queue>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "mata/alphabet.hh"
#include "mata/nfa/nfa.hh"
#include "mata/utils/utils.hh"

using namespace mata::nfa;
using mata::Symbol;

namespace {

/// A character of the regex: a Unicode code point.
using Char = std::uint32_t;
/// A regex stored in @c Regexes.
using Regex = std::uint32_t;
/// A number of repetitions.
using Count = std::uint32_t;

constexpr Char MAX_CHAR{0x10'FF'FF};
constexpr Char MIN_SURROGATE{0xD8'00};
constexpr Char MAX_SURROGATE{0xDF'FF};
constexpr Count UNBOUNDED{std::numeric_limits<Count>::max()};

bool is_unicode_scalar_value(const Char character) {
	return character <= MAX_CHAR && (character < MIN_SURROGATE || character > MAX_SURROGATE);
}
constexpr Regex NO_REGEX{std::numeric_limits<Regex>::max()};
constexpr std::uint64_t MAX_LENGTH{std::numeric_limits<std::uint64_t>::max()};
/// RE2 rejects a repetition count above 1000, also as the product of the counts of nested repetitions.
constexpr Count MAX_BRACED_COUNT{1'000};
/// The regex of the empty language: a character class without characters, as there is no syntax for it.
constexpr std::string_view EMPTY_LANGUAGE{R"([^\s\S])"};

std::uint64_t add_lengths(const std::uint64_t lhs, const std::uint64_t rhs) {
	return lhs > MAX_LENGTH - rhs ? MAX_LENGTH : lhs + rhs;
}

std::uint64_t multiply_length(const std::uint64_t length, const std::uint64_t times) {
	return times != 0 && length > MAX_LENGTH / times ? MAX_LENGTH : length * times;
}

/// The sum of two counts, unless it does not fit.
std::optional<Count> add_counts(const Count lhs, const Count rhs) {
	if (lhs == UNBOUNDED || rhs == UNBOUNDED) { return UNBOUNDED; }
	if (lhs >= UNBOUNDED - rhs) { return std::nullopt; }
	return lhs + rhs;
}

/// The product of two counts, unless it does not fit.
std::optional<Count> multiply_counts(const Count lhs, const Count rhs) {
	if (lhs == 0 || rhs == 0) { return 0; }
	if (lhs == UNBOUNDED || rhs == UNBOUNDED) { return UNBOUNDED; }
	if (lhs >= UNBOUNDED / rhs) { return std::nullopt; }
	return lhs * rhs;
}

unsigned num_of_digits(Count count) {
	unsigned digits{1};
	for (; count >= 10; count /= 10) { ++digits; }
	return digits;
}

struct CharRange {
	Char low;
	Char high;

	bool operator==(const CharRange&) const = default;
};

/// Sorts @p ranges and joins the ones that overlap or follow each other.
void normalize(std::vector<CharRange>& ranges) {
	std::ranges::sort(ranges, {}, &CharRange::low);
	size_t last{0};
	for (size_t i{1}; i < ranges.size(); ++i) {
		if (ranges[i].low <= ranges[last].high + 1) {
			ranges[last].high = std::max(ranges[last].high, ranges[i].high);
		} else {
			ranges[++last] = ranges[i];
		}
	}
	if (!ranges.empty()) { ranges.resize(last + 1); }
}

/// Is every character of the normalized @p ranges in the normalized @p superset?
bool is_subset(const std::span<const CharRange> ranges, const std::span<const CharRange> superset) {
	size_t super{0};
	for (const CharRange& range : ranges) {
		while (super < superset.size() && superset[super].high < range.low) { ++super; }
		if (super == superset.size() || superset[super].low > range.low || superset[super].high < range.high) {
			return false;
		}
	}
	return true;
}

/// Appends @p character as it is written inside (@p in_class) or outside of a character class.
void append_char(std::string& text, const Char character, const bool in_class) {
	constexpr std::string_view hex_digits{"0123456789ABCDEF"};
	switch (character) {
		case '\t':
			text += "\\t";
			return;
		case '\n':
			text += "\\n";
			return;
		case '\v':
			text += "\\v";
			return;
		case '\f':
			text += "\\f";
			return;
		case '\r':
			text += "\\r";
			return;
		default:
			break;
	}
	if (character > 0xFF) {
		text += "\\x{";
		bool leading{true};
		for (int shift{20}; shift >= 0; shift -= 4) {
			const Char digit{(character >> shift) & 0xF};
			leading = leading && digit == 0;
			if (!leading) { text += hex_digits[digit]; }
		}
		text += '}';
	} else if (character < 0x20 || character >= 0x7F) {
		text += "\\x";
		text += hex_digits[character >> 4];
		text += hex_digits[character & 0xF];
	} else {
		// Only what the syntax reserves is escaped: an escaped ordinary character has a meaning of its own in some
		//  regex dialects ('\<' is a word boundary in GNU ones).
		const std::string_view reserved{in_class ? R"(\]^-[)" : R"(\.[](){}*+?|^$)"};
		const auto ascii{static_cast<char>(character)};
		if (reserved.contains(ascii)) { text += '\\'; }
		text += ascii;
	}
}

/// Appends the character class of the normalized @p ranges; a single character is written without the brackets.
void append_class(std::string& text, const std::span<const CharRange> ranges) {
	if (ranges.size() == 1 && ranges.front().low == ranges.front().high) {
		append_char(text, ranges.front().low, false);
		return;
	}
	text += '[';
	for (const CharRange& range : ranges) {
		append_char(text, range.low, true);
		if (range.high == range.low) { continue; }
		if (range.high > range.low + 1) { text += '-'; }
		append_char(text, range.high, true);
	}
	text += ']';
}

/// The printed regex, passed on to a stream in blocks when it has one.
class Output {
  public:
	explicit Output(std::ostream* stream = nullptr) : stream_{stream} {}
	Output(const Output&) = delete;
	Output(Output&&) = delete;
	Output& operator=(const Output&) = delete;
	Output& operator=(Output&&) = delete;
	~Output() = default;

	std::string& text() { return text_; }

	void flush_block() {
		constexpr size_t block_size{size_t{1} << 16U};
		if (stream_ != nullptr && text_.size() >= block_size) { flush(); }
	}

	void flush() {
		if (stream_ == nullptr) { return; }
		stream_->write(text_.data(), static_cast<std::streamsize>(text_.size()));
		flushed_ += text_.size();
		text_.clear();
	}

	/// The number of characters printed so far.
	std::uint64_t written() const { return flushed_ + text_.size(); }

  private:
	std::ostream* stream_;
	std::string text_{};
	std::uint64_t flushed_{0};
};

enum class Kind : std::uint8_t {
	Epsilon, ///< The empty word.
	Chars, ///< One character of a set.
	Concat, ///< A concatenation of at least two regexes.
	Union, ///< A union of at least two regexes.
	Repeat, ///< A regex repeated @c min to @c max times.
};

struct Node {
	Kind kind{Kind::Epsilon};
	bool nullable{true}; ///< Whether the regex accepts the empty word.
	std::uint32_t hash{0};
	/// The first range of @c Chars, the first child of @c Concat and @c Union, and the child of @c Repeat.
	std::uint32_t begin{0};
	std::uint32_t size{0}; ///< The number of ranges of @c Chars and of children of @c Concat and @c Union.
	Count min{1};
	Count max{1};
	Count copies{1}; ///< The largest product of the counts written in braces of nested repetitions in the regex.
	Regex alphabet{NO_REGEX}; ///< The @c Chars of all characters of the regex; only kept when polishing.
	std::uint64_t length{0}; ///< The number of characters the regex is printed with; stays @c MAX_LENGTH once there.
};

/// How a repetition is printed: @c plain copies of the repeated regex followed by @c suffixed copies with @c suffix
///  each, 'aaa?a?' for 'a{2,4}', or a single copy with the counts in braces.
struct RepeatForm {
	bool braces{false};
	Count plain{0};
	Count suffixed{1};
	char suffix{'*'};
	std::uint64_t length{0};
};

/**
 * @brief A store of regexes, each kept only once.
 *
 * A regex is a node that refers to its subregexes, so the regexes share their common parts and take space
 *  proportional to the number of different subregexes, not to their printed length. Regexes are made by @c chars(),
 *  @c concat(), @c alternate() and @c repeat(), which simplify what they make: with a few rules of constant cost, or,
 *  when polishing, with all the rules below, which keep the regexes in a canonical form on top of that
 *  (concatenations and unions are flat, unions are sorted and hold no @c EPSILON).
 */
class Regexes {
  public:
	static constexpr Regex EPSILON{0};

	explicit Regexes(const bool polish) : polish_{polish}, unique_{0, NodeHash{this}, NodeEqual{this}} {
		nodes_.push_back(Node{.length = 2});
		unique_.insert(EPSILON);
	}
	Regexes(const Regexes&) = delete;
	Regexes(Regexes&&) = delete;
	Regexes& operator=(const Regexes&) = delete;
	Regexes& operator=(Regexes&&) = delete;
	~Regexes() = default;

	std::uint64_t length(const Regex regex) const { return nodes_[regex].length; }

	/// The number of regexes asked for so far, as a measure of the work done.
	size_t work() const { return work_; }

	/// One character of the normalized, nonempty @p ranges.
	Regex chars(const std::span<const CharRange> ranges) {
		const size_t begin{ranges_.size()};
		ranges_.insert(ranges_.end(), ranges.begin(), ranges.end());
		return intern(Kind::Chars, begin, ranges.size());
	}

	Regex concat(const Regex lhs, const Regex rhs) {
		if (lhs == EPSILON) { return rhs; }
		if (rhs == EPSILON) { return lhs; }
		if (polish_) {
			std::vector<Regex> factors{};
			append_factors(factors, lhs);
			append_factors(factors, rhs);
			return make_concat(factors);
		}
		// A regex next to its star, also as the nearest part of the other operand: 'aa*' and 'a(a*b)' have 'a+'.
		if (is_star_of(rhs, lhs)) { return repeat(lhs, 1, UNBOUNDED); }
		if (is_star_of(lhs, rhs)) { return repeat(rhs, 1, UNBOUNDED); }
		if (nodes_[rhs].kind == Kind::Concat) {
			const Regex first{children_[nodes_[rhs].begin]};
			const Regex rest{children_[nodes_[rhs].begin + 1]};
			if (is_star_of(first, lhs)) { return concat(repeat(lhs, 1, UNBOUNDED), rest); }
			if (is_star_of(lhs, first)) { return concat(repeat(first, 1, UNBOUNDED), rest); }
		}
		if (nodes_[lhs].kind == Kind::Concat) {
			const Regex rest{children_[nodes_[lhs].begin]};
			const Regex last{children_[nodes_[lhs].begin + 1]};
			if (is_star_of(last, rhs)) { return concat(rest, repeat(rhs, 1, UNBOUNDED)); }
			if (is_star_of(rhs, last)) { return concat(rest, repeat(last, 1, UNBOUNDED)); }
		}
		const size_t begin{children_.size()};
		children_.push_back(lhs);
		children_.push_back(rhs);
		return intern(Kind::Concat, begin, 2);
	}

	Regex alternate(const Regex lhs, const Regex rhs) {
		if (lhs == rhs) { return lhs; }
		if (polish_) {
			std::vector<Regex> alternatives{};
			append_alternatives(alternatives, lhs);
			append_alternatives(alternatives, rhs);
			return make_union(alternatives);
		}
		if (lhs == EPSILON) { return repeat(rhs, 0, 1); }
		if (rhs == EPSILON) { return repeat(lhs, 0, 1); }
		if (nodes_[lhs].kind == Kind::Chars && nodes_[rhs].kind == Kind::Chars) {
			std::vector<CharRange> merged{};
			append_ranges(merged, lhs);
			append_ranges(merged, rhs);
			normalize(merged);
			return chars(merged);
		}
		const size_t begin{children_.size()};
		children_.push_back(std::min(lhs, rhs));
		children_.push_back(std::max(lhs, rhs));
		return intern(Kind::Union, begin, 2);
	}

	Regex repeat(Regex regex, Count min, const Count max) {
		if (regex == EPSILON || max == 0) { return EPSILON; }
		if (nodes_[regex].nullable) { min = 0; }
		if (max == 1 && (min == 1 || nodes_[regex].nullable)) { return regex; }
		if (nodes_[regex].kind == Kind::Repeat) {
			// The numbers of copies of the inner regex are the whole range from the smallest to the largest one when
			//  the inner repetition can be taken once or skipped, when it has no upper bound, or when both counts are
			//  exact.
			const Count inner_min{nodes_[regex].min};
			const Count inner_max{nodes_[regex].max};
			const bool is_range{
				inner_min <= 1 || (inner_max == UNBOUNDED && min >= 1) || (inner_min == inner_max && min == max)
			};
			const std::optional<Count> joined_min{multiply_counts(min, inner_min)};
			const std::optional<Count> joined_max{multiply_counts(max, inner_max)};
			if (is_range && joined_min.has_value() && joined_max.has_value()) {
				return repeat(nodes_[regex].begin, *joined_min, *joined_max);
			}
		}
		if (polish_ && max == UNBOUNDED && min <= 1 && depth_ < MAX_DEPTH) {
			std::vector<Regex> alternatives{};
			if (star_alternatives(regex, alternatives)) { return repeat(make_union(alternatives), min, max); }
		}
		return intern(Kind::Repeat, regex, 0, min, max);
	}

	void print(const Regex root, Output& output) const {
		struct Item {
			enum class Type : std::uint8_t { Regex, Literal, Braces } type;
			Regex regex;
			char literal;
		};
		std::vector<Item> stack{};
		const auto push_operand = [&](const Regex operand, const bool parentheses) {
			if (parentheses) { stack.push_back({Item::Type::Literal, NO_REGEX, ')'}); }
			stack.push_back({Item::Type::Regex, operand, '\0'});
			if (parentheses) { stack.push_back({Item::Type::Literal, NO_REGEX, '('}); }
		};
		std::string& text{output.text()};
		stack.push_back({Item::Type::Regex, root, '\0'});
		// Not recursive: a concatenation made by concat() without polishing is as deep as it is long.
		while (!stack.empty()) {
			const Item item{stack.back()};
			stack.pop_back();
			if (item.type == Item::Type::Literal) {
				text += item.literal;
				continue;
			}
			const Node& node{nodes_[item.regex]};
			if (item.type == Item::Type::Braces) {
				text += '{';
				text += std::to_string(node.min);
				if (node.max != node.min) { text += ','; }
				if (node.max != node.min && node.max != UNBOUNDED) { text += std::to_string(node.max); }
				text += '}';
				continue;
			}
			switch (node.kind) {
				case Kind::Epsilon:
					text += "()";
					break;
				case Kind::Chars:
					append_class(text, ranges(node));
					break;
				case Kind::Concat:
					for (const Regex child : children(node) | std::views::reverse) {
						push_operand(child, nodes_[child].kind == Kind::Union);
					}
					break;
				case Kind::Union: {
					bool last{true};
					for (const Regex child : children(node) | std::views::reverse) {
						if (!last) { stack.push_back({Item::Type::Literal, NO_REGEX, '|'}); }
						last = false;
						stack.push_back({Item::Type::Regex, child, '\0'});
					}
					break;
				}
				case Kind::Repeat: {
					const RepeatForm form{repeat_form(node.begin, node.min, node.max)};
					const Kind child_kind{nodes_[node.begin].kind};
					for (Count i{0}; i < form.suffixed; ++i) {
						if (form.braces) {
							stack.push_back({Item::Type::Braces, item.regex, '\0'});
						} else {
							stack.push_back({Item::Type::Literal, NO_REGEX, form.suffix});
						}
						push_operand(node.begin, child_kind != Kind::Chars);
					}
					for (Count i{0}; i < form.plain; ++i) { push_operand(node.begin, child_kind == Kind::Union); }
					break;
				}
			}
			output.flush_block();
		}
	}

  private:
	struct NodeHash {
		const Regexes* regexes;
		size_t operator()(const Regex regex) const { return regexes->nodes_[regex].hash; }
	};

	struct NodeEqual {
		const Regexes* regexes;
		bool operator()(const Regex lhs, const Regex rhs) const {
			const Node& left{regexes->nodes_[lhs]};
			const Node& right{regexes->nodes_[rhs]};
			if (left.kind != right.kind || left.size != right.size || left.min != right.min || left.max != right.max) {
				return false;
			}
			switch (left.kind) {
				case Kind::Epsilon:
					return true;
				case Kind::Chars:
					return std::ranges::equal(regexes->ranges(left), regexes->ranges(right));
				case Kind::Concat:
				case Kind::Union:
					return std::ranges::equal(regexes->children(left), regexes->children(right));
				case Kind::Repeat:
					return left.begin == right.begin;
			}
			return false;
		}
	};

	/// A repetition with its counts, and any other regex as the regex itself once.
	struct Counted {
		Regex base;
		Count min;
		Count max;
	};

	/// The constructors that polish call each other, at most this deep: beyond it they stop simplifying.
	static constexpr unsigned MAX_DEPTH{128};
	/// The number of regexes @c star_contains() looks at before it gives up.
	static constexpr unsigned CONTAINMENT_BUDGET{64};
	/// The largest number of alternatives of a starred union that are tested against the star of the others.
	static constexpr size_t MAX_STAR_ALTERNATIVES{8};

	std::span<const CharRange> ranges(const Node& node) const { return {ranges_.data() + node.begin, node.size}; }
	std::span<const Regex> children(const Node& node) const { return {children_.data() + node.begin, node.size}; }

	bool is_star(const Regex regex) const {
		const Node& node{nodes_[regex]};
		return node.kind == Kind::Repeat && node.min == 0 && node.max == UNBOUNDED;
	}

	bool is_star_of(const Regex star, const Regex regex) const { return is_star(star) && nodes_[star].begin == regex; }

	Counted counted(const Regex regex) const {
		const Node& node{nodes_[regex]};
		if (node.kind == Kind::Repeat) { return {node.begin, node.min, node.max}; }
		return {regex, 1, 1};
	}

	void append_ranges(std::vector<CharRange>& ranges_of, const Regex chars) const {
		const std::span<const CharRange> added{ranges(nodes_[chars])};
		ranges_of.insert(ranges_of.end(), added.begin(), added.end());
	}

	void append_factors(std::vector<Regex>& factors, const Regex regex) const {
		if (regex == EPSILON) { return; }
		if (nodes_[regex].kind == Kind::Concat) {
			const std::span<const Regex> added{children(nodes_[regex])};
			factors.insert(factors.end(), added.begin(), added.end());
		} else {
			factors.push_back(regex);
		}
	}

	void append_alternatives(std::vector<Regex>& alternatives, const Regex regex) const {
		if (nodes_[regex].kind == Kind::Union) {
			const std::span<const Regex> added{children(nodes_[regex])};
			alternatives.insert(alternatives.end(), added.begin(), added.end());
		} else {
			alternatives.push_back(regex);
		}
	}

	std::uint64_t concat_operand_length(const Regex operand) const {
		const Node& node{nodes_[operand]};
		return add_lengths(node.length, node.kind == Kind::Union ? 2 : 0);
	}

	std::uint64_t repeat_operand_length(const Regex operand) const {
		const Node& node{nodes_[operand]};
		return add_lengths(node.length, node.kind == Kind::Chars ? 0 : 2);
	}

	/// How @p child repeated @p min to @p max times is printed: in the shortest way RE2 accepts.
	RepeatForm repeat_form(const Regex child, const Count min, const Count max) const {
		RepeatForm form{};
		if (max == UNBOUNDED) {
			form.plain = min == 0 ? 0 : min - 1;
			form.suffix = min == 0 ? '*' : '+';
		} else {
			form.plain = min;
			form.suffixed = max - min;
			form.suffix = '?';
		}
		const std::uint64_t operand_length{repeat_operand_length(child)};
		form.length = add_lengths(
			multiply_length(concat_operand_length(child), form.plain),
			multiply_length(add_lengths(operand_length, 1), form.suffixed)
		);
		const Count count{max == UNBOUNDED ? min : max};
		const bool single{max == UNBOUNDED ? min <= 1 : max == 1};
		if (single || count > MAX_BRACED_COUNT / nodes_[child].copies) { return form; }
		std::uint64_t braces_length{operand_length + 2 + num_of_digits(min)};
		if (max != min) { braces_length += 1 + (max == UNBOUNDED ? 0 : num_of_digits(max)); }
		if (braces_length <= form.length) {
			form = {.braces = true, .plain = 0, .suffixed = 1, .length = braces_length};
		}
		return form;
	}

	/// The regex of @p kind made of the last @p size ranges or children added, or of the child @p begin for a
	///  repetition: the regex stored already when there is one, a new one otherwise.
	Regex intern(const Kind kind, const size_t begin, const size_t size, const Count min = 1, const Count max = 1) {
		constexpr size_t limit{std::numeric_limits<std::uint32_t>::max()};
		if (nodes_.size() >= limit || children_.size() > limit || ranges_.size() > limit) {
			throw std::length_error("print_to_regex: the regex is too large");
		}
		++work_;
		Node node{
			.kind = kind,
			.begin = static_cast<std::uint32_t>(begin),
			.size = static_cast<std::uint32_t>(size),
			.min = min,
			.max = max
		};
		size_t hash{mata::utils::hash_combine(static_cast<size_t>(kind), min)};
		hash = mata::utils::hash_combine(hash, max);
		switch (kind) {
			case Kind::Epsilon:
				break;
			case Kind::Chars:
				for (const CharRange& range : ranges(node)) {
					hash = mata::utils::hash_combine(mata::utils::hash_combine(hash, range.low), range.high);
				}
				break;
			case Kind::Concat:
			case Kind::Union:
				for (const Regex child : children(node)) { hash = mata::utils::hash_combine(hash, child); }
				break;
			case Kind::Repeat:
				hash = mata::utils::hash_combine(hash, node.begin);
				break;
		}
		node.hash = static_cast<std::uint32_t>(hash);

		const auto regex{static_cast<Regex>(nodes_.size())};
		nodes_.push_back(node);
		if (const auto [stored, is_new]{unique_.insert(regex)}; !is_new) {
			nodes_.pop_back();
			if (kind == Kind::Chars) {
				ranges_.resize(begin);
			} else if (kind != Kind::Repeat) {
				children_.resize(begin);
			}
			return *stored;
		}
		describe(regex);
		return regex;
	}

	/// Fills in what is derived from the parts of the new @p regex.
	void describe(const Regex regex) {
		Node node{nodes_[regex]};
		switch (node.kind) {
			case Kind::Epsilon:
				break;
			case Kind::Chars: {
				std::string text{};
				append_class(text, ranges(node));
				node.nullable = false;
				node.length = text.size();
				node.alphabet = regex;
				break;
			}
			case Kind::Concat:
				for (const Regex child : children(node)) {
					node.nullable = node.nullable && nodes_[child].nullable;
					node.length = add_lengths(node.length, concat_operand_length(child));
					node.copies = std::max(node.copies, nodes_[child].copies);
				}
				break;
			case Kind::Union:
				node.nullable = false;
				node.length = node.size - 1;
				for (const Regex child : children(node)) {
					node.nullable = node.nullable || nodes_[child].nullable;
					node.length = add_lengths(node.length, nodes_[child].length);
					node.copies = std::max(node.copies, nodes_[child].copies);
				}
				break;
			case Kind::Repeat: {
				const RepeatForm form{repeat_form(node.begin, node.min, node.max)};
				node.nullable = node.min == 0 || nodes_[node.begin].nullable;
				node.length = form.length;
				node.copies = nodes_[node.begin].copies;
				if (form.braces) { node.copies *= node.max == UNBOUNDED ? node.min : node.max; }
				node.alphabet = nodes_[node.begin].alphabet;
				break;
			}
		}
		if (polish_ && (node.kind == Kind::Concat || node.kind == Kind::Union)) {
			// Mostly, the children are over the same characters.
			const std::span<const Regex> parts{children(node)};
			node.alphabet = nodes_[parts.front()].alphabet;
			if (std::ranges::any_of(parts, [&](const Regex part) { return nodes_[part].alphabet != node.alphabet; })) {
				std::vector<CharRange> merged{};
				for (const Regex part : parts) { append_ranges(merged, nodes_[part].alphabet); }
				normalize(merged);
				node.alphabet = chars(merged);
			}
		}
		nodes_[regex] = node;
	}

	/// The concatenation of @p factors, none of which is @c EPSILON or a concatenation.
	Regex make_concat(std::vector<Regex>& factors) {
		++depth_;
		if (depth_ < MAX_DEPTH) {
			while (simplify_factors(factors)) {}
			join_runs(factors);
		}
		--depth_;
		if (factors.empty()) { return EPSILON; }
		if (factors.size() == 1) { return factors.front(); }
		const size_t begin{children_.size()};
		children_.insert(children_.end(), factors.begin(), factors.end());
		return intern(Kind::Concat, begin, factors.size());
	}

	/// Replaces @p count factors from @p position by the factors of @p replacement.
	void replace_factors(
		std::vector<Regex>& factors, const size_t position, const size_t count, const Regex replacement
	) const {
		std::vector<Regex> replaced{};
		append_factors(replaced, replacement);
		const auto first{factors.begin() + static_cast<std::ptrdiff_t>(position)};
		factors.insert(
			factors.erase(first, first + static_cast<std::ptrdiff_t>(count)), replaced.begin(), replaced.end()
		);
	}

	/// Applies one rule to neighbouring @p factors; returns whether any applied.
	bool simplify_factors(std::vector<Regex>& factors) {
		for (size_t i{0}; i < factors.size(); ++i) {
			const Regex factor{factors[i]};
			if (nodes_[factor].kind == Kind::Repeat && nodes_[nodes_[factor].begin].kind == Kind::Concat) {
				// A repeated concatenation next to its copy: 'ab(ab)*' is '(ab)+'.
				const Regex repeated{nodes_[factor].begin};
				const std::vector<Regex> copy(children(nodes_[repeated]).begin(), children(nodes_[repeated]).end());
				const std::optional<Count> min{add_counts(nodes_[factor].min, 1)};
				const std::optional<Count> max{add_counts(nodes_[factor].max, 1)};
				const std::span<const Regex> all{factors};
				if (min.has_value() && max.has_value()) {
					if (i >= copy.size() && std::ranges::equal(all.subspan(i - copy.size(), copy.size()), copy)) {
						replace_factors(factors, i - copy.size(), copy.size() + 1, repeat(repeated, *min, *max));
						return true;
					}
					if (all.size() - i > copy.size() && std::ranges::equal(all.subspan(i + 1, copy.size()), copy)) {
						replace_factors(factors, i, copy.size() + 1, repeat(repeated, *min, *max));
						return true;
					}
				}
			}
			if (i + 1 == factors.size()) { break; }
			const Regex next{factors[i + 1]};
			const Counted lhs{counted(factor)};
			const Counted rhs{counted(next)};
			if (lhs.base == rhs.base && (lhs.base != factor || rhs.base != next)) {
				// Repetitions of the same regex: 'aa*' is 'a+', 'a?a*' is 'a*'.
				const std::optional<Count> min{add_counts(lhs.min, rhs.min)};
				const std::optional<Count> max{add_counts(lhs.max, rhs.max)};
				if (min.has_value() && max.has_value()) {
					replace_factors(factors, i, 2, repeat(lhs.base, *min, *max));
					return true;
				}
			}
			// A star next to what it contains: '[ab]*a?' is '[ab]*'.
			if (is_star(factor) && nodes_[next].nullable && star_contains(nodes_[factor].begin, next)) {
				factors.erase(factors.begin() + static_cast<std::ptrdiff_t>(i) + 1);
				return true;
			}
			if (is_star(next) && nodes_[factor].nullable && star_contains(nodes_[next].begin, factor)) {
				factors.erase(factors.begin() + static_cast<std::ptrdiff_t>(i));
				return true;
			}
			if (is_star(factor) && is_star(next)) {
				// Nested stars: 'a*(ba*)*' and '(a*b)*a*' are '(a|b)*'.
				const Regex inner{nodes_[factor].begin};
				const Regex next_inner{nodes_[next].begin};
				std::vector<Regex> rest{};
				Regex starred{NO_REGEX};
				if (nodes_[next_inner].kind == Kind::Concat && children(nodes_[next_inner]).back() == factor) {
					const std::span<const Regex> parts{children(nodes_[next_inner])};
					rest.assign(parts.begin(), parts.end() - 1);
					starred = inner;
				} else if (nodes_[inner].kind == Kind::Concat && children(nodes_[inner]).front() == next) {
					const std::span<const Regex> parts{children(nodes_[inner])};
					rest.assign(parts.begin() + 1, parts.end());
					starred = next_inner;
				}
				if (starred != NO_REGEX) {
					std::vector<Regex> alternatives{make_concat(rest)};
					append_alternatives(alternatives, starred);
					replace_factors(factors, i, 2, repeat(make_union(alternatives), 0, UNBOUNDED));
					return true;
				}
			}
		}
		return false;
	}

	/// Replaces the runs of the same factor in @p factors by a repetition when that is printed shorter.
	void join_runs(std::vector<Regex>& factors) {
		for (size_t first{0}; first < factors.size(); ++first) {
			size_t end{first + 1};
			while (end < factors.size() && factors[end] == factors[first]) { ++end; }
			const size_t run{end - first};
			if (run == 1 || run >= UNBOUNDED || nodes_[factors[first]].kind == Kind::Repeat) { continue; }
			const auto count{static_cast<Count>(run)};
			if (!repeat_form(factors[first], count, count).braces) {
				first = end - 1;
				continue;
			}
			const Regex repeated{repeat(factors[first], count, count)};
			factors.erase(
				factors.begin() + static_cast<std::ptrdiff_t>(first) + 1,
				factors.begin() + static_cast<std::ptrdiff_t>(end)
			);
			factors[first] = repeated;
		}
	}

	/// The union of @p alternatives, which may include @c EPSILON.
	Regex make_union(std::vector<Regex>& alternatives) {
		// The empty word and the optional alternatives make the whole union optional: 'a?|b' is '(a|b)?'.
		bool optional{false};
		std::vector<CharRange> merged{};
		size_t num_of_chars{0};
		for (size_t i{0}; i < alternatives.size();) {
			const Node& node{nodes_[alternatives[i]]};
			if (node.kind == Kind::Epsilon || (node.kind == Kind::Repeat && node.min == 0 && node.max == 1)) {
				optional = true;
				const Regex child{node.kind == Kind::Epsilon ? EPSILON : node.begin};
				alternatives.erase(alternatives.begin() + static_cast<std::ptrdiff_t>(i));
				if (child != EPSILON) { append_alternatives(alternatives, child); }
			} else if (node.kind == Kind::Chars) {
				// Single characters make one character class: 'a|b' is '[ab]'.
				append_ranges(merged, alternatives[i]);
				++num_of_chars;
				if (num_of_chars == 1) {
					++i;
				} else {
					alternatives.erase(alternatives.begin() + static_cast<std::ptrdiff_t>(i));
				}
			} else {
				++i;
			}
		}
		if (num_of_chars > 1) {
			normalize(merged);
			const Regex joined{chars(merged)};
			*std::ranges::find_if(alternatives, [&](const Regex alternative) {
				return nodes_[alternative].kind == Kind::Chars;
			}) = joined;
		}
		sort_unique(alternatives);

		++depth_;
		if (depth_ < MAX_DEPTH) {
			join_repetitions(alternatives);
			drop_contained(alternatives);
			if (factor_out(alternatives, true) || factor_out(alternatives, false)) {
				if (optional) { alternatives.push_back(EPSILON); }
				const Regex factored{make_union(alternatives)};
				--depth_;
				return factored;
			}
		}
		--depth_;

		Regex result{EPSILON};
		if (alternatives.size() == 1) {
			result = alternatives.front();
		} else if (alternatives.size() > 1) {
			const size_t begin{children_.size()};
			children_.insert(children_.end(), alternatives.begin(), alternatives.end());
			result = intern(Kind::Union, begin, alternatives.size());
		}
		return optional ? repeat(result, 0, 1) : result;
	}

	static void sort_unique(std::vector<Regex>& regexes) {
		std::ranges::sort(regexes);
		const auto [first, last]{std::ranges::unique(regexes)};
		regexes.erase(first, last);
	}

	/// Joins the repetitions of the same regex in @p alternatives whose counts overlap or follow each other: 'a|a+'
	///  is 'a+', 'a|a{2,3}' is 'a{1,3}'.
	void join_repetitions(std::vector<Regex>& alternatives) {
		std::vector<Counted> repetitions{};
		repetitions.reserve(alternatives.size());
		for (const Regex alternative : alternatives) { repetitions.push_back(counted(alternative)); }
		std::ranges::sort(repetitions, [](const Counted& lhs, const Counted& rhs) {
			return std::pair{lhs.base, lhs.min} < std::pair{rhs.base, rhs.min};
		});
		bool joined{false};
		size_t last{0};
		for (size_t i{1}; i < repetitions.size(); ++i) {
			Counted& previous{repetitions[last]};
			if (repetitions[i].base == previous.base &&
				(previous.max == UNBOUNDED || repetitions[i].min <= previous.max + 1)) {
				previous.max = std::max(previous.max, repetitions[i].max);
				joined = true;
			} else {
				repetitions[++last] = repetitions[i];
			}
		}
		if (!joined) { return; }
		repetitions.resize(last + 1);
		alternatives.clear();
		for (const Counted& repetition : repetitions) {
			alternatives.push_back(repeat(repetition.base, repetition.min, repetition.max));
		}
		sort_unique(alternatives);
	}

	/// Removes the @p alternatives contained in another one that is a star or a plus: 'ab|[ab]*' is '[ab]*'.
	void drop_contained(std::vector<Regex>& alternatives) const {
		for (const Regex alternative : std::vector<Regex>{alternatives}) {
			const Node& node{nodes_[alternative]};
			if (node.kind != Kind::Repeat || node.max != UNBOUNDED || node.min > 1) { continue; }
			// Two alternatives may contain each other; one of them has to stay.
			if (!std::ranges::binary_search(alternatives, alternative)) { continue; }
			std::erase_if(alternatives, [&](const Regex other) {
				return other != alternative && (node.min == 0 || !nodes_[other].nullable) &&
					   star_contains(node.begin, other);
			});
		}
	}

	/// Moves what the alternatives start with (@p prefix) or end with out of the union: 'ab|ac' is 'a[bc]'.
	/// @return Whether @p alternatives changed.
	bool factor_out(std::vector<Regex>& alternatives, const bool prefix) {
		if (alternatives.size() < 2) { return false; }
		struct Factored {
			std::vector<Regex> factors{};
			Regex alternative{NO_REGEX};
		};
		std::vector<Factored> all(alternatives.size());
		for (size_t i{0}; i < alternatives.size(); ++i) {
			all[i].alternative = alternatives[i];
			append_factors(all[i].factors, alternatives[i]);
			// The common end is the common start of the reversed factors.
			if (!prefix) { std::ranges::reverse(all[i].factors); }
		}
		std::ranges::sort(all, {}, [](const Factored& of) { return of.factors.front(); });

		bool changed{false};
		std::vector<Regex> result{};
		for (size_t first{0}, end{0}; first < all.size(); first = end) {
			end = first + 1;
			while (end < all.size() && all[end].factors.front() == all[first].factors.front()) { ++end; }
			if (end - first == 1) {
				result.push_back(all[first].alternative);
				continue;
			}
			changed = true;
			const std::vector<Regex>& pattern{all[first].factors};
			size_t common{pattern.size()};
			for (size_t i{first + 1}; i < end; ++i) {
				const auto differing{std::ranges::mismatch(all[i].factors, pattern).in1};
				common = std::min(common, static_cast<size_t>(differing - all[i].factors.begin()));
			}
			const auto common_length{static_cast<std::ptrdiff_t>(common)};
			std::vector<Regex> rests{};
			for (size_t i{first}; i < end; ++i) {
				std::vector<Regex> rest(all[i].factors.begin() + common_length, all[i].factors.end());
				if (!prefix) { std::ranges::reverse(rest); }
				rests.push_back(make_concat(rest));
			}
			std::vector<Regex> factored(pattern.begin(), pattern.begin() + common_length);
			if (!prefix) { std::ranges::reverse(factored); }
			std::vector<Regex> rest_factors{};
			append_factors(rest_factors, make_union(rests));
			factored.insert(prefix ? factored.end() : factored.begin(), rest_factors.begin(), rest_factors.end());
			result.push_back(make_concat(factored));
		}
		if (changed) { alternatives = std::move(result); }
		return changed;
	}

	/// Is every word of @p regex in the star of @p starred? May answer no when it is.
	bool star_contains(const Regex starred, const Regex regex) const {
		unsigned budget{CONTAINMENT_BUDGET};
		return star_contains(starred, regex, budget);
	}

	bool star_contains(const Regex starred, const Regex regex, unsigned& budget) const {
		if (regex == EPSILON || regex == starred) { return true; }
		if (budget == 0) { return false; }
		--budget;
		const Node& star_node{nodes_[starred]};
		const Node& node{nodes_[regex]};
		const std::span<const Regex> alternatives{
			star_node.kind == Kind::Union ? children(star_node) : std::span<const Regex>{}
		};
		if (std::ranges::binary_search(alternatives, regex)) { return true; }
		// A word over the characters that are words of the starred regex by themselves.
		Regex single_chars{star_node.kind == Kind::Chars ? starred : NO_REGEX};
		for (const Regex alternative : alternatives) {
			if (nodes_[alternative].kind == Kind::Chars) { single_chars = alternative; }
		}
		if (single_chars != NO_REGEX && node.alphabet != NO_REGEX &&
			is_subset(ranges(nodes_[node.alphabet]), ranges(nodes_[single_chars]))) {
			return true;
		}
		switch (node.kind) {
			case Kind::Repeat:
				return star_contains(starred, node.begin, budget);
			case Kind::Concat:
			case Kind::Union:
				return std::ranges::all_of(children(node), [&](const Regex child) {
					return star_contains(starred, child, budget);
				});
			default:
				return false;
		}
	}

	/// Collects in @p alternatives what the starred @p body can be replaced with: in a star, 'a*', 'a+' and 'a?' are
	///  'a', a concatenation of regexes that accept the empty word is their union, and an alternative contained in
	///  the star of the others is not needed.
	/// @return Whether that differs from @p body.
	bool star_alternatives(const Regex body, std::vector<Regex>& alternatives) {
		bool changed{false};
		std::vector<Regex> pending{body};
		while (!pending.empty()) {
			const Regex regex{pending.back()};
			pending.pop_back();
			const Node& node{nodes_[regex]};
			if (node.kind == Kind::Union || (node.kind == Kind::Concat && node.nullable)) {
				const std::span<const Regex> parts{children(node)};
				pending.insert(pending.end(), parts.begin(), parts.end());
				changed = changed || node.kind == Kind::Concat;
			} else if (node.kind == Kind::Repeat && node.min <= 1) {
				pending.push_back(node.begin);
				changed = true;
			} else {
				alternatives.push_back(regex);
			}
		}
		sort_unique(alternatives);
		if (alternatives.size() > MAX_STAR_ALTERNATIVES) { return changed; }
		++depth_;
		for (size_t i{0}; alternatives.size() > 1 && i < alternatives.size();) {
			std::vector<Regex> others{alternatives};
			others.erase(others.begin() + static_cast<std::ptrdiff_t>(i));
			if (nodes_[alternatives[i]].kind != Kind::Chars && star_contains(make_union(others), alternatives[i])) {
				alternatives.erase(alternatives.begin() + static_cast<std::ptrdiff_t>(i));
				changed = true;
			} else {
				++i;
			}
		}
		--depth_;
		return changed;
	}

	bool polish_; ///< Whether to simplify with all the rules.
	unsigned depth_{0}; ///< How deep the constructors that polish are in each other.
	size_t work_{0};
	std::vector<Node> nodes_{};
	std::vector<Regex> children_{};
	std::vector<CharRange> ranges_{};
	std::unordered_set<Regex, NodeHash, NodeEqual> unique_;
};

/// Translates the symbols of an automaton to the characters of its regex.
class Characters {
  public:
	explicit Characters(const mata::Alphabet* alphabet) : alphabet_{alphabet} {}
	Characters(const Characters&) = delete;
	Characters(Characters&&) = delete;
	Characters& operator=(const Characters&) = delete;
	Characters& operator=(Characters&&) = delete;
	~Characters() = default;

	Char operator()(const Symbol symbol) {
		if (alphabet_ == nullptr) {
			if (!is_unicode_scalar_value(symbol)) {
				throw std::invalid_argument(
					"print_to_regex: symbol " + std::to_string(symbol) + " is not a Unicode scalar value"
				);
			}
			return symbol;
		}
		// Reverse lookups may scan the whole alphabet; translate each symbol once.
		const auto [named, is_new]{names_.try_emplace(symbol, 0)};
		if (is_new) {
			const std::string name{alphabet_->reverse_translate_symbol(symbol)};
			const std::optional<Char> character{single_character(name)};
			if (!character.has_value()) {
				throw std::invalid_argument(
					"print_to_regex: the name '" + name + "' of symbol " + std::to_string(symbol) +
					" is not a single character"
				);
			}
			named->second = *character;
		}
		return named->second;
	}

  private:
	/// The character @p name is the UTF-8 encoding of, if it is one of a single character.
	static std::optional<Char> single_character(const std::string_view name) {
		if (name.empty()) { return std::nullopt; }
		const auto lead{static_cast<unsigned char>(name.front())};
		size_t length{1};
		Char character{lead};
		if ((lead & 0xE0U) == 0xC0U) {
			length = 2;
			character = lead & 0x1FU;
		} else if ((lead & 0xF0U) == 0xE0U) {
			length = 3;
			character = lead & 0x0FU;
		} else if ((lead & 0xF8U) == 0xF0U) {
			length = 4;
			character = lead & 0x07U;
		} else if (lead >= 0x80U) {
			return std::nullopt;
		}
		if (name.size() != length) { return std::nullopt; }
		for (const char continuation : name.substr(1)) {
			const auto byte{static_cast<unsigned char>(continuation)};
			if ((byte & 0xC0U) != 0x80U) { return std::nullopt; }
			character = (character << 6U) | (byte & 0x3FU);
		}
		if ((length == 2 && character < 0x80) || (length == 3 && character < 0x8'00) ||
			(length == 4 && character < 0x1'00'00)) {
			return std::nullopt;
		}
		return is_unicode_scalar_value(character) ? std::optional{character} : std::nullopt;
	}

	const mata::Alphabet* alphabet_;
	std::unordered_map<Symbol, Char> names_{};
};

struct Edge {
	State target;
	Regex label;
};

/**
 * @brief The useful states of an automaton with at most one transition between two states, labelled by a regex.
 *
 * A new initial state @c source() leads to the initial states and the final states lead to a new final state
 *  @c sink(), both by the empty word.
 */
struct LabelledAutomaton {
	std::vector<std::vector<Edge>> out{}; ///< The transitions from each state, sorted by target.
	std::vector<std::vector<State>> in{}; ///< The sources of the transitions to each state except itself, sorted.
	std::vector<bool> is_initial{};
	std::vector<bool> is_final{};

	size_t num_of_states() const { return is_initial.size(); }
	State source() const { return static_cast<State>(num_of_states()); }
	State sink() const { return static_cast<State>(num_of_states() + 1); }
};

LabelledAutomaton label_by_regexes(const Nfa& aut, Characters& characters, Regexes& regexes) {
	const mata::BoolVector useful{aut.get_useful_states()};
	std::vector<State> renaming(useful.size());
	size_t num_of_states{0};
	for (size_t state{0}; state < useful.size(); ++state) {
		if (useful[state]) {
			renaming[state] = static_cast<State>(num_of_states);
			++num_of_states;
		}
	}
	LabelledAutomaton result{};
	if (num_of_states == 0) { return result; }
	check_num_of_states(num_of_states + 2);
	result.out.resize(num_of_states + 2);
	result.in.resize(num_of_states + 2);
	result.is_initial.resize(num_of_states);
	result.is_final.resize(num_of_states);
	const State sink{result.sink()};

	std::vector<std::pair<State, Char>> moves{};
	std::vector<State> epsilon_targets{};
	std::vector<CharRange> ranges{};
	for (State state{0}; state < useful.size(); ++state) {
		if (!useful[state]) { continue; }
		const State source{renaming[state]};
		moves.clear();
		epsilon_targets.clear();
		for (const SymbolPost& symbol_post : aut.delta[state]) {
			std::optional<Char> character{};
			for (const State target : symbol_post.targets) {
				if (!useful[target]) { continue; }
				if (Nfa::is_epsilon(symbol_post.symbol)) {
					epsilon_targets.push_back(renaming[target]);
				} else {
					if (!character.has_value()) { character = characters(symbol_post.symbol); }
					moves.emplace_back(renaming[target], *character);
				}
			}
		}
		std::ranges::sort(moves);

		std::vector<Edge>& out{result.out[source]};
		for (size_t move{0}, epsilon{0}; move < moves.size() || epsilon < epsilon_targets.size();) {
			State target{move < moves.size() ? moves[move].first : epsilon_targets[epsilon]};
			if (epsilon < epsilon_targets.size()) { target = std::min(target, epsilon_targets[epsilon]); }
			Regex label{NO_REGEX};
			ranges.clear();
			for (; move < moves.size() && moves[move].first == target; ++move) {
				const Char character{moves[move].second};
				if (!ranges.empty() && character <= ranges.back().high + 1) {
					ranges.back().high = character;
				} else {
					ranges.push_back({character, character});
				}
			}
			if (!ranges.empty()) { label = regexes.chars(ranges); }
			if (epsilon < epsilon_targets.size() && epsilon_targets[epsilon] == target) {
				label = label == NO_REGEX ? Regexes::EPSILON : regexes.alternate(label, Regexes::EPSILON);
				++epsilon;
			}
			out.push_back({target, label});
			if (target != source) { result.in[target].push_back(source); }
		}
		if (aut.final[state]) {
			result.is_final[source] = true;
			out.push_back({sink, Regexes::EPSILON});
			result.in[sink].push_back(source);
		}
	}
	for (const State initial : aut.initial) {
		if (initial < useful.size() && useful[initial]) { result.is_initial[renaming[initial]] = true; }
	}
	for (State state{0}; state < num_of_states; ++state) {
		if (result.is_initial[state]) {
			result.out[result.source()].push_back({state, Regexes::EPSILON});
			result.in[state].push_back(result.source());
		}
	}
	return result;
}

/// The order in which the states are eliminated.
struct Order {
	bool initial_last{false}; ///< Eliminate the initial states after the other ones.
	bool final_last{false}; ///< Eliminate the final states after the other ones.
	/// When not 0, the weights of the states are multiplied by random factors made from the seed.
	std::uint64_t seed{0};
};

/// Eliminates the states of an automaton labelled by regexes.
class Elimination {
  public:
	Elimination(LabelledAutomaton aut, Regexes& regexes) : aut_{std::move(aut)}, regexes_{regexes} {}

	/// @return The regex of the automaton, the label of the transition left from its source to its sink.
	Regex run(const Order& order) {
		const size_t num_of_states{aut_.num_of_states()};
		std::vector<std::uint32_t> versions(num_of_states, 0);
		std::uint64_t random{order.seed};
		// A state is first by its tier, then by its weight.
		using Entry = std::tuple<bool, double, State, std::uint32_t>;
		std::priority_queue<Entry, std::vector<Entry>, std::greater<>> queue{};
		const auto push = [&](const State state) {
			double key{weight(state)};
			if (order.seed != 0) {
				// SplitMix64.
				random += 0x9E'37'79'B9'7F'4A'7C'15ULL;
				std::uint64_t mixed{random};
				mixed = (mixed ^ (mixed >> 30U)) * 0xBF'58'47'6D'1C'E4'E5'B9ULL;
				mixed = (mixed ^ (mixed >> 27U)) * 0x94'D0'49'BB'13'31'11'EBULL;
				mixed ^= mixed >> 31U;
				key *= 1.0 + static_cast<double>(mixed >> 11U) / static_cast<double>(std::uint64_t{1} << 53U);
			}
			const bool last{
				(order.initial_last && aut_.is_initial[state]) || (order.final_last && aut_.is_final[state])
			};
			queue.emplace(last, key, state, versions[state]);
		};
		for (State state{0}; state < num_of_states; ++state) { push(state); }

		std::vector<State> neighbours{};
		while (!queue.empty()) {
			const auto [last, key, state, version]{queue.top()};
			queue.pop();
			if (version != versions[state]) { continue; }
			neighbours = aut_.in[state];
			for (const Edge& edge : aut_.out[state]) { neighbours.push_back(edge.target); }
			eliminate(state);
			for (const State neighbour : neighbours) {
				if (neighbour < num_of_states && neighbour != state) {
					++versions[neighbour];
					push(neighbour);
				}
			}
		}
		const std::vector<Edge>& from_source{aut_.out[aut_.source()]};
		return from_source.empty() ? NO_REGEX : from_source.front().label;
	}

  private:
	/// The weight of Delgado and Morais: by how much the labels get longer when @p state is eliminated.
	double weight(const State state) const {
		const std::vector<State>& in{aut_.in[state]};
		const std::vector<Edge>& out{aut_.out[state]};
		double in_length{0};
		double out_length{0};
		double loop_length{0};
		for (const State source : in) { in_length += static_cast<double>(regexes_.length(label(source, state))); }
		for (const Edge& edge : out) {
			(edge.target == state ? loop_length : out_length) += static_cast<double>(regexes_.length(edge.label));
		}
		const auto num_of_in{static_cast<double>(in.size())};
		const double num_of_out{static_cast<double>(out.size()) - (loop_length > 0 ? 1.0 : 0.0)};
		return in_length * (num_of_out - 1) + out_length * (num_of_in - 1) + loop_length * (num_of_in * num_of_out - 1);
	}

	Regex label(const State source, const State target) const {
		return std::ranges::lower_bound(aut_.out[source], target, {}, &Edge::target)->label;
	}

	/// Removes @p state: every pair of a transition to it and a transition from it becomes one transition, labelled
	///  by the two labels with the star of the label of the loop on @p state between them.
	void eliminate(const State state) {
		std::vector<Edge> out{std::move(aut_.out[state])};
		std::vector<State> in{std::move(aut_.in[state])};
		aut_.out[state].clear();
		aut_.in[state].clear();

		Regex loop{Regexes::EPSILON};
		if (const auto self{std::ranges::lower_bound(out, state, {}, &Edge::target)};
			self != out.end() && self->target == state) {
			loop = regexes_.repeat(self->label, 0, UNBOUNDED);
			out.erase(self);
		}
		for (Edge& edge : out) { edge.label = regexes_.concat(loop, edge.label); }

		std::vector<Edge> merged{};
		for (const State source : in) {
			std::vector<Edge>& source_out{aut_.out[source]};
			const auto to_state{std::ranges::lower_bound(source_out, state, {}, &Edge::target)};
			const Regex prefix{to_state->label};
			source_out.erase(to_state);
			merged.clear();
			merged.reserve(source_out.size() + out.size());
			auto old{source_out.cbegin()};
			for (const Edge& edge : out) {
				for (; old != source_out.cend() && old->target < edge.target; ++old) { merged.push_back(*old); }
				Regex through{regexes_.concat(prefix, edge.label)};
				if (old != source_out.cend() && old->target == edge.target) {
					through = regexes_.alternate(old->label, through);
					++old;
				}
				merged.push_back({edge.target, through});
			}
			merged.insert(merged.end(), old, source_out.cend());
			source_out.swap(merged);
		}

		std::vector<State> sources{};
		for (const Edge& edge : out) {
			std::vector<State>& target_in{aut_.in[edge.target]};
			sources.clear();
			sources.reserve(target_in.size() + in.size());
			std::ranges::set_union(target_in, in, std::back_inserter(sources));
			std::erase_if(sources, [&](const State source) { return source == state || source == edge.target; });
			target_in.swap(sources);
		}
	}

	LabelledAutomaton aut_;
	Regexes& regexes_;
};

/// The minimal deterministic automaton of @p aut, unless determinization makes more than @p max_states states.
std::optional<Nfa> minimize_within(const Nfa& aut, const size_t max_states) {
	bool within{true};
	Nfa determinized{determinize(aut, nullptr, [&](const Nfa& result, const State, const StateSet&) {
		within = result.num_of_states() <= max_states;
		return within;
	})};
	return within ? std::optional{minimize(determinized)} : std::nullopt;
}

/// The shortest regex found for @p aut by eliminating equivalent automata in several orders.
Regex find_short_regex(const Nfa& aut, Characters& characters, Regexes& regexes) {
	/// An equivalent automaton is only made of an automaton with at most this many states.
	constexpr size_t max_states_to_rebuild{256};
	/// No further elimination is started once this many regexes have been asked for.
	constexpr size_t work_limit{2'000'000};
	constexpr unsigned num_of_random_orders{24};

	std::vector<LabelledAutomaton> automata{};
	automata.push_back(label_by_regexes(aut, characters, regexes));
	if (automata.front().num_of_states() <= max_states_to_rebuild) {
		// Automata of the same language: a smaller nondeterministic one, the minimal deterministic one, and the
		//  minimal one deterministic backwards. Which of them has the shortest regex depends on the language.
		const Nfa plain{remove_epsilon(trim(aut))};
		const size_t max_states{4 * plain.num_of_states() + 16};
		automata.push_back(label_by_regexes(reduce(plain), characters, regexes));
		if (const std::optional<Nfa> minimal{minimize_within(plain, max_states)}; minimal.has_value()) {
			automata.push_back(label_by_regexes(*minimal, characters, regexes));
		}
		if (const std::optional<Nfa> minimal{minimize_within(revert(plain), max_states)}; minimal.has_value()) {
			automata.push_back(label_by_regexes(revert(*minimal), characters, regexes));
		}
	}

	Regex shortest{NO_REGEX};
	const auto eliminate = [&](const LabelledAutomaton& labelled, const Order& order) {
		if (shortest != NO_REGEX && regexes.work() > work_limit) { return; }
		const Regex regex{Elimination{labelled, regexes}.run(order)};
		if (shortest == NO_REGEX || regexes.length(regex) < regexes.length(shortest)) { shortest = regex; }
	};
	for (const LabelledAutomaton& labelled : automata) {
		for (const bool initial_last : {true, false}) {
			for (const bool final_last : {false, true}) {
				eliminate(labelled, {.initial_last = initial_last, .final_last = final_last});
			}
		}
	}
	for (std::uint64_t seed{1}; seed <= num_of_random_orders; ++seed) {
		for (const LabelledAutomaton& labelled : automata) { eliminate(labelled, {.seed = seed}); }
	}
	return shortest;
}

void write_regex(const Nfa& aut, Output& output, const RegexPrintMode mode, const mata::Alphabet* alphabet) {
	if (aut.is_lang_empty()) {
		output.text() += EMPTY_LANGUAGE;
		return;
	}
	Characters characters{alphabet != nullptr ? alphabet : aut.alphabet.get()};
	Regexes plain{false};
	const Regex fast{Elimination{label_by_regexes(aut, characters, plain), plain}.run({.initial_last = true})};
	if (mode == RegexPrintMode::Fast) {
		plain.print(fast, output);
		assert(output.written() == plain.length(fast));
		return;
	}
	Regexes polished{true};
	const Regex readable{find_short_regex(aut, characters, polished)};
	// The rules that polish mostly shorten a regex, but not always.
	if (plain.length(fast) < polished.length(readable)) {
		plain.print(fast, output);
	} else {
		polished.print(readable, output);
	}
	assert(output.written() == std::min(plain.length(fast), polished.length(readable)));
}

} // namespace

std::string Nfa::print_to_regex(const RegexPrintMode mode, const Alphabet* alphabet) const {
	Output output{};
	write_regex(*this, output, mode, alphabet);
	return std::move(output.text());
}

void Nfa::print_to_regex(std::ostream& output, const RegexPrintMode mode, const Alphabet* alphabet) const {
	Output blocks{&output};
	write_regex(*this, blocks, mode, alphabet);
	blocks.flush();
	output.flush();
}

void Nfa::print_to_regex(const std::string& filename, const RegexPrintMode mode, const Alphabet* alphabet) const {
	std::ofstream output(filename);
	if (!output) { throw std::ios_base::failure("Failed to open file: " + filename); }
	print_to_regex(output, mode, alphabet);
}
