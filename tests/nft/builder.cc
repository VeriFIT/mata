// TODO: some header

#include <unordered_set>
#include <vector>
#include <fstream>

#include <catch2/catch.hpp>

#include "mata/nft/nft.hh"
#include "mata/nft/builder.hh"

using namespace mata::nft;
using Symbol = mata::Symbol;
using IntAlphabet = mata::IntAlphabet;
using OnTheFlyAlphabet = mata::OnTheFlyAlphabet;

using Word = std::vector<Symbol>;

TEST_CASE("nft::parse_from_mata()") {
    Delta delta;

    SECTION("Simple automaton") {
        delta.add(0, 0, 0);
        delta.add(0, 1, 1);
        delta.add(1, 2, 0);
        Nft nft{ delta, { 0 }, { 1 }, { 0 }, 1};

        SECTION("from string") {
            Nft parsed{ mata::nft::builder::parse_from_mata(nft.print_to_mata()) };
            CHECK(are_equivalent(parsed, nft));
        }

        SECTION("from stream") {
            std::stringstream nft_stream;
            nft.print_to_mata(nft_stream);
            Nft parsed{ mata::nft::builder::parse_from_mata(nft_stream) };
            CHECK(are_equivalent(parsed, nft));
        }

        SECTION("from file") {
            std::filesystem::path nft_file{ "./temp-test-parse_from_mata-simple_nft.mata" };
            std::fstream file{ nft_file, std::fstream::in | std::fstream::out | std::fstream::trunc};
            nft.print_to_mata(file);
            Nft parsed{ mata::nft::builder::parse_from_mata(nft_file) };
            file.close();
            std::filesystem::remove(nft_file);

            CHECK(are_equivalent(parsed, nft));
        }

    }

    SECTION("larger automaton") {
        Nft nft;
        nft.initial = { 1, 2, 50 };
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
        nft.final = { 3, 103 };
        nft.levels = std::vector<Level>(nft.num_of_states(), 0);
        nft.levels[3] = 42;
        nft.levels[103] = 42;
        nft.levels_cnt = 43;

        SECTION("from string") {
            Nft parsed{ mata::nft::builder::parse_from_mata(nft.print_to_mata()) };
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

            CHECK(are_equivalent(parsed, nft));
        }

        SECTION("from stream") {
            std::stringstream nft_stream;
            nft.print_to_mata(nft_stream);
            Nft parsed{ mata::nft::builder::parse_from_mata(nft_stream) };
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

            CHECK(are_equivalent(parsed, nft));
        }

        SECTION("from file") {
            std::filesystem::path nft_file{ "./temp-test-parse_from_mata-larger_nft.mata" };
            std::fstream file{ nft_file, std::fstream::in | std::fstream::out | std::fstream::trunc };
            nft.print_to_mata(file);
            Nft parsed{ mata::nft::builder::parse_from_mata(nft_file) };
            file.close();
            std::filesystem::remove(nft_file);

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
            nft.levels = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
            nft.levels_cnt = 11;

            SECTION("from string") {
                Nft parsed{ mata::nft::builder::parse_from_mata(nft.print_to_mata()) };

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
                std::stringstream nft_stream;
                nft.print_to_mata(nft_stream);
                Nft parsed{ mata::nft::builder::parse_from_mata(nft_stream) };

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
                std::filesystem::path nft_file{ "./temp-test-parse_from_mata-levels_testing.mata" };
                std::fstream file{ nft_file, std::fstream::in | std::fstream::out | std::fstream::trunc };
                nft.print_to_mata(file);
                Nft parsed{ mata::nft::builder::parse_from_mata(nft_file) };
                file.close();
                std::filesystem::remove(nft_file);

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
            nft.levels = { 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };
            nft.levels_cnt = 11;

            SECTION("from string") {
                Nft parsed{ mata::nft::builder::parse_from_mata(nft.print_to_mata()) };

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
                std::stringstream nft_stream;
                nft.print_to_mata(nft_stream);
                Nft parsed{ mata::nft::builder::parse_from_mata(nft_stream) };

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
                std::filesystem::path nft_file{ "./temp-test-parse_from_mata-levels_testing.mata" };
                std::fstream file{ nft_file, std::fstream::in | std::fstream::out | std::fstream::trunc };
                nft.print_to_mata(file);
                Nft parsed{ mata::nft::builder::parse_from_mata(nft_file) };
                file.close();
                std::filesystem::remove(nft_file);

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
