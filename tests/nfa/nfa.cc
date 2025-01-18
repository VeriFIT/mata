// TODO: some header

#include <unordered_set>

#include <catch2/catch.hpp>

#include "utils.hh"

#include "mata/utils/sparse-set.hh"
#include "mata/nfa/delta.hh"
#include "mata/nfa/nfa.hh"
#include "mata/nfa/strings.hh"
#include "mata/nfa/builder.hh"
#include "mata/nfa/plumbing.hh"
#include "mata/nfa/algorithms.hh"
#include "mata/parser/re2parser.hh"

using namespace mata;
using namespace mata::nfa::algorithms;
using namespace mata::nfa;
using namespace mata::strings;
using namespace mata::nfa::plumbing;
using namespace mata::utils;
using namespace mata::parser;
using Symbol = mata::Symbol;
using Word = mata::Word;
using IntAlphabet = mata::IntAlphabet;
using OnTheFlyAlphabet = mata::OnTheFlyAlphabet;

TEST_CASE("mata::nfa::size()") {
    Nfa nfa{};
    CHECK(nfa.num_of_states() == 0);

    nfa.add_state(3);
    CHECK(nfa.num_of_states() == 4);

    nfa.clear();
    nfa.add_state();
    CHECK(nfa.num_of_states() == 1);

    nfa.clear();
    FILL_WITH_AUT_A(nfa);
    CHECK(nfa.num_of_states() == 11);

    nfa.clear();
    FILL_WITH_AUT_B(nfa);
    CHECK(nfa.num_of_states() == 15);

    nfa = Nfa{ 0, {}, {} };
    CHECK(nfa.num_of_states() == 0);
}

TEST_CASE("StatePost::emplace_back()") {
    StatePost state_post{};
    state_post.emplace_back(1, StateSet{2, 3});
    CHECK(state_post == StatePost{ SymbolPost{ Symbol{ 1 }, StateSet{ 2, 3 } } });
    CHECK(*state_post.find(1) == SymbolPost{ SymbolPost{ Symbol{ 1 }, StateSet{ 2, 3 } } });
}

TEST_CASE("mata::nfa::Trans::operator<<") {
    Transition trans(1, 2, 3);
    REQUIRE(std::to_string(trans) == "(1, 2, 3)");
}

TEST_CASE("mata::nfa::create_alphabet()") {
    Nfa a{1};
    a.delta.add(0, 'a', 0);

    Nfa b{1};
    b.delta.add(0, 'b', 0);
    b.delta.add(0, 'a', 0);
    Nfa c{1};
    b.delta.add(0, 'c', 0);

    auto alphabet{ create_alphabet(a, b, c) };

    auto symbols{alphabet.get_alphabet_symbols() };
    CHECK(symbols == mata::utils::OrdVector<Symbol>{ 'c', 'b', 'a' });

    // create_alphabet(1, 3, 4); // Will not compile: '1', '3', '4' are not of the required type.
    // create_alphabet(a, b, 4); // Will not compile: '4' is not of the required type.
}

TEST_CASE("mata::nfa::Nfa::delta.add()/delta.contains()")
{ // {{{
    Nfa a(3);

    SECTION("Empty automata have now transitions")
    {
        REQUIRE(!a.delta.contains(1, 'a', 1));
    }

    SECTION("If I add a transition, it is in the automaton")
    {
        a.delta.add(1, 'a', 1);

        REQUIRE(a.delta.contains(1, 'a', 1));
    }

    SECTION("If I add a transition, only it is added")
    {
        a.delta.add(1, 'a', 1);

        REQUIRE(a.delta.contains(1, 'a', 1));
        REQUIRE(!a.delta.contains(1, 'a', 2));
        REQUIRE(!a.delta.contains(1, 'b', 2));
        REQUIRE(!a.delta.contains(2, 'a', 1));
    }

    SECTION("Adding multiple transitions")
    {
        a.delta.add(2, 'b', {2,1,0});
        REQUIRE(a.delta.contains(2, 'b', 0));
        REQUIRE(a.delta.contains(2, 'b', 1));
        REQUIRE(a.delta.contains(2, 'b', 2));
        REQUIRE(!a.delta.contains(0, 'b', 0));

        a.delta.add(0, 'b', StateSet({0}));
        REQUIRE(a.delta.contains(0, 'b', 0));
    }

    SECTION("Iterating over transitions") {
        Transition t1{ 0, 0, 0};
        Transition t2{ 0, 1, 0};
        Transition t3{ 1, 1, 1};
        Transition t4{ 2, 2, 2};
        a.delta.add(t1);
        a.delta.add(t2);
        a.delta.add(t3);
        a.delta.add(t4);
        a.delta.add(t3);
        size_t transitions_cnt{ 0 };
        std::vector<Transition> expected_transitions{ t1, t2, t3, t4 };
        std::vector<Transition> iterated_transitions{};
        const Delta::Transitions transitions{ a.delta.transitions() };
        const Delta::Transitions::const_iterator transitions_end{ transitions.end() };
        for (Delta::Transitions::const_iterator trans_it{ transitions.begin()}; trans_it != transitions_end; ++trans_it) {
            iterated_transitions.push_back(*trans_it);
            ++transitions_cnt;
        }
        CHECK(transitions_cnt == 4);
        CHECK(expected_transitions == iterated_transitions);

        transitions_cnt = 0;
        iterated_transitions.clear();
        for (const Transition& trans: a.delta.transitions()) {
            iterated_transitions.push_back(trans);
            ++transitions_cnt;
        }
        CHECK(transitions_cnt == 4);
        CHECK(expected_transitions == iterated_transitions);
    }

} // }}}

TEST_CASE("mata::nfa::Delta.transform/append")
{ // {{{
    Nfa a(3);
    a.delta.add(1, 'a', 1);
    a.delta.add(2, 'b', {2,1,0});

    SECTION("transform")
    {
        auto upd_fnc = [&](State st) {
            return st + 5;
        };
        std::vector<StatePost> state_posts = a.delta.renumber_targets(upd_fnc);
        a.delta.append(state_posts);

        REQUIRE(a.delta.contains(4, 'a', 6));
        REQUIRE(a.delta.contains(5, 'b', 7));
        REQUIRE(a.delta.contains(5, 'b', 5));
        REQUIRE(a.delta.contains(5, 'b', 6));
    }

} // }}}

TEST_CASE("mata::nfa::is_lang_empty()")
{ // {{{
    Nfa aut(14);
    Run cex;

    SECTION("An empty automaton has an empty language")
    {
        REQUIRE(aut.is_lang_empty());
    }

    SECTION("An automaton with a state that is both initial and final does not have an empty language")
    {
        aut.initial = {1, 2};
        aut.final = {2, 3};

        bool is_empty = aut.is_lang_empty(&cex);
        REQUIRE(!is_empty);
    }

    SECTION("More complicated automaton")
    {
        aut.initial = {1, 2};
        aut.delta.add(1, 'a', 2);
        aut.delta.add(1, 'a', 3);
        aut.delta.add(1, 'b', 4);
        aut.delta.add(2, 'a', 2);
        aut.delta.add(2, 'a', 3);
        aut.delta.add(2, 'b', 4);
        aut.delta.add(3, 'b', 4);
        aut.delta.add(3, 'c', 7);
        aut.delta.add(3, 'b', 2);
        aut.delta.add(7, 'a', 8);

        SECTION("with final states")
        {
            aut.final = {7};
            REQUIRE(!aut.is_lang_empty());
        }

        SECTION("without final states")
        {
            REQUIRE(aut.is_lang_empty());
        }

        SECTION("another complicated automaton")
        {
            FILL_WITH_AUT_A(aut);

            REQUIRE(!aut.is_lang_empty());
        }

        SECTION("a complicated automaton with unreachable final states")
        {
            FILL_WITH_AUT_A(aut);
            aut.final = {13};

            REQUIRE(aut.is_lang_empty());
        }
    }

    SECTION("An automaton with a state that is both initial and final does not have an empty language")
    {
        aut.initial = {1, 2};
        aut.final = {2, 3};

        bool is_empty = aut.is_lang_empty(&cex);
        REQUIRE(!is_empty);

        // check the counterexample
        REQUIRE(cex.path.size() == 1);
        REQUIRE(cex.path[0] == 2);
    }

    SECTION("Counterexample of an automaton with non-empty language")
    {
        aut.initial = {1, 2};
        aut.final = {8, 9};
        aut.delta.add(1, 'c', 2);
        aut.delta.add(2, 'a', 4);
        aut.delta.add(2, 'c', 1);
        aut.delta.add(2, 'c', 3);
        aut.delta.add(3, 'e', 5);
        aut.delta.add(4, 'c', 8);

        bool is_empty = aut.is_lang_empty(&cex);
        REQUIRE(!is_empty);

        // check the counterexample
        REQUIRE(cex.path.size() == 3);
        REQUIRE(cex.path[0] == 2);
        REQUIRE(cex.path[1] == 4);
        REQUIRE(cex.path[2] == 8);
    }
} // }}}

TEST_CASE("mata::nfa::is_acyclic")
{ // {{{
    Nfa aut(14);

    SECTION("An empty automaton is acyclic")
    {
        REQUIRE(aut.is_acyclic());
    }

    SECTION("An automaton with a state that is both initial and final is acyclic")
    {
        aut.initial = {1, 2};
        aut.final = {2, 3};
        REQUIRE(aut.is_acyclic());
    }

    SECTION("More complicated automaton")
    {
        aut.initial = {1, 2};
        aut.delta.add(1, 'a', 2);
        aut.delta.add(1, 'a', 3);
        aut.delta.add(1, 'b', 4);
        aut.delta.add(2, 'a', 3);
        aut.delta.add(2, 'b', 4);
        aut.delta.add(3, 'b', 4);
        aut.delta.add(3, 'c', 7);
        aut.delta.add(7, 'a', 8);

        SECTION("without final states")
        {
            REQUIRE(aut.is_lang_empty());
        }
    }

    SECTION("Cyclic automaton")
    {
        aut.initial = {1, 2};
        aut.final = {8, 9};
        aut.delta.add(1, 'c', 2);
        aut.delta.add(2, 'a', 4);
        aut.delta.add(2, 'c', 1);
        aut.delta.add(2, 'c', 3);
        aut.delta.add(3, 'e', 5);
        aut.delta.add(4, 'c', 8);
        REQUIRE(!aut.is_acyclic());
    }

    SECTION("Automaton with self-loops")
    {
        Nfa aut(2);
        aut.initial = {0};
        aut.final = {1};
        aut.delta.add(0, 'c', 1);
        aut.delta.add(1, 'a', 1);
        REQUIRE(!aut.is_acyclic());
    }
} // }}}

TEST_CASE("mata::nfa::is_flat")
{ // {{{
    Nfa aut(14);

    SECTION("An empty automaton is flat")
    {
        REQUIRE(aut.is_flat());
    }

    SECTION("An automaton with a state that is both initial and final is acyclic")
    {
        aut.initial = {1, 2};
        aut.final = {2, 3};
        REQUIRE(aut.is_flat());
    }

    SECTION("More complicated automaton")
    {
        aut.initial = {0};
        aut.final = {4};
        aut.delta.add(0, 'a', 1);
        aut.delta.add(1, 'a', 3);
        aut.delta.add(3, 'b', 2);
        aut.delta.add(2, 'a', 1);
        aut.delta.add(1, 'b', 4);
        aut.delta.add(4, 'b', 6);
        aut.delta.add(6, 'c', 5);
        aut.delta.add(5, 'a', 4);
        REQUIRE(aut.is_flat());
    }

    SECTION("Nonflat automaton")
    {
        aut.initial = {0};
        aut.final = {4};
        aut.delta.add(0, 'a', 1);
        aut.delta.add(1, 'a', 3);
        aut.delta.add(3, 'b', 2);
        aut.delta.add(2, 'a', 1);
        aut.delta.add(1, 'b', 4);
        aut.delta.add(4, 'b', 6);
        aut.delta.add(6, 'c', 5);
        aut.delta.add(5, 'a', 4);
        aut.delta.add(1, 'c', 2);
        REQUIRE(!aut.is_flat());
    }
} // }}}

TEST_CASE("mata::nfa::get_word_for_path()")
{ // {{{
    Nfa aut(5);
    Run path;
    Word word;

    SECTION("empty word")
    {
        path = { };

        auto word_bool_pair = aut.get_word_for_path(path);
        REQUIRE(word_bool_pair.second);
        REQUIRE(word_bool_pair.first.word.empty());
    }

    SECTION("empty word 2")
    {
        aut.initial = {1};
        path.path = {1};

        auto word_bool_pair = aut.get_word_for_path(path);
        REQUIRE(word_bool_pair.second);
        REQUIRE(word_bool_pair.first.word.empty());
    }

    SECTION("nonempty word")
    {
        aut.initial = {1};
        aut.delta.add(1, 'c', 2);
        aut.delta.add(2, 'a', 4);
        aut.delta.add(2, 'c', 1);
        aut.delta.add(2, 'b', 3);

        path.path = {1,2,3};

        auto word_bool_pair = aut.get_word_for_path(path);
        REQUIRE(word_bool_pair.second);
        REQUIRE(word_bool_pair.first.word == Word({'c', 'b'}));
    }

    SECTION("longer word")
    {
        aut.initial = {1};
        aut.delta.add(1, 'a', 2);
        aut.delta.add(1, 'c', 2);
        aut.delta.add(2, 'a', 4);
        aut.delta.add(2, 'c', 1);
        aut.delta.add(2, 'b', 3);
        aut.delta.add(3, 'd', 2);

        path.path = {1,2,3,2,4};

        auto word_bool_pair = aut.get_word_for_path(path);
        std::set<Word> possible({
            Word({'c', 'b', 'd', 'a'}),
            Word({'a', 'b', 'd', 'a'})});
        REQUIRE(word_bool_pair.second);
        REQUIRE(haskey(possible, word_bool_pair.first.word));
    }

    SECTION("invalid path")
    {
        aut.initial = {1};
        aut.delta.add(1, 'a', 2);
        aut.delta.add(1, 'c', 2);
        aut.delta.add(2, 'a', 4);
        aut.delta.add(2, 'c', 1);
        aut.delta.add(2, 'b', 3);
        aut.delta.add(3, 'd', 2);

        path.path = {1,2,3,1,2};

        auto word_bool_pair = aut.get_word_for_path(path);
        REQUIRE(!word_bool_pair.second);
    }
}


TEST_CASE("mata::nfa::is_lang_empty_cex()")
{
    Nfa aut(10);
    Run cex;

    SECTION("Counterexample of an automaton with non-empty language")
    {
        aut.initial = {1, 2};
        aut.final = {8, 9};
        aut.delta.add(1, 'c', 2);
        aut.delta.add(2, 'a', 4);
        aut.delta.add(2, 'c', 1);
        aut.delta.add(2, 'c', 3);
        aut.delta.add(3, 'e', 5);
        aut.delta.add(4, 'c', 8);

        bool is_empty = aut.is_lang_empty(&cex);
        REQUIRE(!is_empty);

        // check the counterexample
        REQUIRE(cex.word.size() == 2);
        REQUIRE(cex.word[0] == 'a');
        REQUIRE(cex.word[1] == 'c');
    }
}

TEST_CASE("mata::nfa::determinize()")
{
    Nfa aut(3);
    Nfa result;
    std::unordered_map<StateSet, State> subset_map;

    SECTION("empty automaton")
    {
        result = determinize(aut);

        REQUIRE(result.final.empty());
        REQUIRE(result.delta.empty());
        CHECK(result.is_lang_empty());
    }

    SECTION("simple automaton 1")
    {
        aut.initial = {1 };
        aut.final = {1 };
        result = determinize(aut, &subset_map);

        REQUIRE(result.initial[subset_map[{1}]]);
        REQUIRE(result.final[subset_map[{1}]]);
        REQUIRE(result.delta.empty());
    }

    SECTION("simple automaton 2")
    {
        aut.initial = {1 };
        aut.final = {2 };
        aut.delta.add(1, 'a', 2);
        result = determinize(aut, &subset_map);

        REQUIRE(result.initial[subset_map[{1}]]);
        REQUIRE(result.final[subset_map[{2}]]);
        REQUIRE(result.delta.contains(subset_map[{1}], 'a', subset_map[{2}]));
    }

    SECTION("This broke Delta when delta[q] could cause re-allocation of post")
    {
        Nfa x{};
        x.initial.insert(0);
        x.final.insert(4);
        x.delta.add(0, 1, 3);
        x.delta.add(3, 1, 3);
        x.delta.add(3, 2, 3);
        x.delta.add(3, 0, 1);
        x.delta.add(1, 1, 1);
        x.delta.add(1, 2, 1);
        x.delta.add(1, 0, 2);
        x.delta.add(2, 0, 2);
        x.delta.add(2, 1, 2);
        x.delta.add(2, 2, 2);
        x.delta.add(2, 0, 4);
        OnTheFlyAlphabet alphabet{};
        auto complement_result{determinize(x)};
    }
} // }}}

TEST_CASE("mata::nfa::determinize_boost()")
{
    Nfa aut(3);
    Nfa result;
    std::unordered_map<BoostSet, State> subset_map;

    SECTION("empty automaton")
    {
        result = determinize_boost(aut);

        REQUIRE(result.final.empty());
        REQUIRE(result.delta.empty());
        CHECK(result.is_lang_empty());
    }

    SECTION("simple automaton 1")
    {
        aut.initial = {1 };
        aut.final = {1 };
        result = determinize_boost(aut, &subset_map);

        REQUIRE(result.initial[subset_map[{1, false, true}]]);
        REQUIRE(result.final[subset_map[{1, false, true}]]);
        REQUIRE(result.delta.empty());
    }

    SECTION("simple automaton 2")
    {
        aut.initial = {1 };
        aut.final = {2 };
        aut.delta.add(1, 'a', 2);
        result = determinize_boost(aut, &subset_map);

        // State 1 should be a initial states of the determinized automaton
        REQUIRE(result.initial[subset_map[{1, false, true}]]);

        // State 2 should be a final state of the determinized automaton
        REQUIRE(result.final[subset_map[{2, false, true}]]);

        // The delta of the automaton should contain a rule 1a -> 2
        REQUIRE(result.delta.contains(subset_map[{1, false, true}], 'a', subset_map[{2, false, true}]));
    }

    SECTION("This broke Delta when delta[q] could cause re-allocation of post")
    {
        Nfa x{};
        x.initial.insert(0);
        x.final.insert(4);
        x.delta.add(0, 1, 3);
        x.delta.add(3, 1, 3);
        x.delta.add(3, 2, 3);
        x.delta.add(3, 0, 1);
        x.delta.add(1, 1, 1);
        x.delta.add(1, 2, 1);
        x.delta.add(1, 0, 2);
        x.delta.add(2, 0, 2);
        x.delta.add(2, 1, 2);
        x.delta.add(2, 2, 2);
        x.delta.add(2, 0, 4);
        OnTheFlyAlphabet alphabet{};
        auto complement_result{determinize_boost(x)};
    }
}

