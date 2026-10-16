/* re2parser.cc -- parser transforming re2 regular expressions to our Nfa
 *
 * Copyright (c) 2022 Michal Horky
 */

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>

#include "mata/alphabet.hh"
#include "mata/nfa/nfa.hh"
#include "mata/parser/re2parser.hh"
#include "re2/prog.h"
#include "re2/regexp.h"

namespace {
using namespace mata::nfa;

static_assert(
	static_cast<int>(Encoding::Latin1) == re2::Regexp::Latin1, "Encoding::Latin1 must match re2::Regexp::Latin1"
);
class RegexParser {
  public:
	/**
	 * Default RE2 options
	 */
	RE2::Options options{};

	RegexParser() = default;

	/**
	 * Creates parsed regex (i.e. Regexp*) from string regex_string
	 * @param regex_string Regex to be parsed as a string
	 * @param encoding Encoding of the regex, default is Latin1
	 * @return Parsed regex as RE2 Regexp*
	 */
	re2::Regexp* parse_regex_string(const std::string& regex_string, const Encoding encoding = Encoding::Latin1) const {
		re2::RegexpStatus status;

		const auto parsed_regex = re2::Regexp::Parse(
			regex_string, static_cast<re2::Regexp::ParseFlags>(options.ParseFlags() | static_cast<int>(encoding)),
			&status
		);
		if (parsed_regex == nullptr) {
			if (options.log_errors()) { LOG(ERROR) << "Error parsing '" << regex_string << "': " << status.Text(); }
			throw std::invalid_argument("Error parsing '" + regex_string + "': " + status.Text());
		}
		return parsed_regex;
	}

	/**
	 * Translates @p prog to an NFA with epsilon transitions.
	 *
	 * Every instruction reachable from the start becomes the state with the same number. A nop, a capture and an
	 *  empty-width instruction get an @p epsilon transition to their target, a byte range gets a transition over each
	 *  of its bytes, and every entry of an instruction list except the last one gets an @p epsilon transition to the
	 *  next entry.
	 * @param prog Flattened RE2 program to translate
	 * @param epsilon Symbol representing epsilon transitions
	 * @return Nfa with @p epsilon transitions created from prog
	 */
	static Nfa convert_prog_with_epsilon(re2::Prog* prog, const mata::Symbol epsilon) {
		const auto prog_size = static_cast<size_t>(prog->size());
		const auto start = static_cast<State>(prog->start());
		Nfa nfa(prog_size);
		nfa.initial.insert(start);

		std::vector<bool> visited(prog_size, false);
		std::vector<State> worklist{start};
		visited[start] = true;
		const auto visit = [&](const State pc) {
			if (!visited[pc]) {
				visited[pc] = true;
				worklist.push_back(pc);
			}
		};
		std::vector<mata::Symbol> symbols;
		while (!worklist.empty()) {
			const State pc{worklist.back()};
			worklist.pop_back();
			re2::Prog::Inst* inst = prog->inst(static_cast<int>(pc));
			switch (inst->opcode()) {
				case re2::kInstMatch:
					nfa.final.insert(pc);
					break;
				case re2::kInstFail:
				case re2::kInstAltMatch:
					break;
				case re2::kInstNop:
				case re2::kInstCapture:
				case re2::kInstEmptyWidth: {
					const auto target = static_cast<State>(inst->out());
					nfa.delta.add(pc, epsilon, target);
					visit(target);
					break;
				}
				case re2::kInstByteRange: {
					const auto target = static_cast<State>(inst->out());
					symbols.clear();
					append_byte_range_symbols(inst, symbols);
					for (const mata::Symbol symbol : symbols) { nfa.delta.add(pc, symbol, target); }
					visit(target);
					break;
				}
				default:
					throw_unsupported_opcode(inst);
			}
			if (!inst->last()) {
				nfa.delta.add(pc, epsilon, pc + 1);
				visit(pc + 1);
			}
		}
		return nfa;
	}

