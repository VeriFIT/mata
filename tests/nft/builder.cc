// TODO: some header

#include <fstream>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "mata/nft/builder.hh"
#include "mata/nft/nft.hh"
#include "mata/parser/inter-aut.hh"
#include "mata/parser/parser.hh"

using namespace mata::nft;
using mata::nfa::Nfa;
using Symbol = mata::Symbol;
using IntAlphabet = mata::IntAlphabet;
using OnTheFlyAlphabet = mata::OnTheFlyAlphabet;

using Word = std::vector<Symbol>;

TEST_CASE("nft::from_nfa_with_levels_zero()") {
	Nft nft{};
	Nft expected{};
	Nfa nfa{};

	SECTION("small nfa to 2 level NFT") {
		constexpr Level NUM_OF_LEVELS{2};
		nfa.initial = {0};
		nfa.final = {3};
		nfa.delta.add(0, 1, 2);
		nfa.delta.add(1, EPSILON, 3);
		nfa.delta.add(3, 2, 3);
		nfa.delta.add(2, 3, 1);
		nfa.delta.add(2, 3, 0);
		nft = builder::from_nfa_with_levels_zero(nfa, NUM_OF_LEVELS);
		expected = mata::nft::builder::parse_from_mata(
			std::string(
				"@NFT-explicit\n%Alphabet-auto\n%Initial q0\n%Final q4\n%Levels q0:0 q1:1 q2:0 q3:0 q4:0 q5:1 "
				"q6:1\n%LevelsNum 2\nq0 1 q1\nq1 1 q2\nq2 3 q5\nq3 4294967295 q4\nq4 2 q6\nq5 3 q0\nq5 3 q3\nq6 2 q4\n"
			)
		);
		expected.levels.num_of_levels = NUM_OF_LEVELS;
		CHECK(mata::nft::are_equivalent(nft, expected));
	}

	SECTION("small nfa to 3 level NFT") {
		constexpr Level NUM_OF_LEVELS{3};
		nfa.initial = {0};
		nfa.final = {3};
		nfa.delta.add(0, 1, 2);
		nfa.delta.add(1, EPSILON, 3);
		nfa.delta.add(3, 2, 3);
		nfa.delta.add(2, 3, 1);
		nfa.delta.add(2, 3, 0);
		nft = builder::from_nfa_with_levels_zero(nfa, NUM_OF_LEVELS);
		expected = mata::nft::builder::parse_from_mata(
			std::string(
				"@NFT-explicit\n%Alphabet-auto\n%Initial q0\n%Final q5\n%Levels q0:0 q1:1 q2:2 q3:0 q4:0 q5:0 q6:1 "
				"q7:2 q8:1 q9:2\n%LevelsNum 3\nq0 1 q1\nq1 1 q2\nq2 1 q3\nq3 3 q6\nq4 4294967295 q5\nq5 2 q8\nq6 3 "
				"q7\nq7 3 q0\nq7 3 q4\nq8 2 q9\nq9 2 q5\n"
			)
		);
		expected.levels.num_of_levels = NUM_OF_LEVELS;
		CHECK(mata::nft::are_equivalent(nft, expected));
	}

	SECTION("regex cb+a+") {
		constexpr Level NUM_OF_LEVELS{2};
		nfa.initial = {0};
		nfa.final = {3};
		nfa.delta.add(0, 'c', 1);
		nfa.delta.add(1, 'b', 1);
		nfa.delta.add(1, 'b', 2);
		nfa.delta.add(2, 'a', 2);
		nfa.delta.add(2, 'a', 3);
		nft = builder::from_nfa_with_levels_zero(nfa, NUM_OF_LEVELS);
		expected = mata::nft::builder::parse_from_mata(
			std::string(
				"@NFT-explicit\n%Alphabet-auto\n%Initial q0\n%Final q6\n%Levels q0:0 q1:1 q2:0 q3:1 q4:0 q5:1 "
				"q6:0\n%LevelsNum 2\nq0 99 q1\nq1 99 q2\nq2 98 q3\nq3 98 q2\nq3 98 q4\nq4 97 q5\nq5 97 q4\nq5 97 q6\n"
			)
		);
		expected.levels.num_of_levels = NUM_OF_LEVELS;
		CHECK(mata::nft::are_equivalent(nft, expected));
	}

	SECTION("regex cb+a+ with epsilon on added levels") {
		constexpr Level NUM_OF_LEVELS{2};
		nfa.initial = {0};
		nfa.final = {3};
		nfa.delta.add(0, 'c', 1);
		nfa.delta.add(1, 'b', 1);
		nfa.delta.add(1, 'b', 2);
		nfa.delta.add(2, 'a', 2);
		nfa.delta.add(2, 'a', 3);
		nft = builder::from_nfa_with_levels_zero(nfa, NUM_OF_LEVELS, true, {EPSILON});
		expected = mata::nft::builder::parse_from_mata(
			std::string(
				"@NFT-explicit\n%Alphabet-auto\n%Initial q0\n%Final q6\n%Levels q0:0 q1:1 q2:0 q3:1 q4:0 q5:1 "
				"q6:0\n%LevelsNum 2\nq0 99 q1\nq1 4294967295 q2\nq2 98 q3\nq3 4294967295 q2\nq3 4294967295 q4\nq4 97 "
				"q5\nq5 4294967295 q4\nq5 4294967295 q6\n"
			)
		);
		expected.levels.num_of_levels = NUM_OF_LEVELS;
		CHECK(mata::nft::are_equivalent(nft, expected));
	}
}

