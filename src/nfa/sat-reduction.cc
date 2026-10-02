/** @file
 * @brief Reduction of NFAs to automata with the minimum number of states using external SAT and QBF solvers.
 *
 * The reduction is a counterexample-guided search. For k = 1, 2, ..., a solver looks for an automaton with k states
 *  consistent with a set of sample words: the words to accept and the words to reject. A found candidate is checked
 *  for equivalence with the reduced automaton. If they differ, the counterexample is added to the samples and the
 *  solver is asked again. If no k-state automaton is consistent with the samples, no k-state automaton accepts the
 *  language either, and k is increased.
 *
 * Based on the original implementation by Veronika Molnarova (https://github.com/VeriFIT/mata/pull/407).
 */

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
	#define MATA_EXTERNAL_SOLVERS_SUPPORTED
	#include <fcntl.h>
	#include <spawn.h>
	#include <sys/stat.h>
	#include <sys/wait.h>
	#include <unistd.h>
	#if defined(__APPLE__)
		#include <crt_externs.h>
	#endif
#endif

#include "mata/nfa/algorithms.hh"
#include "mata/nfa/nfa.hh"

using namespace mata::nfa;
using mata::Symbol;
using mata::Word;
using mata::nfa::algorithms::ExternalSolverNotFound;

namespace {

/// A literal in the DIMACS format: a positive or a negated variable.
using Literal = int;
using Clause = std::vector<Literal>;

/// Quantifier blocks of the formulae passed to QBF solvers, from the outermost one.
enum class Quantifier : uint8_t {
	OuterExists, ///< The automaton and other variables whose values are read from the solver.
	ForAll, ///< Runs over the words to reject.
	InnerExists, ///< Auxiliary variables depending on the universally quantified ones.
};

/// A formula in conjunctive normal form, optionally with a quantifier prefix.
class Formula {
  public:
	Literal new_variable(const Quantifier quantifier = Quantifier::OuterExists) {
		if (quantifiers_.size() >= static_cast<size_t>(std::numeric_limits<Literal>::max())) {
			throw std::length_error("the formula for the solver has too many variables");
		}
		quantifiers_.push_back(quantifier);
		return static_cast<Literal>(quantifiers_.size());
	}

	/// Create @p count consecutive variables and return the first one (0 if @p count is 0).
	Literal new_variables(const size_t count, const Quantifier quantifier = Quantifier::OuterExists) {
		Literal first{0};
		for (size_t index{0}; index < count; ++index) {
			const Literal variable{new_variable(quantifier)};
			if (index == 0) { first = variable; }
		}
		return first;
	}

	void add_clause(Clause clause) { clauses_.push_back(std::move(clause)); }

	size_t num_of_variables() const { return quantifiers_.size(); }

	/// Write the formula in the DIMACS CNF format, with the quantifier prefix of the QDIMACS format if @p quantified.
	void write(std::ostream& output, const bool quantified) const {
		output << "p cnf " << num_of_variables() << ' ' << clauses_.size() << '\n';
		if (quantified) { write_prefix(output); }
		for (const Clause& clause : clauses_) {
			for (const Literal literal : clause) { output << literal << ' '; }
			output << "0\n";
		}
	}

  private:
	std::vector<Quantifier> quantifiers_{}; ///< The quantifier of each variable; variable v is at index v - 1.
	std::vector<Clause> clauses_{};

	void write_prefix(std::ostream& output) const {
		std::array<std::vector<Literal>, 3> blocks{};
		for (size_t index{0}; index < quantifiers_.size(); ++index) {
			blocks[static_cast<size_t>(quantifiers_[index])].push_back(static_cast<Literal>(index + 1));
		}
		// Without universal variables, both existential blocks form a single one.
		if (blocks[static_cast<size_t>(Quantifier::ForAll)].empty()) {
			auto& outer = blocks[static_cast<size_t>(Quantifier::OuterExists)];
			auto& inner = blocks[static_cast<size_t>(Quantifier::InnerExists)];
			outer.insert(outer.end(), inner.begin(), inner.end());
			inner.clear();
		}
		const std::array<char, 3> types{'e', 'a', 'e'};
		for (size_t block{0}; block < blocks.size(); ++block) {
			if (blocks[block].empty()) { continue; }
			output << types[block];
			for (const Literal variable : blocks[block]) { output << ' ' << variable; }
			output << " 0\n";
		}
	}
};

/// Variables describing an automaton with states 0, ..., num_of_states - 1 over symbols 0, ..., num_of_symbols - 1.
class AutomatonVariables {
  public:
	AutomatonVariables(
		Formula& formula, const size_t num_of_states, const size_t num_of_symbols, const bool deterministic
	)
		: num_of_states_{num_of_states},
		  num_of_symbols_{num_of_symbols},
		  deterministic_{deterministic},
		  first_transition_{formula.new_variables(num_of_states * num_of_symbols * num_of_states)},
		  // A deterministic automaton has the single initial state 0, other automata a variable for each state.
		  first_initial_{deterministic ? 0 : formula.new_variables(num_of_states)},
		  first_final_{formula.new_variables(num_of_states)} {}