	/**
	 * Converts @p prog directly to an NFA without epsilon transitions.
	 *
	 * A state stands for the instruction list the program continues at: the start, and the target of every byte
	 *  range reachable from it. Its closure follows the non-consuming steps (a nop, a capture or an empty-width
	 *  instruction to its target, and every list entry except the last one to the next entry) to byte ranges and
	 *  matches. The state gets a transition over each byte of each byte range in the closure to the state of the
	 *  range's target, and is final when the closure contains a match. States are only created for lists
	 *  reachable from the start.
	 * @param prog Flattened RE2 program to convert
	 * @return Nfa without epsilon transitions created from prog
	 */
	static Nfa convert_prog_without_epsilon(re2::Prog* prog) {
		const auto prog_size = static_cast<size_t>(prog->size());
		constexpr State no_state{Limits::max_state};
		// The state continuing at each instruction, and the instruction each state continues at.
		std::vector<State> state_of_pc(prog_size, no_state);
		std::vector<int> pc_of_state;
		const auto state_continuing_at = [&](const int pc) {
			State& state = state_of_pc[static_cast<size_t>(pc)];
			if (state == no_state) {
				state = static_cast<State>(pc_of_state.size());
				pc_of_state.push_back(pc);
			}
			return state;
		};

		Nfa nfa;
		nfa.initial.insert(state_continuing_at(prog->start()));
		// The state whose closure last visited each instruction. Closures may contain cycles, e.g., in '(a*)*'.
		std::vector<State> visited_by(prog_size, no_state);
		std::vector<int> stack;
		std::vector<mata::Symbol> symbols;
		std::vector<std::pair<mata::Symbol, State>> moves;
		for (State state{0}; state < pc_of_state.size(); ++state) {
			bool is_final{false};
			moves.clear();
			stack.push_back(pc_of_state[state]);
			while (!stack.empty()) {
				const int pc{stack.back()};
				stack.pop_back();
				if (visited_by[static_cast<size_t>(pc)] == state) { continue; }
				visited_by[static_cast<size_t>(pc)] = state;
				re2::Prog::Inst* inst = prog->inst(pc);
				if (!inst->last()) { stack.push_back(pc + 1); }
				switch (inst->opcode()) {
					case re2::kInstMatch:
						is_final = true;
						break;
					case re2::kInstFail:
					case re2::kInstAltMatch:
						break;
					case re2::kInstNop:
					case re2::kInstCapture:
					case re2::kInstEmptyWidth:
						stack.push_back(inst->out());
						break;
					case re2::kInstByteRange: {
						const State target{state_continuing_at(inst->out())};
						symbols.clear();
						append_byte_range_symbols(inst, symbols);
						for (const mata::Symbol symbol : symbols) { moves.emplace_back(symbol, target); }
						break;
					}
					default:
						throw_unsupported_opcode(inst);
				}
			}

			if (is_final) { nfa.final.insert(state); }
			if (moves.empty()) { continue; }
			std::ranges::sort(moves);
			const auto [duplicates_begin, duplicates_end] = std::ranges::unique(moves);
			moves.erase(duplicates_begin, duplicates_end);
			StatePost& state_post{nfa.delta.mutable_state_post(state)};
			for (const auto& [symbol, target] : moves) {
				if (state_post.empty() || state_post.back().symbol != symbol) { state_post.emplace_back(symbol); }
				state_post.back().push_back(target);
			}
		}
		// States without transitions that are not final still exist.
		nfa.add_state(static_cast<State>(pc_of_state.size() - 1));
		return nfa;
	}

  private:
	/**
	 * Appends the bytes the byte range @p inst consumes to @p symbols.
	 *
	 * A case-folding range consumes the upper-case variant of each of its lower-case ASCII letters as well.
	 */
	static void append_byte_range_symbols(re2::Prog::Inst* inst, std::vector<mata::Symbol>& symbols) {
		const auto hi = static_cast<mata::Symbol>(inst->hi());
		for (auto symbol = static_cast<mata::Symbol>(inst->lo()); symbol <= hi; ++symbol) {
			symbols.push_back(symbol);
			// The same symbol in lowercase and uppercase is 32 symbols from each other in ASCII.
			if (inst->foldcase() && symbol >= 'a' && symbol <= 'z') {
				constexpr int ascii_shift_value{32};
				symbols.push_back(symbol - ascii_shift_value);
			}
		}
	}

	/// A flattened program contains no other opcodes than the ones handled by the converters.
	[[noreturn]] static void throw_unsupported_opcode(re2::Prog::Inst* inst) {
		throw std::runtime_error{
			"create_nfa: unsupported RE2 instruction opcode " + std::to_string(static_cast<int>(inst->opcode()))
		};
	}
};
} // namespace

mata::nfa::Nfa mata::parser::create_nfa(const std::string& pattern, const Re2Options& options) {
	RegexParser regex_parser{};
	const auto parsed_regex = regex_parser.parse_regex_string(pattern, options.encoding);
	const auto program = parsed_regex->CompileToProg(regex_parser.options.max_mem() * 2 / 3);
	mata::nfa::Nfa result{
		options.keep_epsilon ? RegexParser::convert_prog_with_epsilon(program, options.epsilon)
							 : RegexParser::convert_prog_without_epsilon(program)
	};
	delete program;
	// Decrements reference count and deletes object if the count reaches 0
	parsed_regex->Decref();

	if (options.trim) { result.trim(); }
	if (options.reduce) { result = mata::nfa::reduce(result); }

	return result;
}

void mata::parser::create_nfa(nfa::Nfa* nfa, const std::string& pattern, const Re2Options& options) {
	*nfa = create_nfa(pattern, options);
}

mata::nfa::Nfa mata::parser::create_nfa(
	const std::string& pattern,
	const bool use_epsilon,
	const mata::Symbol epsilon_value,
	const bool use_reduce,
	const Encoding encoding
) {
	return create_nfa(
		pattern, {.keep_epsilon = use_epsilon,
				  .epsilon = epsilon_value,
				  .encoding = encoding,
				  .trim = true,
				  .reduce = use_reduce}
	);
}

void mata::parser::create_nfa(
	nfa::Nfa* nfa,
	const std::string& pattern,
	const bool use_epsilon,
	const mata::Symbol epsilon_value,
	const bool use_reduce,
	const Encoding encoding
) {
	*nfa = create_nfa(pattern, use_epsilon, epsilon_value, use_reduce, encoding);
}
