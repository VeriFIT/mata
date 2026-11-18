/* sat-reduction.cc -- Tests for the reduction of NFAs to minimal automata using external SAT and QBF solvers.
 *
 * Tests with real solvers are skipped when no solver is found, unless the environment variable
 *  MATA_TESTS_REQUIRE_SOLVERS is set, in which case they fail. Sections exercising the reduction with fake shell
 *  solvers additionally skip when the temporary directory cannot execute scripts (mounted noexec).
 */

#include <array>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <locale>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "mata/alphabet.hh"
#include "mata/nfa/algorithms.hh"
#include "mata/nfa/nfa.hh"

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
	#include <spawn.h>
	#include <sys/wait.h>
	#include <unistd.h>
	#define MATA_TESTS_FAKE_SOLVERS
#endif

using namespace mata::nfa;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::MessageMatches;
using mata::EnumAlphabet;
using mata::Symbol;
using mata::nfa::algorithms::ExternalSolverNotFound;

namespace {

/// NFA over {0, 1} accepting the words whose @p k-th last symbol is 1. Its minimal NFA has k + 1 states and its
///  minimal DFA 2^k states.
Nfa kth_last_symbol_is_one(const size_t k, const Symbol zero = 0, const Symbol one = 1) {
	Nfa aut{k + 1, {0}, {static_cast<State>(k)}};
	aut.delta.add(0, zero, 0);
	aut.delta.add(0, one, 0);
	aut.delta.add(0, one, 1);
	for (State state{1}; state < k; ++state) {
		aut.delta.add(state, zero, state + 1);
		aut.delta.add(state, one, state + 1);
	}
	return aut;
}

/// Reduce @p aut, or skip the test if the solver is not available.
Nfa reduce_or_skip(const Nfa& aut, const ParameterMap& params) {
	try {
		return reduce(aut, nullptr, params);
	} catch (const ExternalSolverNotFound& error) {
		if (std::getenv("MATA_TESTS_REQUIRE_SOLVERS") != nullptr) { FAIL(error.what()); }
		SKIP(error.what());
	}
	return Nfa{};
}

/// A deterministic pseudo-random NFA with @p num_of_states states over symbols 0 and 1 and a nonempty language:
///  reducing an automaton accepting nothing never reaches a solver.
Nfa random_nfa(const size_t num_of_states, unsigned long long seed) {
	const auto next{[&seed] {
		seed = seed * 6'364'136'223'846'793'005ULL + 1'442'695'040'888'963'407ULL;
		return seed >> 33;
	}};
	while (true) {
		Nfa aut{num_of_states, {0}, {}};
		for (State source{0}; source < num_of_states; ++source) {
			if (next() % 3 == 0) { aut.final.insert(source); }
			for (Symbol symbol{0}; symbol < 2; ++symbol) {
				for (State target{0}; target < num_of_states; ++target) {
					if (next() % 4 == 0) { aut.delta.add(source, symbol, target); }
				}
			}
		}
		if (!aut.is_lang_empty()) { return aut; }
	}
}

/// A 6-state NFA which reduction by simulation does not reduce, residual reduction reduces to 4 states, and whose
///  minimal NFA has 3 states.
Nfa nfa_with_three_state_minimum() {
	Nfa aut{6, {0}, {0}};
	for (const auto& [source, symbol, target] : std::vector<Transition>{
			 {0, 0, 0},
			 {0, 0, 1},
			 {0, 0, 4},
			 {1, 0, 1},
			 {1, 1, 0},
			 {1, 1, 4},
			 {1, 1, 5},
			 {2, 0, 2},
			 {2, 0, 4},
			 {2, 1, 1},
			 {2, 1, 2},
			 {2, 1, 3},
			 {3, 0, 0},
			 {3, 0, 4},
			 {3, 0, 5},
			 {3, 1, 0},
			 {4, 1, 2},
			 {5, 0, 5},
			 {5, 1, 1}
		 }) {
		aut.delta.add(source, symbol, target);
	}
	return aut;
}

#ifdef MATA_TESTS_FAKE_SOLVERS
/// A private temporary directory removed with its contents on destruction.
class TemporaryDirectory {
  public:
	explicit TemporaryDirectory(
		const std::string& name, const std::filesystem::path& root = std::filesystem::temp_directory_path()
	)
		: path_{create(name, root)} {}
	TemporaryDirectory(const TemporaryDirectory&) = delete;
	TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
	~TemporaryDirectory() {
		std::error_code error{};
		std::filesystem::remove_all(path_, error);
	}