TEST_CASE("mata::nfa::Nfa::get_word_from_complement()") {
    Nfa aut{};
    std::optional<mata::Word> result;
    std::unordered_map<StateSet, State> subset_map;
    EnumAlphabet alphabet{ 'a', 'b', 'c' };

    SECTION("empty automaton") {
        result = aut.get_word_from_complement();
        REQUIRE(result.has_value());
        CHECK(*result == Word{});
    }

    SECTION("empty automaton 2") {
        aut.initial = { 0 };
        result = aut.get_word_from_complement();
        REQUIRE(result.has_value());
        CHECK(*result == Word{});
    }

    SECTION("empty automaton 3") {
        aut.initial = { 0 };
        aut.final = { 1 };
        result = aut.get_word_from_complement();
        REQUIRE(result.has_value());
        CHECK(*result == Word{});
    }

    SECTION("simple automaton 1") {
        aut.initial = { 0 };
        aut.final = { 0 };
        result = aut.get_word_from_complement(&alphabet);
        REQUIRE(result.has_value());
        CHECK(*result == Word{ 'a' });
    }

    SECTION("simple automaton 2") {
        aut.initial = { 0 };
        aut.final = { 1 };
        aut.delta.add(0, 'a', 1);
        result = aut.get_word_from_complement();
        REQUIRE(result.has_value());
        CHECK(*result == Word{});
    }

    SECTION("simple automaton 2 with epsilon") {
        aut.alphabet = &alphabet;
        aut.initial = { 0 };
        aut.final = { 0, 1 };
        aut.delta.add(0, 'a', 1);
        result = aut.get_word_from_complement();
        REQUIRE(result.has_value());
        CHECK(*result == Word{ 'b' });
    }

    SECTION("nfa accepting \\eps+a+b+c") {
        aut.alphabet = &alphabet;
        aut.initial = { 0 };
        aut.final = { 0, 1 };
        aut.delta.add(0, 'a', 1);
        aut.delta.add(0, 'b', 1);
        aut.delta.add(0, 'c', 1);
        result = aut.get_word_from_complement();
        REQUIRE(result.has_value());
        CHECK(*result == Word{ 'a', 'a' });
    }

    SECTION("nfa accepting \\eps+a+b+c+aa") {
        aut.initial = { 0 };
        aut.final = { 0, 1 };
        aut.delta.add(0, 'a', 1);
        aut.delta.add(0, 'b', 1);
        aut.delta.add(0, 'c', 1);
        result = aut.get_word_from_complement();
        REQUIRE(result.has_value());
        CHECK(*result == Word{ 'a', 'a' });
    }

    SECTION("simple automaton 3") {
        aut.initial = { 1 };
        aut.final = { 1, 2, 3, 4, 5 };
        aut.delta.add(1, 'a', 2);
        aut.delta.add(2, 'a', 3);
        aut.delta.add(2, 'a', 6);
        aut.delta.add(6, 'a', 6);
        aut.delta.add(3, 'a', 4);
        aut.delta.add(4, 'a', 5);
        result = aut.get_word_from_complement();
        REQUIRE(result.has_value());
        CHECK(*result == Word{ 'a', 'a','a', 'a', 'a' });
    }

    SECTION("universal language") {
        aut.initial = { 1 };
        aut.final = { 1 };
        aut.delta.add(1, 'a', 1);
        result = aut.get_word_from_complement();
        CHECK(!result.has_value());
    }

    SECTION("smaller alphabet symbol") {
        aut.initial = { 1 };
        aut.final = { 1 };
        aut.delta.add(1, 'b', 1);
        result = aut.get_word_from_complement(&alphabet);
        REQUIRE(result.has_value());
        CHECK(*result == Word{ 'a' });
    }

    SECTION("smaller transition symbol") {
        aut.initial = { 1 };
        aut.final = { 1 };
        aut.delta.add(1, 'a', 1);
        aut.delta.add(1, 0, 2);
        result = aut.get_word_from_complement(&alphabet);
        REQUIRE(result.has_value());
        CHECK(*result == Word{ 0 });
    }

    SECTION("smaller transition symbol 2") {
        aut.initial = { 1 };
        aut.final = { 1 };
        aut.delta.add(1, 0, 2);
        result = aut.get_word_from_complement(&alphabet);
        REQUIRE(result.has_value());
        CHECK(*result == Word{ 0 });
    }
}

TEST_CASE("mata::nfa::lang_difference()") {
    Nfa nfa_included{};
    Nfa nfa_excluded{};
    Nfa result{};
    Nfa expected{};

    SECTION("empty automata") {
        result = lang_difference(nfa_included, nfa_excluded);
        CHECK(result.is_lang_empty());
    }

    SECTION("empty included") {
        nfa_excluded.initial = { 0 };
        nfa_excluded.final = { 0 };
        result = lang_difference(nfa_included, nfa_excluded);
        CHECK(result.is_lang_empty());
    }

    SECTION("empty excluded") {
        nfa_included.initial = { 0 };
        nfa_included.final = { 0 };
        result = lang_difference(nfa_included, nfa_excluded);
        expected = nfa_included;
        CHECK(are_equivalent(result, expected));
    }

    SECTION("included { '', 'a' }, excluded { '' }") {
        nfa_included.initial = { 0 };
        nfa_included.delta.add(0, 'a', 1);
        nfa_included.final = { 0, 1 };

        nfa_excluded.initial = { 0 };
        nfa_excluded.final = { 0 };

        expected.initial = { 0 };
        expected.delta.add(0, 'a', 1);
        expected.final = { 1 };

        result = lang_difference(nfa_included, nfa_excluded);
        CHECK(are_equivalent(result, expected));
    }

    SECTION("included { '', 'a' }, excluded { 'a' }") {
        nfa_included.initial = { 0 };
        nfa_included.delta.add(0, 'a', 1);
        nfa_included.final = { 0 };

        nfa_excluded.initial = { 0 };
        nfa_excluded.delta.add(0, 'a', 1);
        nfa_excluded.final = { 1 };

        expected.initial = { 0 };
        expected.final = { 0 };

        result = lang_difference(nfa_included, nfa_excluded);
        CHECK(are_equivalent(result, expected));
    }

    SECTION("included { '', 'a' }, excluded { '', 'a' }") {
        nfa_included.initial = { 0 };
        nfa_included.delta.add(0, 'a', 1);
        nfa_included.final = { 0, 1 };

        nfa_excluded.initial = { 0 };
        nfa_excluded.delta.add(0, 'a', 1);
        nfa_excluded.final = { 0, 1 };

        result = lang_difference(nfa_included, nfa_excluded);
        CHECK(are_equivalent(result, expected));
    }

    SECTION("included { '', 'a', 'ab' }, excluded { '', 'ab' }") {
        nfa_included.initial = { 0 };
        nfa_included.delta.add(0, 'a', 1);
        nfa_included.delta.add(1, 'b', 2);
        nfa_included.final = { 0, 1, 2 };

        nfa_excluded.initial = { 0 };
        nfa_excluded.delta.add(0, 'a', 1);
        nfa_excluded.delta.add(1, 'b', 2);
        nfa_excluded.final = { 0, 2 };

        expected.initial = { 0 };
        expected.delta.add(0, 'a', 1);
        expected.final = { 1 };

        result = lang_difference(nfa_included, nfa_excluded);
        CHECK(are_equivalent(result, expected));
    }

    SECTION("included { '', 'a+', 'a+b' }, excluded { '', 'ab' }") {
        nfa_included.initial = { 0 };
        nfa_included.delta.add(0, 'a', 1);
        nfa_included.delta.add(1, 'a', 1);
        nfa_included.delta.add(1, 'b', 2);
        nfa_included.final = { 0, 1, 2 };

        nfa_excluded.initial = { 0 };
        nfa_excluded.delta.add(0, 'a', 1);
        nfa_excluded.delta.add(1, 'b', 2);
        nfa_excluded.final = { 0, 2 };

        expected.initial = { 0 };
        expected.delta.add(0, 'a', 1);
        expected.delta.add(1, 'a', 1);
        expected.delta.add(1, 'a', 2);
        expected.delta.add(2, 'b', 3);
        expected.final = { 1, 3 };

        result = lang_difference(nfa_included, nfa_excluded);
        CHECK(are_equivalent(result, expected));
    }

    SECTION("included { '', 'ab' }, excluded { '', 'a+', 'a+b' }") {
        nfa_included.initial = { 0 };
        nfa_included.delta.add(0, 'a', 1);
        nfa_included.delta.add(1, 'b', 2);
        nfa_included.final = { 0, 2 };

        nfa_excluded.initial = { 0 };
        nfa_excluded.delta.add(0, 'a', 1);
        nfa_excluded.delta.add(1, 'a', 1);
        nfa_excluded.delta.add(1, 'b', 2);
        nfa_excluded.final = { 0, 1, 2 };

        result = lang_difference(nfa_included, nfa_excluded);
        CHECK(are_equivalent(result, expected));
    }

    SECTION("included { 'a', 'ab', '(abc)+a', '(abc)+ab' }, excluded { 'a', 'ab' }") {
        nfa_included.initial = { 0 };
        nfa_included.delta.add(0, 'a', 1);
        nfa_included.delta.add(0, 'a', 2);
        nfa_included.delta.add(0, 'a', 3);
        nfa_included.delta.add(3, 'b', 4);
        nfa_included.delta.add(2, 'b', 5);
        nfa_included.delta.add(1, 'b', 6);
        nfa_included.delta.add(6, 'c', 0);
        nfa_included.final = { 2, 6 };

        nfa_excluded.initial = { 0 };
        nfa_excluded.delta.add(0, 'a', 1);
        nfa_excluded.delta.add(1, 'b', 2);
        nfa_excluded.final = { 0, 1, 2 };

        expected.initial = { 0 };
        expected.delta.add(0, 'a', 1);
        expected.delta.add(1, 'b', 2);
        expected.delta.add(2, 'c', 3);
        expected.delta.add(3, 'a', 4);
        expected.delta.add(4, 'b', 5);
        expected.delta.add(5, 'c', 3);
        expected.final = { 4, 5 };

        result = lang_difference(nfa_included, nfa_excluded);
        CHECK(are_equivalent(result, expected));
    }
}

TEST_CASE("mata::nfa::Nfa::get_word_from_lang_difference()") {
    Nfa nfa_included{};
    Nfa nfa_excluded{};
    std::optional<Word> result{};
    Nfa expected{};

    SECTION("empty automata") {
        result = get_word_from_lang_difference(nfa_included, nfa_excluded);
        CHECK(!result.has_value());
    }

    SECTION("empty included") {
        nfa_excluded.initial = { 0 };
        nfa_excluded.final = { 0 };
        result = get_word_from_lang_difference(nfa_included, nfa_excluded);
        CHECK(!result.has_value());
    }

    SECTION("empty excluded") {
        nfa_included.initial = { 0 };
        nfa_included.final = { 0 };
        result = get_word_from_lang_difference(nfa_included, nfa_excluded);
        REQUIRE(result.has_value());
        CHECK(*result == Word{});
    }

    SECTION("included { '', 'a' }, excluded { '' }") {
        nfa_included.initial = { 0 };
        nfa_included.delta.add(0, 'a', 1);
        nfa_included.final = { 0, 1 };

        nfa_excluded.initial = { 0 };
        nfa_excluded.final = { 0 };

        result = get_word_from_lang_difference(nfa_included, nfa_excluded);
        REQUIRE(result.has_value());
        CHECK(*result == Word{ 'a' });
    }

    SECTION("included { '', 'a' }, excluded { 'a' }") {
        nfa_included.initial = { 0 };
        nfa_included.delta.add(0, 'a', 1);
        nfa_included.final = { 0 };

        nfa_excluded.initial = { 0 };
        nfa_excluded.delta.add(0, 'a', 1);
        nfa_excluded.final = { 1 };

        result = get_word_from_lang_difference(nfa_included, nfa_excluded);
        REQUIRE(result.has_value());
        CHECK(*result == Word{});
    }

    SECTION("included { '', 'a' }, excluded { '', 'a' }") {
        nfa_included.initial = { 0 };
        nfa_included.delta.add(0, 'a', 1);
        nfa_included.final = { 0, 1 };

        nfa_excluded.initial = { 0 };
        nfa_excluded.delta.add(0, 'a', 1);
        nfa_excluded.final = { 0, 1 };

        result = get_word_from_lang_difference(nfa_included, nfa_excluded);
        CHECK(!result.has_value());
    }

    SECTION("included { '', 'a', 'ab' }, excluded { '', 'ab' }") {
        nfa_included.initial = { 0 };
        nfa_included.delta.add(0, 'a', 1);
        nfa_included.delta.add(1, 'b', 2);
        nfa_included.final = { 0, 1, 2 };

        nfa_excluded.initial = { 0 };
        nfa_excluded.delta.add(0, 'a', 1);
        nfa_excluded.delta.add(1, 'b', 2);
        nfa_excluded.final = { 0, 2 };

        result = get_word_from_lang_difference(nfa_included, nfa_excluded);
        REQUIRE(result.has_value());
        CHECK(*result == Word{ 'a' });
    }

    SECTION("included { '', 'a+', 'a+b' }, excluded { '', 'ab' }") {
        nfa_included.initial = { 0 };
        nfa_included.delta.add(0, 'a', 1);
        nfa_included.delta.add(1, 'a', 1);
        nfa_included.delta.add(1, 'b', 2);
        nfa_included.final = { 0, 1, 2 };

        nfa_excluded.initial = { 0 };
        nfa_excluded.delta.add(0, 'a', 1);
        nfa_excluded.delta.add(1, 'b', 2);
        nfa_excluded.final = { 0, 2 };

        expected.initial = { 0 };
        expected.delta.add(0, 'a', 1);
        expected.delta.add(1, 'a', 1);
        expected.delta.add(1, 'a', 2);
        expected.delta.add(2, 'b', 3);
        expected.final = { 1, 3 };

        result = get_word_from_lang_difference(nfa_included, nfa_excluded);
        REQUIRE(result.has_value());
        CHECK(expected.is_in_lang(*result));
    }

    SECTION("included { '', 'ab' }, excluded { '', 'a+', 'a+b' }") {
        nfa_included.initial = { 0 };
        nfa_included.delta.add(0, 'a', 1);
        nfa_included.delta.add(1, 'b', 2);
        nfa_included.final = { 0, 2 };

        nfa_excluded.initial = { 0 };
        nfa_excluded.delta.add(0, 'a', 1);
        nfa_excluded.delta.add(1, 'a', 1);
        nfa_excluded.delta.add(1, 'b', 2);
        nfa_excluded.final = { 0, 1, 2 };

        result = get_word_from_lang_difference(nfa_included, nfa_excluded);
        CHECK(!result.has_value());
    }

    SECTION("included { 'a', 'ab', '(abc)+a', '(abc)+ab' }, excluded { 'a', 'ab' }") {
        nfa_included.initial = { 0 };
        nfa_included.delta.add(0, 'a', 1);
        nfa_included.delta.add(0, 'a', 2);
        nfa_included.delta.add(0, 'a', 3);
        nfa_included.delta.add(3, 'b', 4);
        nfa_included.delta.add(2, 'b', 5);
        nfa_included.delta.add(1, 'b', 6);
        nfa_included.delta.add(6, 'c', 0);
        nfa_included.final = { 2, 6 };

        nfa_excluded.initial = { 0 };
        nfa_excluded.delta.add(0, 'a', 1);
        nfa_excluded.delta.add(1, 'b', 2);
        nfa_excluded.final = { 0, 1, 2 };

        expected.initial = { 0 };
        expected.delta.add(0, 'a', 1);
        expected.delta.add(1, 'b', 2);
        expected.delta.add(2, 'c', 3);
        expected.delta.add(3, 'a', 4);
        expected.delta.add(4, 'b', 5);
        expected.delta.add(5, 'c', 3);
        expected.final = { 4, 5 };

        result = get_word_from_lang_difference(nfa_included, nfa_excluded);
        REQUIRE(result.has_value());
        CHECK(expected.is_in_lang(*result));
    }
}

TEST_CASE("mata::nfa::minimize() for profiling", "[.profiling],[minimize]") {
    Nfa aut(4);
    Nfa result;
    std::unordered_map<StateSet, State> subset_map;

    aut.initial.insert(0);
    aut.final.insert(3);
    aut.delta.add(0, 46, 0);
    aut.delta.add(0, 47, 0);
    aut.delta.add(0, 58, 0);
    aut.delta.add(0, 58, 1);
    aut.delta.add(0, 64, 0);
    aut.delta.add(0, 64, 0);
    aut.delta.add(0, 82, 0);
    aut.delta.add(0, 92, 0);
    aut.delta.add(0, 98, 0);
    aut.delta.add(0, 100, 0);
    aut.delta.add(0, 103, 0);
    aut.delta.add(0, 109, 0);
    aut.delta.add(0, 110, 0);
    aut.delta.add(0, 111, 0);
    aut.delta.add(0, 114, 0);
    aut.delta.add(1, 47, 2);
    aut.delta.add(2, 47, 3);
    aut.delta.add(3, 46, 3);
    aut.delta.add(3, 47, 3);
    aut.delta.add(3, 58, 3);
    aut.delta.add(3, 64, 3);
    aut.delta.add(3, 82, 3);
    aut.delta.add(3, 92, 3);
    aut.delta.add(3, 98, 3);
    aut.delta.add(3, 100, 3);
    aut.delta.add(3, 103, 3);
    aut.delta.add(3, 109, 3);
    aut.delta.add(3, 110, 3);
    aut.delta.add(3, 111, 3);
    aut.delta.add(3, 114, 3);
    minimize(&result, aut);
}

TEST_CASE("mata::nfa::construct() correct calls")
{ // {{{
    Nfa aut(10);
    mata::parser::ParsedSection parsec;
    OnTheFlyAlphabet alphabet;

    SECTION("construct an empty automaton")
    {
        parsec.type = nfa::TYPE_NFA;

        aut = builder::construct(parsec);

        REQUIRE(aut.is_lang_empty());
    }

    SECTION("construct a simple non-empty automaton accepting the empty word")
    {
        parsec.type = nfa::TYPE_NFA;
        parsec.dict.insert({"Initial", {"q1"}});
        parsec.dict.insert({"Final", {"q1"}});

        aut = builder::construct(parsec);

        REQUIRE(!aut.is_lang_empty());
    }

    SECTION("construct an automaton with more than one initial/final states")
    {
        parsec.type = nfa::TYPE_NFA;
        parsec.dict.insert({"Initial", {"q1", "q2"}});
        parsec.dict.insert({"Final", {"q1", "q2", "q3"}});

        aut = builder::construct(parsec);

        REQUIRE(aut.initial.size() == 2);
        REQUIRE(aut.final.size() == 3);
    }

    SECTION("construct a simple non-empty automaton accepting only the word 'a'")
    {
        parsec.type = nfa::TYPE_NFA;
        parsec.dict.insert({"Initial", {"q1"}});
        parsec.dict.insert({"Final", {"q2"}});
        parsec.body = { {"q1", "a", "q2"} };

        aut = builder::construct(parsec, &alphabet);

        Run cex;
        REQUIRE(!aut.is_lang_empty(&cex));
        auto word_bool_pair = aut.get_word_for_path(cex);
        REQUIRE(word_bool_pair.second);
        REQUIRE(word_bool_pair.first.word == encode_word(&alphabet, { "a"}).word);

        REQUIRE(aut.is_in_lang(encode_word(&alphabet, { "a"})));
    }

    SECTION("construct a more complicated non-empty automaton")
    {
        parsec.type = nfa::TYPE_NFA;
        parsec.dict.insert({"Initial", {"q1", "q3"}});
        parsec.dict.insert({"Final", {"q5"}});
        parsec.body.push_back({"q1", "a", "q3"});
        parsec.body.push_back({"q1", "a", "q10"});
        parsec.body.push_back({"q1", "b", "q7"});
        parsec.body.push_back({"q3", "a", "q7"});
        parsec.body.push_back({"q3", "b", "q9"});
        parsec.body.push_back({"q9", "a", "q9"});
        parsec.body.push_back({"q7", "b", "q1"});
        parsec.body.push_back({"q7", "a", "q3"});
        parsec.body.push_back({"q7", "c", "q3"});
        parsec.body.push_back({"q10", "a", "q7"});
        parsec.body.push_back({"q10", "b", "q7"});
        parsec.body.push_back({"q10", "c", "q7"});
        parsec.body.push_back({"q7", "a", "q5"});
        parsec.body.push_back({"q5", "a", "q5"});
        parsec.body.push_back({"q5", "c", "q9"});

        aut = builder::construct(parsec, &alphabet);

        // some samples
        REQUIRE(aut.is_in_lang(encode_word(&alphabet, { "b", "a"})));
        REQUIRE(aut.is_in_lang(encode_word(&alphabet, { "a", "c", "a", "a"})));
        REQUIRE(aut.is_in_lang(encode_word(&alphabet,
                                            {"a", "a", "a", "a", "a", "a", "a", "a", "a", "a", "a", "a"})));
        // some wrong samples
        REQUIRE(!aut.is_in_lang(encode_word(&alphabet, { "b", "c"})));
        REQUIRE(!aut.is_in_lang(encode_word(&alphabet, { "a", "c", "c", "a"})));
        REQUIRE(!aut.is_in_lang(encode_word(&alphabet, { "b", "a", "c", "b"})));
    }
} // }}}