TEST_CASE("mata::nft::parse_from_mata()") {
	Delta delta;

	SECTION("Simple automaton") {
		delta.add(0, 0, 0);
		delta.add(0, 1, 1);
		delta.add(1, 2, 0);
		Nft nft{Nft::with_levels({1, {0, 0}}, delta, {0}, {1})};

		SECTION("from string") {
			Nft parsed{mata::nft::builder::parse_from_mata(nft.print_to_mata())};
			CHECK(are_equivalent(parsed, nft));
		}

		SECTION("from stream") {
			std::stringstream nft_stream;
			nft.print_to_mata(nft_stream);
			Nft parsed{mata::nft::builder::parse_from_mata(nft_stream)};
			CHECK(are_equivalent(parsed, nft));
		}

		SECTION("from file") {
			std::filesystem::path nft_file{"./temp-test-parse_from_mata-simple_nft.mata"};
			std::fstream file{nft_file, std::fstream::in | std::fstream::out | std::fstream::trunc};
			nft.print_to_mata(file);
			Nft parsed{mata::nft::builder::parse_from_mata(nft_file)};
			file.close();
			std::filesystem::remove(nft_file);

			CHECK(are_equivalent(parsed, nft));
		}
	}

	SECTION("transducer with EPSILON and DONT_CARE transitions") {
		// Two levels: 0 -a-> 1 -<eps>-> 2 (level 0 again), plus a DONT_CARE edge across one level.
		Nft nft{Nft::with_levels({2, {0, 1, 0, 1}}, Delta{}, {0}, {2})};
		nft.delta.add(0, 'a', 1);
		nft.delta.add(1, EPSILON, 2);
		nft.delta.add(2, DONT_CARE, 3);
		nft.delta.add(3, 'b', 0);

		const std::string printed{nft.print_to_mata()};
		CHECK(printed.find(mata::EPSILON_NAME) != std::string::npos);
		CHECK(printed.find(mata::DONT_CARE_NAME) != std::string::npos);

		Nft parsed{mata::nft::builder::parse_from_mata(printed)};
		// The parser renames states by order of appearance, so compare the symbols, not the IDs.
		size_t num_of_epsilons{0};
		size_t num_of_dont_cares{0};
		for (const Transition& transition : parsed.delta.transitions()) {
			if (transition.symbol == EPSILON) { ++num_of_epsilons; }
			if (transition.symbol == DONT_CARE) { ++num_of_dont_cares; }
		}
		CHECK(num_of_epsilons == 1);
		CHECK(num_of_dont_cares == 1);
		CHECK(parsed.delta.num_of_transitions() == nft.delta.num_of_transitions());
		CHECK(parsed.levels.num_of_levels == nft.levels.num_of_levels);
		CHECK(parsed.num_of_states() == nft.num_of_states());
	}

	SECTION("larger automaton") {
		Nft nft;
		nft.initial = {1, 2, 50};
		nft.delta.add(1, 'a', 2);
		nft.delta.add(1, 'a', 3);
		nft.delta.add(1, 'b', 4);
		nft.delta.add(2, 'a', 2);
		nft.delta.add(2, 'b', 2);
		nft.delta.add(2, 'a', 3);
		nft.delta.add(2, 'b', 4);
		nft.delta.add(3, 'b', 4);
		nft.delta.add(3, 'c', 7);
		nft.delta.add(3, 'b', 2);
		nft.delta.add(5, 'c', 3);
		nft.delta.add(7, 'a', 8);
		nft.delta.add(12, 'b', 15);
		nft.delta.add(1, 'b', 40);
		nft.delta.add(51, 'z', 42);
		nft.final = {3, 103};
		nft.levels = Levels(43, nft.num_of_states(), 0);
		nft.levels[3] = 42;
		nft.levels[103] = 42;
		nft.levels.num_of_levels = 43;

		SECTION("from string") {
			Nft parsed{mata::nft::builder::parse_from_mata(nft.print_to_mata())};
			parsed.final.contains(103);
			parsed.initial.contains(50);
			parsed.delta.contains(51, 'z', 42);
			CHECK(parsed.levels.num_of_levels == 43);

			Levels test_levels(parsed.levels);
			for (const State& s : parsed.final) {
				CHECK(test_levels[s] == 42);
				test_levels[s] = 0;
			}
			CHECK(std::all_of(test_levels.begin(), test_levels.end(), [](Level l) { return l == 0; }));

			CHECK(are_equivalent(parsed, nft));
		}

		SECTION("from stream") {
			std::stringstream nft_stream;
			nft.print_to_mata(nft_stream);
			Nft parsed{mata::nft::builder::parse_from_mata(nft_stream)};
			parsed.final.contains(103);
			parsed.initial.contains(50);
			parsed.delta.contains(51, 'z', 42);
			CHECK(parsed.levels.num_of_levels == 43);

			auto test_levels(parsed.levels);
			for (const State& s : parsed.final) {
				CHECK(test_levels[s] == 42);
				test_levels[s] = 0;
			}
			CHECK(std::all_of(test_levels.begin(), test_levels.end(), [](Level l) { return l == 0; }));

			CHECK(are_equivalent(parsed, nft));
		}

		SECTION("from file") {
			std::filesystem::path nft_file{"./temp-test-parse_from_mata-larger_nft.mata"};
			std::fstream file{nft_file, std::fstream::in | std::fstream::out | std::fstream::trunc};
			nft.print_to_mata(file);
			Nft parsed{mata::nft::builder::parse_from_mata(nft_file)};
			file.close();
			std::filesystem::remove(nft_file);

			parsed.final.contains(103);
			parsed.initial.contains(50);
			parsed.delta.contains(51, 'z', 42);
			CHECK(parsed.levels.num_of_levels == 43);

			auto test_levels(parsed.levels);
			for (const State& s : parsed.final) {
				CHECK(test_levels[s] == 42);
				test_levels[s] = 0;
			}
			CHECK(std::all_of(test_levels.begin(), test_levels.end(), [](Level l) { return l == 0; }));

			CHECK(are_equivalent(parsed, nft));
		}
	}

	SECTION("levels testing") {
		SECTION("ascending") {
			Nft nft;
			nft.delta.add(0, 1, 1);
			nft.delta.add(1, 1, 2);
			nft.delta.add(2, 1, 3);
			nft.delta.add(3, 1, 4);
			nft.delta.add(4, 1, 5);
			nft.delta.add(5, 1, 6);
			nft.delta.add(6, 1, 7);
			nft.delta.add(7, 1, 8);
			nft.delta.add(8, 1, 9);
			nft.delta.add(9, 1, 10);
			nft.initial.insert(0);
			nft.final.insert(10);
			nft.levels.num_of_levels = 11;
			nft.levels.set({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10});

			SECTION("from string") {
				Nft parsed{mata::nft::builder::parse_from_mata(nft.print_to_mata())};

				REQUIRE(parsed.initial.size() == 1);
				REQUIRE(parsed.final.size() == 1);
				CHECK(parsed.levels.num_of_levels == 11);
				State s{*parsed.initial.begin()};
				Level level = 0;
				while (s != *parsed.final.begin()) {
					CHECK(parsed.levels[s] == level);
					REQUIRE(parsed.delta[s].size() == 1);
					SymbolPost symbol_post = *parsed.delta[s].begin();
					REQUIRE(symbol_post.targets.size() == 1);
					s = *symbol_post.targets.begin();
					level++;
				}
				CHECK(parsed.final.contains(s));
				CHECK(parsed.levels[s] == 10);
			}

			SECTION("from stream") {
				std::stringstream nft_stream;
				nft.print_to_mata(nft_stream);
				Nft parsed{mata::nft::builder::parse_from_mata(nft_stream)};

				REQUIRE(parsed.initial.size() == 1);
				REQUIRE(parsed.final.size() == 1);
				CHECK(parsed.levels.num_of_levels == 11);
				State s{*parsed.initial.begin()};
				Level level = 0;
				while (s != *parsed.final.begin()) {
					CHECK(parsed.levels[s] == level);
					REQUIRE(parsed.delta[s].size() == 1);
					SymbolPost symbol_post = *parsed.delta[s].begin();
					REQUIRE(symbol_post.targets.size() == 1);
					s = *symbol_post.targets.begin();
					level++;
				}
				CHECK(parsed.final.contains(s));
				CHECK(parsed.levels[s] == 10);
			}

			SECTION("from file") {
				std::filesystem::path nft_file{"./temp-test-parse_from_mata-levels_testing.mata"};
				std::fstream file{nft_file, std::fstream::in | std::fstream::out | std::fstream::trunc};
				nft.print_to_mata(file);
				Nft parsed{mata::nft::builder::parse_from_mata(nft_file)};
				file.close();
				std::filesystem::remove(nft_file);

				REQUIRE(parsed.initial.size() == 1);
				REQUIRE(parsed.final.size() == 1);
				CHECK(parsed.levels.num_of_levels == 11);
				State s{*parsed.initial.begin()};
				Level level = 0;
				while (s != *parsed.final.begin()) {
					CHECK(parsed.levels[s] == level);
					REQUIRE(parsed.delta[s].size() == 1);
					SymbolPost symbol_post = *parsed.delta[s].begin();
					REQUIRE(symbol_post.targets.size() == 1);
					s = *symbol_post.targets.begin();
					level++;
				}
				CHECK(parsed.final.contains(s));
				CHECK(parsed.levels[s] == 10);
			}
		}

		SECTION("descending") {
			Nft nft;
			nft.delta.add(0, 1, 1);
			nft.delta.add(1, 1, 2);
			nft.delta.add(2, 1, 3);
			nft.delta.add(3, 1, 4);
			nft.delta.add(4, 1, 5);
			nft.delta.add(5, 1, 6);
			nft.delta.add(6, 1, 7);
			nft.delta.add(7, 1, 8);
			nft.delta.add(8, 1, 9);
			nft.delta.add(9, 1, 10);
			nft.initial.insert(0);
			nft.final.insert(10);
			nft.levels.num_of_levels = 11;
			nft.levels.set({10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0});

			SECTION("from string") {
				Nft parsed{mata::nft::builder::parse_from_mata(nft.print_to_mata())};

				REQUIRE(parsed.initial.size() == 1);
				REQUIRE(parsed.final.size() == 1);
				CHECK(parsed.levels.num_of_levels == 11);
				State s{*parsed.initial.begin()};
				Level level = 10;
				while (s != *parsed.final.begin()) {
					CHECK(parsed.levels[s] == level);
					REQUIRE(parsed.delta[s].size() == 1);
					SymbolPost symbol_post = *parsed.delta[s].begin();
					REQUIRE(symbol_post.targets.size() == 1);
					s = *symbol_post.targets.begin();
					level--;
				}
				CHECK(parsed.final.contains(s));
				CHECK(parsed.levels[s] == 0);
			}

			SECTION("from stream") {
				std::stringstream nft_stream;
				nft.print_to_mata(nft_stream);
				Nft parsed{mata::nft::builder::parse_from_mata(nft_stream)};

				REQUIRE(parsed.initial.size() == 1);
				REQUIRE(parsed.final.size() == 1);
				CHECK(parsed.levels.num_of_levels == 11);
				State s{*parsed.initial.begin()};
				Level level = 10;
				while (s != *parsed.final.begin()) {
					CHECK(parsed.levels[s] == level);
					REQUIRE(parsed.delta[s].size() == 1);
					SymbolPost symbol_post = *parsed.delta[s].begin();
					REQUIRE(symbol_post.targets.size() == 1);
					s = *symbol_post.targets.begin();
					level--;
				}
				CHECK(parsed.final.contains(s));
				CHECK(parsed.levels[s] == 0);
			}

			SECTION("from file") {
				std::filesystem::path nft_file{"./temp-test-parse_from_mata-levels_testing.mata"};
				std::fstream file{nft_file, std::fstream::in | std::fstream::out | std::fstream::trunc};
				nft.print_to_mata(file);
				Nft parsed{mata::nft::builder::parse_from_mata(nft_file)};
				file.close();
				std::filesystem::remove(nft_file);

				REQUIRE(parsed.initial.size() == 1);
				REQUIRE(parsed.final.size() == 1);
				CHECK(parsed.levels.num_of_levels == 11);
				State s{*parsed.initial.begin()};
				Level level = 10;
				while (s != *parsed.final.begin()) {
					CHECK(parsed.levels[s] == level);
					REQUIRE(parsed.delta[s].size() == 1);
					SymbolPost symbol_post = *parsed.delta[s].begin();
					REQUIRE(symbol_post.targets.size() == 1);
					s = *symbol_post.targets.begin();
					level--;
				}
				CHECK(parsed.final.contains(s));
				CHECK(parsed.levels[s] == 0);
			}
		}
	}
}