	size_t num_of_states() const { return num_of_states_; }
	size_t num_of_symbols() const { return num_of_symbols_; }

	Literal transition(const State source, const Symbol symbol, const State target) const {
		return first_transition_ + static_cast<Literal>((source * num_of_symbols_ + symbol) * num_of_states_ + target);
	}

	Literal initial(const State state) const { return first_initial_ + static_cast<Literal>(state); }
	Literal final(const State state) const { return first_final_ + static_cast<Literal>(state); }

	/// Construct the automaton described by the values of the variables in @p model.
	Nfa decode(const std::vector<bool>& model) const {
		Nfa result{num_of_states_};
		for (State state{0}; state < num_of_states_; ++state) {
			if (deterministic_ ? state == 0 : model[static_cast<size_t>(initial(state))]) {
				result.initial.insert(state);
			}
			if (model[static_cast<size_t>(final(state))]) { result.final.insert(state); }
			for (Symbol symbol{0}; symbol < num_of_symbols_; ++symbol) {
				for (State target{0}; target < num_of_states_; ++target) {
					if (model[static_cast<size_t>(transition(state, symbol, target))]) {
						result.delta.add(state, symbol, target);
					}
				}
			}
		}
		return result;
	}

  private:
	size_t num_of_states_;
	size_t num_of_symbols_;
	bool deterministic_;
	Literal first_transition_;
	Literal first_initial_;
	Literal first_final_;
};

/// The sample words the searched automaton has to accept and to reject.
struct Samples {
	std::set<Word> accepted{};
	std::set<Word> rejected{};
};

/// A prefix tree of sample words.
class PrefixTree {
  public:
	struct Node {
		std::map<Symbol, size_t> children{}; ///< The node of the prefix extended by a symbol.
		bool accepted{false}; ///< A word to accept ends here.
		bool rejected{false}; ///< A word to reject ends here.
		bool prefix_of_accepted{false}; ///< The node is a prefix of a word to accept.
	};

	PrefixTree() : nodes_{Node{}} {}

	void insert(const Word& word, const bool accepted) {
		size_t node{0};
		nodes_[node].prefix_of_accepted |= accepted;
		for (const Symbol symbol : word) {
			const auto [child, inserted] = nodes_[node].children.try_emplace(symbol, nodes_.size());
			if (inserted) { nodes_.emplace_back(); }
			node = child->second;
			nodes_[node].prefix_of_accepted |= accepted;
		}
		(accepted ? nodes_[node].accepted : nodes_[node].rejected) = true;
	}

	const std::vector<Node>& nodes() const { return nodes_; }