TEST_CASE("mata::nfa::construct() invalid calls")
{ // {{{
    Nfa aut;
    mata::parser::ParsedSection parsec;

    SECTION("construct() call with invalid ParsedSection object")
    {
        parsec.type = "FA";

        CHECK_THROWS_WITH(builder::construct(parsec),
                          Catch::Contains("expecting type"));
    }

    SECTION("construct() call with an epsilon transition")
    {
        parsec.type = nfa::TYPE_NFA;
        parsec.body = { {"q1", "q2"} };

        CHECK_THROWS_WITH(builder::construct(parsec),
                          Catch::Contains("Epsilon transition"));
    }

    SECTION("construct() call with a nonsense transition")
    {
        parsec.type = nfa::TYPE_NFA;
        parsec.body = { {"q1", "a", "q2", "q3"} };

        CHECK_THROWS_WITH(plumbing::construct(&aut, parsec),
                          Catch::Contains("Invalid transition"));
    }
} // }}}

TEST_CASE("mata::nfa::construct() from IntermediateAut correct calls")
{ // {{{
    Nfa aut;
    mata::IntermediateAut inter_aut;
    OnTheFlyAlphabet alphabet;

    SECTION("construct an empty automaton")
    {
        inter_aut.automaton_type = mata::IntermediateAut::AutomatonType::NFA;
        REQUIRE(aut.is_lang_empty());
        aut = builder::construct(inter_aut);
        REQUIRE(aut.is_lang_empty());
    }

    SECTION("construct a simple non-empty automaton accepting the empty word from intermediate automaton")
    {
        std::string file =
                "@NFA-explicit\n"
                "%States-enum p q r\n"
                "%Alphabet-auto\n"
                "%Initial p | q\n"
                "%Final p | q\n";
        const auto auts = mata::IntermediateAut::parse_from_mf(parse_mf(file));
        inter_aut = auts[0];

        aut = builder::construct(inter_aut);

        REQUIRE(!aut.is_lang_empty());
    }

    SECTION("construct an automaton with more than one initial/final states from intermediate automaton")
    {
        std::string file =
                "@NFA-explicit\n"
                "%States-enum p q 3\n"
                "%Alphabet-auto\n"
                "%Initial p | q\n"
                "%Final p | q | r\n";
        const auto auts = mata::IntermediateAut::parse_from_mf(parse_mf(file));
        inter_aut = auts[0];

        plumbing::construct(&aut, inter_aut);

        REQUIRE(aut.initial.size() == 2);
        REQUIRE(aut.final.size() == 3);
    }

    SECTION("construct an automaton with implicit operator completion one initial/final states from intermediate automaton")
    {
        std::string file =
                "@NFA-explicit\n"
                "%States-enum p q r\n"
                "%Alphabet-auto\n"
                "%Initial p q\n"
                "%Final p q r\n";
        const auto auts = mata::IntermediateAut::parse_from_mf(parse_mf(file));
        inter_aut = auts[0];

        plumbing::construct(&aut, inter_aut);

        REQUIRE(aut.initial.size() == 2);
        REQUIRE(aut.final.size() == 3);
    }

    SECTION("construct an automaton with implicit operator completion one initial/final states from intermediate automaton")
    {
        std::string file =
                "@NFA-explicit\n"
                "%States-enum p q r m n\n"
                "%Alphabet-auto\n"
                "%Initial p q r\n"
                "%Final p q m n\n";
        const auto auts = mata::IntermediateAut::parse_from_mf(parse_mf(file));
        inter_aut = auts[0];

        plumbing::construct(&aut, inter_aut);

        REQUIRE(aut.initial.size() == 3);
        REQUIRE(aut.final.size() == 4);
    }

    SECTION("construct a simple non-empty automaton accepting only the word 'a' from intermediate automaton")
    {
        std::string file =
                "@NFA-explicit\n"
                "%States-enum p q 3\n"
                "%Alphabet-auto\n"
                "%Initial q1\n"
                "%Final q2\n"
                "q1 a q2\n";

        const auto auts = mata::IntermediateAut::parse_from_mf(parse_mf(file));
        inter_aut = auts[0];
        plumbing::construct(&aut, inter_aut, &alphabet);

        Run cex;
        REQUIRE(!aut.is_lang_empty(&cex));
        auto word_bool_pair = aut.get_word_for_path(cex);
        REQUIRE(word_bool_pair.second);
        REQUIRE(word_bool_pair.first.word == encode_word(&alphabet, { "a" }).word);

        REQUIRE(aut.is_in_lang(encode_word(&alphabet, { "a" })));
    }

    SECTION("construct a more complicated non-empty automaton from intermediate automaton")
    {
        std::string file =
                "@NFA-explicit\n"
                "%States-enum p q 3\n"
                "%Alphabet-auto\n"
                "%Initial q1 | q3\n"
                "%Final q5\n"
                "q1 a q3\n"
                "q1 a q10\n"
                "q1 b q7\n"
                "q3 a q7\n"
                "q3 b q9\n"
                "q9 a q9\n"
                "q7 b q1\n"
                "q7 a q3\n"
                "q7 c q3\n"
                "q10 a q7\n"
                "q10 b q7\n"
                "q10 c q7\n"
                "q7 a q5\n"
                "q5 c q9\n";

        const auto auts = mata::IntermediateAut::parse_from_mf(parse_mf(file));
        inter_aut = auts[0];

        plumbing::construct(&aut, inter_aut, &alphabet);

        // some samples
        REQUIRE(aut.is_in_lang(encode_word(&alphabet, { "b", "a"})));
        REQUIRE(aut.is_in_lang(encode_word(&alphabet, { "a", "c", "a", "a"})));
        REQUIRE(aut.is_in_lang(encode_word(&alphabet,
                                            {"a", "a", "a", "a", "a", "a", "a", "a", "a", "a", "a", "a"})));
        // some wrong samples
        REQUIRE(!aut.is_in_lang(encode_word(&alphabet, { "b", "c"})));
        REQUIRE(!aut.is_in_lang(encode_word(&alphabet, { "a", "c", "c", "a"})));
        REQUIRE(!aut.is_in_lang(encode_word(&alphabet, { "b", "a", "c", "b"})));
    }

    SECTION("construct - final states from negation")
    {
        std::string file =
                "@NFA-bits\n"
                "%Alphabet-auto\n"
                "%Initial q0 q8\n"
                "%Final !q0 & !q1 & !q4 & !q5 & !q6\n"
                "q0 a1 q1\n"
                "q1 a2 q2\n"
                "q2 a3 q3\n"
                "q2 a4 q4\n"
                "q3 a5 q5\n"
                "q3 a6 q6\n"
                "q5 a7 q7\n";

        const auto auts = mata::IntermediateAut::parse_from_mf(parse_mf(file));
        inter_aut = auts[0];

        plumbing::construct(&aut, inter_aut, &alphabet);
        REQUIRE(aut.final.size() == 4);
        REQUIRE(aut.is_in_lang(encode_word(&alphabet, { "a1", "a2"})));
        REQUIRE(aut.is_in_lang(encode_word(&alphabet, { "a1", "a2", "a3"})));
        REQUIRE(!aut.is_in_lang(encode_word(&alphabet, { "a1", "a2", "a3", "a4"})));
        REQUIRE(aut.is_in_lang(encode_word(&alphabet, { "a1", "a2", "a3", "a5", "a7"})));
    }

    SECTION("construct - final states given as true")
    {
        std::string file =
                "@NFA-bits\n"
                "%Alphabet-auto\n"
                "%Initial q0 q8\n"
                "%Final \\true\n"
                "q0 a1 q1\n"
                "q1 a2 q2\n"
                "q2 a3 q3\n"
                "q2 a4 q4\n"
                "q3 a5 q5\n"
                "q3 a6 q6\n"
                "q5 a7 q7\n";

        const auto auts = mata::IntermediateAut::parse_from_mf(parse_mf(file));
        inter_aut = auts[0];

        nfa::builder::NameStateMap state_map;
        plumbing::construct(&aut, inter_aut, &alphabet, &state_map);
        CHECK(aut.final.size() == 9);
        CHECK(aut.final[state_map.at("0")]);
        CHECK(aut.final[state_map.at("1")]);
        CHECK(aut.final[state_map.at("2")]);
        CHECK(aut.final[state_map.at("3")]);
        CHECK(aut.final[state_map.at("4")]);
        CHECK(aut.final[state_map.at("5")]);
        CHECK(aut.final[state_map.at("6")]);
        CHECK(aut.final[state_map.at("7")]);
        CHECK(aut.final[state_map.at("8")]);
    }

    SECTION("construct - final states given as false")
    {
        std::string file =
                "@NFA-bits\n"
                "%Alphabet-auto\n"
                "%Initial q0 q8\n"
                "%Final \\false\n"
                "q0 a1 q1\n"
                "q1 a2 q2\n"
                "q2 a3 q3\n"
                "q2 a4 q4\n"
                "q3 a5 q5\n"
                "q3 a6 q6\n"
                "q5 a7 q7\n";

        const auto auts = mata::IntermediateAut::parse_from_mf(parse_mf(file));
        inter_aut = auts[0];

        nfa::builder::NameStateMap state_map;
        plumbing::construct(&aut, inter_aut, &alphabet, &state_map);
        CHECK(aut.final.empty());
    }
} // }}}

TEST_CASE("mata::nfa::make_complete()")
{ // {{{
    Nfa aut{};

    SECTION("empty automaton, empty alphabet")
    {
        OnTheFlyAlphabet alph{};

        aut.make_complete(&alph, 0);

        REQUIRE(aut.initial.empty());
        REQUIRE(aut.final.empty());
        REQUIRE(aut.delta.empty());
    }

    SECTION("empty automaton")
    {
        OnTheFlyAlphabet alph{ std::vector<std::string>{ "a", "b" } };

        aut.make_complete(&alph, 0);

        CHECK(aut.initial.empty());
        CHECK(aut.final.empty());
        CHECK(!aut.delta.contains(0, alph["a"], 0));
        CHECK(!aut.delta.contains(0, alph["b"], 0));
    }

    SECTION("non-empty automaton, empty alphabet")
    {
        OnTheFlyAlphabet alphabet{};

        aut.initial = {1};

        aut.make_complete(&alphabet, 0);

        CHECK(aut.initial.size() == 1);
        CHECK(*aut.initial.begin() == 1);
        CHECK(aut.final.empty());
        CHECK(aut.delta.empty());
    }

    SECTION("one-state automaton")
    {
        OnTheFlyAlphabet alph{ std::vector<std::string>{ "a", "b" } };
        const State SINK = 10;

        aut.initial = {1};

        aut.make_complete(&alph, SINK);

        CHECK(aut.initial.size() == 1);
        CHECK(*aut.initial.begin() == 1);
        CHECK(aut.final.empty());
        CHECK(aut.delta.contains(1, alph["a"], SINK));
        CHECK(aut.delta.contains(1, alph["b"], SINK));
        CHECK(aut.delta.contains(SINK, alph["a"], SINK));
        CHECK(aut.delta.contains(SINK, alph["b"], SINK));
    }

    SECTION("bigger automaton")
    {
        OnTheFlyAlphabet alph{ std::vector<std::string>{ "a", "b", "c" } };
        const State SINK = 9;

        aut.initial = {1, 2};
        aut.final = {8};
        aut.delta.add(1, alph["a"], 2);
        aut.delta.add(2, alph["a"], 4);
        aut.delta.add(2, alph["c"], 1);
        aut.delta.add(2, alph["c"], 3);
        aut.delta.add(3, alph["b"], 5);
        aut.delta.add(4, alph["c"], 8);

        aut.make_complete(&alph, SINK);

        CHECK(aut.delta.contains(1, alph["a"], 2));
        CHECK(aut.delta.contains(1, alph["b"], SINK));
        CHECK(aut.delta.contains(1, alph["c"], SINK));
        CHECK(aut.delta.contains(2, alph["a"], 4));
        CHECK(aut.delta.contains(2, alph["c"], 1));
        CHECK(aut.delta.contains(2, alph["c"], 3));
        CHECK(aut.delta.contains(2, alph["b"], SINK));
        CHECK(aut.delta.contains(3, alph["b"], 5));
        CHECK(aut.delta.contains(3, alph["a"], SINK));
        CHECK(aut.delta.contains(3, alph["c"], SINK));
        CHECK(aut.delta.contains(4, alph["c"], 8));
        CHECK(aut.delta.contains(4, alph["a"], SINK));
        CHECK(aut.delta.contains(4, alph["b"], SINK));
        CHECK(aut.delta.contains(5, alph["a"], SINK));
        CHECK(aut.delta.contains(5, alph["b"], SINK));
        CHECK(aut.delta.contains(5, alph["c"], SINK));
        CHECK(aut.delta.contains(8, alph["a"], SINK));
        CHECK(aut.delta.contains(8, alph["b"], SINK));
        CHECK(aut.delta.contains(8, alph["c"], SINK));
        CHECK(aut.delta.contains(SINK, alph["a"], SINK));
        CHECK(aut.delta.contains(SINK, alph["b"], SINK));
        CHECK(aut.delta.contains(SINK, alph["c"], SINK));
    }

    SECTION("bigger automaton parameters from automaton with alphabet") {
        constexpr State SINK = 9;
        aut.initial = {1, 2};
        aut.final = {8};
        aut.delta.add(1, 'a', 2);
        aut.delta.add(2, 'a', 4);
        aut.delta.add(2, 'c', 1);
        aut.delta.add(2, 'c', 3);
        aut.delta.add(3, 'b', 5);
        aut.delta.add(4, 'c', 8);
        EnumAlphabet alphabet{ 'a', 'b', 'c' };
        aut.alphabet = &alphabet;

        aut.make_complete();
        CHECK(aut.delta.contains(1, 'a', 2));
        CHECK(aut.delta.contains(1, 'b', SINK));
        CHECK(aut.delta.contains(1, 'c', SINK));
        CHECK(aut.delta.contains(2, 'a', 4));
        CHECK(aut.delta.contains(2, 'c', 1));
        CHECK(aut.delta.contains(2, 'c', 3));
        CHECK(aut.delta.contains(2, 'b', SINK));
        CHECK(aut.delta.contains(3, 'b', 5));
        CHECK(aut.delta.contains(3, 'a', SINK));
        CHECK(aut.delta.contains(3, 'c', SINK));
        CHECK(aut.delta.contains(4, 'c', 8));
        CHECK(aut.delta.contains(4, 'a', SINK));
        CHECK(aut.delta.contains(4, 'b', SINK));
        CHECK(aut.delta.contains(5, 'a', SINK));
        CHECK(aut.delta.contains(5, 'b', SINK));
        CHECK(aut.delta.contains(5, 'c', SINK));
        CHECK(aut.delta.contains(8, 'a', SINK));
        CHECK(aut.delta.contains(8, 'b', SINK));
        CHECK(aut.delta.contains(8, 'c', SINK));
        CHECK(aut.delta.contains(SINK, 'a', SINK));
        CHECK(aut.delta.contains(SINK, 'b', SINK));
        CHECK(aut.delta.contains(SINK, 'c', SINK));
    }

    SECTION("bigger automaton parameters from automaton") {
        constexpr State SINK = 9;
        aut.initial = {1, 2};
        aut.final = {8};
        aut.delta.add(1, 'a', 2);
        aut.delta.add(2, 'a', 4);
        aut.delta.add(2, 'c', 1);
        aut.delta.add(2, 'c', 3);
        aut.delta.add(3, 'b', 5);
        aut.delta.add(4, 'c', 8);

        aut.make_complete();
        CHECK(aut.delta.contains(1, 'a', 2));
        CHECK(aut.delta.contains(1, 'b', SINK));
        CHECK(aut.delta.contains(1, 'c', SINK));
        CHECK(aut.delta.contains(2, 'a', 4));
        CHECK(aut.delta.contains(2, 'c', 1));
        CHECK(aut.delta.contains(2, 'c', 3));
        CHECK(aut.delta.contains(2, 'b', SINK));
        CHECK(aut.delta.contains(3, 'b', 5));
        CHECK(aut.delta.contains(3, 'a', SINK));
        CHECK(aut.delta.contains(3, 'c', SINK));
        CHECK(aut.delta.contains(4, 'c', 8));
        CHECK(aut.delta.contains(4, 'a', SINK));
        CHECK(aut.delta.contains(4, 'b', SINK));
        CHECK(aut.delta.contains(5, 'a', SINK));
        CHECK(aut.delta.contains(5, 'b', SINK));
        CHECK(aut.delta.contains(5, 'c', SINK));
        CHECK(aut.delta.contains(8, 'a', SINK));
        CHECK(aut.delta.contains(8, 'b', SINK));
        CHECK(aut.delta.contains(8, 'c', SINK));
        CHECK(aut.delta.contains(SINK, 'a', SINK));
        CHECK(aut.delta.contains(SINK, 'b', SINK));
        CHECK(aut.delta.contains(SINK, 'c', SINK));
    }
} // }}}

