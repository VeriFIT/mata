// TODO: some header

#include "nfa-util.hh"

#include "mata/alphabet.hh"
#include "mata/nfa/types.hh"
#include "mata/nfa/delta.hh"
#include "mata/nfa/nfa.hh"

#include "../3rdparty/catch.hpp"

using namespace Mata::Nfa;

using Symbol = Mata::Symbol;

TEST_CASE("Mata::Nfa::SymbolPost") {
    CHECK(SymbolPost{ 0, StateSet{} } == SymbolPost{ 0, StateSet{ 0, 1 } });
    CHECK(SymbolPost{ 1, StateSet{} } != SymbolPost{ 0, StateSet{} });
    CHECK(SymbolPost{ 0, StateSet{ 1 } } < SymbolPost{ 1, StateSet{} });
    CHECK(SymbolPost{ 0, StateSet{ 1 } } <= SymbolPost{ 1, StateSet{} });
    CHECK(SymbolPost{ 0, StateSet{ 1 } } <= SymbolPost{ 0, StateSet{} });
    CHECK(SymbolPost{ 1, StateSet{ 0 } } > SymbolPost{ 0, StateSet{ 1 } });
    CHECK(SymbolPost{ 1, StateSet{ 0 } } >= SymbolPost{ 0, StateSet{ 1 } });
    CHECK(SymbolPost{ 1, StateSet{ 0 } } >= SymbolPost{ 0, StateSet{ 1 } });
}

TEST_CASE("Mata::Nfa::Delta::state_post()") {
    Mata::Nfa::Nfa aut{};

    SECTION("Add new states within the limit") {
        aut.add_state(19);
        aut.initial.insert(0);
        aut.initial.insert(1);
        aut.initial.insert(2);
        REQUIRE_NOTHROW(aut.delta.state_post(0));
        REQUIRE_NOTHROW(aut.delta.state_post(1));
        REQUIRE_NOTHROW(aut.delta.state_post(2));
        REQUIRE(aut.delta.state_post(0).empty());
        REQUIRE(aut.delta.state_post(1).empty());
        REQUIRE(aut.delta.state_post(2).empty());

        CHECK(&aut.delta.state_post(4) == &aut.delta[4]);
    }

    SECTION("Add new states over the limit") {
        aut.add_state(1);
        REQUIRE_NOTHROW(aut.initial.insert(0));
        REQUIRE_NOTHROW(aut.initial.insert(1));
        REQUIRE_NOTHROW(aut.delta.state_post(0));
        REQUIRE_NOTHROW(aut.delta.state_post(1));
        REQUIRE_NOTHROW(aut.delta.state_post(2));
        CHECK(aut.delta.state_post(0).empty());
        CHECK(aut.delta.state_post(1).empty());
        CHECK(aut.delta.state_post(2).empty());
    }

    SECTION("Add new states without specifying the number of states") {
        CHECK_NOTHROW(aut.initial.insert(0));
        CHECK_NOTHROW(aut.delta.state_post(2));
        CHECK(aut.delta.state_post(0).empty());
        CHECK(aut.delta.state_post(2).empty());
    }

    SECTION("Add new initial without specifying the number of states with over +1 number") {
        REQUIRE_NOTHROW(aut.initial.insert(25));
        CHECK_NOTHROW(aut.delta.state_post(25));
        CHECK_NOTHROW(aut.delta.state_post(26));
        CHECK(aut.delta.state_post(25).empty());
        CHECK(aut.delta.state_post(26).empty());
    }

    SECTION("Add multiple targets at once") {
        CHECK_NOTHROW(aut.delta.add(0, 1, { 3, 4, 5, 6 }));
        CHECK_NOTHROW(aut.delta.add(26, 1, StateSet{}));
        CHECK_NOTHROW(aut.delta.add(42, 1, StateSet{ 43 }));
        CHECK(aut.get_num_of_trans() == 5);
    }
}

TEST_CASE("Mata::Nfa::Delta::contains()") {
    Nfa nfa;
    CHECK(!nfa.delta.contains(0, 1, 0));
    CHECK(!nfa.delta.contains(Transition{ 0, 1, 0 }));
    nfa.delta.add(0, 1, 0);
    CHECK(nfa.delta.contains(0, 1, 0));
    CHECK(nfa.delta.contains(Transition{ 0, 1, 0 }));
}

TEST_CASE("Mata::Nfa::Delta::remove()") {
    Nfa nfa;

    SECTION("Simple remove") {
        nfa.delta.add(0, 1, 0);
        CHECK_NOTHROW(nfa.delta.remove(3, 5, 6));
        CHECK_NOTHROW(nfa.delta.remove(0, 1, 0));
        CHECK(nfa.delta.empty());
        nfa.delta.add(10, 1, 0);
        CHECK_THROWS_AS(nfa.delta.remove(3, 5, 6), std::invalid_argument);
    }
}

TEST_CASE("Mata::Nfa::Delta::mutable_post()") {
    Nfa nfa;

    SECTION("Default initialized") {
        CHECK(nfa.delta.num_of_states() == 0);
        CHECK(nfa.delta.mutable_state_post(0).empty());
        CHECK(nfa.delta.num_of_states() == 1);

        CHECK(nfa.delta.mutable_state_post(9).empty());
        CHECK(nfa.delta.num_of_states() == 10);

        CHECK(nfa.delta.mutable_state_post(9).empty());
        CHECK(nfa.delta.num_of_states() == 10);
    }
}