namespace {
/// Checks that parse_from_mata() reads @p input as builder::construct() reads mata::parser::parse_mf() of it, with
///  the same state numbering, or fails as it does.
void check_reads_as_general_reader(const std::string& input) {
	CAPTURE(input);
	std::optional<Nft> general;
	try {
		const mata::parser::Parsed parsed{mata::parser::parse_mf(input)};
		if (parsed.size() == 1) {
			IntAlphabet alphabet;
			general = builder::construct(parsed[0], &alphabet);
		}
	} catch (const std::exception&) {}
	if (!general.has_value()) {
		CHECK_THROWS(builder::parse_from_mata(input));
		return;
	}
	const Nft nft{builder::parse_from_mata(input)};
	CHECK(nft.num_of_states() == general->num_of_states());
	CHECK(nft.delta == general->delta);
	CHECK(
		std::set<State>(nft.initial.begin(), nft.initial.end()) ==
		std::set<State>(general->initial.begin(), general->initial.end())
	);
	CHECK(
		std::set<State>(nft.final.begin(), nft.final.end()) ==
		std::set<State>(general->final.begin(), general->final.end())
	);
	CHECK(nft.levels == general->levels);
}
} // namespace

TEST_CASE("mata::nft::parse_from_mata() reads what the general reader reads") {
	SECTION("printed transducers") {
		Nft nft{Nft::with_levels({3, {0, 1, 2, 0, 1, 2}}, 6, {0}, {3, 5})};
		nft.delta.add(0, 1, 1);
		nft.delta.add(1, EPSILON, 2);
		nft.delta.add(2, DONT_CARE, 3);
		nft.delta.add(3, 4, 4);
		nft.delta.add(4, 5, 5);
		check_reads_as_general_reader(nft.print_to_mata());
		check_reads_as_general_reader(Nft{}.print_to_mata());
	}

	SECTION("layout") {
		// The keys are read before the body in a fixed order, whatever their order in the file.
		check_reads_as_general_reader(
			"# comment\n@NFT\n%LevelsNum 2\n%Levels s:1 r:0\n%Final r\n%Initial \"p q\" \\\n s\n%Final x\n%Foo\n"
			"s 1 r # comment\n\"p q\" 2 s\nr\t3 \"p q\"\n"
		);
		check_reads_as_general_reader("@NFT-explicit\n%Initial a\na 1 b\n%Foo x\n");
	}

	SECTION("late keys, formulas, and unusual names") {
		check_reads_as_general_reader("@NFT-explicit\n%Initial a\na 1 b\n%Final b\n");
		check_reads_as_general_reader("@NFT-explicit\n%LevelsNum 1\n%LevelsNum 2\na 1 b\n");
		check_reads_as_general_reader("@NFT-explicit\n%Levels a:+1\n%LevelsNum 2\na 1 b\n");
		check_reads_as_general_reader("@NFT-explicit\n%Initial a|b\na 1 b\n");
		check_reads_as_general_reader("@NFT-explicit\n%Levels :1 b:0\n%LevelsNum 2\nb 1 b\n");
	}

	SECTION("malformed inputs") {
		check_reads_as_general_reader("");
		check_reads_as_general_reader("@NFT-bits\na x b\n");
		check_reads_as_general_reader("@NFT-explicit\n%Levels a:1:2\na 1 b\n");
		check_reads_as_general_reader("@NFT-explicit\n%Levels a\na 1 b\n");
		check_reads_as_general_reader("@NFT-explicit\n%Levels a:-1\na 1 b\n");
		check_reads_as_general_reader("@NFT-explicit\n%Levels a:99999999999999999999\na 1 b\n");
		check_reads_as_general_reader("@NFT-explicit\n%LevelsNum 1 2\na 1 b\n");
		check_reads_as_general_reader("@NFT-explicit\n%LevelsNum\na 1 b\n");
		check_reads_as_general_reader("@NFT-explicit\na 1\n");
		check_reads_as_general_reader("@NFT-explicit\na x b\n");
		check_reads_as_general_reader("@NFT-explicit\na 1 b\n@NFT-explicit\na 1 b\n");
	}
}