TEST_CASE("mata::nfa::complement()")
{ // {{{
    Nfa aut(3);
    Nfa cmpl;

    SECTION("empty automaton, empty alphabet")
    {
        OnTheFlyAlphabet alph{};

        cmpl = complement(aut, alph, { {"algorithm", "classical"} });
        Nfa empty_string_nfa{ nfa::builder::create_sigma_star_nfa(&alph) };
        CHECK(are_equivalent(cmpl, empty_string_nfa));
    }

    SECTION("empty automaton")
    {
        OnTheFlyAlphabet alph{ std::vector<std::string>{ "a", "b" } };

        cmpl = complement(aut, alph, { {"algorithm", "classical"} });

        REQUIRE(cmpl.is_in_lang({}));
        REQUIRE(cmpl.is_in_lang(Run{{ alph["a"] }, {}}));
        REQUIRE(cmpl.is_in_lang(Run{{ alph["b"] }, {}}));
        REQUIRE(cmpl.is_in_lang(Run{{ alph["a"], alph["a"]}, {}}));
        REQUIRE(cmpl.is_in_lang(Run{{ alph["a"], alph["b"], alph["b"], alph["a"] }, {}}));

        Nfa sigma_star_nfa{ nfa::builder::create_sigma_star_nfa(&alph) };
        CHECK(are_equivalent(cmpl, sigma_star_nfa));
    }

    SECTION("empty automaton accepting epsilon, empty alphabet")
    {
        OnTheFlyAlphabet alph{};
        aut.initial = {1};
        aut.final = {1};

        cmpl = complement(aut, alph, { {"algorithm", "classical"} });

        CHECK(cmpl.is_lang_empty());
    }

    SECTION("empty automaton accepting epsilon")
    {
        OnTheFlyAlphabet alph{ std::vector<std::string>{ "a", "b" } };
        aut.initial = {1};
        aut.final = {1};

        cmpl = complement(aut, alph, { {"algorithm", "classical"} });

        REQUIRE(!cmpl.is_in_lang({}));
        REQUIRE(cmpl.is_in_lang(Run{{ alph["a"]}, {}}));
        REQUIRE(cmpl.is_in_lang(Run{{ alph["b"]}, {}}));
        REQUIRE(cmpl.is_in_lang(Run{{ alph["a"], alph["a"]}, {}}));
        REQUIRE(cmpl.is_in_lang(Run{{ alph["a"], alph["b"], alph["b"], alph["a"]}, {}}));
        REQUIRE(cmpl.initial.size() == 1);
        REQUIRE(cmpl.final.size() == 1);
        REQUIRE(cmpl.delta.num_of_transitions() == 4);
    }

    SECTION("non-empty automaton accepting a*b*")
    {
        OnTheFlyAlphabet alph{ std::vector<std::string>{ "a", "b" } };
        aut.initial = {1, 2};
        aut.final = {1, 2};

        aut.delta.add(1, alph["a"], 1);
        aut.delta.add(1, alph["a"], 2);
        aut.delta.add(2, alph["b"], 2);

        cmpl = complement(aut, alph, {{"algorithm", "classical"} });

        REQUIRE(!cmpl.is_in_lang(Word{}));
        REQUIRE(!cmpl.is_in_lang(Word{ alph["a"] }));
        REQUIRE(!cmpl.is_in_lang(Word{ alph["b"] }));
        REQUIRE(!cmpl.is_in_lang(Word{ alph["a"], alph["a"] }));
        REQUIRE(cmpl.is_in_lang(Word{ alph["a"], alph["b"], alph["b"], alph["a"] }));
        REQUIRE(!cmpl.is_in_lang(Word{ alph["a"], alph["a"], alph["b"], alph["b"] }));
        REQUIRE(cmpl.is_in_lang(Word{ alph["b"], alph["a"], alph["a"], alph["a"] }));

        REQUIRE(cmpl.initial.size() == 1);
        REQUIRE(cmpl.final.size() == 1);
        REQUIRE(cmpl.delta.num_of_transitions() == 6);
    }

    SECTION("empty automaton, empty alphabet, minimization")
    {
        OnTheFlyAlphabet alph{};

        cmpl = complement(aut, alph, { {"algorithm", "classical"} });
        Nfa empty_string_nfa{ nfa::builder::create_sigma_star_nfa(&alph) };
        CHECK(are_equivalent(empty_string_nfa, cmpl));
    }

    SECTION("empty automaton, minimization")
    {
        OnTheFlyAlphabet alph{ std::vector<std::string>{ "a", "b" } };

        cmpl = complement(aut, alph, { {"algorithm", "classical"} });

        REQUIRE(cmpl.is_in_lang({}));
        REQUIRE(cmpl.is_in_lang(Run{{ alph["a"] }, {}}));
        REQUIRE(cmpl.is_in_lang(Run{{ alph["b"] }, {}}));
        REQUIRE(cmpl.is_in_lang(Run{{ alph["a"], alph["a"]}, {}}));
        REQUIRE(cmpl.is_in_lang(Run{{ alph["a"], alph["b"], alph["b"], alph["a"] }, {}}));

        Nfa sigma_star_nfa{ nfa::builder::create_sigma_star_nfa(&alph) };
        CHECK(are_equivalent(sigma_star_nfa, cmpl));
    }

    SECTION("minimization vs no minimization") {
        OnTheFlyAlphabet alph{ std::vector<std::string>{ "a", "b" } };
        aut.initial = {0, 1};
        aut.final = {1, 2};

        aut.delta.add(1, alph["b"], 1);
        aut.delta.add(1, alph["a"], 2);
        aut.delta.add(2, alph["b"], 2);
        aut.delta.add(0, alph["a"], 1);
        aut.delta.add(0, alph["a"], 2);

        cmpl = complement(aut, alph, { {"algorithm", "classical"} });
        Nfa cmpl_min = complement(aut, alph, { { "algorithm", "brzozowski"} });
        CHECK(are_equivalent(cmpl, cmpl_min, &alph));
        CHECK(cmpl_min.num_of_states() == 4);
        CHECK(cmpl.num_of_states() == 5);
    }

} // }}}

TEST_CASE("mata::nfa::is_universal()")
{ // {{{
    Nfa aut(6);
    Run cex;
    ParameterMap params;

    const std::unordered_set<std::string> ALGORITHMS = {
        "naive",
        "antichains",
    };

    SECTION("empty automaton, empty alphabet")
    {
        OnTheFlyAlphabet alph{};

        for (const auto& algo : ALGORITHMS) {
            params["algorithm"] = algo;
            bool is_univ = aut.is_universal(alph, params);

            REQUIRE(!is_univ);
        }
    }

    SECTION("empty automaton accepting epsilon, empty alphabet")
    {
        OnTheFlyAlphabet alph{};
        aut.initial = {1};
        aut.final = {1};

        for (const auto& algo : ALGORITHMS) {
            params["algorithm"] = algo;
            bool is_univ = aut.is_universal(alph, &cex, params);

            REQUIRE(is_univ);
            REQUIRE(cex.word.empty());
        }
    }

    SECTION("empty automaton accepting epsilon")
    {
        OnTheFlyAlphabet alph{ std::vector<std::string>{ "a" } };
        aut.initial = {1};
        aut.final = {1};

        for (const auto& algo : ALGORITHMS) {
            params["algorithm"] = algo;
            bool is_univ = aut.is_universal(alph, &cex, params);

            REQUIRE(!is_univ);
            REQUIRE(((cex.word == Word{alph["a"]}) || (cex.word == Word{alph["b"]})));
        }
    }

    SECTION("automaton for a*b*")
    {
        OnTheFlyAlphabet alph{ std::vector<std::string>{ "a", "b" } };
        aut.initial = {1, 2};
        aut.final = {1, 2};

        aut.delta.add(1, alph["a"], 1);
        aut.delta.add(1, alph["a"], 2);
        aut.delta.add(2, alph["b"], 2);

        for (const auto& algo : ALGORITHMS) {
            params["algorithm"] = algo;
            bool is_univ = aut.is_universal(alph, params);

            REQUIRE(!is_univ);
        }
    }

    SECTION("automaton for a* + b*")
    {
        OnTheFlyAlphabet alph{ std::vector<std::string>{ "a", "b"} };
        aut.initial = {1, 2};
        aut.final = {1, 2};

        aut.delta.add(1, alph["a"], 1);
        aut.delta.add(2, alph["b"], 2);

        for (const auto& algo : ALGORITHMS) {
            params["algorithm"] = algo;
            bool is_univ = aut.is_universal(alph, params);

            REQUIRE(!is_univ);
        }
    }

    SECTION("automaton for (a + b)*")
    {
        OnTheFlyAlphabet alph{ std::vector<std::string>{ "a", "b"} };
        aut.initial = {1};
        aut.final = {1};

        aut.delta.add(1, alph["a"], 1);
        aut.delta.add(1, alph["b"], 1);

        for (const auto& algo : ALGORITHMS) {
            params["algorithm"] = algo;
            bool is_univ = aut.is_universal(alph, params);

            REQUIRE(is_univ);
        }
    }

    SECTION("automaton for eps + (a+b) + (a+b)(a+b)(a* + b*)")
    {
        OnTheFlyAlphabet alph{ std::vector<std::string>{ "a", "b"} };
        aut.initial = {1};
        aut.final = {1, 2, 3, 4, 5};

        aut.delta.add(1, alph["a"], 2);
        aut.delta.add(1, alph["b"], 2);
        aut.delta.add(2, alph["a"], 3);
        aut.delta.add(2, alph["b"], 3);

        aut.delta.add(3, alph["a"], 4);
        aut.delta.add(4, alph["a"], 4);

        aut.delta.add(3, alph["b"], 5);
        aut.delta.add(5, alph["b"], 5);

        for (const auto& algo : ALGORITHMS) {
            params["algorithm"] = algo;
            bool is_univ = aut.is_universal(alph, &cex, params);

            REQUIRE(!is_univ);

            REQUIRE(cex.word.size() == 4);
            REQUIRE((cex.word[0] == alph["a"] || cex.word[0] == alph["b"]));
            REQUIRE((cex.word[1] == alph["a"] || cex.word[1] == alph["b"]));
            REQUIRE((cex.word[2] == alph["a"] || cex.word[2] == alph["b"]));
            REQUIRE((cex.word[3] == alph["a"] || cex.word[3] == alph["b"]));
            REQUIRE(cex.word[2] != cex.word[3]);
        }
    }

    SECTION("automaton for epsilon + a(a + b)* + b(a + b)*")
    {
        OnTheFlyAlphabet alph{ std::vector<std::string>{ "a", "b"} };
        aut.initial = {1, 3};
        aut.final = {1, 2, 4};

        aut.delta.add(1, alph["a"], 2);
        aut.delta.add(2, alph["a"], 2);
        aut.delta.add(2, alph["b"], 2);
        aut.delta.add(3, alph["b"], 4);
        aut.delta.add(4, alph["a"], 4);
        aut.delta.add(4, alph["b"], 4);

        for (const auto& algo : ALGORITHMS) {
            params["algorithm"] = algo;
            bool is_univ = aut.is_universal(alph, &cex, params);

            REQUIRE(is_univ);
        }
    }

    SECTION("example from Abdulla et al. TACAS'10")
    {
        OnTheFlyAlphabet alph{ std::vector<std::string>{ "a", "b"} };
        aut.initial = {1, 2};
        aut.final = {1, 2, 3};

        aut.delta.add(1, alph["b"], 1);
        aut.delta.add(1, alph["a"], 2);
        aut.delta.add(1, alph["b"], 4);
        aut.delta.add(2, alph["b"], 2);
        aut.delta.add(2, alph["a"], 3);
        aut.delta.add(3, alph["b"], 3);
        aut.delta.add(3, alph["a"], 1);
        aut.delta.add(4, alph["b"], 2);
        aut.delta.add(4, alph["b"], 3);

        for (const auto& algo : ALGORITHMS) {
            params["algorithm"] = algo;
            bool is_univ = aut.is_universal(alph, &cex, params);

            REQUIRE(is_univ);
        }
    }

    SECTION("subsumption-pruning in processed")
    {
        OnTheFlyAlphabet alph{ std::vector<std::string>{ "a" } };
        aut.initial = {1, 2};
        aut.final = {1};

        aut.delta.add(1, alph["a"], 1);

        for (const auto& algo : ALGORITHMS) {
            params["algorithm"] = algo;
            bool is_univ = aut.is_universal(alph, &cex, params);

            REQUIRE(is_univ);
        }
    }

    SECTION("wrong parameters 1")
    {
        OnTheFlyAlphabet alph{};

        CHECK_THROWS_WITH(aut.is_universal(alph, params),
            Catch::Contains("requires setting the \"algo\" key"));
    }

    SECTION("wrong parameters 2")
    {
        OnTheFlyAlphabet alph{};
        params["algorithm"] = "foo";

        CHECK_THROWS_WITH(aut.is_universal(alph, params),
            Catch::Contains("received an unknown value"));
    }
} // }}}

TEST_CASE("mata::nfa::is_included()")
{ // {{{
    Nfa smaller(10);
    Nfa bigger(16);
    Run cex;
    ParameterMap params;

    const std::unordered_set<std::string> ALGORITHMS = {
        "naive",
        "antichains",
    };

    SECTION("{} <= {}, empty alphabet")
    {
        OnTheFlyAlphabet alph{};

        for (const auto& algo : ALGORITHMS) {
            params["algorithm"] = algo;
            bool is_incl = is_included(smaller, bigger, &alph, params);
            CHECK(is_incl);

            is_incl = is_included(bigger, smaller, &alph, params);
            CHECK(is_incl);
        }
    }

    SECTION("{} <= {epsilon}, empty alphabet")
    {
        OnTheFlyAlphabet alph{};
        bigger.initial = {1};
        bigger.final = {1};

        for (const auto& algo : ALGORITHMS) {
            params["algorithm"] = algo;
            bool is_incl = is_included(smaller, bigger, &cex, &alph, params);
            CHECK(is_incl);

            is_incl = is_included(bigger, smaller, &cex, &alph, params);
            CHECK(!is_incl);
        }
    }

    SECTION("{epsilon} <= {epsilon}, empty alphabet")
    {
        OnTheFlyAlphabet alph{};
        smaller.initial = {1};
        smaller.final = {1};
        bigger.initial = {11};
        bigger.final = {11};

        for (const auto& algo : ALGORITHMS) {
            params["algorithm"] = algo;
            bool is_incl = is_included(smaller, bigger, &cex, &alph, params);
            CHECK(is_incl);

            is_incl = is_included(bigger, smaller, &cex, &alph, params);
            CHECK(is_incl);
        }
    }

    SECTION("{epsilon} !<= {}, empty alphabet")
    {
        OnTheFlyAlphabet alph{};
        smaller.initial = {1};
        smaller.final = {1};

        for (const auto& algo : ALGORITHMS) {
            params["algorithm"] = algo;
            bool is_incl = is_included(smaller, bigger, &cex, &alph, params);

            REQUIRE(!is_incl);
            REQUIRE(cex.word.empty());

            is_incl = is_included(bigger, smaller, &cex, &alph, params);
            REQUIRE(cex.word.empty());
            REQUIRE(is_incl);
        }
    }

    SECTION("a* + b* <= (a+b)*")
    {
        OnTheFlyAlphabet alph{ std::vector<std::string>{ "a", "b"} };
        smaller.initial = {1, 2};
        smaller.final = {1, 2};
        smaller.delta.add(1, alph["a"], 1);
        smaller.delta.add(2, alph["b"], 2);

        bigger.initial = {11};
        bigger.final = {11};
        bigger.delta.add(11, alph["a"], 11);
        bigger.delta.add(11, alph["b"], 11);

        for (const auto& algo : ALGORITHMS) {
            params["algorithm"] = algo;
            bool is_incl = is_included(smaller, bigger, &alph, params);
            REQUIRE(is_incl);

            is_incl = is_included(bigger, smaller, &alph, params);
            REQUIRE(!is_incl);
        }
    }

    SECTION("(a+b)* !<= a* + b*")
    {
        OnTheFlyAlphabet alph{ std::vector<std::string>{ "a", "b"} };
        smaller.initial = {1};
        smaller.final = {1};
        smaller.delta.add(1, alph["a"], 1);
        smaller.delta.add(1, alph["b"], 1);

        bigger.initial = {11, 12};
        bigger.final = {11, 12};
        bigger.delta.add(11, alph["a"], 11);
        bigger.delta.add(12, alph["b"], 12);

        for (const auto& algo : ALGORITHMS) {
            params["algorithm"] = algo;

            bool is_incl = is_included(smaller, bigger, &cex, &alph, params);

            REQUIRE(!is_incl);
            REQUIRE((
                cex.word == Word{alph["a"], alph["b"]} ||
                cex.word == Word{alph["b"], alph["a"]}));

            is_incl = is_included(bigger, smaller, &cex, &alph, params);
            REQUIRE(is_incl);
            REQUIRE((
                cex.word == Word{alph["a"], alph["b"]} ||
                cex.word == Word{alph["b"], alph["a"]}));
        }
    }

    SECTION("(a+b)* !<= eps + (a+b) + (a+b)(a+b)(a* + b*)")
    {
        OnTheFlyAlphabet alph{ std::vector<std::string>{ "a", "b"} };
        smaller.initial = {1};
        smaller.final = {1};
        smaller.delta.add(1, alph["a"], 1);
        smaller.delta.add(1, alph["b"], 1);

        bigger.initial = {11};
        bigger.final = {11, 12, 13, 14, 15};

        bigger.delta.add(11, alph["a"], 12);
        bigger.delta.add(11, alph["b"], 12);
        bigger.delta.add(12, alph["a"], 13);
        bigger.delta.add(12, alph["b"], 13);

        bigger.delta.add(13, alph["a"], 14);
        bigger.delta.add(14, alph["a"], 14);

        bigger.delta.add(13, alph["b"], 15);
        bigger.delta.add(15, alph["b"], 15);

        for (const auto& algo : ALGORITHMS) {
            params["algorithm"] = algo;
            bool is_incl = is_included(smaller, bigger, &cex, &alph, params);
            REQUIRE(!is_incl);

            REQUIRE(cex.word.size() == 4);
            REQUIRE((cex.word[0] == alph["a"] || cex.word[0] == alph["b"]));
            REQUIRE((cex.word[1] == alph["a"] || cex.word[1] == alph["b"]));
            REQUIRE((cex.word[2] == alph["a"] || cex.word[2] == alph["b"]));
            REQUIRE((cex.word[3] == alph["a"] || cex.word[3] == alph["b"]));
            REQUIRE(cex.word[2] != cex.word[3]);

            is_incl = is_included(bigger, smaller, &cex, &alph, params);
            REQUIRE(is_incl);

            REQUIRE(cex.word.size() == 4);
            REQUIRE((cex.word[0] == alph["a"] || cex.word[0] == alph["b"]));
            REQUIRE((cex.word[1] == alph["a"] || cex.word[1] == alph["b"]));
            REQUIRE((cex.word[2] == alph["a"] || cex.word[2] == alph["b"]));
            REQUIRE((cex.word[3] == alph["a"] || cex.word[3] == alph["b"]));
            REQUIRE(cex.word[2] != cex.word[3]);
        }
    }

    SECTION("wrong parameters 1")
    {
        OnTheFlyAlphabet alph{};

        CHECK_THROWS_WITH(is_included(smaller, bigger, &alph, params),
            Catch::Contains("requires setting the \"algo\" key"));
        CHECK_NOTHROW(is_included(smaller, bigger, &alph));
    }

    SECTION("wrong parameters 2")
    {
        OnTheFlyAlphabet alph{};
        params["algorithm"] = "foo";

        CHECK_THROWS_WITH(is_included(smaller, bigger, &alph, params),
            Catch::Contains("received an unknown value"));
        CHECK_NOTHROW(is_included(smaller, bigger, &alph));
    }
} // }}}

// TEST_CASE("mata::nfa::is_included_antichain_boost()")
// { // {{{
//     Nfa smaller(10);
//     Nfa bigger(16);
//     Run cex;

//     SECTION("{} <= {}, empty alphabet")
//     {
//         bool is_incl = antichain_boost_test(smaller, bigger, nullptr);
//         CHECK(is_incl);

//         is_incl = antichain_boost_test(bigger, smaller, nullptr);
//         CHECK(is_incl);
//     }

//     SECTION("{} <= {epsilon}, empty alphabet")
//     {
//         bigger.initial = {1};
//         bigger.final = {1};

//         bool is_incl = antichain_boost_test(smaller, bigger, &cex);
//         CHECK(is_incl);

//         is_incl = antichain_boost_test(bigger, smaller, &cex);
//         CHECK(!is_incl);
//     }

//     SECTION("{epsilon} <= {epsilon}, empty alphabet")
//     {
//         OnTheFlyAlphabet alph{};
//         smaller.initial = {1};
//         smaller.final = {1};
//         bigger.initial = {11};
//         bigger.final = {11};

//         bool is_incl = antichain_boost_test(smaller, bigger, &cex);
//         CHECK(is_incl);

//         is_incl = antichain_boost_test(bigger, smaller, &cex);
//         CHECK(is_incl);
//     }

//     SECTION("{epsilon} !<= {}, empty alphabet")
//     {
//         OnTheFlyAlphabet alph{};
//         smaller.initial = {1};
//         smaller.final = {1};

//         bool is_incl = antichain_boost_test(smaller, bigger, &cex);

//         REQUIRE(!is_incl);
//         REQUIRE(cex.word.empty());

//         is_incl = antichain_boost_test(bigger, smaller, &cex);
//         REQUIRE(cex.word.empty());
//         REQUIRE(is_incl);
//     }

//     SECTION("a* + b* <= (a+b)*")
//     {
//         OnTheFlyAlphabet alph{ std::vector<std::string>{ "a", "b"} };
//         smaller.initial = {1, 2};
//         smaller.final = {1, 2};
//         smaller.delta.add(1, alph["a"], 1);
//         smaller.delta.add(2, alph["b"], 2);

