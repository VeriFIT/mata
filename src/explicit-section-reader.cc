/** @file
 * @brief Fast reader of the `.mata` sections whose lines are plain lists of names.
 */

#include "explicit-section-reader.hh"

#include <algorithm>
#include <array>
#include <istream>

using mata::parser::ExplicitSectionReader;

namespace {
/// @c std::isspace() in the "C" locale, which @c parse_mf() runs in.
constexpr bool is_space(const char ch) { return ch == ' ' || (ch >= '\t' && ch <= '\r'); }

/// Whether @p ch may occur in the name of a section type, as in @c parse_mf().
bool is_type_char(const char ch) {
	switch (ch) {
		case '"':
		case '(':
		case ')':
		case '#':
		case '%':
		case '@':
		case '\\':
		case ' ':
		case '\t':
			return false;
		default:
			return true;
	}
}

/// The kinds of characters the tokenizer tells apart.
enum class CharKind : std::uint8_t {
	/// Continues an unquoted token.
	Ordinary,
	/// See @c is_space().
	Space,
	/// Starts a comment.
	Hash,
	Quote,
	/// An operator or parenthesis of a transition formula, which @c parse_mf() makes a token of its own.
	Formula,
	/// `@` and `%`, which @c parse_mf() reads specially.
	Special,
};

constexpr std::array<CharKind, 256> CHAR_KINDS{[] {
	std::array<CharKind, 256> kinds{};
	for (size_t ch{0}; ch < kinds.size(); ++ch) {
		if (is_space(static_cast<char>(ch))) { kinds[ch] = CharKind::Space; }
	}
	kinds['#'] = CharKind::Hash;
	kinds['"'] = CharKind::Quote;
	kinds['&'] = CharKind::Formula;
	kinds['|'] = CharKind::Formula;
	kinds['!'] = CharKind::Formula;
	kinds['('] = CharKind::Formula;
	kinds[')'] = CharKind::Formula;
	kinds['@'] = CharKind::Special;
	kinds['%'] = CharKind::Special;
	return kinds;
}()};

CharKind char_kind(const char ch) { return CHAR_KINDS[static_cast<unsigned char>(ch)]; }
} // namespace

std::string mata::parser::read_whole_stream(std::istream& input) {
	std::string text;
	std::array<char, 1 << 16> chunk{};
	while (input.read(chunk.data(), chunk.size()) || input.gcount() > 0) {
		text.append(chunk.data(), static_cast<size_t>(input.gcount()));
	}
	return text;
}

mata::parser::NewStates::NewStates(const utils::StringMap<State>& mapped) {
	mapped_.reserve(mapped.size());
	for (const auto& [name, state] : mapped) { mapped_.push_back(state); }
	std::ranges::sort(mapped_);
}

mata::parser::NewStates::State mata::parser::NewStates::next(size_t num_of_states) {
	// Two names may share a state, so a state of the map is passed over as often as it occurs.
	while (passed_ < mapped_.size() && mapped_[passed_] <= num_of_states) {
		if (mapped_[passed_] == num_of_states) { ++num_of_states; }
		++passed_;
	}
	return nfa::to_state(num_of_states);
}

mata::parser::StateNameMap::StateNameMap(const Map& names, const size_t max_number)
	: max_number_{max_number},
	  new_states_{names} {
	for (const auto& [name, state] : names) { insert(name, state); }
}

std::optional<size_t> mata::parser::StateNameMap::number_of(const std::string_view name) {
	size_t digits_begin{name.size()};
	while (digits_begin > 0 && name[digits_begin - 1] >= '0' && name[digits_begin - 1] <= '9') { --digits_begin; }
	const std::string_view digits{name.substr(digits_begin)};
	// Leading zeros would map two names to one number.
	if (digits.empty() || (digits.size() > 1 && digits.front() == '0')) { return std::nullopt; }
	const std::string_view prefix{name.substr(0, digits_begin)};
	if (!prefix_.has_value()) {
		prefix_.emplace(prefix);
	} else if (*prefix_ != prefix) {
		return std::nullopt;
	}
	size_t number{0};
	for (const char digit : digits) {
		// Checked before the number can wrap around and so merge two names.
		const auto value{static_cast<size_t>(digit - '0')};
		if (number > max_number_ / 10 || value > max_number_ - (number * 10)) { return std::nullopt; }
		number = (number * 10) + value;
	}
	return number;
}

std::optional<mata::parser::StateNameMap::State> mata::parser::StateNameMap::find(const std::string_view name) {
	if (const std::optional<size_t> number{number_of(name)}) {
		if (*number < numbered_.size() && numbered_[*number] != NO_STATE) { return numbered_[*number]; }
		return std::nullopt;
	}
	if (const auto it{others_.find(name)}; it != others_.end()) { return it->second; }
	return std::nullopt;
}

void mata::parser::StateNameMap::insert(const std::string_view name, const State state) {
	if (const std::optional<size_t> number{number_of(name)}) {
		if (*number >= numbered_.size()) { numbered_.resize(*number + 1, NO_STATE); }
		numbered_[*number] = state;
	} else {
		others_.emplace(name, state);
	}
}