TEST_CASE("Mata::Nfa::StatePost iteration over moves") {
    Nfa nfa;
    std::vector<Move> iterated_moves{};
    std::vector<Move> expected_moves{};
    StatePost state_post{};

    SECTION("Simple NFA") {
        nfa.initial.insert(0);
        nfa.final.insert(3);
        nfa.delta.add(0, 1, 1);
        nfa.delta.add(0, 2, 1);
        nfa.delta.add(0, 5, 1);
        nfa.delta.add(1, 3, 2);
        nfa.delta.add(2, 0, 1);
        nfa.delta.add(2, 0, 3);

        state_post = nfa.delta.state_post(0);
        expected_moves = std::vector<Move>{ { 1, 1 }, { 2, 1 }, { 5, 1 } };
        Mata::Nfa::StatePost::Moves moves{ state_post.moves() };
        iterated_moves.clear();
        for (auto move_it{ moves.begin() }; move_it != moves.end(); ++move_it) {
            iterated_moves.push_back(*move_it);
        }
        CHECK(iterated_moves == expected_moves);

        iterated_moves = { moves.begin(), moves.end() };
        CHECK(iterated_moves == expected_moves);

        iterated_moves.clear();
        for (const Move& move: state_post.moves()) { iterated_moves.push_back(move); }
        CHECK(iterated_moves == expected_moves);


        state_post = nfa.delta.state_post(1);
        moves = state_post.moves();
        iterated_moves.clear();
        for (auto move_it{ moves.begin() }; move_it != moves.end(); ++move_it) {
            iterated_moves.push_back(*move_it);
        }
        expected_moves = std::vector<Move>{ { 3, 2 } };
        CHECK(iterated_moves == expected_moves);
        iterated_moves = { moves.begin(), moves.end() };
        CHECK(iterated_moves == expected_moves);
        iterated_moves.clear();
        for (const Move& move: state_post.moves()) { iterated_moves.push_back(move); }
        CHECK(iterated_moves == expected_moves);

        state_post = nfa.delta.state_post(2);
        moves = state_post.moves();
        iterated_moves.clear();
        for (auto move_it{ moves.begin() }; move_it != moves.end(); ++move_it) {
            iterated_moves.push_back(*move_it);
        }
        expected_moves = std::vector<Move>{ { 0, 1 }, { 0, 3 } };
        CHECK(iterated_moves == expected_moves);
        iterated_moves = { moves.begin(), moves.end() };
        CHECK(iterated_moves == expected_moves);
        iterated_moves.clear();
        for (const Move& move: state_post.moves()) { iterated_moves.push_back(move); }
        CHECK(iterated_moves == expected_moves);

        state_post = nfa.delta.state_post(3);
        moves = state_post.moves();
        iterated_moves.clear();
        for (auto move_it{ moves.begin() }; move_it != moves.end(); ++move_it) {
            iterated_moves.push_back(*move_it);
        }
        CHECK(iterated_moves.empty());
        iterated_moves = { moves.begin(), moves.end() };
        CHECK(iterated_moves.empty());
        iterated_moves.clear();
        for (const Move& move: state_post.moves()) { iterated_moves.push_back(move); }
        CHECK(iterated_moves.empty());

        state_post = nfa.delta.state_post(4);
        moves = state_post.moves();
        iterated_moves.clear();
        for (auto move_it{ moves.begin() }; move_it != moves.end(); ++move_it) {
            iterated_moves.push_back(*move_it);
        }
        CHECK(iterated_moves.empty());
        iterated_moves = { moves.begin(), moves.end() };
        CHECK(iterated_moves.empty());
        iterated_moves.clear();
        for (const Move& move: state_post.moves()) { iterated_moves.push_back(move); }
        CHECK(iterated_moves.empty());
    }
}

TEST_CASE("Mata::Nfa::Delta iteration over transitions") {
    Nfa nfa;
    std::vector<Transition> iterated_transitions{};
    std::vector<Transition> expected_transitions{};

    SECTION("Simple NFA") {
        nfa.initial.insert(0);
        nfa.final.insert(3);
        nfa.delta.add(0, 1, 1);
        nfa.delta.add(0, 2, 1);
        nfa.delta.add(0, 5, 1);
        nfa.delta.add(1, 3, 2);
        nfa.delta.add(2, 0, 1);
        nfa.delta.add(2, 0, 3);

        Mata::Nfa::Delta::Transitions transitions{ nfa.delta.transitions() };
        iterated_transitions.clear();
        for (auto transitions_it{ transitions.begin() };
             transitions_it != transitions.end(); ++transitions_it) {
            iterated_transitions.push_back(*transitions_it);
        }
        expected_transitions = std::vector<Transition>{
            { 0, 1, 1 }, { 0, 2, 1 }, { 0, 5, 1 }, { 1, 3, 2 }, { 2, 0, 1 }, { 2, 0, 3 }
        };
        CHECK(iterated_transitions == expected_transitions);

        iterated_transitions = { transitions.begin(), transitions.end() };
        CHECK(iterated_transitions == expected_transitions);

        iterated_transitions.clear();
        for (const Transition& transition: nfa.delta.transitions()) { iterated_transitions.push_back(transition); }
        CHECK(iterated_transitions == expected_transitions);
    }
}