//         bigger.initial = {11};
//         bigger.final = {11};
//         bigger.delta.add(11, alph["a"], 11);
//         bigger.delta.add(11, alph["b"], 11);

//         bool is_incl = antichain_boost_test(smaller, bigger, nullptr);
//         REQUIRE(is_incl);

//         is_incl = antichain_boost_test(bigger, smaller, nullptr);
//         REQUIRE(!is_incl);
//     }

//     SECTION("(a+b)* !<= a* + b*")
//     {
//         OnTheFlyAlphabet alph{ std::vector<std::string>{ "a", "b"} };
//         smaller.initial = {1};
//         smaller.final = {1};
//         smaller.delta.add(1, alph["a"], 1);
//         smaller.delta.add(1, alph["b"], 1);

//         bigger.initial = {11, 12};
//         bigger.final = {11, 12};
//         bigger.delta.add(11, alph["a"], 11);
//         bigger.delta.add(12, alph["b"], 12);


//         bool is_incl = antichain_boost_test(smaller, bigger, &cex);

//         REQUIRE(!is_incl);
//         REQUIRE((
//             cex.word == Word{alph["a"], alph["b"]} ||
//             cex.word == Word{alph["b"], alph["a"]}));

//         is_incl = antichain_boost_test(bigger, smaller, &cex);
//         REQUIRE(is_incl);
//         REQUIRE((
//             cex.word == Word{alph["a"], alph["b"]} ||
//             cex.word == Word{alph["b"], alph["a"]}));
//     }

//     SECTION("(a+b)* !<= eps + (a+b) + (a+b)(a+b)(a* + b*)")
//     {
//         OnTheFlyAlphabet alph{ std::vector<std::string>{ "a", "b"} };
//         smaller.initial = {1};
//         smaller.final = {1};
//         smaller.delta.add(1, alph["a"], 1);
//         smaller.delta.add(1, alph["b"], 1);

//         bigger.initial = {11};
//         bigger.final = {11, 12, 13, 14, 15};

//         bigger.delta.add(11, alph["a"], 12);
//         bigger.delta.add(11, alph["b"], 12);
//         bigger.delta.add(12, alph["a"], 13);
//         bigger.delta.add(12, alph["b"], 13);

//         bigger.delta.add(13, alph["a"], 14);
//         bigger.delta.add(14, alph["a"], 14);

//         bigger.delta.add(13, alph["b"], 15);
//         bigger.delta.add(15, alph["b"], 15);

//         bool is_incl = antichain_boost_test(smaller, bigger, &cex);
//         REQUIRE(!is_incl);

//         REQUIRE(cex.word.size() == 4);
//         REQUIRE((cex.word[0] == alph["a"] || cex.word[0] == alph["b"]));
//         REQUIRE((cex.word[1] == alph["a"] || cex.word[1] == alph["b"]));
//         REQUIRE((cex.word[2] == alph["a"] || cex.word[2] == alph["b"]));
//         REQUIRE((cex.word[3] == alph["a"] || cex.word[3] == alph["b"]));
//         REQUIRE(cex.word[2] != cex.word[3]);

//         is_incl = antichain_boost_test(bigger, smaller, &cex);
//         REQUIRE(is_incl);

//         REQUIRE(cex.word.size() == 4);
//         REQUIRE((cex.word[0] == alph["a"] || cex.word[0] == alph["b"]));
//         REQUIRE((cex.word[1] == alph["a"] || cex.word[1] == alph["b"]));
//         REQUIRE((cex.word[2] == alph["a"] || cex.word[2] == alph["b"]));
//         REQUIRE((cex.word[3] == alph["a"] || cex.word[3] == alph["b"]));
//         REQUIRE(cex.word[2] != cex.word[3]);
//     }
// } // }}}

TEST_CASE("mata::nfa::are_equivalent")
{
    Nfa smaller(10);
    Nfa bigger(16);
    Word cex;
    ParameterMap params;

    const std::unordered_set<std::string> ALGORITHMS = {
            "naive",
            "antichains",
    };

    SECTION("{} == {}, empty alphabet")
    {
        OnTheFlyAlphabet alph{};

        for (const auto& algo : ALGORITHMS) {
            params["algorithm"] = algo;

            CHECK(are_equivalent(smaller, bigger, &alph, params));
            CHECK(are_equivalent(smaller, bigger, params));
            CHECK(are_equivalent(smaller, bigger));

            CHECK(are_equivalent(bigger, smaller, &alph, params));
            CHECK(are_equivalent(bigger, smaller, params));
            CHECK(are_equivalent(bigger, smaller));
        }
    }

    SECTION("{} == {epsilon}, empty alphabet")
    {
        OnTheFlyAlphabet alph{};
        bigger.initial = {1};
        bigger.final = {1};

        for (const auto& algo : ALGORITHMS) {
            params["algorithm"] = algo;

            CHECK(!are_equivalent(smaller, bigger, &alph, params));
            CHECK(!are_equivalent(smaller, bigger, params));
            CHECK(!are_equivalent(smaller, bigger));

            CHECK(!are_equivalent(bigger, smaller, &alph, params));
            CHECK(!are_equivalent(bigger, smaller, params));
            CHECK(!are_equivalent(bigger, smaller));
        }
    }

    SECTION("{epsilon} == {epsilon}, empty alphabet")
    {
        OnTheFlyAlphabet alph{};
        smaller.initial = {1};
        smaller.final = {1};
        bigger.initial = {11};
        bigger.final = {11};

        for (const auto& algo : ALGORITHMS) {
            params["algorithm"] = algo;

            CHECK(are_equivalent(smaller, bigger, &alph, params));
            CHECK(are_equivalent(smaller, bigger, params));
            CHECK(are_equivalent(smaller, bigger));

            CHECK(are_equivalent(bigger, smaller, &alph, params));
            CHECK(are_equivalent(bigger, smaller, params));
            CHECK(are_equivalent(bigger, smaller));
        }
    }

    SECTION("a* + b* == (a+b)*")
    {
        OnTheFlyAlphabet alph{ std::vector<std::string>{ "a", "b"} };
        smaller.initial = {1, 2};
        smaller.final = {1, 2};
        smaller.delta.add(1, alph["a"], 1);
        smaller.delta.add(2, alph["b"], 2);

        bigger.initial = {11};
        bigger.final = {11};
        bigger.delta.add(11, alph["a"], 11);
        bigger.delta.add(11, alph["b"], 11);

        for (const auto& algo : ALGORITHMS) {
            params["algorithm"] = algo;

            //TODO:what about we test the plumbing versions primarily?
            // Debugging with the dispatcher is annoying.

            CHECK(!are_equivalent(smaller, bigger, &alph, params));
            CHECK(!are_equivalent(smaller, bigger, params));
            CHECK(!are_equivalent(smaller, bigger));

            CHECK(!are_equivalent(bigger, smaller, &alph, params));
            CHECK(!are_equivalent(bigger, smaller, params));
            CHECK(!are_equivalent(bigger, smaller));
        }
    }

    SECTION("a* != (a|b)*, was throwing exception")
    {
        Nfa aut;
        mata::parser::create_nfa(&aut, "a*");
        Nfa aut2;
        mata::parser::create_nfa(&aut2, "(a|b)*");
        CHECK(!are_equivalent(aut, aut2));
    }

    SECTION("(a+b)* !<= eps + (a+b) + (a+b)(a+b)(a* + b*)")
    {
        OnTheFlyAlphabet alph{ std::vector<std::string>{ "a", "b"} };
        smaller.initial = {1};
        smaller.final = {1};
        smaller.delta.add(1, alph["a"], 1);
        smaller.delta.add(1, alph["b"], 1);

        bigger.initial = {11};
        bigger.final = {11, 12, 13, 14, 15};

        bigger.delta.add(11, alph["a"], 12);
        bigger.delta.add(11, alph["b"], 12);
        bigger.delta.add(12, alph["a"], 13);
        bigger.delta.add(12, alph["b"], 13);

        bigger.delta.add(13, alph["a"], 14);
        bigger.delta.add(14, alph["a"], 14);

        bigger.delta.add(13, alph["b"], 15);
        bigger.delta.add(15, alph["b"], 15);

        for (const auto& algo : ALGORITHMS) {
            params["algorithm"] = algo;

            CHECK(!are_equivalent(smaller, bigger, &alph, params));
            CHECK(!are_equivalent(smaller, bigger, params));
            CHECK(!are_equivalent(smaller, bigger));

            CHECK(!are_equivalent(bigger, smaller, &alph, params));
            CHECK(!are_equivalent(bigger, smaller, params));
            CHECK(!are_equivalent(bigger, smaller));
        }
    }

    SECTION("wrong parameters 1")
    {
        OnTheFlyAlphabet alph{};

        CHECK_THROWS_WITH(are_equivalent(smaller, bigger, &alph, params),
                          Catch::Contains("requires setting the \"algo\" key"));
        CHECK_THROWS_WITH(are_equivalent(smaller, bigger, params),
                          Catch::Contains("requires setting the \"algo\" key"));
        CHECK_NOTHROW(are_equivalent(smaller, bigger));
    }

    SECTION("wrong parameters 2")
    {
        OnTheFlyAlphabet alph{};
        params["algorithm"] = "foo";

        CHECK_THROWS_WITH(are_equivalent(smaller, bigger, &alph, params),
                          Catch::Contains("received an unknown value"));
        CHECK_THROWS_WITH(are_equivalent(smaller, bigger, params),
                          Catch::Contains("received an unknown value"));
        CHECK_NOTHROW(are_equivalent(smaller, bigger));
    }
}

TEST_CASE("mata::nfa::revert()")
{ // {{{
    Nfa aut(9);

    SECTION("empty automaton")
    {
        Nfa result = revert(aut);

        REQUIRE(result.delta.empty());
        REQUIRE(result.initial.empty());
        REQUIRE(result.final.empty());
    }

    SECTION("no-transition automaton")
    {
        aut.initial.insert(1);
        aut.initial.insert(3);

        aut.final.insert(2);
        aut.final.insert(5);

        Nfa result = revert(aut);

        REQUIRE(result.delta.empty());
        REQUIRE(result.initial[2]);
        REQUIRE(result.initial[5]);
        REQUIRE(result.final[1]);
        REQUIRE(result.final[3]);
    }

    SECTION("one-transition automaton")
    {
        aut.initial.insert(1);
        aut.final.insert(2);
        aut.delta.add(1, 'a', 2);

        Nfa result = revert(aut);

        REQUIRE(result.initial[2]);
        REQUIRE(result.final[1]);
        REQUIRE(result.delta.contains(2, 'a', 1));
        REQUIRE(result.delta.num_of_transitions() == aut.delta.num_of_transitions());
    }

    SECTION("bigger automaton")
    {
        aut.initial = {1, 2};
        aut.delta.add(1, 'a', 2);
        aut.delta.add(1, 'a', 3);
        aut.delta.add(1, 'b', 4);
        aut.delta.add(2, 'a', 2);
        aut.delta.add(2, 'a', 3);
        aut.delta.add(2, 'b', 4);
        aut.delta.add(3, 'b', 4);
        aut.delta.add(3, 'c', 7);
        aut.delta.add(3, 'b', 2);
        aut.delta.add(7, 'a', 8);
        aut.final = {3};

        Nfa result = revert(aut);
        //REQUIRE(result.final == StateSet({1, 2}));
        REQUIRE(StateSet(result.final) == StateSet({1, 2}));
        REQUIRE(result.delta.contains(2, 'a', 1));
        REQUIRE(result.delta.contains(3, 'a', 1));
        REQUIRE(result.delta.contains(4, 'b', 1));
        REQUIRE(result.delta.contains(2, 'a', 2));
        REQUIRE(result.delta.contains(3, 'a', 2));
        REQUIRE(result.delta.contains(4, 'b', 2));
        REQUIRE(result.delta.contains(4, 'b', 3));
        REQUIRE(result.delta.contains(7, 'c', 3));
        REQUIRE(result.delta.contains(2, 'b', 3));
        REQUIRE(result.delta.contains(8, 'a', 7));
        REQUIRE(StateSet(result.initial) == StateSet({3}));
    }

    SECTION("Automaton A") {
        Nfa nfa{ 11 };
        FILL_WITH_AUT_A(nfa);
        Nfa res = revert(nfa);
        CHECK(res.initial[5]);
        CHECK(res.final[1]);
        CHECK(res.final[3]);
        CHECK(res.delta.num_of_transitions() == 15);
        CHECK(res.delta.contains(5, 'a', 5));
        CHECK(res.delta.contains(5, 'a', 7));
        CHECK(res.delta.contains(9, 'a', 9));
        CHECK(res.delta.contains(9, 'c', 5));
        CHECK(res.delta.contains(9, 'b', 3));
        CHECK(res.delta.contains(7, 'a', 3));
        CHECK(res.delta.contains(7, 'a', 10));
        CHECK(res.delta.contains(7, 'b', 10));
        CHECK(res.delta.contains(7, 'c', 10));
        CHECK(res.delta.contains(7, 'b', 1));
        CHECK(res.delta.contains(3, 'a', 7));
        CHECK(res.delta.contains(3, 'c', 7));
        CHECK(res.delta.contains(3, 'a', 1));
        CHECK(res.delta.contains(1, 'b', 7));
        CHECK(res.delta.contains(10, 'a', 1));
    }

    SECTION("Automaton B") {
        Nfa nfa{ 15 };
        FILL_WITH_AUT_B(nfa);
        Nfa res = revert(nfa);
        CHECK(res.initial[2]);
        CHECK(res.initial[12]);
        CHECK(res.final[4]);
        CHECK(res.delta.num_of_transitions() == 12);
        CHECK(res.delta.contains(8, 'a', 4));
        CHECK(res.delta.contains(8, 'c', 4));
        CHECK(res.delta.contains(4, 'b', 8));
        CHECK(res.delta.contains(6, 'b', 4));
        CHECK(res.delta.contains(6, 'a', 4));
        CHECK(res.delta.contains(2, 'a', 6));
        CHECK(res.delta.contains(2, 'a', 0));
        CHECK(res.delta.contains(2, 'b', 2));
        CHECK(res.delta.contains(0, 'a', 2));
        CHECK(res.delta.contains(12, 'c', 2));
        CHECK(res.delta.contains(12, 'b', 14));
        CHECK(res.delta.contains(14, 'a', 12));
    }
} // }}}


TEST_CASE("mata::nfa::Nfa::is_deterministic()")
{ // {{{
    Nfa aut('s'+1);

    SECTION("(almost) empty automaton") {
        // no initial states
        REQUIRE(!aut.is_deterministic());

        // add an initial state
        aut.initial.insert('q');
        REQUIRE(aut.is_deterministic());

        // add the same initial state
        aut.initial.insert('q');
        REQUIRE(aut.is_deterministic());

        // add another initial state
        aut.initial.insert('r');
        REQUIRE(!aut.is_deterministic());

        // add a final state
        aut.final.insert('q');
        REQUIRE(!aut.is_deterministic());
    }

    SECTION("trivial automata") {
        aut.initial.insert('q');
        aut.delta.add('q', 'a', 'r');
        REQUIRE(aut.is_deterministic());

        // unreachable states
        aut.delta.add('s', 'a', 'r');
        REQUIRE(aut.is_deterministic());

        // transitions over a different symbol
        aut.delta.add('q', 'b', 'h');
        REQUIRE(aut.is_deterministic());

        // nondeterminism
        aut.delta.add('q', 'a', 's');
        REQUIRE(!aut.is_deterministic());
    }

    SECTION("larger automaton 1") {
        FILL_WITH_AUT_A(aut);
        REQUIRE(!aut.is_deterministic());
    }

    SECTION("larger automaton 2") {
        FILL_WITH_AUT_B(aut);
        REQUIRE(!aut.is_deterministic());
    }
} // }}}

TEST_CASE("mata::nfa::is_complete()")
{ // {{{
    Nfa aut('q'+1);

    SECTION("empty automaton")
    {
        OnTheFlyAlphabet alph{};

        // is complete for the empty alphabet
        REQUIRE(aut.is_complete(&alph));

        alph.translate_symb("a1");
        alph.translate_symb("a2");

        // the empty automaton is complete even for a non-empty alphabet
        REQUIRE(aut.is_complete(&alph));

        // add a non-reachable state (the automaton should still be complete)
        aut.delta.add('q', alph["a1"], 'q');
        REQUIRE(aut.is_complete(&alph));
    }

    SECTION("small automaton")
    {
        OnTheFlyAlphabet alph{};

        aut.initial.insert(4);
        aut.delta.add(4, alph["a"], 8);
        aut.delta.add(4, alph["c"], 8);
        aut.delta.add(4, alph["a"], 6);
        aut.delta.add(4, alph["b"], 6);
        aut.delta.add(8, alph["b"], 4);
        aut.delta.add(6, alph["a"], 2);
        aut.delta.add(2, alph["b"], 2);
        aut.delta.add(2, alph["a"], 0);
        aut.delta.add(2, alph["c"], 12);
        aut.delta.add(0, alph["a"], 2);
        aut.delta.add(12, alph["a"], 14);
        aut.delta.add(14, alph["b"], 12);
        aut.final.insert({2, 12});

        REQUIRE(!aut.is_complete(&alph));

        aut.make_complete(&alph, 100);
        REQUIRE(aut.is_complete(&alph));
    }

    SECTION("using a non-alphabet symbol")
    {
        OnTheFlyAlphabet alph{};

        aut.initial.insert(4);
        aut.delta.add(4, alph["a"], 8);
        aut.delta.add(4, alph["c"], 8);
        aut.delta.add(4, alph["a"], 6);
        aut.delta.add(4, alph["b"], 6);
        aut.delta.add(6, 100, 4);

        CHECK_THROWS_WITH(aut.is_complete(&alph),
            Catch::Contains("symbol that is not in the provided alphabet"));
    }
} // }}}

TEST_CASE("mata::nfa::is_prfx_in_lang()")
{ // {{{
    Nfa aut('q'+1);

    SECTION("empty automaton")
    {
        Run w;
        w.word = {'a', 'b', 'd'};
        REQUIRE(!aut.is_prfx_in_lang(w));

        w.word = { };
        REQUIRE(!aut.is_prfx_in_lang(w));
    }

    SECTION("automaton accepting only epsilon")
    {
        aut.initial.insert('q');
        aut.final.insert('q');

        Run w;
        w.word = { };
        REQUIRE(aut.is_prfx_in_lang(w));

        w.word = {'a', 'b'};
        REQUIRE(aut.is_prfx_in_lang(w));
    }

    SECTION("small automaton")
    {
        FILL_WITH_AUT_B(aut);

        Run w;
        w.word = {'b', 'a'};
        REQUIRE(aut.is_prfx_in_lang(w));

        w.word = { };
        REQUIRE(!aut.is_prfx_in_lang(w));

        w.word = {'c', 'b', 'a'};
        REQUIRE(!aut.is_prfx_in_lang(w));

        w.word = {'c', 'b', 'a', 'a'};
        REQUIRE(aut.is_prfx_in_lang(w));

        w.word = {'a', 'a'};
        REQUIRE(aut.is_prfx_in_lang(w));

        w.word = {'c', 'b', 'b', 'a', 'c', 'b'};
        REQUIRE(aut.is_prfx_in_lang(w));

        w.word = Word(100000, 'a');
        REQUIRE(aut.is_prfx_in_lang(w));

        w.word = Word(100000, 'b');
        REQUIRE(!aut.is_prfx_in_lang(w));
    }
} // }}}