mata::parser::StateNameMap::Map mata::parser::StateNameMap::to_map() const {
	Map names{others_};
	for (size_t number{0}; number < numbered_.size(); ++number) {
		if (numbered_[number] != NO_STATE) { names.emplace(*prefix_ + std::to_string(number), numbered_[number]); }
	}
	return names;
}

std::optional<std::string_view> ExplicitSectionReader::read_type() {
	while (true) {
		while (position_ < input_.size() && is_space(input_[position_])) { ++position_; }
		if (position_ == input_.size()) { return std::nullopt; }

		const size_t line_end{std::min(input_.find('\n', position_), input_.size())};
		const std::string_view line{input_.substr(position_, line_end - position_)};
		position_ = line_end;
		if (line[0] == '#') { continue; }
		if (line[0] != '@') { return std::nullopt; }

		size_t i{1};
		while (i < line.size() && is_type_char(line[i])) { ++i; }
		const std::string_view type{line.substr(1, i - 1)};
		if (type.empty()) { return std::nullopt; }
		for (; i < line.size() && line[i] != '#'; ++i) {
			if (!is_space(line[i])) { return std::nullopt; }
		}
		return type;
	}
}

ExplicitSectionReader::Line ExplicitSectionReader::read_line() {
	tokens_.clear();
	unescaped_.clear();
	bool continued{false};
	while (true) {
		while (position_ < input_.size() && is_space(input_[position_])) { ++position_; }
		// parse_mf() silently drops a continuation left open at the end of the input.
		if (position_ == input_.size()) { return continued ? Line::Unsupported : Line::End; }
		// Another section.
		if (input_[position_] == '@') { return Line::Unsupported; }

		const size_t line_end{std::min(input_.find('\n', position_), input_.size())};
		std::string_view line{input_.substr(position_, line_end - position_)};
		position_ = line_end;
		const bool backslash_ending{line.back() == '\\'};
		if (backslash_ending) { line.remove_suffix(1); }

		const size_t num_of_tokens_before{tokens_.size()};
		if (!tokenize(line)) { return Line::Unsupported; }
		// A line without tokens neither ends nor starts a continuation.
		if (tokens_.size() == num_of_tokens_before) { continue; }
		continued = backslash_ending;
		if (continued) { continue; }

		if (const ExplicitToken& first{tokens_.front()}; !first.quoted && first.text.front() == '%') {
			key_ = first.text.substr(1);
			if (key_.empty()) { return Line::Unsupported; }
			tokens_.erase(tokens_.begin());
			return Line::Key;
		}
		return Line::Body;
	}
}

bool ExplicitSectionReader::tokenize(const std::string_view line) {
	const size_t line_size{line.size()};
	size_t i{0};
	bool first{true};
	while (true) {
		while (i < line_size && char_kind(line[i]) == CharKind::Space) { ++i; }
		if (i == line_size) { return true; }
		const CharKind kind{char_kind(line[i])};
		if (kind == CharKind::Hash) { return true; }

		if (kind == CharKind::Quote) {
			// parse_mf() reads an operator glued to the quote as the start of an unquoted token, which a quote may
			//  not continue.
			if (formulas_ && i > 0 && (line[i - 1] == '&' || line[i - 1] == '|' || line[i - 1] == '!')) {
				return false;
			}
			const size_t start{++i};
			std::string* unescaped{nullptr};
			for (; i < line_size && line[i] != '"'; ++i) {
				if (line[i] == '\\') {
					if (i + 1 == line_size) { return false; }
					if (unescaped == nullptr) { unescaped = &unescaped_.emplace_back(line.substr(start, i - start)); }
					// Only a quote is escaped; any other character keeps its backslash.
					if (line[++i] != '"') { unescaped->push_back('\\'); }
					unescaped->push_back(line[i]);
				} else if (unescaped != nullptr) {
					unescaped->push_back(line[i]);
				}
			}
			if (i == line_size) { return false; } // Missing ending quote.
			const std::string_view text{
				unescaped != nullptr ? std::string_view{*unescaped} : line.substr(start, i - start)
			};
			++i;
			if (i < line_size && !is_space(line[i]) && line[i] != '#' && line[i] != ')') { return false; }
			tokens_.push_back({text, true});
			first = false;
			continue;
		}

		const size_t start{i};
		if (kind == CharKind::Formula && formulas_) {
			tokens_.push_back({line.substr(i, 1), false});
			++i;
			first = false;
			continue;
		}
		// Only the first token of a line may be a %KEY.
		if (kind != CharKind::Ordinary && (line[i] != '%' || !first)) { return false; }
		for (++i; i < line_size; ++i) {
			const CharKind next_kind{char_kind(line[i])};
			if (next_kind == CharKind::Ordinary) { continue; }
			if (next_kind == CharKind::Space || next_kind == CharKind::Hash ||
				(next_kind == CharKind::Formula && formulas_)) {
				break;
			}
			return false;
		}
		tokens_.push_back({line.substr(start, i - start), false});
		first = false;
	}
}
