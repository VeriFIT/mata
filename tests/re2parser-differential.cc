/**
 * Differential tests of the RE2 converter on generated regexes.
 *
 * The direct epsilon-free conversion is compared with the explicit epsilon translation followed by remove_epsilon(),
 *  and with the language of the regex syntax tree on all short words. The bundled RE2 has no matcher to serve as an
 *  oracle. The regexes come from raw std::mt19937 output, so a seed gives the same regex under every standard
 *  library.
 */

#include <bitset>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "mata/nfa/nfa.hh"
#include "mata/parser/re2parser.hh"

using namespace mata::nfa;
using mata::Symbol;
using mata::Word;
using mata::parser::Re2Options;

namespace {

constexpr Symbol epsilon{306};

size_t draw(std::mt19937& rng, const size_t bound) { return static_cast<size_t>(rng()) % bound; }

/// A regex with its syntax tree, whose language serves as the oracle.
class RandomRegex {
  public:
	std::string text{};

	/**
	 * Generates a regex over 'a' to 'd' covering every instruction form the converter sees: byte ranges with
	 *  and without case folding, captures, alternation lists, empty branches, greedy and non-greedy repetitions,
	 *  counted repetitions, and '(?s:.)' loops, which RE2 compiles to AltMatch. With @p with_anchors, it also emits
	 *  anchors and word boundaries, which the converter ignores.
	 */
	RandomRegex(std::mt19937& rng, const size_t depth, const bool with_anchors)
		: rng_{rng},
		  with_anchors_{with_anchors} {
		root_ = generate(depth, text);
	}

	/// Whether @p word is in the language of the syntax tree.
	bool accepts(const Word& word) const {
		Positions starts(word.size() + 1, false);
		starts[0] = true;
		return ends(root_, word, starts)[word.size()];
	}

  private:
	/// A set of positions in a word, indexed by the position.
	using Positions = std::vector<bool>;
	static constexpr size_t unbounded{std::numeric_limits<size_t>::max()};

	struct Node {
		enum class Kind { Bytes, Concatenation, Alternation, Repetition } kind;
		std::bitset<256> bytes{};
		size_t lhs{};
		size_t rhs{};
		size_t min{};
		size_t max{};
	};

	std::mt19937& rng_;
	bool with_anchors_;
	std::vector<Node> nodes_{};
	size_t root_{};

	size_t add(Node node) {
		nodes_.push_back(node);
		return nodes_.size() - 1;
	}

	size_t add_bytes(std::bitset<256> bytes) { return add({.kind = Node::Kind::Bytes, .bytes = bytes}); }

	/// The empty word: a concatenation of nothing, which is a repetition of an empty byte set at most zero times.
	size_t add_empty() { return add({.kind = Node::Kind::Repetition, .lhs = add_bytes({}), .min = 0, .max = 0}); }

	size_t generate_leaf(std::string& out) {
		if (with_anchors_ && draw(rng_, 6) == 0) {
			static const std::vector<std::string> anchors{"^", "$", "\\b", "\\B", "\\A", "\\z"};
			out += anchors[draw(rng_, anchors.size())];
			return add_empty();
		}
		std::bitset<256> bytes{};
		switch (draw(rng_, 9)) {
			case 0:
				out += "a";
				bytes.set('a');
				break;
			case 1:
				out += "b";
				bytes.set('b');
				break;
			case 2:
				out += "c";
				bytes.set('c');
				break;
			case 3:
				out += "[ab]";
				bytes.set('a');
				bytes.set('b');
				break;
			case 4:
				out += "[^a]";
				bytes.set();
				bytes.reset('a');
				break;
			case 5:
				out += ".";
				bytes.set();
				bytes.reset('\n');
				break;
			case 6:
				out += "(?s:.)";
				bytes.set();
				break;
			case 7:
				// The bundled RE2 parser drops the folded case of a letter that it merges into a character class
				//  already holding the letter (e.g., 'a|(?i:a)' parses to '[a]'), so no other leaf holds 'd' without
				//  'D'.
				out += "(?i:d)";
				bytes.set('d');
				bytes.set('D');
				break;
			default:
				out += "(?:)";
				return add_empty();
		}
		return add_bytes(bytes);
	}