TEST_CASE("mata::nfa::fw-direct-simulation()")
{ // {{{
    Nfa aut;

    SECTION("empty automaton")
    {
        Simlib::Util::BinaryRelation result = compute_relation(aut);

        REQUIRE(result.size() == 0);
    }

    aut.add_state(8);
    SECTION("no-transition automaton")
    {
        aut.initial.insert(1);
        aut.initial.insert(3);

        aut.final.insert(2);
        aut.final.insert(5);

        Simlib::Util::BinaryRelation result = compute_relation(aut);
        REQUIRE(result.get(1,3));
        REQUIRE(result.get(2,5));
        REQUIRE(!result.get(5,1));
        REQUIRE(!result.get(2,3));
    }

    SECTION("small automaton")
    {
        aut.initial.insert(1);
        aut.final.insert(2);
        aut.delta.add(1, 'a', 4);
        aut.delta.add(4, 'b', 5);
        aut.delta.add(2, 'b', 5);
        aut.delta.add(1, 'b', 4);

        Simlib::Util::BinaryRelation result = compute_relation(aut);
        REQUIRE(result.get(4,1));
        REQUIRE(!result.get(2,5));

    }

    Nfa aut_big(9);

    SECTION("bigger automaton")
    {
        aut_big.initial = {1, 2};
        aut_big.delta.add(1, 'a', 2);
        aut_big.delta.add(1, 'a', 3);
        aut_big.delta.add(1, 'b', 4);
        aut_big.delta.add(2, 'a', 2);
        aut_big.delta.add(2, 'b', 2);
        aut_big.delta.add(2, 'a', 3);
        aut_big.delta.add(2, 'b', 4);
        aut_big.delta.add(3, 'b', 4);
        aut_big.delta.add(3, 'c', 7);
        aut_big.delta.add(3, 'b', 2);
        aut_big.delta.add(5, 'c', 3);
        aut_big.delta.add(7, 'a', 8);
        aut_big.final = {3};

        Simlib::Util::BinaryRelation result = compute_relation(aut_big);
        REQUIRE(result.get(1,2));
        REQUIRE(!result.get(2,1));
        REQUIRE(!result.get(3,1));
        REQUIRE(!result.get(3,2));
        REQUIRE(result.get(4,1));
        REQUIRE(result.get(4,2));
        REQUIRE(result.get(4,5));
        REQUIRE(!result.get(5,2));
        REQUIRE(!result.get(5,1));
        REQUIRE(result.get(7,1));
        REQUIRE(result.get(7,2));
        REQUIRE(result.get(8,1));
        REQUIRE(result.get(8,2));
        REQUIRE(result.get(8,5));
    }
} // }}

TEST_CASE("mata::nfa::reduce_size_by_simulation()")
{
    Nfa aut;
    StateRenaming state_renaming;

    SECTION("empty automaton")
    {
        Nfa result = reduce(aut, &state_renaming);

        REQUIRE(result.delta.empty());
        REQUIRE(result.initial.empty());
        REQUIRE(result.final.empty());
    }

    SECTION("simple automaton")
    {
        aut.add_state(2);
        aut.initial.insert(1);

        aut.final.insert(2);
        Nfa result = reduce(aut, &state_renaming);

        REQUIRE(result.delta.empty());
        REQUIRE(result.initial[state_renaming[1]]);
        REQUIRE(result.final[state_renaming[2]]);
        REQUIRE(result.num_of_states() == 2);
        REQUIRE(state_renaming[1] == state_renaming[0]);
        REQUIRE(state_renaming[2] != state_renaming[0]);
    }

    SECTION("big automaton")
    {
        aut.add_state(9);
        aut.initial = {1, 2};
        aut.delta.add(1, 'a', 2);
        aut.delta.add(1, 'a', 3);
        aut.delta.add(1, 'b', 4);
        aut.delta.add(2, 'a', 2);
        aut.delta.add(2, 'b', 2);
        aut.delta.add(2, 'a', 3);
        aut.delta.add(2, 'b', 4);
        aut.delta.add(3, 'b', 4);
        aut.delta.add(3, 'c', 7);
        aut.delta.add(3, 'b', 2);
        aut.delta.add(5, 'c', 3);
        aut.delta.add(7, 'a', 8);
        aut.delta.add(9, 'b', 2);
        aut.delta.add(9, 'c', 0);
        aut.delta.add(0, 'a', 4);
        aut.final = {3, 9};


        Nfa result = reduce(aut, &state_renaming);

        REQUIRE(result.num_of_states() == 6);
        REQUIRE(result.initial[state_renaming[1]]);
        REQUIRE(result.initial[state_renaming[2]]);
        REQUIRE(result.delta.contains(state_renaming[9], 'c', state_renaming[0]));
        REQUIRE(result.delta.contains(state_renaming[9], 'c', state_renaming[7]));
        REQUIRE(result.delta.contains(state_renaming[3], 'c', state_renaming[0]));
        REQUIRE(result.delta.contains(state_renaming[0], 'a', state_renaming[8]));
        REQUIRE(result.delta.contains(state_renaming[7], 'a', state_renaming[4]));
        REQUIRE(result.delta.contains(state_renaming[1], 'a', state_renaming[3]));
        REQUIRE(!result.delta.contains(state_renaming[3], 'b', state_renaming[4]));
        REQUIRE(result.delta.contains(state_renaming[2], 'a', state_renaming[2]));
        REQUIRE(result.final[state_renaming[9]]);
        REQUIRE(result.final[state_renaming[3]]);

        result = reduce(aut.trim(), &state_renaming);
        CHECK(result.num_of_states() == 3);
        CHECK(result.initial == SparseSet<State>{ 0, 1 });
        CHECK(result.final == SparseSet<State>{ 2 });
        CHECK(result.delta.num_of_transitions() == 6);
        CHECK(result.delta.contains(state_renaming[0], 'a', state_renaming[2]));
        CHECK(result.delta.contains(state_renaming[0], 'a', state_renaming[1]));
        CHECK(result.delta.contains(state_renaming[1], 'a', state_renaming[1]));
        CHECK(result.delta.contains(state_renaming[1], 'b', state_renaming[1]));
        CHECK(result.delta.contains(state_renaming[1], 'a', state_renaming[2]));
        CHECK(result.delta.contains(state_renaming[2], 'b', state_renaming[1]));
    }

    SECTION("no transitions from non-final state")
    {
        aut.delta.add(0, 'a', 1);
        aut.initial = { 0 };
        Nfa result = reduce(aut.trim(), &state_renaming);
        CHECK(are_equivalent(result, aut));
    }
}

TEST_CASE("mata::nfa::reduce_size_by_residual()") {
    Nfa aut;
    StateRenaming state_renaming;
    ParameterMap params_after, params_with;
    params_after["algorithm"] = "residual";
    params_with["algorithm"] = "residual";

    SECTION("empty automaton")
    {
        params_after["type"] = "after";
        params_after["direction"] = "forward";
        params_with["type"] = "with";
        params_with["direction"] = "forward";

        Nfa result_after = reduce(aut, &state_renaming, params_after);
        Nfa result_with = reduce(aut, &state_renaming, params_with);

        REQUIRE(result_after.delta.empty());
        REQUIRE(result_after.initial.empty());
        REQUIRE(result_after.final.empty());
        REQUIRE(result_after.is_identical(result_with));
        REQUIRE(are_equivalent(aut, result_after));
    }

    SECTION("simple automaton")
    {
        params_after["type"] = "after";
        params_after["direction"] = "forward";
        params_with["type"] = "with";
        params_with["direction"] = "forward";
        aut.add_state(2);
        aut.initial.insert(1);

        aut.final.insert(2);
        Nfa result_after = reduce(aut, &state_renaming, params_after);
        Nfa result_with = reduce(aut, &state_renaming, params_with);

        REQUIRE(result_after.num_of_states() == 0);
        REQUIRE(result_after.initial.empty());
        REQUIRE(result_after.final.empty());
        REQUIRE(result_after.delta.empty());
        REQUIRE(result_after.is_identical(result_with));
        REQUIRE(are_equivalent(aut, result_after));

        aut.delta.add(1, 'a', 2);
        result_after = reduce(aut, &state_renaming, params_after);
        result_with = reduce(aut, &state_renaming, params_with);

        REQUIRE(result_after.num_of_states() == 2);
        REQUIRE(result_after.initial[0]);
        REQUIRE(result_after.final[1]);
        REQUIRE(result_after.delta.contains(0, 'a', 1));
        REQUIRE(result_after.is_identical(result_with));
        REQUIRE(are_equivalent(aut, result_after));
    }

    SECTION("medium automaton")
    {
        params_after["type"] = "after";
        params_after["direction"] = "forward";
        params_with["type"] = "with";
        params_with["direction"] = "forward";
        aut.add_state(4);

        aut.initial = { 1 };
        aut.final = { 2,3 };
        aut.delta.add(1, 'b', 4);
        aut.delta.add(1, 'a', 3);
        aut.delta.add(4, 'a', 2);
        aut.delta.add(4, 'a', 3);
        aut.delta.add(3, 'a', 2);
        aut.delta.add(3, 'a', 3);
        aut.delta.add(2, 'a', 1);

        Nfa result_after = reduce(aut, &state_renaming, params_after);
        Nfa result_with = reduce(aut, &state_renaming, params_with);

        REQUIRE(result_after.num_of_states() == 4);
        REQUIRE(result_after.initial[0]);
        REQUIRE(result_after.delta.contains(0, 'a', 1));
        REQUIRE(result_after.delta.contains(0, 'b', 2));
        REQUIRE(result_after.delta.contains(1, 'a', 3));
        REQUIRE(!result_after.delta.contains(1, 'b', 3));
        REQUIRE(result_after.delta.contains(2, 'a', 3));
        REQUIRE(!result_after.delta.contains(2, 'a', 2));
        REQUIRE(result_after.delta.contains(3, 'a', 3));
        REQUIRE(result_after.delta.contains(3, 'a', 0));
        REQUIRE(!result_after.delta.contains(3, 'b', 2));
        REQUIRE(result_after.final[1]);
        REQUIRE(result_after.final[3]);
        REQUIRE(result_after.is_identical(result_with));
    }

    SECTION("big automaton")
    {
        params_after["type"] = "after";
        params_after["direction"] = "forward";
        params_with["type"] = "with";
        params_with["direction"] = "forward";
        aut.add_state(7);

        aut.initial = { 0 };
        aut.final = { 0,1,2,4,5,6 };
        aut.delta.add(0, 'a', 1);
        aut.delta.add(0, 'b', 1);
        aut.delta.add(0, 'c', 1);
        aut.delta.add(0, 'd', 2);

        aut.delta.add(1, 'a', 1);
        aut.delta.add(1, 'b', 1);
        aut.delta.add(1, 'c', 1);
        aut.delta.add(1, 'd', 1);

        aut.delta.add(2, 'a', 3);
        aut.delta.add(2, 'b', 1);
        aut.delta.add(2, 'c', 4);
        aut.delta.add(2, 'd', 1);

        aut.delta.add(3, 'a', 1);
        aut.delta.add(3, 'b', 1);
        aut.delta.add(3, 'c', 1);
        aut.delta.add(3, 'd', 1);

        aut.delta.add(4, 'a', 1);
        aut.delta.add(4, 'b', 1);
        aut.delta.add(4, 'c', 1);
        aut.delta.add(4, 'd', 5);

        aut.delta.add(5, 'a', 3);
        aut.delta.add(5, 'b', 1);
        aut.delta.add(5, 'c', 1);
        aut.delta.add(5, 'd', 6);

        aut.delta.add(6, 'a', 3);
        aut.delta.add(6, 'b', 1);
        aut.delta.add(6, 'c', 1);
        aut.delta.add(6, 'd', 1);

        Nfa result_after = reduce(aut, &state_renaming, params_after);
        Nfa result_with = reduce(aut, &state_renaming, params_with);

        REQUIRE(result_after.num_of_states() == 5);
        REQUIRE(result_after.initial[0]);
        REQUIRE(result_after.delta.contains(0, 'd', 1));
        REQUIRE(!result_after.delta.contains(0, 'd', 0));
        REQUIRE(!result_after.delta.contains(0, 'd', 2));
        REQUIRE(result_after.delta.contains(0, 'a', 0));
        REQUIRE(result_after.delta.contains(0, 'a', 1));
        REQUIRE(result_after.delta.contains(0, 'b', 0));
        REQUIRE(result_after.delta.contains(0, 'b', 1));
        REQUIRE(result_after.delta.contains(0, 'c', 0));
        REQUIRE(result_after.delta.contains(0, 'c', 1));

        REQUIRE(result_after.delta.contains(1, 'a', 2));
        REQUIRE(!result_after.delta.contains(1, 'a', 3));
        REQUIRE(result_after.delta.contains(1, 'b', 0));
        REQUIRE(result_after.delta.contains(1, 'b', 1));
        REQUIRE(result_after.delta.contains(1, 'c', 3));
        REQUIRE(!result_after.delta.contains(1, 'c', 2));
        REQUIRE(result_after.delta.contains(1, 'd', 0));
        REQUIRE(result_after.delta.contains(1, 'd', 1));

        REQUIRE(result_after.delta.contains(2, 'a', 0));
        REQUIRE(result_after.delta.contains(2, 'a', 1));
        REQUIRE(result_after.delta.contains(2, 'b', 0));
        REQUIRE(result_after.delta.contains(2, 'b', 1));
        REQUIRE(result_after.delta.contains(2, 'c', 0));
        REQUIRE(result_after.delta.contains(2, 'c', 1));
        REQUIRE(result_after.delta.contains(2, 'd', 0));
        REQUIRE(result_after.delta.contains(2, 'd', 1));

        REQUIRE(result_after.delta.contains(3, 'a', 0));
        REQUIRE(result_after.delta.contains(3, 'a', 1));
        REQUIRE(result_after.delta.contains(3, 'b', 0));
        REQUIRE(result_after.delta.contains(3, 'b', 1));
        REQUIRE(result_after.delta.contains(3, 'c', 0));
        REQUIRE(result_after.delta.contains(3, 'c', 1));
        REQUIRE(result_after.delta.contains(3, 'd', 4));
        REQUIRE(!result_after.delta.contains(3, 'd', 2));
        REQUIRE(!result_after.delta.contains(3, 'd', 3));

        REQUIRE(result_after.delta.contains(4, 'a', 2));
        REQUIRE(result_after.delta.contains(4, 'b', 0));
        REQUIRE(result_after.delta.contains(4, 'b', 1));
        REQUIRE(result_after.delta.contains(4, 'c', 0));
        REQUIRE(result_after.delta.contains(4, 'c', 1));
        REQUIRE(result_after.delta.contains(4, 'd', 1));
        REQUIRE(result_after.delta.contains(4, 'd', 4));
        REQUIRE(!result_after.delta.contains(4, 'd', 0));
        REQUIRE(!result_after.delta.contains(4, 'd', 3));
        REQUIRE(result_after.final[0]);
        REQUIRE(result_after.final[1]);
        REQUIRE(result_after.final[3]);
        REQUIRE(result_after.final[4]);
        REQUIRE(are_equivalent(result_after, result_with));
        REQUIRE(are_equivalent(aut, result_after));
    }

    SECTION("backward residual big automaton")
    {
        params_after["type"] = "after";
        params_after["direction"] = "backward";
        params_with["type"] = "with";
        params_with["direction"] = "backward";

        aut.add_state(7);

        aut.initial = { 0 };
        aut.final = { 0,1,2,4,5,6 };
        aut.delta.add(0, 'a', 1);
        aut.delta.add(0, 'b', 1);
        aut.delta.add(0, 'c', 1);
        aut.delta.add(0, 'd', 2);

        aut.delta.add(1, 'a', 1);
        aut.delta.add(1, 'b', 1);
        aut.delta.add(1, 'c', 1);
        aut.delta.add(1, 'd', 1);

        aut.delta.add(2, 'a', 3);
        aut.delta.add(2, 'b', 1);
        aut.delta.add(2, 'c', 4);
        aut.delta.add(2, 'd', 1);

        aut.delta.add(3, 'a', 1);
        aut.delta.add(3, 'b', 1);
        aut.delta.add(3, 'c', 1);
        aut.delta.add(3, 'd', 1);

        aut.delta.add(4, 'a', 1);
        aut.delta.add(4, 'b', 1);
        aut.delta.add(4, 'c', 1);
        aut.delta.add(4, 'd', 5);

        aut.delta.add(5, 'a', 3);
        aut.delta.add(5, 'b', 1);
        aut.delta.add(5, 'c', 1);
        aut.delta.add(5, 'd', 6);

        aut.delta.add(6, 'a', 3);
        aut.delta.add(6, 'b', 1);
        aut.delta.add(6, 'c', 1);
        aut.delta.add(6, 'd', 1);

        Nfa result_after = reduce(aut, &state_renaming, params_after);
        Nfa result_with = reduce(aut, &state_renaming, params_with);

        REQUIRE(result_after.num_of_states() == 6);
        REQUIRE(result_after.initial[0]);
        REQUIRE(result_after.initial[1]);
        REQUIRE(result_after.initial[3]);
        REQUIRE(result_after.initial[4]);

        REQUIRE(!result_after.delta.contains(0, 'a', 0));
        REQUIRE(!result_after.delta.contains(0, 'c', 2));

        REQUIRE(result_after.delta.contains(1, 'a', 0));
        REQUIRE(!result_after.delta.contains(1, 'd', 2));
        REQUIRE(!result_after.delta.contains(1, 'd', 3));

        REQUIRE(result_after.delta.contains(2, 'd', 1));
        REQUIRE(!result_after.delta.contains(2, 'c', 4));

        REQUIRE(result_after.delta.contains(3, 'c', 2));
        REQUIRE(result_after.delta.contains(3, 'c', 4));

        REQUIRE(result_after.delta.contains(4, 'd', 2));
        REQUIRE(!result_after.delta.contains(4, 'd', 3));

        REQUIRE(result_after.delta.contains(5, 'd', 3));

        REQUIRE(result_after.final[0]);

        REQUIRE(are_equivalent(result_after, result_with));
        REQUIRE(are_equivalent(aut, result_after));

    }

    SECTION("error checking")
    {
        CHECK_THROWS_WITH(reduce(aut, &state_renaming, params_after),
                          Catch::Contains("requires setting the \"type\" key in the \"params\" argument;"));

        params_after["type"] = "bad_type";
        CHECK_THROWS_WITH(reduce(aut, &state_renaming, params_after),
                          Catch::Contains("requires setting the \"direction\" key in the \"params\" argument;"));

        params_after["direction"] = "unknown_direction";
        CHECK_THROWS_WITH(reduce(aut, &state_renaming, params_after),
                          Catch::Contains("received an unknown value of the \"direction\" key"));

        params_after["direction"] = "forward";
        CHECK_THROWS_WITH(reduce(aut, &state_renaming, params_after),
                          Catch::Contains("received an unknown value of the \"type\" key"));

        params_after["type"] = "after";
        CHECK_NOTHROW(reduce(aut, &state_renaming, params_after));

    }
}

TEST_CASE("mata::nfa::union_norename()") {
    Run one{{1},{}};
    Run zero{{0}, {}};

    Nfa lhs(2);
    lhs.initial.insert(0);
    lhs.delta.add(0, 0, 1);
    lhs.final.insert(1);
    REQUIRE(!lhs.is_in_lang(one));
    REQUIRE(lhs.is_in_lang(zero));

    Nfa rhs(2);
    rhs.initial.insert(0);
    rhs.delta.add(0, 1, 1);
    rhs.final.insert(1);
    REQUIRE(rhs.is_in_lang(one));
    REQUIRE(!rhs.is_in_lang(zero));

    SECTION("failing minimal scenario") {
        Nfa result = union_nondet(lhs, rhs);
        REQUIRE(result.is_in_lang(one));
        REQUIRE(result.is_in_lang(zero));
    }
}