TEST_CASE("mata::nft::builder::parse_explicit_from_mata()") {
	SECTION("reads plain transducers with their state map") {
		builder::NameStateMap state_map;
		const std::optional<Nft> nft{builder::parse_explicit_from_mata(
			"@NFT-explicit\n%Initial p\n%Final p\n%Levels p:0 q:1\n%LevelsNum 2\np 7 q\nq 8 p\n", nullptr, &state_map
		)};
		REQUIRE(nft.has_value());
		CHECK(nft->levels.num_of_levels == 2);
		CHECK(nft->levels[state_map.at("q")] == 1);
		CHECK(nft->delta.contains(state_map.at("p"), 7, state_map.at("q")));
		CHECK(builder::parse_explicit_from_mata("@NFT\n%Levels q:4294967295\np 1 q\n").has_value());
	}

	SECTION("leaves the general reader the levels it would narrow, overwrite, or read with leading zeros") {
		builder::NameStateMap state_map{{"x", 0}};
		for (const char* const input : {
				 "@NFT\n%Levels q:4294967296\np 1 q\n",
				 "@NFT\n%LevelsNum 4294967296\np 1 q\n",
				 "@NFT\n%Levels q:5000 q:1\np 1 q\n",
				 "@NFT\n%Levels q:01\np 1 q\n",
			 }) {
			CAPTURE(input);
			CHECK_FALSE(builder::parse_explicit_from_mata(input, nullptr, &state_map).has_value());
			CHECK(state_map == builder::NameStateMap{{"x", 0}});
			check_reads_as_general_reader(input);
		}
	}
}