	size_t generate(const size_t depth, std::string& out) {
		if (depth == 0 || draw(rng_, 4) == 0) { return generate_leaf(out); }
		switch (draw(rng_, 5)) {
			case 0: {
				const size_t lhs{generate(depth - 1, out)};
				out += "(?:";
				const size_t rhs{generate(depth - 1, out)};
				out += ")";
				return add({.kind = Node::Kind::Concatenation, .lhs = lhs, .rhs = rhs});
			}
			case 1: {
				out += "(?:";
				const size_t lhs{generate(depth - 1, out)};
				out += "|";
				const size_t rhs{generate(depth - 1, out)};
				out += ")";
				return add({.kind = Node::Kind::Alternation, .lhs = lhs, .rhs = rhs});
			}
			case 2: {
				out += "(";
				const size_t child{generate(depth - 1, out)};
				out += ")";
				return child;
			}
			default: {
				out += draw(rng_, 2) == 0 ? "(" : "(?:";
				const size_t child{generate(depth - 1, out)};
				out += ")";
				struct Repetition {
					const char* text;
					size_t min;
					size_t max;
				};
				static const std::vector<Repetition> repetitions{
					{"*", 0, unbounded},  {"+", 1, unbounded},	  {"?", 0, 1},	 {"*?", 0, unbounded},
					{"+?", 1, unbounded}, {"??", 0, 1},			  {"{2}", 2, 2}, {"{0,2}", 0, 2},
					{"{1,3}?", 1, 3},	  {"{2,}", 2, unbounded},
				};
				const Repetition& repetition{repetitions[draw(rng_, repetitions.size())]};
				out += repetition.text;
				return add(
					{.kind = Node::Kind::Repetition, .lhs = child, .min = repetition.min, .max = repetition.max}
				);
			}
		}
	}

	/// The positions where a match of @p node starting at one of @p starts in @p word can end.
	Positions ends(const size_t node_index, const Word& word, const Positions& starts) const {
		const Node& node{nodes_[node_index]};
		switch (node.kind) {
			case Node::Kind::Bytes: {
				Positions result(starts.size(), false);
				for (size_t position{0}; position < word.size(); ++position) {
					if (starts[position] && node.bytes.test(word[position])) { result[position + 1] = true; }
				}
				return result;
			}
			case Node::Kind::Concatenation:
				return ends(node.rhs, word, ends(node.lhs, word, starts));
			case Node::Kind::Alternation: {
				Positions result{ends(node.lhs, word, starts)};
				const Positions rhs{ends(node.rhs, word, starts)};
				for (size_t position{0}; position < result.size(); ++position) {
					result[position] = result[position] || rhs[position];
				}
				return result;
			}
			case Node::Kind::Repetition: {
				Positions current{starts};
				for (size_t count{0}; count < node.min; ++count) { current = ends(node.lhs, word, current); }
				Positions result{current};
				for (size_t count{node.min}; count < node.max; ++count) {
					current = ends(node.lhs, word, current);
					bool changed{false};
					for (size_t position{0}; position < result.size(); ++position) {
						if (current[position] && !result[position]) {
							result[position] = true;
							changed = true;
						}
					}
					// The unbounded repetition reached its fixpoint.
					if (!changed && node.max == unbounded) { break; }
					if (node.max == unbounded) { current = result; }
				}
				return result;
			}
		}
		return {};
	}
};

/// All words over @p symbols of length at most @p max_length.
std::vector<Word> all_words(const Word& symbols, const size_t max_length) {
	std::vector<Word> words{Word{}};
	for (size_t begin{0}, length{0}; length < max_length; ++length) {
		const size_t end{words.size()};
		for (size_t index{begin}; index < end; ++index) {
			for (const Symbol symbol : symbols) {
				Word word{words[index]};
				word.push_back(symbol);
				words.push_back(std::move(word));
			}
		}
		begin = end;
	}
	return words;
}

} // namespace

TEST_CASE("mata::parser::create_nfa() agrees with the epsilon translation and the syntax tree on generated regexes") {
	const std::vector<Word> words{all_words(Word{'a', 'b', 'c', 'd', 'D', '\n'}, 4)};
	for (unsigned seed{0}; seed < 400; ++seed) {
		std::mt19937 rng{seed};
		const RandomRegex regex{rng, 1 + seed % 5, seed % 2 == 1};
		CAPTURE(seed, regex.text);

		const Nfa direct{mata::parser::create_nfa(regex.text, Re2Options{.reduce = false})};
		const Nfa untrimmed{mata::parser::create_nfa(regex.text, Re2Options{.trim = false, .reduce = false})};
		const Nfa reduced{mata::parser::create_nfa(regex.text)};
		const Nfa with_epsilon{
			mata::parser::create_nfa(regex.text, Re2Options{.keep_epsilon = true, .epsilon = epsilon, .reduce = false})
		};
		const Nfa oracle{remove_epsilon(with_epsilon, epsilon)};
		CHECK(!direct.delta.get_used_symbols().contains(epsilon));
		CHECK(are_equivalent(direct, oracle));
		CHECK(are_equivalent(untrimmed, oracle));
		CHECK(are_equivalent(reduced, oracle));

		for (const Word& word : words) {
			CAPTURE(word);
			CHECK(direct.is_in_lang(word) == regex.accepts(word));
		}
	}
}
