// TODO: some header

#include "utils.hh"

#include "mata/alphabet.hh"
#include "mata/nfa/types.hh"
#include "mata/nfa/delta.hh"
#include "mata/nfa/nfa.hh"

#include <catch2/catch.hpp>

using namespace mata::nfa;

using Symbol = mata::Symbol;

TEST_CASE("Mata::nfa::SymbolPost") {
    CHECK(SymbolPost{ 0, StateSet{} } == SymbolPost{ 0, StateSet{ 0, 1 } });
    CHECK(SymbolPost{ 1, StateSet{} } != SymbolPost{ 0, StateSet{} });
    CHECK(SymbolPost{ 0, StateSet{ 1 } } < SymbolPost{ 1, StateSet{} });
    CHECK(SymbolPost{ 0, StateSet{ 1 } } <= SymbolPost{ 1, StateSet{} });
    CHECK(SymbolPost{ 0, StateSet{ 1 } } <= SymbolPost{ 0, StateSet{} });
    CHECK(SymbolPost{ 1, StateSet{ 0 } } > SymbolPost{ 0, StateSet{ 1 } });
    CHECK(SymbolPost{ 1, StateSet{ 0 } } >= SymbolPost{ 0, StateSet{ 1 } });
    CHECK(SymbolPost{ 1, StateSet{ 0 } } >= SymbolPost{ 0, StateSet{ 1 } });
}

TEST_CASE("Mata::nfa::Delta::state_post()") {
    mata::nfa::Nfa aut{};

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

TEST_CASE("Mata::nfa::Delta::contains()") {
    Nfa nfa;
    CHECK(!nfa.delta.contains(0, 1, 0));
    CHECK(!nfa.delta.contains(Transition{ 0, 1, 0 }));
    nfa.delta.add(0, 1, 0);
    CHECK(nfa.delta.contains(0, 1, 0));
    CHECK(nfa.delta.contains(Transition{ 0, 1, 0 }));
}

TEST_CASE("Mata::nfa::Delta::remove()") {
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

TEST_CASE("Mata::nfa::Delta::mutable_post()") {
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

TEST_CASE("Mata::nfa::StatePost iteration over moves") {
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
        mata::nfa::StatePost::Moves moves{ state_post.moves() };
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

TEST_CASE("Mata::nfa::Delta iteration over transitions") {
    Nfa nfa;
    std::vector<Transition> iterated_transitions{};
    std::vector<Transition> expected_transitions{};

    SECTION("empty automaton") {
        Mata::Nfa::Delta::transitions_const_iterator it = nfa.delta.transitions.begin();
        REQUIRE(it == nfa.delta.transitions.end());
    }

    SECTION("Simple NFA") {
        nfa.initial.insert(0);
        nfa.final.insert(3);
        nfa.delta.add(0, 1, 1);
        nfa.delta.add(0, 2, 1);
        nfa.delta.add(0, 5, 1);
        nfa.delta.add(1, 3, 2);
        nfa.delta.add(2, 0, 1);
        nfa.delta.add(2, 0, 3);

        mata::nfa::Delta::Transitions transitions{ nfa.delta.transitions() };
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
        for (const Transition& transition: nfa.delta.transitions) { iterated_transitions.push_back(transition); }
        CHECK(iterated_transitions == expected_transitions);
    }

    SECTION("Sparse automaton") {
        const size_t state_num = 'r'+1;
        nfa.delta.increase_size(state_num);

        nfa.delta.add('q', 'a', 'r');
        nfa.delta.add('q', 'b', 'r');
        Delta::transitions_const_iterator it = nfa.delta.transitions.begin();
        Delta::transitions_const_iterator jt = nfa.delta.transitions.begin();
        CHECK(it == jt);
        ++it;
        CHECK(it != jt);
        CHECK((it != nfa.delta.transitions.begin() && it != nfa.delta.transitions.end()));
        CHECK(jt == nfa.delta.transitions.begin());

        ++jt;
        CHECK(it == jt);
        CHECK((jt != nfa.delta.transitions.begin() && jt != nfa.delta.transitions.end()));

        jt = nfa.delta.transitions.end();
        CHECK(it != jt);
        CHECK((jt != nfa.delta.transitions.begin() && jt == nfa.delta.transitions.end()));

        it = nfa.delta.transitions.end();
        CHECK(it == jt);
        CHECK((it != nfa.delta.transitions.begin() && it == nfa.delta.transitions.end()));
    }
}

TEST_CASE("Mata::Nfa::Delta::operator=()") {
    Nfa nfa{};
    nfa.initial.insert(0);
    nfa.final.insert(1);
    nfa.delta.add(0, 'a', 1);

    Nfa copied_nfa{ nfa };
    nfa.delta.add(1, 'b', 0);
    CHECK(nfa.delta.transitions.count() == 2);
    CHECK(copied_nfa.delta.transitions.count() == 1);
}

TEST_CASE("Mata::Nfa::Delta::TransitionsView") {
    Nfa nfa{};
    nfa.initial.insert(0);
    nfa.final.insert(5);
    nfa.delta.add(0, 'a', 1);
    nfa.delta.add(1, 'b', 2);
    nfa.delta.add(1, 'c', 2);
    nfa.delta.add(1, 'd', 2);
    nfa.delta.add(2, 'e', 3);
    nfa.delta.add(3, 'e', 4);
    nfa.delta.add(4, 'f', 5);
    Delta::TransitionsView transitions_from_source{ nfa.delta.transitions.from(0) };
    CHECK(std::vector<Transition>{ transitions_from_source.begin(), transitions_from_source.end() } == std::vector<Transition>{ { 0, 'a', 1 }});
    transitions_from_source = nfa.delta.transitions.from(1);
    CHECK(std::vector<Transition>{ transitions_from_source.begin(), transitions_from_source.end() } ==
        std::vector<Transition>{ { 1, 'b', 2 }, { 1, 'c', 2 }, { 1, 'd', 2 } });
    transitions_from_source = nfa.delta.transitions.from(12);
    CHECK(std::vector<Transition>{ transitions_from_source.begin(), transitions_from_source.end() }.empty());
}

TEST_CASE("Mata::Nfa::Delta::operator==()") {
    Delta delta{};
    Delta delta2{};
    CHECK(delta == delta2);
    delta.add(0, 0, 0);
    CHECK(delta != delta2);
    delta2.add(0, 0, 0);
    CHECK(delta == delta2);
    delta.add(0, 0, 1);
    delta2.add(0, 0, 2);
    CHECK(delta != delta2);
    delta2.add(0, 0, 1);
    CHECK(delta != delta2);
    delta.add(0, 0, 2);
    CHECK(delta == delta2);
    delta2.add(0, 0, 3);
    CHECK(delta != delta2);
    delta.add(0, 0, 3);
    CHECK(delta == delta2);
}