TEST_CASE("mata::nft::builder::from_nfa_with_levels_advancing() gives unreachable states a valid level - issue #763") {
	mata::nfa::Nfa nfa(4, {0}, {0});
	nfa.delta.add(0, 'a', 1);
	nfa.delta.add(1, 'b', 0);
	nfa.delta.add(2, 'c', 3); // Unreachable from the initial states.
	nfa.delta.add(3, 'd', 2);

	const Nft nft = builder::from_nfa_with_levels_advancing(nfa, 2);
	CHECK(nft.levels[0] == 0);
	CHECK(nft.levels[1] == 1);
	// Unreachable states must not keep the sentinel level.
	CHECK(nft.levels[2] == 0);
	CHECK(nft.levels[3] == 0);
	for (const Level level : nft.levels) { CHECK(level < nft.levels.num_of_levels); }
}

namespace {
/// Checks that @p states keeps the names of @p mapped and gives every other name a state of its own, that @p nft has
///  exactly the edges @p edges between the states of the names @p prefix + source and @p prefix + target, and that
///  every state of @p nft has a level.
void check_new_names_keep_off(
	const Nft& nft,
	const builder::NameStateMap& states,
	const builder::NameStateMap& mapped,
	const std::string& prefix,
	const std::vector<std::pair<std::string, std::string>>& edges
) {
	for (const auto& [name, state] : mapped) { CHECK(states.at(name) == state); }
	std::set<State> distinct;
	for (const auto& [name, state] : states) { distinct.insert(state); }
	CHECK(distinct.size() == states.size());
	std::set<std::pair<State, State>> expected;
	for (const auto& [source, target] : edges) {
		expected.emplace(states.at(prefix + source), states.at(prefix + target));
	}
	std::set<std::pair<State, State>> actual;
	for (const auto& transition : nft.delta.transitions()) { actual.emplace(transition.source, transition.target); }
	CHECK(actual == expected);
	CHECK(nft.levels.size() == nft.num_of_states());
}
} // namespace