  private:
	std::vector<Node> nodes_; ///< The root is node 0. Children have larger indices than their parents.
};

/// Variables x[j][q] for positions j = 0, ..., @p num_of_positions - 1 and states q.
std::vector<std::vector<Literal>> new_variable_table(
	Formula& formula, const size_t num_of_positions, const size_t num_of_states, const Quantifier quantifier
) {
	std::vector<std::vector<Literal>> variables(num_of_positions, std::vector<Literal>(num_of_states));
	for (std::vector<Literal>& row : variables) {
		for (Literal& variable : row) { variable = formula.new_variable(quantifier); }
	}
	return variables;
}

/// Require that the (nondeterministic) automaton has an accepting run on @p word.
void add_accepting_run(Formula& formula, const AutomatonVariables& aut, const Word& word) {
	const size_t num_of_states{aut.num_of_states()};
	// run[j][q]: the run is in state q after reading j symbols.
	const auto run{new_variable_table(formula, word.size() + 1, num_of_states, Quantifier::OuterExists)};
	for (const std::vector<Literal>& states : run) { formula.add_clause(states); }
	for (State state{0}; state < num_of_states; ++state) {
		formula.add_clause({-run.front()[state], aut.initial(state)});
		formula.add_clause({-run.back()[state], aut.final(state)});
	}
	for (size_t position{0}; position < word.size(); ++position) {
		for (State source{0}; source < num_of_states; ++source) {
			for (State target{0}; target < num_of_states; ++target) {
				formula.add_clause(
					{-run[position][source], -run[position + 1][target], aut.transition(source, word[position], target)}
				);
			}
		}
	}
}

/// Require that the (nondeterministic) automaton rejects @p words by over-approximating the states reachable by
///  their prefixes and forbidding final states among the states reachable by the whole words.
void add_rejected_words_by_reachability(Formula& formula, const AutomatonVariables& aut, const std::set<Word>& words) {
	if (words.empty()) { return; }
	PrefixTree tree{};
	for (const Word& word : words) { tree.insert(word, false); }
	const size_t num_of_states{aut.num_of_states()};
	// reachable[node][q]: q is (possibly) reachable by the prefix of the node.
	const auto reachable{new_variable_table(formula, tree.nodes().size(), num_of_states, Quantifier::OuterExists)};
	for (State state{0}; state < num_of_states; ++state) {
		formula.add_clause({-aut.initial(state), reachable[0][state]});
	}
	for (size_t node{0}; node < tree.nodes().size(); ++node) {
		for (const auto& [symbol, child] : tree.nodes()[node].children) {
			for (State source{0}; source < num_of_states; ++source) {
				for (State target{0}; target < num_of_states; ++target) {
					formula.add_clause(
						{-reachable[node][source], -aut.transition(source, symbol, target), reachable[child][target]}
					);
				}
			}
		}
		if (tree.nodes()[node].rejected) {
			for (State state{0}; state < num_of_states; ++state) {
				formula.add_clause({-reachable[node][state], -aut.final(state)});
			}
		}
	}
}

/// Require that the (nondeterministic) automaton rejects @p word: every run on @p word, chosen by universally
///  quantified variables, is not accepting.
void add_rejected_word_by_universal_runs(Formula& formula, const AutomatonVariables& aut, const Word& word) {
	const size_t num_of_states{aut.num_of_states()};
	size_t num_of_bits{0}; // Bits encoding the states of the run.
	while ((size_t{1} << num_of_bits) < num_of_states) { ++num_of_bits; }

	// state[j][q]: the chosen run is in state q after reading j symbols. States 0, ..., num_of_states - 2 are chosen
	//  by their binary codes, state num_of_states - 1 by all the remaining codes.
	const auto state{new_variable_table(formula, word.size() + 1, num_of_states, Quantifier::InnerExists)};
	const State last_state{num_of_states - 1};
	for (size_t position{0}; position <= word.size(); ++position) {
		std::vector<Literal> bits(num_of_bits);
		for (Literal& bit : bits) { bit = formula.new_variable(Quantifier::ForAll); }
		Clause other_states_or_last{state[position][last_state]};
		for (State chosen{0}; chosen < last_state; ++chosen) {
			Clause code_or_not_chosen{state[position][chosen]};
			for (size_t bit{0}; bit < num_of_bits; ++bit) {
				const Literal code_bit{(chosen >> bit) & 1 ? bits[bit] : -bits[bit]};
				formula.add_clause({-state[position][chosen], code_bit});
				code_or_not_chosen.push_back(-code_bit);
			}
			formula.add_clause(code_or_not_chosen);
			formula.add_clause({-state[position][last_state], -state[position][chosen]});
			other_states_or_last.push_back(state[position][chosen]);
		}
		formula.add_clause(other_states_or_last);
	}

	// successor[j][p]: p is (possibly) a successor of the chosen state after reading j symbols.
	const auto successor{new_variable_table(formula, word.size(), num_of_states, Quantifier::InnerExists)};
	for (size_t position{0}; position < word.size(); ++position) {
		for (State source{0}; source < num_of_states; ++source) {
			for (State target{0}; target < num_of_states; ++target) {
				formula.add_clause(
					{-state[position][source], -aut.transition(source, word[position], target),
					 successor[position][target]}
				);
			}
		}
	}

	// The chosen run starts in a non-initial state, continues to a non-successor, or ends in a non-final state.
	Clause not_accepting{};
	const auto add_reason{[&](const std::initializer_list<Literal> conjunction) {
		const Literal reason{formula.new_variable(Quantifier::InnerExists)};
		for (const Literal literal : conjunction) { formula.add_clause({-reason, literal}); }
		not_accepting.push_back(reason);
	}};
	for (State target{0}; target < num_of_states; ++target) {
		add_reason({state.front()[target], -aut.initial(target)});
		for (size_t position{0}; position < word.size(); ++position) {
			add_reason({state[position + 1][target], -successor[position][target]});
		}
		add_reason({state.back()[target], -aut.final(target)});
	}
	formula.add_clause(not_accepting);
}

/// Require that the deterministic automaton accepts and rejects the sample words.
void add_deterministic_samples(Formula& formula, const AutomatonVariables& aut, const Samples& samples) {
	const size_t num_of_states{aut.num_of_states()};
	for (State source{0}; source < num_of_states; ++source) {
		for (Symbol symbol{0}; symbol < aut.num_of_symbols(); ++symbol) {
			for (State target{0}; target < num_of_states; ++target) {
				for (State other_target{target + 1}; other_target < num_of_states; ++other_target) {
					formula.add_clause(
						{-aut.transition(source, symbol, target), -aut.transition(source, symbol, other_target)}
					);
				}
			}
		}
	}

	PrefixTree tree{};
	for (const Word& word : samples.accepted) { tree.insert(word, true); }
	for (const Word& word : samples.rejected) { tree.insert(word, false); }
	// state[node][q]: the run on the prefix of the node ends in q. Exact for prefixes of words to accept, which have
	//  to have a run, an over-approximation for the other prefixes, whose run may not exist.
	const auto state{new_variable_table(formula, tree.nodes().size(), num_of_states, Quantifier::OuterExists)};
	formula.add_clause({state[0][0]});
	for (State other{1}; other < num_of_states; ++other) { formula.add_clause({-state[0][other]}); }
	for (size_t node{0}; node < tree.nodes().size(); ++node) {
		for (const auto& [symbol, child] : tree.nodes()[node].children) {
			const bool exact{tree.nodes()[child].prefix_of_accepted};
			if (exact) { formula.add_clause(state[child]); }
			for (State target{0}; target < num_of_states; ++target) {
				for (State other_target{target + 1}; exact && other_target < num_of_states; ++other_target) {
					formula.add_clause({-state[child][target], -state[child][other_target]});
				}
				for (State source{0}; source < num_of_states; ++source) {
					formula.add_clause(
						{-state[node][source], -aut.transition(source, symbol, target), state[child][target]}
					);
					if (exact) {
						formula.add_clause(
							{-state[node][source], -state[child][target], aut.transition(source, symbol, target)}
						);
					}
				}
			}
		}
		const PrefixTree::Node& current{tree.nodes()[node]};
		for (State target{0}; current.accepted && target < num_of_states; ++target) {
			formula.add_clause({-state[node][target], aut.final(target)});
		}
		for (State target{0}; current.rejected && target < num_of_states; ++target) {
			formula.add_clause({-state[node][target], -aut.final(target)});
		}
	}
}

/// What automata the reduction searches for and with which solver.
enum class Mode : uint8_t {
	SatNfa, ///< NFAs, with a SAT solver.
	SatDfa, ///< Deterministic automata, with a SAT solver.
	QbfNfa, ///< NFAs, with a QBF solver.
};

/// Construct the formula describing automata with @p num_of_states states consistent with @p samples.
Formula encode(
	const Mode mode,
	const size_t num_of_states,
	const size_t num_of_symbols,
	const Samples& samples,
	std::optional<AutomatonVariables>& aut
) {
	Formula formula{};
	aut.emplace(formula, num_of_states, num_of_symbols, mode == Mode::SatDfa);
	if (mode == Mode::SatDfa) {
		add_deterministic_samples(formula, *aut, samples);
		return formula;
	}
	// Without loss of generality, state 0 is initial (the reduced language is not empty).
	formula.add_clause({aut->initial(0)});
	for (const Word& word : samples.accepted) { add_accepting_run(formula, *aut, word); }
	if (mode == Mode::SatNfa) {
		add_rejected_words_by_reachability(formula, *aut, samples.rejected);
	} else {
		for (const Word& word : samples.rejected) { add_rejected_word_by_universal_runs(formula, *aut, word); }
	}
	return formula;
}

/// The answer of a solver.
struct SolverAnswer {
	bool satisfiable{false};
	std::vector<bool> model{}; ///< The values of the variables, indexed by the variable number.
};

#ifdef MATA_EXTERNAL_SOLVERS_SUPPORTED

/// Split a solver command into its executable and arguments, separated by whitespace. Single or double quotes enclose
///  parts containing whitespace. No other shell syntax is interpreted.
std::vector<std::string> split_command(const std::string& command) {
	std::vector<std::string> arguments{};
	std::optional<std::string> argument{};
	char quote{'\0'};
	for (const char character : command) {
		if (quote != '\0') {
			if (character == quote) {
				quote = '\0';
			} else {
				argument->push_back(character);
			}
		} else if (character == '\'' || character == '"') {
			quote = character;
			if (!argument.has_value()) { argument.emplace(); }
		} else if (std::isspace(static_cast<unsigned char>(character)) != 0) {
			if (argument.has_value()) { arguments.push_back(std::move(*argument)); }
			argument.reset();
		} else {
			if (!argument.has_value()) { argument.emplace(); }
			argument->push_back(character);
		}
	}
	if (quote != '\0') {
		throw ExternalSolverNotFound("The solver command '" + command + "' has an unterminated quote.");
	}
	if (argument.has_value()) { arguments.push_back(std::move(*argument)); }
	return arguments;
}

/// The last lines of a solver output, for error messages.
std::string output_excerpt(const std::string& output) {
	constexpr size_t max_length{1'000};
	if (output.find_first_not_of(" \t\r\n") == std::string::npos) { return " The solver printed nothing."; }
	const std::string excerpt{output.size() > max_length ? "..." + output.substr(output.size() - max_length) : output};
	return " The solver printed:\n" + excerpt;
}

/// Parse the output of a SAT solver in the SAT competition format, or of a QBF solver in the QDIMACS output format.
SolverAnswer parse_solver_output(
	const std::string& output,
	const int exit_code,
	const bool quantified,
	const size_t num_of_variables,
	const std::string& solver
) {
	std::optional<bool> satisfiable{};
	bool has_model{false};
	SolverAnswer answer{.satisfiable = false, .model = std::vector<bool>(num_of_variables + 1, false)};
	std::istringstream lines{output};
	for (std::string line; std::getline(lines, line);) {
		std::istringstream tokens{line};
		std::string tag{};
		tokens >> tag;
		if (tag == "s") {
			// "s SATISFIABLE" in the SAT competition format, "s cnf 1 <variables> <clauses>" in the QDIMACS format.
			std::string result{};
			tokens >> result;
			if (result == "cnf") { tokens >> result; }
			if (result == "SATISFIABLE" || result == "1") {
				satisfiable = true;
			} else if (result == "UNSATISFIABLE" || result == "0") {
				satisfiable = false;
			} else {
				throw std::runtime_error(
					"The solver '" + solver + "' did not decide the formula." + output_excerpt(output)
				);
			}
		} else if (tag == "v" || (quantified && tag == "V")) {
			has_model = true;
			const auto max_variable{static_cast<long long>(num_of_variables)};
			for (std::string token; tokens >> token;) {
				long long literal{0};
				size_t parsed{0};
				try {
					literal = std::stoll(token, &parsed);
				} catch (const std::exception&) { parsed = 0; }
				if (parsed != token.size() || literal < -max_variable || literal > max_variable) {
					throw std::runtime_error(
						"The solver '" + solver + "' printed an unexpected value '" + token + "'." +
						output_excerpt(output)
					);
				}
				if (literal != 0) { answer.model[static_cast<size_t>(literal < 0 ? -literal : literal)] = literal > 0; }
			}
		}
	}
	// Solvers also report the result by their exit codes: 10 for satisfiable formulae, 20 for unsatisfiable ones.
	if (!satisfiable.has_value() && (exit_code == 10 || exit_code == 20)) { satisfiable = exit_code == 10; }
	if (!satisfiable.has_value()) {
		throw std::runtime_error(
			"The solver '" + solver + "' exited with code " + std::to_string(exit_code) +
			" without reporting a result." + output_excerpt(output)
		);
	}
	if (*satisfiable && !has_model) {
		throw std::runtime_error(
			"The solver '" + solver + "' found the formula satisfiable but did not print " +
			(quantified ? "the values of the outermost existential variables (\"V\" lines); QBF solvers usually need "
						  "an option such as --qdo for that."
						: "a model (\"v\" lines).") +
			output_excerpt(output)
		);
	}
	answer.satisfiable = *satisfiable;
	return answer;
}

/// A temporary directory removed with its contents on destruction.
class TemporaryDirectory {
  public:
	TemporaryDirectory() : path_{create()} {}
	TemporaryDirectory(const TemporaryDirectory&) = delete;
	TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
	~TemporaryDirectory() {
		std::error_code error{};
		std::filesystem::remove_all(path_, error);
	}

	const std::filesystem::path& path() const { return path_; }

  private:
	std::filesystem::path path_;

	static std::filesystem::path create() {
		std::string name{(std::filesystem::temp_directory_path() / "mata-solver-XXXXXX").string()};
		if (::mkdtemp(name.data()) == nullptr) {
			throw std::system_error(
				errno, std::generic_category(), "cannot create a temporary directory for the solver"
			);
		}
		return name;
	}
};

char** environment() {
	#if defined(__APPLE__)
	return *_NSGetEnviron();
	#else
	return environ;
	#endif
}

bool is_executable_file(const std::filesystem::path& path) {
	std::error_code error{};
	return std::filesystem::is_regular_file(path, error) && ::access(path.c_str(), X_OK) == 0;
}

/// Find @p name the same way a shell would: as a path if it contains a slash, otherwise in the directories in PATH.
std::optional<std::filesystem::path> find_executable(const std::string& name) {
	if (name.find('/') != std::string::npos) {
		return is_executable_file(name) ? std::optional<std::filesystem::path>{name} : std::nullopt;
	}
	const char* const path{std::getenv("PATH")};
	if (path == nullptr) { return std::nullopt; }
	std::string_view directories{path};
	while (true) {
		const size_t separator{directories.find(':')};
		const std::string_view directory{directories.substr(0, separator)};
		const std::filesystem::path candidate{std::filesystem::path{directory.empty() ? "." : directory} / name};
		if (is_executable_file(candidate)) { return candidate; }
		if (separator == std::string_view::npos) { return std::nullopt; }
		directories.remove_prefix(separator + 1);
	}
}

/// Run a process with @p arguments, its standard and error output redirected to @p output_file, and return its exit
///  code and output.
std::pair<int, std::string> run_process(std::vector<std::string> arguments, const std::filesystem::path& output_file) {
	posix_spawn_file_actions_t actions{};
	if (const int error{::posix_spawn_file_actions_init(&actions)}; error != 0) {
		throw std::system_error(error, std::generic_category(), "cannot prepare running the solver");
	}
	const std::unique_ptr<posix_spawn_file_actions_t, int (*)(posix_spawn_file_actions_t*)> actions_guard{
		&actions, ::posix_spawn_file_actions_destroy
	};
	const std::string output_path{output_file.string()};
	::posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
	::posix_spawn_file_actions_addopen(
		&actions, STDOUT_FILENO, output_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR
	);
	::posix_spawn_file_actions_adddup2(&actions, STDOUT_FILENO, STDERR_FILENO);

	std::vector<char*> argv{};
	for (std::string& argument : arguments) { argv.push_back(argument.data()); }
	argv.push_back(nullptr);
	pid_t process{};
	if (const int error{::posix_spawn(&process, argv.front(), &actions, nullptr, argv.data(), environment())};
		error != 0) {
		throw ExternalSolverNotFound(
			"Cannot run the solver '" + arguments.front() + "': " + std::generic_category().message(error) + "."
		);
	}
	int status{0};
	while (::waitpid(process, &status, 0) == -1) {
		if (errno != EINTR) {
			throw std::system_error(errno, std::generic_category(), "cannot wait for the solver to finish");
		}
	}
	std::ifstream stream{output_file};
	std::string output{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
	if (WIFSIGNALED(status)) {
		throw std::runtime_error(
			"The solver '" + arguments.front() + "' was terminated by signal " + std::to_string(WTERMSIG(status)) +
			"." + output_excerpt(output)
		);
	}
	return {WIFEXITED(status) ? WEXITSTATUS(status) : -1, std::move(output)};
}

/// The command running a SAT or QBF solver: @p command, or the value of the environment variable, or the first default
///  solver found in PATH. The output of solvers run to check their options is written to @p directory.
std::vector<std::string>
	find_solver(const bool quantified, const std::string& command, const std::filesystem::path& directory) {
	const std::string kind{quantified ? "QBF" : "SAT"};
	const std::string variable{"MATA_" + kind + "_SOLVER"};
	const auto resolve{[&](const std::string& given, const std::string& origin) {
		std::vector<std::string> arguments{split_command(given)};
		if (arguments.empty()) { throw ExternalSolverNotFound("The " + kind + " solver " + origin + " is empty."); }
		const std::optional<std::filesystem::path> executable{find_executable(arguments.front())};
		if (!executable.has_value()) {
			throw ExternalSolverNotFound(
				"The " + kind + " solver '" + arguments.front() + "' " + origin +
				" cannot be found or is not executable."
			);
		}
		arguments.front() = executable->string();
		return arguments;
	}};
	if (!command.empty()) { return resolve(command, "given by the \"solver\" parameter"); }
	if (const char* const value{std::getenv(variable.c_str())}; value != nullptr && *value != '\0') {
		return resolve(value, "given by the environment variable " + variable);
	}

	const std::vector<std::vector<std::string>> defaults{
		quantified ? std::vector<std::vector<std::string>>{{"depqbf", "--qdo"}, {"caqe", "--qdo"}}
				   : std::vector<std::vector<std::string>>{
						 {"cadical", "-q"}, {"kissat", "-q"}, {"cryptominisat5", "--verb", "0"}, {"picosat"}
					 }
	};
	std::string names{};
	for (std::vector<std::string> solver : defaults) {
		if (const std::optional<std::filesystem::path> executable{find_executable(solver.front())}) {
			const bool is_depqbf{solver.front() == "depqbf"};
			solver.front() = executable->string();
			// DepQBF built with Nenofex (e.g., version 6 from Homebrew) prints the values of variables only with
			//  --no-dynamic-nenofex, which older versions reject.
			if (is_depqbf &&
				run_process({solver.front(), "--help"}, directory / "help.txt").second.find("--no-dynamic-nenofex") !=
					std::string::npos) {
				solver.emplace_back("--no-dynamic-nenofex");
			}
			return solver;
		}
		names += (names.empty() ? "" : ", ") + solver.front();
	}
	throw ExternalSolverNotFound(
		"No " + kind + " solver found. Install one of " + names +
		", or give the command running a solver by the environment variable " + variable +
		" or by the \"solver\" parameter."
	);
}

/// An external SAT or QBF solver run on formulae written to temporary files.
class ExternalSolver {
  public:
	/// Find the solver given by @p command, or by the environment, or the first available default solver.
	ExternalSolver(const bool quantified, const std::string& command)
		: quantified_{quantified},
		  directory_{},
		  arguments_{find_solver(quantified, command, directory_.path())} {}

	/// Decide @p formula and return the values of its (outermost existential) variables if it is satisfiable.
	SolverAnswer solve(const Formula& formula) {
		const std::filesystem::path file{directory_.path() / (quantified_ ? "formula.qdimacs" : "formula.cnf")};
		std::ofstream stream{file};
		formula.write(stream, quantified_);
		stream.close();
		if (!stream) { throw std::runtime_error("Cannot write the formula for the solver to " + file.string() + "."); }
		std::vector<std::string> arguments{arguments_};
		arguments.push_back(file.string());
		const auto [exit_code, output]{run_process(std::move(arguments), directory_.path() / "output.txt")};
		return parse_solver_output(output, exit_code, quantified_, formula.num_of_variables(), arguments_.front());
	}

  private:
	bool quantified_;
	TemporaryDirectory directory_; ///< Initialized before @c arguments_, which may need it.
	std::vector<std::string> arguments_;
};

#else

/// External solvers cannot be run on this platform.
class ExternalSolver {
  public:
	ExternalSolver(const bool quantified, const std::string& /* command */) {
		throw ExternalSolverNotFound(
			std::string{"Running an external "} + (quantified ? "QBF" : "SAT") +
			" solver is not supported on this platform."
		);
	}

	SolverAnswer solve(const Formula& /* formula */) { return {}; }
};

#endif

/// Copy of @p aut with the symbols of transitions changed by @p rename.
template <class Rename>
Nfa with_renamed_symbols(const Nfa& aut, const Rename& rename, std::shared_ptr<mata::Alphabet> alphabet = nullptr) {
	Nfa result{aut.num_of_states(), aut.initial, aut.final, std::move(alphabet)};
	for (const Transition& transition : aut.delta.transitions()) {
		result.delta.add(transition.source, rename(transition.symbol), transition.target);
	}
	return result;
}

/// Remove the transitions, initial states, and final states of @p aut that @p language does not need.
void remove_redundant_parts(Nfa& aut, const Nfa& language) {
	const auto equivalent{[&] { return are_equivalent(language, aut); }};
	std::vector<Transition> transitions{};
	for (const Transition& transition : aut.delta.transitions()) { transitions.push_back(transition); }
	for (const Transition& transition : transitions) {
		aut.delta.remove(transition);
		if (!equivalent()) { aut.delta.add(transition); }
	}
	for (const State state : std::vector<State>(aut.initial.begin(), aut.initial.end())) {
		aut.initial.erase(state);
		if (!equivalent()) { aut.initial.insert(state); }
	}
	for (const State state : std::vector<State>(aut.final.begin(), aut.final.end())) {
		aut.final.erase(state);
		if (!equivalent()) { aut.final.insert(state); }
	}
}

Nfa reduce_exactly(const Nfa& nfa, const Mode mode, const std::string& solver_command) {
	// Work with a trimmed epsilon-free automaton over symbols 0, ..., n - 1.
	Nfa input{trim(nfa)};
	const mata::utils::OrdVector<Symbol> used_symbols{input.delta.get_used_symbols()};
	if (used_symbols.contains(EPSILON)) { input = trim(remove_epsilon(input)); }
	const std::vector<Symbol> symbols{input.delta.get_used_symbols().to_vector()}; // Sorted.
	const Nfa target{with_renamed_symbols(input, [&](const Symbol symbol) {
		return static_cast<Symbol>(std::lower_bound(symbols.begin(), symbols.end(), symbol) - symbols.begin());
	})};
	const auto restore_symbols{[&](const Nfa& aut) {
		return with_renamed_symbols(aut, [&](const Symbol symbol) { return symbols[symbol]; }, nfa.alphabet);
	}};
	if (target.num_of_states() == 0) { return Nfa{0, {}, {}, nfa.alphabet}; } // The empty language.

	// An automaton found without a solver, minimal once no smaller automaton exists.
	const Nfa upper_bound{
		mode == Mode::SatDfa ? trim(determinize(target)) : trim(reduce(target, nullptr, {{"algorithm", "simulation"}}))
	};
	Samples samples{};
	(target.is_in_lang(Word{}) ? samples.accepted : samples.rejected).insert(Word{});
	std::optional<ExternalSolver> solver{};
	for (size_t num_of_states{1}; num_of_states < upper_bound.num_of_states(); ++num_of_states) {
		while (true) {
			std::optional<AutomatonVariables> aut{};
			const Formula formula{encode(mode, num_of_states, symbols.size(), samples, aut)};
			if (!solver.has_value()) { solver.emplace(mode == Mode::QbfNfa, solver_command); }
			const SolverAnswer answer{solver->solve(formula)};
			if (!answer.satisfiable) { break; }
			Nfa candidate{aut->decode(answer.model)};
			Run counterexample{};
			if (are_equivalent(target, candidate, {{"algorithm", "antichains"}}, &counterexample)) {
				if (mode != Mode::SatDfa) { remove_redundant_parts(candidate, target); }
				return restore_symbols(candidate);
			}
			std::set<Word>& words{target.is_in_lang(counterexample.word) ? samples.accepted : samples.rejected};
			if (!words.insert(counterexample.word).second) {
				throw std::logic_error(
					"The automaton found by the solver does not agree with the sample words it was built from."
				);
			}
		}
	}
	return restore_symbols(upper_bound);
}

} // namespace

Nfa mata::nfa::algorithms::reduce_sat(const Nfa& nfa, const std::string& type, const std::string& solver) {
	if (type != "nfa" && type != "dfa") {
		throw std::runtime_error(std::string{__func__} + " received an unknown type of automaton: " + type);
	}
	return reduce_exactly(nfa, type == "nfa" ? Mode::SatNfa : Mode::SatDfa, solver);
}

Nfa mata::nfa::algorithms::reduce_qbf(const Nfa& nfa, const std::string& solver) {
	return reduce_exactly(nfa, Mode::QbfNfa, solver);
}