TEST_CASE("mata::nfa::union_inplace") {
    Run one{{1},{}};
    Run zero{{0}, {}};

    Nfa lhs(2);
    lhs.initial.insert(0);
    lhs.delta.add(0, 0, 1);
    lhs.final.insert(1);
    REQUIRE(!lhs.is_in_lang(one));
    REQUIRE(lhs.is_in_lang(zero));

    Nfa rhs(2);
    rhs.initial.insert(0);
    rhs.delta.add(0, 1, 1);
    rhs.final.insert(1);
    REQUIRE(rhs.is_in_lang(one));
    REQUIRE(!rhs.is_in_lang(zero));

    SECTION("failing minimal scenario") {
        Nfa result = lhs.unite_nondet_with(rhs);
        REQUIRE(result.is_in_lang(one));
        REQUIRE(result.is_in_lang(zero));
    }

    SECTION("same automata") {
        const Nfa lhs_copy{ lhs };
        CHECK(are_equivalent(lhs.unite_nondet_with(lhs), lhs_copy));
    }
}

TEST_CASE("mata::nfa::union_product()") {
    Run one{ { 1 },{} };
    Run zero{{ 0 }, {} };
    Run two{ { 2 },{} };
    Run two_three{ { 2, 3 },{} };

    Nfa lhs(4);
    lhs.initial.insert(0);
    lhs.delta.add(0, 0, 1);
    lhs.delta.add(0, 2, 2);
    lhs.delta.add(2, 3, 3);
    lhs.final.insert(1);
    lhs.final.insert(3);
    REQUIRE(!lhs.is_in_lang(one));
    REQUIRE(lhs.is_in_lang(zero));
    REQUIRE(!lhs.is_in_lang(two));
    REQUIRE(lhs.is_in_lang(two_three));

    Nfa rhs(4);
    rhs.initial.insert(0);
    rhs.delta.add(0, 1, 1);
    rhs.delta.add(0, 2, 2);
    rhs.delta.add(2, 3, 3);
    rhs.final.insert(1);
    rhs.final.insert(2);
    REQUIRE(rhs.is_in_lang(one));
    REQUIRE(!rhs.is_in_lang(zero));
    REQUIRE(rhs.is_in_lang(two));
    REQUIRE(!rhs.is_in_lang(two_three));

    SECTION("Minimal example") {
        Nfa result = mata::nfa::union_product(lhs, rhs);
        CHECK(!result.is_in_lang(one));
        CHECK(!result.is_in_lang(zero));
        CHECK(result.is_in_lang(two));
        CHECK(result.is_in_lang(two_three));
    }
}

TEST_CASE("mata::nfa::remove_final()")
{
    Nfa aut('q' + 1);

    SECTION("Automaton B")
    {
        FILL_WITH_AUT_B(aut);
        REQUIRE(aut.final[2]);
        REQUIRE(aut.final[12]);
        aut.final.erase(12);
        REQUIRE(aut.final[2]);
        REQUIRE(!aut.final[12]);
    }
}

TEST_CASE("mata::nfa::delta.remove()")
{
    Nfa aut('q' + 1);

    SECTION("Automaton B")
    {
        FILL_WITH_AUT_B(aut);
        aut.delta.add(1, 3, 4);
        aut.delta.add(1, 3, 5);

        SECTION("Simple remove")
        {
            REQUIRE(aut.delta.contains(1, 3, 4));
            REQUIRE(aut.delta.contains(1, 3, 5));
            aut.delta.remove(1, 3, 5);
            REQUIRE(aut.delta.contains(1, 3, 4));
            REQUIRE(!aut.delta.contains(1, 3, 5));
        }

        SECTION("Remove missing transition")
        {
            REQUIRE_THROWS_AS(aut.delta.remove(1, 1, 5), std::invalid_argument);
        }

        SECTION("Remove the last state_to from targets") {
            REQUIRE(aut.delta.contains(6, 'a', 2));
            aut.delta.remove(6, 'a', 2);
            REQUIRE(!aut.delta.contains(6, 'a', 2));
            REQUIRE(aut.delta[6].empty());

            REQUIRE(aut.delta.contains(4, 'a', 8));
            REQUIRE(aut.delta.contains(4, 'c', 8));
            REQUIRE(aut.delta.contains(4, 'a', 6));
            REQUIRE(aut.delta.contains(4, 'b', 6));
            REQUIRE(aut.delta[4].size() == 3);
            aut.delta.remove(4, 'a', 6);
            REQUIRE(!aut.delta.contains(4, 'a', 6));
            REQUIRE(aut.delta.contains(4, 'b', 6));
            REQUIRE(aut.delta[4].size() == 3);

            aut.delta.remove(4, 'a', 8);
            REQUIRE(!aut.delta.contains(4, 'a', 8));
            REQUIRE(aut.delta.contains(4, 'c', 8));
            REQUIRE(aut.delta[4].size() == 2);

            aut.delta.remove(4, 'c', 8);
            REQUIRE(!aut.delta.contains(4, 'a', 8));
            REQUIRE(!aut.delta.contains(4, 'c', 8));
            REQUIRE(aut.delta[4].size() == 1);
        }
    }
}

TEST_CASE("mata::nfa::get_trans_as_sequence(}") {
    Nfa aut('q' + 1);
    std::vector<Transition> expected{};

    aut.delta.add(1, 2, 3);
    expected.emplace_back(1, 2, 3);
    aut.delta.add(1, 3, 4);
    expected.emplace_back(1, 3, 4);
    aut.delta.add(2, 3, 4);
    expected.emplace_back(2, 3, 4);


    const Delta::Transitions transitions{ aut.delta.transitions() };
    REQUIRE(std::vector<Transition>{ transitions.begin(), transitions.end() } == expected);
}

TEST_CASE("mata::nfa::remove_epsilon()")
{
    Nfa aut{20};
    FILL_WITH_AUT_A(aut);
    aut.remove_epsilon('c');
    REQUIRE(aut.delta.contains(10, 'a', 7));
    REQUIRE(aut.delta.contains(10, 'b', 7));
    REQUIRE(!aut.delta.contains(10, 'c', 7));
    REQUIRE(aut.delta.contains(7, 'a', 5));
    REQUIRE(aut.delta.contains(7, 'a', 3));
    REQUIRE(!aut.delta.contains(7, 'c', 3));
    REQUIRE(aut.delta.contains(7, 'b', 9));
    REQUIRE(aut.delta.contains(7, 'a', 7));
    REQUIRE(aut.delta.contains(5, 'a', 5));
    REQUIRE(!aut.delta.contains(5, 'c', 9));
    REQUIRE(aut.delta.contains(5, 'a', 9));
}

TEST_CASE("Profile mata::nfa::remove_epsilon()", "[.profiling]")
{
    for (size_t n{}; n < 100000; ++n) {
        Nfa aut{20};
        FILL_WITH_AUT_A(aut);
        aut.remove_epsilon('c');
    }
}

TEST_CASE("mata::nfa::get_num_of_trans()")
{
    Nfa aut{20};
    FILL_WITH_AUT_A(aut);
    REQUIRE(aut.delta.num_of_transitions() == 15);
}

TEST_CASE("mata::nfa::get_one_letter_aut()")
{
    Nfa aut(11);
    Symbol abstract_symbol{'x'};
    FILL_WITH_AUT_A(aut);

    Nfa digraph{aut.get_one_letter_aut() };

    REQUIRE(digraph.num_of_states() == aut.num_of_states());
    REQUIRE(digraph.delta.num_of_transitions() == 12);
    REQUIRE(digraph.delta.contains(1, abstract_symbol, 10));
    REQUIRE(digraph.delta.contains(10, abstract_symbol, 7));
    REQUIRE(!digraph.delta.contains(10, 'a', 7));
    REQUIRE(!digraph.delta.contains(10, 'b', 7));
    REQUIRE(!digraph.delta.contains(10, 'c', 7));
}

TEST_CASE("mata::nfa::get_reachable_states()") {
    Nfa aut{20};

    SECTION("Automaton A") {
        FILL_WITH_AUT_A(aut);
        aut.delta.remove(3, 'b', 9);
        aut.delta.remove(5, 'c', 9);
        aut.delta.remove(1, 'a', 10);

        StateSet reachable{ aut.get_reachable_states() };
        CHECK(!reachable.contains(0));
        CHECK(reachable.contains(1));
        CHECK(!reachable.contains(2));
        CHECK(reachable.contains(3));
        CHECK(!reachable.contains(4));
        CHECK(reachable.contains(5));
        CHECK(!reachable.contains(6));
        CHECK(reachable.contains(7));
        CHECK(!reachable.contains(8));
        CHECK(!reachable.contains(9));
        CHECK(!reachable.contains(10));

        aut.initial.erase(1);
        aut.initial.erase(3);
        reachable = aut.get_reachable_states();
        CHECK(reachable.empty());
    }

    SECTION("Automaton B")
    {
        FILL_WITH_AUT_B(aut);
        aut.delta.remove(2, 'c', 12);
        aut.delta.remove(4, 'c', 8);
        aut.delta.remove(4, 'a', 8);

        auto reachable{aut.get_reachable_states()};
        CHECK(reachable.find(0) != reachable.end());
        CHECK(reachable.find(1) == reachable.end());
        CHECK(reachable.find(2) != reachable.end());
        CHECK(reachable.find(3) == reachable.end());
        CHECK(reachable.find(4) != reachable.end());
        CHECK(reachable.find(5) == reachable.end());
        CHECK(reachable.find(6) != reachable.end());
        CHECK(reachable.find(7) == reachable.end());
        CHECK(reachable.find(8) == reachable.end());
        CHECK(reachable.find(9) == reachable.end());
        CHECK(reachable.find(10) == reachable.end());
        CHECK(reachable.find(11) == reachable.end());
        CHECK(reachable.find(12) == reachable.end());
        CHECK(reachable.find(13) == reachable.end());
        CHECK(reachable.find(14) == reachable.end());

        aut.final.erase(2);
        reachable = aut.get_reachable_states();
        CHECK(reachable.size() == 4);
        CHECK(reachable.find(0) != reachable.end());
        CHECK(reachable.find(2) != reachable.end());
        CHECK(reachable.find(4) != reachable.end());
        CHECK(reachable.find(6) != reachable.end());
        CHECK(aut.get_useful_states().count() == 0);

        aut.final.insert(4);
        reachable = aut.get_reachable_states();
        CHECK(reachable.find(4) != reachable.end());
    }
}

TEST_CASE("mata::nfa::trim() for profiling", "[.profiling],[trim]")
{
    Nfa aut{20};
    FILL_WITH_AUT_A(aut);
    aut.delta.remove(1, 'a', 10);

    for (size_t i{ 0 }; i < 10000; ++i) {
        Nfa new_aut{ aut };
        new_aut.trim();
    }
}

//TODO: make this a test for the new version
TEST_CASE("mata::nfa::get_useful_states() for profiling", "[.profiling],[useful_states]")
{
    Nfa aut{20};
    FILL_WITH_AUT_A(aut);
    aut.delta.remove(1, 'a', 10);

    for (size_t i{ 0 }; i < 10000; ++i) {
        aut.get_useful_states();
    }
}

TEST_CASE("mata::nfa::trim() trivial") {
    Nfa aut{1};
    aut.initial.insert(0);
    aut.final.insert(0);
    aut.trim();
}

TEST_CASE("mata::nfa::trim()")
{
    Nfa orig_aut{20};
    FILL_WITH_AUT_A(orig_aut);
    orig_aut.delta.remove(1, 'a', 10);


    SECTION("Without state map") {
        Nfa aut{orig_aut};
        aut.trim();
        CHECK(aut.initial.size() == orig_aut.initial.size());
        CHECK(aut.final.size() == orig_aut.final.size());
        CHECK(aut.num_of_states() == 4);
        for (const Word& word: get_shortest_words(orig_aut))
        {
            CHECK(aut.is_in_lang(Run{word,{}}));
        }

        aut.final.erase(2); // '2' is the new final state in the earlier trimmed automaton.
        aut.trim();
        CHECK(aut.delta.empty());
        CHECK(aut.num_of_states() == 0);
    }

    SECTION("With state map") {
        Nfa aut{orig_aut};
        StateRenaming state_map{};
        aut.trim(&state_map);
        CHECK(aut.initial.size() == orig_aut.initial.size());
        CHECK(aut.final.size() == orig_aut.final.size());
        CHECK(aut.num_of_states() == 4);
        for (const Word& word: get_shortest_words(orig_aut))
        {
            CHECK(aut.is_in_lang(Run{word,{}}));
        }
        REQUIRE(state_map.size() == 4);
        CHECK(state_map.at(1) == 0);
        CHECK(state_map.at(3) == 1);
        CHECK(state_map.at(7) == 3);
        CHECK(state_map.at(5) == 2);

        aut.final.erase(2); // '2' is the new final state in the earlier trimmed automaton.
        aut.trim(&state_map);
        CHECK(aut.delta.empty());
        CHECK(aut.num_of_states() == 0);
        CHECK(state_map.empty());
    }
}

TEST_CASE("mata::nfa::Nfa::delta.empty()")
{
    Nfa aut{};

    SECTION("Empty automaton")
    {
        CHECK(aut.delta.empty());
    }

    SECTION("No transitions automaton")
    {
        aut.add_state();
        CHECK(aut.delta.empty());
    }

    SECTION("Single state automaton with no transitions")
    {
        aut.add_state();
        aut.initial.insert(0);
        aut.final.insert(0);
        CHECK(aut.delta.empty());
    }

    SECTION("Single state automaton with transitions")
    {
        aut.add_state();
        aut.initial.insert(0);
        aut.final.insert(0);
        aut.delta.add(0, 'a', 0);
        CHECK(!aut.delta.empty());
    }

    SECTION("Single state automaton with transitions")
    {
        aut.add_state(1);
        aut.initial.insert(0);
        aut.final.insert(1);
        CHECK(aut.delta.empty());
    }

    SECTION("Single state automaton with transitions")
    {
        aut.add_state(1);
        aut.initial.insert(0);
        aut.final.insert(1);
        aut.delta.add(0, 'a', 1);
        CHECK(!aut.delta.empty());
    }
}

TEST_CASE("mata::nfa::delta.operator[]")
{
    Nfa aut{20};
    FILL_WITH_AUT_A(aut);
    REQUIRE(aut.delta.num_of_transitions() == 15);
    aut.delta[25];
    REQUIRE(aut.num_of_states() == 20);

    aut.delta.mutable_state_post(25);
    REQUIRE(aut.num_of_states() == 26);
    REQUIRE(aut.delta[25].empty());

    aut.delta.mutable_state_post(50);
    REQUIRE(aut.num_of_states() == 51);
    REQUIRE(aut.delta[50].empty());

    Nfa aut1 = aut;
    aut1.delta.mutable_state_post(60);
    REQUIRE(aut1.num_of_states() == 61);
    REQUIRE(aut1.delta[60].empty());

    const Nfa aut2 = aut;
    aut2.delta[60];
    REQUIRE(aut2.num_of_states() == 51);
    REQUIRE(aut2.delta[60].empty());
}

TEST_CASE("mata::nfa::Nfa::unify_(initial/final)()") {
    Nfa nfa{10};

    SECTION("No initial") {
        nfa.unify_initial();
        CHECK(nfa.num_of_states() == 10);
        CHECK(nfa.initial.empty());
    }

    SECTION("initial==final unify final") {
        nfa.initial.insert(0);
        nfa.final.insert(0);
        nfa.final.insert(1);
        nfa.unify_final();
        REQUIRE(nfa.num_of_states() == 11);
        CHECK(nfa.final.size() == 1);
        CHECK(nfa.final[10]);
        CHECK(nfa.initial[10]);
    }

    SECTION("initial==final unify initial") {
        nfa.initial.insert(0);
        nfa.initial.insert(1);
        nfa.final.insert(0);
        nfa.unify_initial();
        REQUIRE(nfa.num_of_states() == 11);
        CHECK(nfa.initial.size() == 1);
        CHECK(nfa.initial[10]);
        CHECK(nfa.final[10]);
    }

    SECTION("Single initial") {
        nfa.initial.insert(0);
        nfa.unify_initial();
        CHECK(nfa.num_of_states() == 10);
        CHECK(nfa.initial.size() == 1);
        CHECK(nfa.initial[0]);
    }

    SECTION("Multiple initial") {
        nfa.initial.insert(0);
        nfa.initial.insert(1);
        nfa.unify_initial();
        CHECK(nfa.num_of_states() == 11);
        CHECK(nfa.initial.size() == 1);
        CHECK(nfa.initial[10]);
    }

    SECTION("With transitions") {
        nfa.initial.insert(0);
        nfa.initial.insert(1);
        nfa.delta.add(0, 'a', 3);
        nfa.delta.add(1, 'b', 0);
        nfa.delta.add(1, 'c', 1);
        nfa.unify_initial();
        CHECK(nfa.num_of_states() == 11);
        CHECK(nfa.initial.size() == 1);
        CHECK(nfa.initial[10]);
        CHECK(nfa.delta.contains(10, 'a', 3));
        CHECK(nfa.delta.contains(10, 'b', 0));
        CHECK(nfa.delta.contains(10, 'c', 1));
        CHECK(nfa.delta.contains(0, 'a', 3));
        CHECK(nfa.delta.contains(1, 'b', 0));
        CHECK(nfa.delta.contains(1, 'c', 1));
    }

    SECTION("No final") {
        nfa.unify_final();
        CHECK(nfa.num_of_states() == 10);
        CHECK(nfa.final.empty());
    }

    SECTION("Single final") {
        nfa.final.insert(0);
        nfa.unify_final();
        CHECK(nfa.num_of_states() == 10);
        CHECK(nfa.final.size() == 1);
        CHECK(nfa.final[0]);
    }

    SECTION("Multiple final") {
        nfa.final.insert(0);
        nfa.final.insert(1);
        nfa.unify_final();
        CHECK(nfa.num_of_states() == 11);
        CHECK(nfa.final.size() == 1);
        CHECK(nfa.final[10]);
    }

    SECTION("With transitions") {
        nfa.final.insert(0);
        nfa.final.insert(1);
        nfa.delta.add(3, 'a', 0);
        nfa.delta.add(4, 'b', 1);
        nfa.delta.add(1, 'c', 1);
        nfa.unify_final();
        CHECK(nfa.num_of_states() == 11);
        CHECK(nfa.final.size() == 1);
        CHECK(nfa.final[10]);
        CHECK(nfa.delta.contains(3, 'a', 10));
        CHECK(nfa.delta.contains(4, 'b', 10));
        CHECK(nfa.delta.contains(1, 'c', 10));
        CHECK(nfa.delta.contains(3, 'a', 0));
        CHECK(nfa.delta.contains(4, 'b', 1));
        CHECK(nfa.delta.contains(1, 'c', 1));
    }

    SECTION("Bug: NFA with empty string unifying initial/final repeatedly") {
        Nfa aut;
        mata::parser::create_nfa(&aut, "a*b*");
        for (size_t i{ 0 }; i < 8; ++i) {
            aut.unify_initial();
            aut.unify_final();
        }
        CHECK(true); // Check that the program does not seg fault.
    }
}

