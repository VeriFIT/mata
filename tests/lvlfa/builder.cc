// TODO: some header

#include <unordered_set>
#include <vector>
#include <fstream>

#include <catch2/catch.hpp>

#include "mata/lvlfa/lvlfa.hh"
#include "mata/lvlfa/builder.hh"

using namespace mata::lvlfa;
using Symbol = mata::Symbol;
using IntAlphabet = mata::IntAlphabet;
using OnTheFlyAlphabet = mata::OnTheFlyAlphabet;

using Word = std::vector<Symbol>;

TEST_CASE("lvlfa::parse_from_mata()") {
    Delta delta;

    SECTION("Simple automaton") {
        delta.add(0, 0, 0);
        delta.add(0, 1, 1);
        delta.add(1, 2, 0);
        Lvlfa lvlfa{ delta, { 0 }, { 1 }, { 0 }, 1};

        SECTION("from string") {
            Lvlfa parsed{ mata::lvlfa::builder::parse_from_mata(lvlfa.print_to_mata()) };
            CHECK(are_equivalent(parsed, lvlfa));
        }

        SECTION("from stream") {
            std::stringstream lvlfa_stream;
            lvlfa.print_to_mata(lvlfa_stream);
            Lvlfa parsed{ mata::lvlfa::builder::parse_from_mata(lvlfa_stream) };
            CHECK(are_equivalent(parsed, lvlfa));
        }

        SECTION("from file") {
            std::filesystem::path lvlfa_file{ "./temp-test-parse_from_mata-simple_lvlfa.mata" };
            std::fstream file{ lvlfa_file, std::fstream::in | std::fstream::out | std::fstream::trunc};
            lvlfa.print_to_mata(file);
            Lvlfa parsed{ mata::lvlfa::builder::parse_from_mata(lvlfa_file) };
            file.close();
            std::filesystem::remove(lvlfa_file);

            CHECK(are_equivalent(parsed, lvlfa));
        }

    }

    SECTION("larger automaton") {
        Lvlfa lvlfa;
        lvlfa.initial = { 1, 2, 50 };
        lvlfa.delta.add(1, 'a', 2);
        lvlfa.delta.add(1, 'a', 3);
        lvlfa.delta.add(1, 'b', 4);
        lvlfa.delta.add(2, 'a', 2);
        lvlfa.delta.add(2, 'b', 2);
        lvlfa.delta.add(2, 'a', 3);
        lvlfa.delta.add(2, 'b', 4);
        lvlfa.delta.add(3, 'b', 4);
        lvlfa.delta.add(3, 'c', 7);
        lvlfa.delta.add(3, 'b', 2);
        lvlfa.delta.add(5, 'c', 3);
        lvlfa.delta.add(7, 'a', 8);
        lvlfa.delta.add(12, 'b', 15);
        lvlfa.delta.add(1, 'b', 40);
        lvlfa.delta.add(51, 'z', 42);
        lvlfa.final = { 3, 103 };
        lvlfa.levels = std::vector<Level>(lvlfa.num_of_states(), 0);
        lvlfa.levels[3] = 42;
        lvlfa.levels[103] = 42;
        lvlfa.levels_cnt = 43;

        SECTION("from string") {
            Lvlfa parsed{ mata::lvlfa::builder::parse_from_mata(lvlfa.print_to_mata()) };
            parsed.final.contains(103);
            parsed.initial.contains(50);
            parsed.delta.contains(51, 'z', 42);
            CHECK(parsed.levels_cnt == 43);

            std::vector test_levels(parsed.levels);
            for (const State &s : parsed.final) {
                CHECK(test_levels[s] == 42);
                test_levels[s] = 0;
            }
            CHECK(std::all_of(test_levels.begin(), test_levels.end(), [](Level l) { return l==0; }));

            CHECK(are_equivalent(parsed, lvlfa));
        }

        SECTION("from stream") {
            std::stringstream lvlfa_stream;
            lvlfa.print_to_mata(lvlfa_stream);
            Lvlfa parsed{ mata::lvlfa::builder::parse_from_mata(lvlfa_stream) };
            parsed.final.contains(103);
            parsed.initial.contains(50);
            parsed.delta.contains(51, 'z', 42);
            CHECK(parsed.levels_cnt == 43);

            std::vector test_levels(parsed.levels);
            for (const State &s : parsed.final) {
                CHECK(test_levels[s] == 42);
                test_levels[s] = 0;
            }
            CHECK(std::all_of(test_levels.begin(), test_levels.end(), [](Level l) { return l==0; }));

            CHECK(are_equivalent(parsed, lvlfa));
        }

        SECTION("from file") {
            std::filesystem::path lvlfa_file{ "./temp-test-parse_from_mata-larger_lvlfa.mata" };
            std::fstream file{ lvlfa_file, std::fstream::in | std::fstream::out | std::fstream::trunc };
            lvlfa.print_to_mata(file);
            Lvlfa parsed{ mata::lvlfa::builder::parse_from_mata(lvlfa_file) };
            file.close();
            std::filesystem::remove(lvlfa_file);

            parsed.final.contains(103);
            parsed.initial.contains(50);
            parsed.delta.contains(51, 'z', 42);
            CHECK(parsed.levels_cnt == 43);

            std::vector test_levels(parsed.levels);
            for (const State &s : parsed.final) {
                CHECK(test_levels[s] == 42);
                test_levels[s] = 0;
            }
            CHECK(std::all_of(test_levels.begin(), test_levels.end(), [](Level l) { return l==0; }));

            CHECK(are_equivalent(parsed, lvlfa));
        }
    }

    SECTION("levels testing") {
        SECTION("ascending") {
            Lvlfa lvlfa;
            lvlfa.delta.add(0, 1, 1);
            lvlfa.delta.add(1, 1, 2);
            lvlfa.delta.add(2, 1, 3);
            lvlfa.delta.add(3, 1, 4);
            lvlfa.delta.add(4, 1, 5);
            lvlfa.delta.add(5, 1, 6);
            lvlfa.delta.add(6, 1, 7);
            lvlfa.delta.add(7, 1, 8);
            lvlfa.delta.add(8, 1, 9);
            lvlfa.delta.add(9, 1, 10);
            lvlfa.initial.insert(0);
            lvlfa.final.insert(10);
            lvlfa.levels = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
            lvlfa.levels_cnt = 11;

            SECTION("from string") {
                Lvlfa parsed{ mata::lvlfa::builder::parse_from_mata(lvlfa.print_to_mata()) };

                REQUIRE(parsed.initial.size() == 1);
                REQUIRE(parsed.final.size() == 1);
                CHECK(parsed.levels_cnt == 11);
                State s{ *parsed.initial.begin() };
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
                std::stringstream lvlfa_stream;
                lvlfa.print_to_mata(lvlfa_stream);
                Lvlfa parsed{ mata::lvlfa::builder::parse_from_mata(lvlfa_stream) };

                REQUIRE(parsed.initial.size() == 1);
                REQUIRE(parsed.final.size() == 1);
                CHECK(parsed.levels_cnt == 11);
                State s{ *parsed.initial.begin() };
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
                std::filesystem::path lvlfa_file{ "./temp-test-parse_from_mata-levels_testing.mata" };
                std::fstream file{ lvlfa_file, std::fstream::in | std::fstream::out | std::fstream::trunc };
                lvlfa.print_to_mata(file);
                Lvlfa parsed{ mata::lvlfa::builder::parse_from_mata(lvlfa_file) };
                file.close();
                std::filesystem::remove(lvlfa_file);

                REQUIRE(parsed.initial.size() == 1);
                REQUIRE(parsed.final.size() == 1);
                CHECK(parsed.levels_cnt == 11);
                State s{ *parsed.initial.begin() };
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
            Lvlfa lvlfa;
            lvlfa.delta.add(0, 1, 1);
            lvlfa.delta.add(1, 1, 2);
            lvlfa.delta.add(2, 1, 3);
            lvlfa.delta.add(3, 1, 4);
            lvlfa.delta.add(4, 1, 5);
            lvlfa.delta.add(5, 1, 6);
            lvlfa.delta.add(6, 1, 7);
            lvlfa.delta.add(7, 1, 8);
            lvlfa.delta.add(8, 1, 9);
            lvlfa.delta.add(9, 1, 10);
            lvlfa.initial.insert(0);
            lvlfa.final.insert(10);
            lvlfa.levels = { 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };
            lvlfa.levels_cnt = 11;

            SECTION("from string") {
                Lvlfa parsed{ mata::lvlfa::builder::parse_from_mata(lvlfa.print_to_mata()) };

                REQUIRE(parsed.initial.size() == 1);
                REQUIRE(parsed.final.size() == 1);
                CHECK(parsed.levels_cnt == 11);
                State s{ *parsed.initial.begin() };
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
                std::stringstream lvlfa_stream;
                lvlfa.print_to_mata(lvlfa_stream);
                Lvlfa parsed{ mata::lvlfa::builder::parse_from_mata(lvlfa_stream) };

                REQUIRE(parsed.initial.size() == 1);
                REQUIRE(parsed.final.size() == 1);
                CHECK(parsed.levels_cnt == 11);
                State s{ *parsed.initial.begin() };
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
                std::filesystem::path lvlfa_file{ "./temp-test-parse_from_mata-levels_testing.mata" };
                std::fstream file{ lvlfa_file, std::fstream::in | std::fstream::out | std::fstream::trunc };
                lvlfa.print_to_mata(file);
                Lvlfa parsed{ mata::lvlfa::builder::parse_from_mata(lvlfa_file) };
                file.close();
                std::filesystem::remove(lvlfa_file);

                REQUIRE(parsed.initial.size() == 1);
                REQUIRE(parsed.final.size() == 1);
                CHECK(parsed.levels_cnt == 11);
                State s{ *parsed.initial.begin() };
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