TEST_CASE("mata::nft::builder readers keep new names off the states of a caller's map") {
	// The caller maps q1, which the input names after, before, or around new names, and q7, which it does not name.
	//  The %Levels line names a state before the body. IntermediateAut drops the marker q of the state names and
	//  ignores the levels; its \true final states include q7.
	const builder::NameStateMap mapped{{"q1", 0}, {"q7", 3}};
	const builder::NameStateMap unmarked_mapped{{"1", 0}, {"7", 3}};
	for (const auto& [leveled, edges] : {
			 std::pair{"q1", std::vector<std::pair<std::string, std::string>>{{"1", "2"}}},
			 std::pair{"q2", std::vector<std::pair<std::string, std::string>>{{"2", "1"}}},
			 std::pair{
				 "q2", std::vector<std::pair<std::string, std::string>>{{"2", "4"}, {"4", "5"}, {"5", "6"}, {"6", "1"}}
			 },
		 }) {
		std::string body;
		for (const auto& [source, target] : edges) { body += "q" + source + " 5 q" + target + "\n"; }
		const std::string input{"@NFT\n%Levels " + std::string{leveled} + ":1\n%LevelsNum 2\n" + body};
		CAPTURE(input);

		builder::NameStateMap explicit_states{mapped};
		const std::optional<Nft> explicit_nft{builder::parse_explicit_from_mata(input, nullptr, &explicit_states)};
		REQUIRE(explicit_nft.has_value());
		check_new_names_keep_off(*explicit_nft, explicit_states, mapped, "q", edges);
		CHECK(explicit_nft->levels[explicit_states.at(leveled)] == 1);

		IntAlphabet alphabet;
		builder::NameStateMap general_states{mapped};
		const Nft general{builder::construct(mata::parser::parse_mf(input)[0], &alphabet, &general_states)};
		check_new_names_keep_off(general, general_states, mapped, "q", edges);
		CHECK(general.levels[general_states.at(leveled)] == 1);
		CHECK(general_states == explicit_states);
		CHECK(general.delta == explicit_nft->delta);

		const mata::IntermediateAut inter_aut{mata::IntermediateAut::parse_from_mf(
			mata::parser::parse_mf("@NFT-explicit\n%Alphabet-auto\n%Final \\true\n" + body)
		)[0]};
		OnTheFlyAlphabet inter_alphabet;
		builder::NameStateMap inter_states{unmarked_mapped};
		check_new_names_keep_off(
			builder::construct(inter_aut, &inter_alphabet, &inter_states), inter_states, unmarked_mapped, "", edges
		);
		const auto alphabets{std::make_shared<mata::AlphabetLevels>(std::make_shared<OnTheFlyAlphabet>())};
		builder::NameStateMap levels_states{unmarked_mapped};
		check_new_names_keep_off(
			builder::construct(inter_aut, alphabets, &levels_states), levels_states, unmarked_mapped, "", edges
		);
	}
}