TEST_CASE("mata::nfa::Nfa::get_delta.epsilon_symbol_posts()") {
    Nfa aut{20};
    FILL_WITH_AUT_A(aut);
    aut.delta.add(0, EPSILON, 3);
    aut.delta.add(3, EPSILON, 3);
    aut.delta.add(3, EPSILON, 4);

    auto state_eps_trans{ aut.delta.epsilon_symbol_posts(0) };
    CHECK(state_eps_trans->symbol == EPSILON);
    CHECK(state_eps_trans->targets == StateSet{3 });
    state_eps_trans = aut.delta.epsilon_symbol_posts(3);
    CHECK(state_eps_trans->symbol == EPSILON);
    CHECK(state_eps_trans->targets == StateSet{3, 4 });

    aut.delta.add(8, 42, 3);
    aut.delta.add(8, 42, 4);
    aut.delta.add(8, 42, 6);

    state_eps_trans = aut.delta.epsilon_symbol_posts(8, 42);
    CHECK(state_eps_trans->symbol == 42);
    CHECK(state_eps_trans->targets == StateSet{3, 4, 6 });

    CHECK(aut.delta.epsilon_symbol_posts(1) == aut.delta.state_post(1).end());
    CHECK(aut.delta.epsilon_symbol_posts(5) == aut.delta.state_post(5).end());
    CHECK(aut.delta.epsilon_symbol_posts(19) == aut.delta.state_post(19).end());

    StatePost state_post{ aut.delta[0] };
    state_eps_trans = aut.delta.epsilon_symbol_posts(state_post);
    CHECK(state_eps_trans->symbol == EPSILON);
    CHECK(state_eps_trans->targets == StateSet{3 });
    state_post = aut.delta[3];
    state_eps_trans = Delta::epsilon_symbol_posts(state_post);
    CHECK(state_eps_trans->symbol == EPSILON);
    CHECK(state_eps_trans->targets == StateSet{3, 4 });

    state_post = aut.delta.state_post(1);
    CHECK(aut.delta.epsilon_symbol_posts(state_post) == state_post.end());
    state_post = aut.delta.state_post(5);
    CHECK(aut.delta.epsilon_symbol_posts(state_post) == state_post.end());
    state_post = aut.delta.state_post(19);
    CHECK(aut.delta.epsilon_symbol_posts(state_post) == state_post.end());
}

TEST_CASE("mata::nfa::Nfa::delta()") {
    Delta delta(6);
}

TEST_CASE("A segmentation fault in the make_complement") {
    Nfa r(1);
    OnTheFlyAlphabet alph{};
    alph["a"];
    alph["b"];

    r.initial = {0};
    r.delta.add(0, 0, 0);
    REQUIRE(not r.is_complete(&alph));
    r.make_complete(&alph, 1);
    REQUIRE(r.is_complete(&alph));
}

TEST_CASE("mata::nfa:: create simple automata") {
    Nfa nfa{ builder::create_empty_string_nfa() };
    CHECK(nfa.is_in_lang(Word{}));
    CHECK(get_word_lengths(nfa) == std::set<std::pair<int, int>>{ std::make_pair(0, 0) });

    OnTheFlyAlphabet alphabet{ { "a", 0 }, { "b", 1 }, { "c", 2 } };
    nfa = builder::create_sigma_star_nfa(&alphabet);
    CHECK(nfa.is_in_lang({ {}, {} }));
    CHECK(nfa.is_in_lang({  0 , {} }));
    CHECK(nfa.is_in_lang({  1 , {} }));
    CHECK(nfa.is_in_lang({  2 , {} }));
    CHECK(nfa.is_in_lang({ { 0, 1 }, {} }));
    CHECK(nfa.is_in_lang({ { 1, 0 }, {} }));
    CHECK(nfa.is_in_lang({ { 2, 2, 2 }, {} }));
    CHECK(nfa.is_in_lang({ { 0, 1, 2, 2, 0, 1, 2, 1, 0, 0, 2, 1 }, {} }));
    CHECK(!nfa.is_in_lang({  3 , {} }));
}

TEST_CASE("mata::nfa:: print_to_mata") {
    Nfa aut_big;
    aut_big.initial = {1, 2};
    aut_big.delta.add(1, 'a', 2);
    aut_big.delta.add(1, 'a', 3);
    aut_big.delta.add(1, 'b', 4);
    aut_big.delta.add(2, 'a', 2);
    aut_big.delta.add(2, 'b', 2);
    aut_big.delta.add(2, 'a', 3);
    aut_big.delta.add(2, 'b', 4);
    aut_big.delta.add(3, 'b', 4);
    aut_big.delta.add(3, 'c', 7);
    aut_big.delta.add(3, 'b', 2);
    aut_big.delta.add(5, 'c', 3);
    aut_big.delta.add(7, 'a', 8);
    aut_big.final = {3};

    std::string aut_big_mata = aut_big.print_to_mata();
    // for parsing output of print_to_mata() we need to use IntAlphabet to get the same alphabet
    IntAlphabet int_alph;
    Nfa aut_big_from_mata = builder::construct(mata::IntermediateAut::parse_from_mf(parse_mf(aut_big_mata))[0], &int_alph);

    CHECK(are_equivalent(aut_big, aut_big_from_mata));
}

TEST_CASE("mata::nfa::Nfa::trim() bug") {
	Nfa aut(5, {0}, {4});
	aut.delta.add(0, 122, 1);
	aut.delta.add(1, 98, 1);
	aut.delta.add(1, 122, 1);
	aut.delta.add(1, 97, 2);
	aut.delta.add(2, 122, 1);
	aut.delta.add(2, 97, 1);
	aut.delta.add(1, 97, 4);
	aut.delta.add(3, 97, 4);

	Nfa aut_copy {aut};
	CHECK(are_equivalent(aut_copy.trim(), aut));
}

TEST_CASE("mata::nfa::get_useful_states_tarjan") {
	SECTION("Nfa 1") {
		Nfa aut(5, {0}, {4});
		aut.delta.add(0, 122, 1);
		aut.delta.add(1, 98, 1);
		aut.delta.add(1, 122, 1);
		aut.delta.add(1, 97, 2);
		aut.delta.add(2, 122, 1);
		aut.delta.add(2, 97, 1);
		aut.delta.add(1, 97, 4);
		aut.delta.add(3, 97, 4);

		mata::BoolVector bv = aut.get_useful_states();
		mata::BoolVector ref({ 1, 1, 1, 0, 1});
		CHECK(bv == ref);
	}

	SECTION("Empty NFA") {
		Nfa aut;
		mata::BoolVector bv = aut.get_useful_states();
		CHECK(bv == mata::BoolVector({}));
	}

	SECTION("Single-state NFA") {
		Nfa aut(1, {0}, {});
		mata::BoolVector bv = aut.get_useful_states();
		CHECK(bv == mata::BoolVector({ 0}));
	}

	SECTION("Single-state NFA acc") {
		Nfa aut(1, {0}, {0});
		mata::BoolVector bv = aut.get_useful_states();
		CHECK(bv == mata::BoolVector({ 1}));
	}

	SECTION("Nfa 2") {
		Nfa aut(5, {0, 1}, {2});
		aut.delta.add(0, 122, 2);
		aut.delta.add(2, 98, 3);
		aut.delta.add(1, 98, 4);
		aut.delta.add(4, 97, 3);

		mata::BoolVector bv = aut.get_useful_states();
		mata::BoolVector ref({ 1, 0, 1, 0, 0});
		CHECK(bv == ref);
	}

	SECTION("Nfa 3") {
		Nfa aut(2, {0, 1}, {0, 1});
		aut.delta.add(0, 122, 0);
		aut.delta.add(1, 98, 1);

		mata::BoolVector bv = aut.get_useful_states();
		mata::BoolVector ref({ 1, 1});
		CHECK(bv == ref);
	}

	SECTION("Nfa no final") {
		Nfa aut(5, {0}, {});
		aut.delta.add(0, 122, 1);
		aut.delta.add(1, 98, 1);
		aut.delta.add(1, 122, 1);
		aut.delta.add(1, 97, 2);
		aut.delta.add(2, 122, 1);
		aut.delta.add(2, 97, 1);
		aut.delta.add(1, 97, 4);
		aut.delta.add(3, 97, 4);

		mata::BoolVector bv = aut.get_useful_states();
		mata::BoolVector ref({ 0, 0, 0, 0, 0});
		CHECK(bv == ref);
	}

    SECTION("from regex (a+b*a*)") {
        Nfa aut;
        mata::parser::create_nfa(&aut, "(a+b*a*)", false, EPSILON, false);

        mata::BoolVector bv = aut.get_useful_states();
        mata::BoolVector ref({ 1, 0, 1, 0, 1, 0, 1, 0, 0});
        CHECK(bv == ref);

        aut = reduce(aut.trim());
        bv = aut.get_useful_states();
        CHECK(bv == mata::BoolVector({ 1, 1, 1, 1}));
    }

    SECTION("more initials") {
        Nfa aut(4, {0, 1, 2}, {0, 3});
        aut.delta.add(1, 48, 0);
        aut.delta.add(2, 53, 3);
        CHECK(aut.get_useful_states() == mata::BoolVector{ 1, 1, 1, 1});
    }
}

TEST_CASE("mata::nfa::Nfa::get_words") {
    SECTION("empty") {
        Nfa aut;
        CHECK(aut.get_words(0) == std::set<mata::Word>());
        CHECK(aut.get_words(1) == std::set<mata::Word>());
        CHECK(aut.get_words(5) == std::set<mata::Word>());
    }

    SECTION("empty word") {
        Nfa aut(1, {0}, {0});
        CHECK(aut.get_words(0) == std::set<mata::Word>{{}});
        CHECK(aut.get_words(1) == std::set<mata::Word>{{}});
        CHECK(aut.get_words(5) == std::set<mata::Word>{{}});
    }

    SECTION("noodle - one final") {
        Nfa aut(3, {0}, {2});
        aut.delta.add(0, 0, 1);
        aut.delta.add(1, 1, 2);
        CHECK(aut.get_words(0) == std::set<mata::Word>{});
        CHECK(aut.get_words(1) == std::set<mata::Word>{});
        CHECK(aut.get_words(2) == std::set<mata::Word>{{0, 1}});
        CHECK(aut.get_words(3) == std::set<mata::Word>{{0, 1}});
        CHECK(aut.get_words(5) == std::set<mata::Word>{{0, 1}});
    }

    SECTION("noodle - two finals") {
        Nfa aut(3, {0}, {1,2});
        aut.delta.add(0, 0, 1);
        aut.delta.add(1, 1, 2);
        CHECK(aut.get_words(0) == std::set<mata::Word>{});
        CHECK(aut.get_words(1) == std::set<mata::Word>{{0}});
        CHECK(aut.get_words(2) == std::set<mata::Word>{{0}, {0, 1}});
        CHECK(aut.get_words(3) == std::set<mata::Word>{{0}, {0, 1}});
        CHECK(aut.get_words(5) == std::set<mata::Word>{{0}, {0, 1}});
    }

    SECTION("noodle - three finals") {
        Nfa aut(3, {0}, {0,1,2});
        aut.delta.add(0, 0, 1);
        aut.delta.add(1, 1, 2);
        CHECK(aut.get_words(0) == std::set<mata::Word>{{}});
        CHECK(aut.get_words(1) == std::set<mata::Word>{{}, {0}});
        CHECK(aut.get_words(2) == std::set<mata::Word>{{}, {0}, {0, 1}});
        CHECK(aut.get_words(3) == std::set<mata::Word>{{}, {0}, {0, 1}});
        CHECK(aut.get_words(5) == std::set<mata::Word>{{}, {0}, {0, 1}});
    }

    SECTION("more complex") {
        Nfa aut(6, {0,1}, {1,3,4,5});
        aut.delta.add(0, 0, 3);
        aut.delta.add(3, 1, 4);
        aut.delta.add(0, 2, 2);
        aut.delta.add(3, 3, 2);
        aut.delta.add(1, 4, 2);
        aut.delta.add(2, 5, 5);
        CHECK(aut.get_words(0) == std::set<mata::Word>{{}});
        CHECK(aut.get_words(1) == std::set<mata::Word>{{}, {0}});
        CHECK(aut.get_words(2) == std::set<mata::Word>{{}, {0}, {0, 1}, {2,5}, {4,5}});
        CHECK(aut.get_words(3) == std::set<mata::Word>{{}, {0}, {0, 1}, {2,5}, {4,5}, {0,3,5}});
        CHECK(aut.get_words(4) == std::set<mata::Word>{{}, {0}, {0, 1}, {2,5}, {4,5}, {0,3,5}});
        CHECK(aut.get_words(5) == std::set<mata::Word>{{}, {0}, {0, 1}, {2,5}, {4,5}, {0,3,5}});
    }

    SECTION("cycle") {
        Nfa aut(6, {0,1}, {0,1});
        aut.delta.add(0, 0, 1);
        aut.delta.add(1, 1, 0);
        CHECK(aut.get_words(0) == std::set<mata::Word>{{}});
        CHECK(aut.get_words(1) == std::set<mata::Word>{{}, {0}, {1}});
        CHECK(aut.get_words(2) == std::set<mata::Word>{{}, {0}, {1}, {0, 1}, {1, 0}});
        CHECK(aut.get_words(3) == std::set<mata::Word>{{}, {0}, {1}, {0, 1}, {1, 0}, {0,1,0}, {1,0,1}});
        CHECK(aut.get_words(4) == std::set<mata::Word>{{}, {0}, {1}, {0, 1}, {1, 0}, {0,1,0}, {1,0,1}, {0,1,0,1}, {1,0,1,0}});
        CHECK(aut.get_words(5) == std::set<mata::Word>{{}, {0}, {1}, {0, 1}, {1, 0}, {0,1,0}, {1,0,1}, {0,1,0,1}, {1,0,1,0}, {0,1,0,1,0}, {1,0,1,0,1}});
    }
}

TEST_CASE("mata::nfa::Nfa::get_word()") {
   SECTION("empty") {
       Nfa aut;
       CHECK(aut.get_word(0) == std::nullopt);
   }

   SECTION("empty word") {
       Nfa aut(1, { 0 }, { 0 });
       CHECK(aut.get_word() == Word{});
   }

   SECTION("noodle - one final") {
       Nfa aut(3, { 0 }, { 2 });
       aut.delta.add(0, 0, 1);
       aut.delta.add(1, 1, 2);
       CHECK(aut.get_word() == Word{ 0, 1 });
   }

   SECTION("noodle - two finals") {
       Nfa aut(3, { 0 }, { 1, 2 });
       aut.delta.add(0, 0, 1);
       aut.delta.add(1, 1, 2);
       CHECK(aut.get_word() == Word{ 0 });
   }

   SECTION("noodle - three finals") {
       Nfa aut(3, { 0 }, { 0, 1, 2 });
       aut.delta.add(0, 0, 1);
       aut.delta.add(1, 1, 2);
       CHECK(aut.get_word() == Word{});
   }

   SECTION("more complex initial final") {
       Nfa aut(6, { 0, 1 }, { 1, 3, 4, 5 });
       aut.delta.add(0, 0, 3);
       aut.delta.add(3, 1, 4);
       aut.delta.add(0, 2, 2);
       aut.delta.add(3, 3, 2);
       aut.delta.add(1, 4, 2);
       aut.delta.add(2, 5, 5);
       CHECK(aut.get_word() == Word{});
   }

   SECTION("more complex") {
       Nfa aut(6, { 0, 1 }, { 5 });
       aut.delta.add(0, 0, 3);
       aut.delta.add(3, 1, 4);
       aut.delta.add(0, 2, 2);
       aut.delta.add(3, 3, 2);
       aut.delta.add(1, 4, 2);
       aut.delta.add(2, 5, 5);
       CHECK(aut.get_word() == Word{ 0, 3, 5 });
   }

   SECTION("cycle") {
       Nfa aut(6, { 0, 2 }, { 4 });
       aut.delta.add(2, 2, 3);
       aut.delta.add(3, 3, 2);
       aut.delta.add(0, 0, 1);
       aut.delta.add(1, 1, 4);
       CHECK(aut.get_word() == Word{ 0, 1 });
   }

   SECTION("epsilons") {
       Nfa aut(6, { 0, 2 }, { 4 });
       aut.delta.add(2, 2, 3);
       aut.delta.add(3, 3, 2);
       aut.delta.add(0, EPSILON, 1);
       aut.delta.add(1, 1, 4);
       CHECK(aut.get_word() == Word{ 1 });
   }

    SECTION("Complex automaton with self loops, epsilons, and nonterminating states") {
        Nfa aut{};
        aut.initial = { 0 };
        aut.final = { 4 };
        aut.delta.add(0, 'a', 0);
        aut.delta.add(0, 'b', 1);
        aut.delta.add(1, 'b', 1);
        aut.delta.add(0, 'b', 2);
        aut.delta.add(2, EPSILON, 3);
        aut.delta.add(2, EPSILON, 1);
        aut.delta.add(2, 'a', 0);
        aut.delta.add(2, 'a', 2);
        aut.delta.add(3, 'a', 5);
        aut.delta.add(3, 'c', 4);
        aut.delta.add(4, 'a', 4);
        CHECK(aut.get_word() == Word{ 'b', 'c' });
    }

    SECTION("Complex automaton with self loops, epsilons, and nonterminating states, one separate final") {
        Nfa aut{};
        aut.initial = { 0, 6 };
        aut.final = { 7 };
        aut.delta.add(6, 'd', 7);
        aut.delta.add(0, 'a', 0);
        aut.delta.add(0, 'b', 1);
        aut.delta.add(1, 'b', 1);
        aut.delta.add(0, 'b', 2);
        aut.delta.add(2, EPSILON, 3);
        aut.delta.add(2, EPSILON, 1);
        aut.delta.add(2, 'a', 0);
        aut.delta.add(2, 'a', 2);
        aut.delta.add(3, 'a', 5);
        aut.delta.add(3, 'c', 4);
        aut.delta.add(4, 'a', 4);
        CHECK(aut.get_word() == Word{ 'd' });
    }

    SECTION("Break after finding the first final without iterating over other initial states") {
        Nfa aut{};
        aut.initial = { 0, 1 };
        aut.final = { 4, 5 };
        aut.delta.add(0, 'a', 2);
        aut.delta.add(2, 'c', 4);
        aut.delta.add(1, 'd', 3);
        aut.delta.add(3, 'd', 5);
        CHECK(aut.get_word() == Word{ 'a', 'c' });
   }

    SECTION("Complex automaton with self loops, epsilons, and nonterminating states, no reachable final") {
        Nfa aut{};
        aut.initial = { 0 };
        aut.final = { 6 };
        aut.delta.add(0, 'a', 0);
        aut.delta.add(0, 'b', 1);
        aut.delta.add(1, 'b', 1);
        aut.delta.add(0, 'b', 2);
        aut.delta.add(2, EPSILON, 3);
        aut.delta.add(2, EPSILON, 1);
        aut.delta.add(2, 'a', 0);
        aut.delta.add(2, 'a', 2);
        aut.delta.add(3, 'a', 5);
        aut.delta.add(3, 'c', 4);
        aut.delta.add(4, 'a', 4);
        CHECK(!aut.get_word().has_value());
    }

    SECTION("Complex automaton with self loops, epsilons, and nonterminating states, first initial nonterminating") {
        Nfa aut{};
        aut.initial = { 1, 2 };
        aut.final = { 4 };
        aut.delta.add(0, 'a', 0);
        aut.delta.add(0, 'b', 1);
        aut.delta.add(1, 'b', 1);
        aut.delta.add(0, 'b', 2);
        aut.delta.add(2, EPSILON, 3);
        aut.delta.add(2, EPSILON, 1);
        aut.delta.add(2, 'a', 0);
        aut.delta.add(2, 'a', 2);
        aut.delta.add(3, 'a', 5);
        aut.delta.add(3, 'c', 4);
        aut.delta.add(4, 'a', 4);
        CHECK(aut.get_word() == Word{ 'c' });
    }

    SECTION("Complex automaton with self loops, epsilons, and nonterminating states, long nonterminating sequence") {
        Nfa aut{};
        aut.initial = { 0 };
        aut.final = { 2 };
        aut.delta.add(0, 'a', 0);
        aut.delta.add(0, 'b', 2);
        aut.delta.add(1, 'b', 1);
        aut.delta.add(0, 'b', 1);
        aut.delta.add(1, EPSILON, 3);
        aut.delta.add(1, 'a', 0);
        aut.delta.add(1, 'a', 1);
        aut.delta.add(3, 'a', 5);
        aut.delta.add(3, 'c', 4);
        aut.delta.add(4, 'a', 4);
        CHECK(aut.get_word() == Word{ 'b' });
    }
}
