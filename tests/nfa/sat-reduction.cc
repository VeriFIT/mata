/* sat-reduction.cc -- Tests for the reduction of NFAs to minimal automata using external SAT and QBF solvers.
 *
 * Tests with real solvers are skipped when no solver is found, unless the environment variable
 *  MATA_TESTS_REQUIRE_SOLVERS is set, in which case they fail.
 */

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "mata/alphabet.hh"
#include "mata/nfa/algorithms.hh"
#include "mata/nfa/nfa.hh"

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
	#include <unistd.h>
	#define MATA_TESTS_FAKE_SOLVERS
#endif

using namespace mata::nfa;
using Catch::Matchers::ContainsSubstring;
using mata::EnumAlphabet;
using mata::Symbol;
using mata::nfa::algorithms::ExternalSolverNotFound;

namespace {

/// NFA over {0, 1} accepting the words whose @p k-th last symbol is 1. Its minimal NFA has k + 1 states and its
///  minimal DFA 2^k states.
Nfa kth_last_symbol_is_one(const size_t k, const Symbol zero = 0, const Symbol one = 1) {
	Nfa aut{k + 1, {0}, {k}};
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

/// A deterministic pseudo-random NFA with @p num_of_states states over symbols 0 and 1.
Nfa random_nfa(const size_t num_of_states, unsigned long long seed) {
	const auto next{[&seed] {
		seed = seed * 6'364'136'223'846'793'005ULL + 1'442'695'040'888'963'407ULL;
		return seed >> 33;
	}};
	Nfa aut{num_of_states, {0}, {}};
	for (State source{0}; source < num_of_states; ++source) {
		if (next() % 3 == 0) { aut.final.insert(source); }
		for (Symbol symbol{0}; symbol < 2; ++symbol) {
			for (State target{0}; target < num_of_states; ++target) {
				if (next() % 4 == 0) { aut.delta.add(source, symbol, target); }
			}
		}
	}
	return aut;
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
/// A shell script standing in for a solver, in a temporary directory removed on destruction.
class FakeSolver {
  public:
	explicit FakeSolver(const std::string& script)
		: directory_{std::filesystem::temp_directory_path() / ("mata-fake-solver-" + std::to_string(::getpid()) + "-" + std::to_string(counter_++))},
		  path_{directory_ / "solver"} {
		std::filesystem::create_directories(directory_);
		std::ofstream{path_} << "#!/bin/sh\n" << script << '\n';
		std::filesystem::permissions(path_, std::filesystem::perms::owner_all);
	}
	FakeSolver(const FakeSolver&) = delete;
	FakeSolver& operator=(const FakeSolver&) = delete;
	~FakeSolver() {
		std::error_code error{};
		std::filesystem::remove_all(directory_, error);
	}

	std::string command() const { return path_.string(); }

  private:
	static inline unsigned counter_{0};
	std::filesystem::path directory_;
	std::filesystem::path path_;
};

/// Set an environment variable for the lifetime of the object.
class EnvironmentVariable {
  public:
	EnvironmentVariable(std::string name, const std::string& value) : name_{std::move(name)}, previous_{} {
		if (const char* const previous{std::getenv(name_.c_str())}) { previous_ = previous; }
		::setenv(name_.c_str(), value.c_str(), 1);
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
#endif

} // namespace

#ifdef MATA_TESTS_FAKE_SOLVERS
TEST_CASE("mata::nfa::reduce() with algorithms \"sat\" and \"qbf\" running fake solvers") {
	const Nfa input{determinize(kth_last_symbol_is_one(2))};

	SECTION("A solver proving that no smaller automaton exists keeps the automaton found without the solver") {
		const FakeSolver unsat{"echo 's UNSATISFIABLE'\nexit 20"};
		const FakeSolver qbf_unsat{"echo 's cnf 0 10 20'\nexit 20"};
		const Nfa by_simulation{trim(reduce(input))};

		const Nfa nfa{reduce(input, nullptr, {{"algorithm", "sat"}, {"solver", unsat.command()}})};
		CHECK(nfa.num_of_states() == by_simulation.num_of_states());
		CHECK(are_equivalent(nfa, input));

		const Nfa dfa{reduce(input, nullptr, {{"algorithm", "sat"}, {"type", "dfa"}, {"solver", unsat.command()}})};
		CHECK(dfa.num_of_states() == 4);
		CHECK(dfa.is_deterministic());
		CHECK(are_equivalent(dfa, input));

		const Nfa qbf{reduce(input, nullptr, {{"algorithm", "qbf"}, {"solver", qbf_unsat.command()}})};
		CHECK(qbf.num_of_states() == by_simulation.num_of_states());
		CHECK(are_equivalent(qbf, input));
	}

	SECTION("Solvers are found through environment variables, and results do not map states") {
		const FakeSolver unsat{"echo 's UNSATISFIABLE'\nexit 20"};
		const EnvironmentVariable variable{"MATA_SAT_SOLVER", unsat.command()};
		StateRenaming renaming{{0, 0}, {1, 1}};
		const Nfa nfa{reduce(input, &renaming, {{"algorithm", "sat"}})};
		CHECK(are_equivalent(nfa, input));
		CHECK(renaming.empty());
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
		CHECK_THROWS_AS(reduce(input, nullptr, {{"algorithm", "sat"}, {"solver", wrong.command()}}), std::logic_error);
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

		const FakeSolver qbf_without_certificate{"echo 's cnf 1 10 20'\nexit 10"};
		CHECK_THROWS_WITH(
			reduce(input, nullptr, {{"algorithm", "qbf"}, {"solver", qbf_without_certificate.command()}}),
			ContainsSubstring("--qdo")
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
		CHECK_THROWS_WITH(
			reduce(input, nullptr, {{"algorithm", "sat"}, {"type", "after"}}), ContainsSubstring("unknown type")
		);
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
		for (unsigned long long seed{1}; seed <= 20; ++seed) {
			const Nfa input{random_nfa(5, seed)};
			const Nfa nfa{reduce_or_skip(input, {{"algorithm", "sat"}})};
			CHECK(nfa.num_of_states() <= trim(reduce(input)).num_of_states());
			CHECK(are_equivalent(nfa, input));
		}
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