	const std::filesystem::path& path() const { return path_; }

  private:
	std::filesystem::path path_{};

	static std::filesystem::path create(const std::string& name, const std::filesystem::path& root) {
		std::string directory{(root / (name + "-XXXXXX")).string()};
		if (::mkdtemp(directory.data()) == nullptr) {
			throw std::system_error(errno, std::generic_category(), "cannot create a test directory");
		}
		return directory;
	}
};

/// Whether a shell script written to a fresh private directory under @p root can be executed. Runs on every call.
bool probe_executable_scripts(const std::filesystem::path& root) {
	const TemporaryDirectory directory{"mata exec probe", root};
	const std::filesystem::path path{directory.path() / "probe"};
	std::ofstream{path} << "#!/bin/sh\nexit 0\n";
	std::filesystem::permissions(path, std::filesystem::perms::owner_all);
	std::string argument{path.string()};
	std::array<char*, 2> argv{argument.data(), nullptr};
	std::array<char*, 1> environment{nullptr};
	pid_t process{};
	if (::posix_spawn(&process, argv.front(), nullptr, nullptr, argv.data(), environment.data()) != 0) { return false; }
	int status{0};
	pid_t waited{-1};
	while ((waited = ::waitpid(process, &status, 0)) == -1 && errno == EINTR) {}
	// Without the child's own status (e.g. ECHILD when SIGCHLD is ignored) the probe is inconclusive; report it as
	//  not working rather than reading an unset status as a successful exit.
	return waited == process && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

/// Whether a shell script written next to the fake solvers can be executed at all: TMPDIR or /tmp is mounted noexec
///  on hardened systems and in containers started with '--tmpfs /tmp'. Probed once per process.
bool executable_scripts_work() {
	static const bool works{probe_executable_scripts(std::filesystem::temp_directory_path())};
	return works;
}

/// A shell script standing in for a solver, in a temporary directory (with a space in its name) removed on destruction.
class FakeSolver {
  public:
	explicit FakeSolver(const std::string& script, const std::string& name = "solver")
		: directory_{"mata fake solver"},
		  path_{directory_.path() / name} {
		if (!executable_scripts_work()) {
			SKIP(
				"Scripts in " + std::filesystem::temp_directory_path().string() +
				" cannot be executed; the directory is probably mounted noexec."
			);
		}
		std::ofstream{path_} << "#!/bin/sh\n" << script << '\n';
		std::filesystem::permissions(path_, std::filesystem::perms::owner_all);
	}
	FakeSolver(const FakeSolver&) = delete;
	FakeSolver& operator=(const FakeSolver&) = delete;

	/// The command running the solver, with its path quoted for Mata's command parser (not for a shell): it has no
	///  escapes, so the path is enclosed in the kind of quote it does not contain, e.g. "/tmp/team's work/solver".
	std::string command() const {
		const std::string path{path_.string()};
		const char quote{path.find('"') == std::string::npos ? '"' : '\''};
		if (path.find(quote) != std::string::npos) { SKIP("The temporary path contains both kinds of quotes."); }
		return quote + path + quote;
	}

	const std::filesystem::path& path() const { return path_; }
	const std::filesystem::path& directory() const { return directory_.path(); }

  private:
	TemporaryDirectory directory_;
	std::filesystem::path path_;
};

/// Set (or unset, if @p value is empty) an environment variable for the lifetime of the object.
class EnvironmentVariable {
  public:
	EnvironmentVariable(std::string name, const std::optional<std::string>& value)
		: name_{std::move(name)},
		  previous_{} {
		if (const char* const previous{std::getenv(name_.c_str())}) { previous_ = previous; }
		if (value.has_value()) {
			::setenv(name_.c_str(), value->c_str(), 1);
		} else {
			::unsetenv(name_.c_str());
		}
	}
	EnvironmentVariable(const EnvironmentVariable&) = delete;
	EnvironmentVariable& operator=(const EnvironmentVariable&) = delete;
	~EnvironmentVariable() {
		if (previous_.has_value()) {
			::setenv(name_.c_str(), previous_->c_str(), 1);
		} else {
			::unsetenv(name_.c_str());
		}
	}

  private:
	std::string name_;
	std::optional<std::string> previous_;
};

/// The beginning of a fake solver script, checking that the solver is run with @p arguments followed by the file with
///  the formula, and that the formula contains nothing but the tokens of the (Q)DIMACS format. The script prints a
///  message and fails if not, which the reduction reports as an error.
std::string check_arguments(const std::vector<std::string>& arguments = {}) {
	std::string script{
		"if [ $# -ne " + std::to_string(arguments.size() + 1) +
		" ]; then echo \"The solver got $# arguments: $*\"; exit 1; fi\n"
	};
	for (size_t index{0}; index < arguments.size(); ++index) {
		const std::string position{std::to_string(index + 1)};
		script += "if [ \"$" + position + "\" != '" + arguments[index] + "' ]; then echo \"Argument " + position +
				  " is '$" + position + "'\"; exit 1; fi\n";
	}
	// A digit-grouping locale writes "p cnf 320 1,294" and the literals of the clauses with separators as well.
	return script + "for formula do :; done\n" +
		   "if [ ! -s \"$formula\" ]; then echo \"The formula file '$formula' is missing or empty\"; exit 1; fi\n" +
		   "if grep -q '[^-0-9 acefnp]' \"$formula\"; then echo 'The formula has unexpected characters'; exit 1; fi\n";
}

/// A global C++ locale grouping digits, as en_US.UTF-8 does, restored on destruction. It groups every single digit,
///  so that even the small formulae of the tests are affected.
class GroupingLocale {
  public:
	GroupingLocale() : previous_{std::locale::global(std::locale{std::locale::classic(), new Grouping{}})} {}
	GroupingLocale(const GroupingLocale&) = delete;
	GroupingLocale& operator=(const GroupingLocale&) = delete;
	~GroupingLocale() { std::locale::global(previous_); }

  private:
	class Grouping : public std::numpunct<char> {
	  protected:
		char do_thousands_sep() const override { return ','; }
		std::string do_grouping() const override { return "\1"; }
	};

	std::locale previous_;
};

/// Ignore SIGCHLD for the lifetime of the object: the system then reaps the children itself and waitpid() fails with
///  ECHILD, as it does with SA_NOCLDWAIT or under a launcher reaping children in a handler.
class IgnoredChildSignal {
  public:
	IgnoredChildSignal() {
		struct sigaction ignore{};
		ignore.sa_handler = SIG_IGN;
		::sigemptyset(&ignore.sa_mask);
		// Save the whole previous action (flags such as SA_NOCLDWAIT and the mask, not only the handler).
		if (::sigaction(SIGCHLD, &ignore, &previous_) != 0) {
			throw std::system_error(errno, std::generic_category(), "cannot ignore SIGCHLD");
		}
	}
	IgnoredChildSignal(const IgnoredChildSignal&) = delete;
	IgnoredChildSignal& operator=(const IgnoredChildSignal&) = delete;
	~IgnoredChildSignal() { ::sigaction(SIGCHLD, &previous_, nullptr); }

  private:
	struct sigaction previous_{};
};
#endif

} // namespace

#ifdef MATA_TESTS_FAKE_SOLVERS
TEST_CASE("mata::nfa::reduce() execution probe does not follow a predictable symlink") {
	const TemporaryDirectory victim_directory{"mata symlink target"};
	const std::filesystem::path victim{victim_directory.path() / "victim"};
	std::ofstream{victim} << "do not overwrite";
	// The predictable name lives in a fresh fixture-owned root, so nothing in the shared temporary directory is
	// removed.
	const TemporaryDirectory root{"mata probe root"};
	std::filesystem::create_symlink(victim, root.path() / ("mata exec probe-" + std::to_string(::getpid())));
	// Uncached: the cached executable_scripts_work() may have probed earlier, before the link existed.
	(void) probe_executable_scripts(root.path());
	std::string contents{};
	std::getline(std::ifstream{victim}, contents);
	CHECK(contents == "do not overwrite");
}

TEST_CASE("mata::nfa::reduce() with algorithms \"sat\" and \"qbf\" running fake solvers") {
	const Nfa input{determinize(kth_last_symbol_is_one(2))};

	SECTION("A solver proving that no smaller automaton exists keeps the automaton found without the solver") {
		const FakeSolver unsat{check_arguments() + "echo 's UNSATISFIABLE'\nexit 20"};
		const FakeSolver qbf_unsat{check_arguments() + "echo 's cnf 0 10 20'\nexit 20"};
		const Nfa by_simulation{trim(reduce(input))};
		REQUIRE(by_simulation.num_of_states() == 4); // The minimal NFA has 3 states; only the fake solver stops here.

		const Nfa nfa{reduce(input, nullptr, {{"algorithm", "sat"}, {"solver", unsat.command()}})};
		CHECK(nfa.num_of_states() == 4);
		CHECK(are_equivalent(nfa, input));

		const Nfa dfa{reduce(input, nullptr, {{"algorithm", "sat"}, {"type", "dfa"}, {"solver", unsat.command()}})};
		CHECK(dfa.num_of_states() == 4);
		CHECK(dfa.is_deterministic());
		CHECK(are_equivalent(dfa, input));

		const Nfa qbf{reduce(input, nullptr, {{"algorithm", "qbf"}, {"solver", qbf_unsat.command()}})};
		CHECK(qbf.num_of_states() == 4);
		CHECK(are_equivalent(qbf, input));
	}

	SECTION("Solvers are found through environment variables, and results do not map states") {
		const FakeSolver unsat{check_arguments() + "echo 's UNSATISFIABLE'\nexit 20"};
		const EnvironmentVariable variable{"MATA_SAT_SOLVER", unsat.command()};
		StateRenaming renaming{{0, 0}, {1, 1}};
		const Nfa nfa{reduce(input, &renaming, {{"algorithm", "sat"}})};
		// The minimal NFA has 3 states: a solver other than the fake one would be found by a different route.
		CHECK(nfa.num_of_states() == 4);
		CHECK(are_equivalent(nfa, input));
		CHECK(renaming.empty());
	}

	SECTION("The formula is written in the classic locale") {
		const FakeSolver unsat{check_arguments() + "echo 's UNSATISFIABLE'\nexit 20"};
		// With a digit-grouping locale, the header would read "p cnf 3,2,0 1,2,9,4" and the solver reject it.
		const GroupingLocale locale{};
		const Nfa nfa{reduce(input, nullptr, {{"algorithm", "sat"}, {"solver", unsat.command()}})};
		CHECK(nfa.num_of_states() == 4);
		CHECK(are_equivalent(nfa, input));
	}

	SECTION("A solver whose exit status is lost is judged by its output") {
		const FakeSolver unsat{check_arguments() + "echo 's UNSATISFIABLE'\nexit 20"};
		const IgnoredChildSignal ignored_child_signal{};
		const Nfa nfa{reduce(input, nullptr, {{"algorithm", "sat"}, {"solver", unsat.command()}})};
		CHECK(nfa.num_of_states() == 4);
		CHECK(are_equivalent(nfa, input));

		const FakeSolver silent{check_arguments() + "exit 20"};
		CHECK_THROWS_WITH(
			reduce(input, nullptr, {{"algorithm", "sat"}, {"solver", silent.command()}}),
			ContainsSubstring("did not report a result")
		);
	}

	SECTION("A solver reporting the result only by its exit code and printing the model on several lines") {
		// a*, which reduction by simulation does not reduce.
		Nfa aut{3, {0}, {0, 1}};
		aut.delta.add(0, 'a', 1);
		aut.delta.add(0, 'a', 2);
		aut.delta.add(2, 'a', 0);
		REQUIRE(trim(reduce(aut)).num_of_states() == 3);
		// For one state and one symbol, variable 1 is the transition, 2 the initial state, 3 the final state, and 4
		//  the run accepting the empty word.
		const FakeSolver solver{"printf 'c a comment\\nv 1 2\\nv 3\\nv 4 0\\n'\nexit 10"};
		const Nfa nfa{reduce(aut, nullptr, {{"algorithm", "sat"}, {"solver", solver.command()}})};
		CHECK(nfa.num_of_states() == 1);
		CHECK(are_equivalent(nfa, aut));
	}

	SECTION("A model inconsistent with the sample words is detected") {
		const FakeSolver wrong{"echo 's SATISFIABLE'\necho 'v 0'\nexit 10"};
		CHECK_THROWS_AS(
			reduce(input, nullptr, {{"algorithm", "sat"}, {"solver", wrong.command()}}), std::runtime_error
		);
		CHECK_THROWS_WITH(
			reduce(input, nullptr, {{"algorithm", "sat"}, {"solver", wrong.command()}}),
			ContainsSubstring("does not agree with the sample words")
		);
	}

	SECTION("Error messages quote the solver output as whole lines of valid UTF-8") {
		// More than the 1,000 bytes of the excerpt, with a line of Latin-1 in the middle.
		const FakeSolver noisy{
			"i=0\nwhile [ $i -lt 100 ]; do printf 'line %03d padded to make the output long enough\\n' $i; "
			"i=$((i + 1)); done\nprintf 'caf\\351 au lait\\n'\necho 'Segmentation fault'"
		};
		std::string message{};
		try {
			reduce(input, nullptr, {{"algorithm", "sat"}, {"solver", noisy.command()}});
		} catch (const std::runtime_error& error) { message = error.what(); }
		CHECK_THAT(message, ContainsSubstring("without reporting a result"));
		CHECK_THAT(message, ContainsSubstring("...\nline ")); // The excerpt starts at a line boundary.
		CHECK_THAT(message, ContainsSubstring("caf\xEF\xBF\xBD au lait")); // The invalid byte is replaced.
		CHECK(message.find('\xE9') == std::string::npos);

		// A solver command that is not valid UTF-8 either, here reported as not found.
		std::string not_found{};
		try {
			reduce(input, nullptr, {{"algorithm", "sat"}, {"solver", "/nonexistent/caf\xE9-solver"}});
		} catch (const ExternalSolverNotFound& error) { not_found = error.what(); }
		CHECK_THAT(not_found, ContainsSubstring("caf\xEF\xBF\xBD-solver"));
		CHECK(not_found.find('\xE9') == std::string::npos);
	}

	SECTION("Failing solvers are reported") {
		const auto check_error{[&](const std::string& script, const std::string& message) {
			const FakeSolver solver{script};
			CHECK_THROWS_WITH(
				reduce(input, nullptr, {{"algorithm", "sat"}, {"solver", solver.command()}}), ContainsSubstring(message)
			);
		}};
		check_error("echo 'Segmentation fault'", "without reporting a result");
		check_error("echo 's SATISFIABLE'\nexit 10", "did not print a model");
		check_error("echo 's SATISFIABLE'\necho 'v 1 x 0'", "unexpected value 'x'");
		check_error("echo 's SATISFIABLE'\necho 'v 1000000 0'", "unexpected value '1000000'");
		check_error("echo 's UNKNOWN'", "did not decide");
		check_error("kill -9 $$", "terminated by signal 9");
		// One line longer than the excerpt limit, then a newline: its tail is kept instead of an empty excerpt.
		check_error("printf 'x%.0s' $(seq 1 1500); printf 'TAIL\\n'", "xTAIL");

		const FakeSolver qbf_without_certificate{"echo 's cnf 1 10 20'\nexit 10"};
		CHECK_THROWS_WITH(
			reduce(input, nullptr, {{"algorithm", "qbf"}, {"solver", qbf_without_certificate.command()}}),
			ContainsSubstring("--qdo")
		);
	}

	SECTION("A DepQBF --help probe killed by a signal leaves the command unchanged") {
		// A wrapper that dies without its configured arguments: the probe runs the executable alone, so it is killed,
		//  but the run with the full command works and decides the outcome.
		const FakeSolver wrapper{
			"case \"$*\" in *--configured*--qdo*) echo 's cnf 0 10 20'; exit 20;; esac\n"
			"kill -9 $$",
			"depqbf"
		};
		const Nfa nfa{
			reduce(input, nullptr, {{"algorithm", "qbf"}, {"solver", wrapper.command() + " --configured --qdo"}})
		};
		CHECK(nfa.num_of_states() == 4);
		CHECK(are_equivalent(nfa, input));
	}

	SECTION("DepQBF is run with --no-dynamic-nenofex only if it supports the option") {
		// Both versions abort when given wrong options, as DepQBF does.
		const FakeSolver with_nenofex{
			"case \"$1\" in --help) echo '  --no-dynamic-nenofex  disable dynamic nenofex'; exit 0;; esac\n"
			"case \"$*\" in *--qdo*--no-dynamic-nenofex*) echo 's cnf 0 10 20'; exit 20;; esac\n"
			"echo 'Must configure solver with --no-dynamic-nenofex'; kill -6 $$",
			"depqbf"
		};
		const FakeSolver without_nenofex{
			"case \"$1\" in --help) echo '  --qdo  QDIMACS output'; exit 0;; esac\n"
			"case \"$*\" in *--no-dynamic-nenofex*) echo 'unknown option'; kill -6 $$;; esac\n"
			"case \"$*\" in *--qdo*) echo 's cnf 0 10 20'; exit 20;; esac\n"
			"kill -6 $$",
			"depqbf"
		};
		for (const FakeSolver* const depqbf : {&with_nenofex, &without_nenofex}) {
			const EnvironmentVariable path{"PATH", depqbf->directory().string()};
			{ // Found in PATH.
				const EnvironmentVariable no_qbf_solver{"MATA_QBF_SOLVER", std::nullopt};
				const Nfa nfa{reduce(input, nullptr, {{"algorithm", "qbf"}})};
				CHECK(nfa.num_of_states() == 4);
				CHECK(are_equivalent(nfa, input));
			}
			// The explicit commands below are needed for these cases to pass: PATH holds no solver, so default
			//  discovery cannot find one if the command were ignored.
			const TemporaryDirectory no_solvers{"mata no solvers"};
			const EnvironmentVariable empty_path{"PATH", no_solvers.path().string()};
			{ // Given by the environment variable, by its full path.
				const EnvironmentVariable qbf_solver{"MATA_QBF_SOLVER", depqbf->command() + " --qdo"};
				const Nfa nfa{reduce(input, nullptr, {{"algorithm", "qbf"}})};
				CHECK(nfa.num_of_states() == 4);
				CHECK(are_equivalent(nfa, input));
			}
			{ // Given by the "solver" parameter, by its full path.
				const EnvironmentVariable no_qbf_solver{"MATA_QBF_SOLVER", std::nullopt};
				const Nfa nfa{reduce(input, nullptr, {{"algorithm", "qbf"}, {"solver", depqbf->command() + " --qdo"}})};
				CHECK(nfa.num_of_states() == 4);
				CHECK(are_equivalent(nfa, input));
			}
		}
	}

	SECTION("Solver commands with quotes") {
		const FakeSolver unsat{check_arguments({"--unused", "an argument"}) + "echo 's UNSATISFIABLE'\nexit 20"};
		const Nfa nfa{reduce(
			input, nullptr,
			{{"algorithm", "sat"}, {"solver", "\"" + unsat.path().string() + "\" --unused 'an argument'"}}
		)};
		CHECK(nfa.num_of_states() == 4);
		CHECK(are_equivalent(nfa, input));
		CHECK_THROWS_WITH(
			reduce(input, nullptr, {{"algorithm", "sat"}, {"solver", "'/path/to/solver"}}),
			ContainsSubstring("unterminated quote")
		);
	}

	SECTION("Missing solvers are reported") {
		CHECK_THROWS_AS(
			reduce(input, nullptr, {{"algorithm", "sat"}, {"solver", "/nonexistent/sat-solver"}}),
			ExternalSolverNotFound
		);
		const EnvironmentVariable variable{"MATA_QBF_SOLVER", "nonexistent-qbf-solver --qdo"};
		CHECK_THROWS_WITH(reduce(input, nullptr, {{"algorithm", "qbf"}}), ContainsSubstring("MATA_QBF_SOLVER"));
	}

	SECTION("Unknown types of automata are rejected") {
		// No solver is run: the type is checked first. The error is a plain std::runtime_error, not an
		//  ExternalSolverNotFound, so the bindings do not report it as a missing solver.
		const auto check_rejected{[&](const ParameterMap& params) {
			CHECK_THROWS_MATCHES(
				reduce(input, nullptr, params), std::runtime_error, MessageMatches(ContainsSubstring("unknown type"))
			);
			try {
				(void) reduce(input, nullptr, params);
			} catch (const ExternalSolverNotFound&) {
				FAIL("an unknown type was reported as a missing solver");
			} catch (const std::runtime_error&) {}
		}};
		for (const char* const algorithm : {"sat", "qbf"}) {
			check_rejected({{"algorithm", algorithm}, {"type", "after"}});
		}
		// "dfa" is a type of "sat" only; "qbf" always computes an NFA.
		check_rejected({{"algorithm", "qbf"}, {"type", "dfa"}});
	}
}
#endif

TEST_CASE("mata::nfa::reduce() with algorithm \"sat\"") {
	SECTION("Minimal NFAs and DFAs of the k-th last symbol languages") {
		for (size_t k{1}; k <= 3; ++k) {
			const Nfa input{determinize(kth_last_symbol_is_one(k))};
			const Nfa nfa{reduce_or_skip(input, {{"algorithm", "sat"}})};
			CHECK(nfa.num_of_states() == k + 1);
			CHECK(are_equivalent(nfa, input));
			const Nfa dfa{reduce_or_skip(kth_last_symbol_is_one(k), {{"algorithm", "sat"}, {"type", "dfa"}})};
			CHECK(dfa.num_of_states() == size_t{1} << k);
			CHECK(dfa.is_deterministic());
			CHECK(are_equivalent(dfa, input));
		}
	}

	SECTION("Smaller than the reductions by simulation and residual automata") {
		const Nfa input{nfa_with_three_state_minimum()};
		const Nfa nfa{reduce_or_skip(input, {{"algorithm", "sat"}})};
		CHECK(nfa.num_of_states() == 3);
		CHECK(are_equivalent(nfa, input));
		CHECK(reduce(input).num_of_states() == 6);
		CHECK(
			reduce(input, nullptr, {{"algorithm", "residual"}, {"type", "after"}, {"direction", "forward"}})
				.num_of_states() == 4
		);
	}

	SECTION("Never larger than the reduction by simulation") {
		size_t built_by_solver{0};
		for (unsigned long long seed{1}; seed <= 20; ++seed) {
			const Nfa input{random_nfa(5, seed)};
			const Nfa by_simulation{trim(reduce(input))};
			const Nfa nfa{reduce_or_skip(input, {{"algorithm", "sat"}})};
			CHECK(nfa.num_of_states() <= by_simulation.num_of_states());
			CHECK(are_equivalent(nfa, input));
			if (nfa.num_of_states() < by_simulation.num_of_states()) { ++built_by_solver; }
		}
		// The solver built the result of 8 of the 20 seeds; without this the section would pass on inputs it never
		//  sees, as it did on the seeds whose language was empty.
		CHECK(built_by_solver >= 8);
	}

	SECTION("Symbols that are not 0, ..., n - 1 and the alphabet are kept") {
		const auto alphabet{std::make_shared<EnumAlphabet>(mata::utils::OrdVector<Symbol>{2, 97})};
		Nfa input{determinize(kth_last_symbol_is_one(2, 2, 97))};
		input.alphabet = alphabet;
		const Nfa nfa{reduce_or_skip(input, {{"algorithm", "sat"}})};
		CHECK(nfa.num_of_states() == 3);
		CHECK(are_equivalent(nfa, input));
		CHECK(nfa.delta.get_used_symbols() == mata::utils::OrdVector<Symbol>{2, 97});
		CHECK(nfa.alphabet == alphabet);
	}

	SECTION("Epsilon transitions") {
		// a* ε b*, built with redundant epsilon transitions.
		Nfa input{4, {0}, {3}};
		input.delta.add(0, 'a', 0);
		input.delta.add(0, EPSILON, 1);
		input.delta.add(1, EPSILON, 2);
		input.delta.add(2, EPSILON, 3);
		input.delta.add(3, 'b', 3);
		const Nfa nfa{reduce_or_skip(input, {{"algorithm", "sat"}})};
		CHECK(nfa.num_of_states() == 2);
		CHECK(!nfa.delta.get_used_symbols().contains(EPSILON));
		CHECK(are_equivalent(nfa, remove_epsilon(input)));
	}

	SECTION("Trivial languages need no solver") {
		const ParameterMap params{{"algorithm", "sat"}, {"solver", "/nonexistent/sat-solver"}};
		Nfa empty{2, {0}, {}};
		empty.delta.add(0, 0, 1);
		CHECK(reduce(empty, nullptr, params).num_of_states() == 0);

		ParameterMap dfa_params{params};
		dfa_params["type"] = "dfa";
		const Nfa empty_dfa{reduce(empty, nullptr, dfa_params)};
		CHECK(empty_dfa.num_of_states() == 1);
		CHECK(empty_dfa.is_lang_empty());
		// An automaton without an initial state is not deterministic, and operations requiring a DFA reject it.
		CHECK(empty_dfa.is_deterministic());
		Nfa complete_empty_dfa{empty_dfa};
		complete_empty_dfa.make_complete(mata::utils::OrdVector<Symbol>{0});
		CHECK(union_det_complete(complete_empty_dfa, complete_empty_dfa).is_lang_empty());

		const Nfa epsilon_only{reduce(Nfa{2, {0, 1}, {0, 1}}, nullptr, params)};
		CHECK(epsilon_only.num_of_states() == 1);
		CHECK(epsilon_only.is_in_lang(mata::Word{}));
		CHECK(epsilon_only.delta.empty());
	}
}

TEST_CASE("mata::nfa::reduce() with algorithm \"qbf\"") {
	SECTION("Minimal NFAs of the k-th last symbol languages") {
		for (size_t k{1}; k <= 2; ++k) {
			const Nfa input{determinize(kth_last_symbol_is_one(k))};
			const Nfa nfa{reduce_or_skip(input, {{"algorithm", "qbf"}})};
			CHECK(nfa.num_of_states() == k + 1);
			CHECK(are_equivalent(nfa, input));
		}
	}

	SECTION("Symbols that are not 0, ..., n - 1, and epsilon transitions") {
		Nfa input{determinize(kth_last_symbol_is_one(1, 2, 97))};
		const State new_initial{input.add_state()};
		input.delta.add(new_initial, EPSILON, *input.initial.begin());
		input.initial = {new_initial};
		const Nfa nfa{reduce_or_skip(input, {{"algorithm", "qbf"}})};
		CHECK(nfa.num_of_states() == 2);
		CHECK(are_equivalent(nfa, remove_epsilon(input)));
		CHECK(nfa.delta.get_used_symbols() == mata::utils::OrdVector<Symbol>{2, 97});
	}
}
