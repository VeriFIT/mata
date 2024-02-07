// TODO: some header

#include "utils.hh"

#include "mata/alphabet.hh"
#include "mata/lvlfa/types.hh"
#include "mata/lvlfa/delta.hh"
#include "mata/lvlfa/lvlfa.hh"

#include <catch2/catch.hpp>

using namespace mata::lvlfa;

using Symbol = mata::Symbol;

TEST_CASE("mata::lvlfa::SymbolPost") {
    CHECK(SymbolPost{ 0, StateSet{} } == SymbolPost{ 0, StateSet{ 0, 1 } });
    CHECK(SymbolPost{ 1, StateSet{} } != SymbolPost{ 0, StateSet{} });
    CHECK(SymbolPost{ 0, StateSet{ 1 } } < SymbolPost{ 1, StateSet{} });
    CHECK(SymbolPost{ 0, StateSet{ 1 } } <= SymbolPost{ 1, StateSet{} });
    CHECK(SymbolPost{ 0, StateSet{ 1 } } <= SymbolPost{ 0, StateSet{} });
    CHECK(SymbolPost{ 1, StateSet{ 0 } } > SymbolPost{ 0, StateSet{ 1 } });
    CHECK(SymbolPost{ 1, StateSet{ 0 } } >= SymbolPost{ 0, StateSet{ 1 } });
    CHECK(SymbolPost{ 1, StateSet{ 0 } } >= SymbolPost{ 0, StateSet{ 1 } });
}

TEST_CASE("mata::lvlfa::Delta::state_post()") {
    Lvlfa aut{};

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
        CHECK(aut.delta.num_of_transitions() == 5);
    }
}

TEST_CASE("mata::lvlfa::Delta::contains()") {
    Lvlfa lvlfa;
    CHECK(!lvlfa.delta.contains(0, 1, 0));
    CHECK(!lvlfa.delta.contains(Transition{ 0, 1, 0 }));
    lvlfa.delta.add(0, 1, 0);
    CHECK(lvlfa.delta.contains(0, 1, 0));
    CHECK(lvlfa.delta.contains(Transition{ 0, 1, 0 }));
}

TEST_CASE("mata::lvlfa::Delta::remove()") {
    Lvlfa lvlfa;

    SECTION("Simple remove") {
        lvlfa.delta.add(0, 1, 0);
        CHECK_NOTHROW(lvlfa.delta.remove(3, 5, 6));
        CHECK_NOTHROW(lvlfa.delta.remove(0, 1, 0));
        CHECK(lvlfa.delta.empty());
        lvlfa.delta.add(10, 1, 0);
        CHECK_THROWS_AS(lvlfa.delta.remove(3, 5, 6), std::invalid_argument);
    }
}

TEST_CASE("mata::lvlfa::Delta::mutable_post()") {
    Lvlfa lvlfa;

    SECTION("Default initialized") {
        CHECK(lvlfa.delta.num_of_states() == 0);
        CHECK(!lvlfa.delta.uses_state(0));
        CHECK(lvlfa.delta.mutable_state_post(0).empty());
        CHECK(lvlfa.delta.num_of_states() == 1);
        CHECK(lvlfa.delta.uses_state(0));

        CHECK(lvlfa.delta.mutable_state_post(9).empty());
        CHECK(lvlfa.delta.num_of_states() == 10);
        CHECK(lvlfa.delta.uses_state(1));
        CHECK(lvlfa.delta.uses_state(2));
        CHECK(lvlfa.delta.uses_state(9));
        CHECK(!lvlfa.delta.uses_state(10));

        CHECK(lvlfa.delta.mutable_state_post(9).empty());
        CHECK(lvlfa.delta.num_of_states() == 10);
        CHECK(lvlfa.delta.uses_state(9));
        CHECK(!lvlfa.delta.uses_state(10));
    }
}

TEST_CASE("mata::lvlfa::StatePost iteration over moves") {
    Lvlfa lvlfa;
    std::vector<Move> iterated_moves{};
    std::vector<Move> expected_moves{};
    StatePost state_post{};

    SECTION("Simple LVLFA") {
        lvlfa.initial.insert(0);
        lvlfa.final.insert(3);
        lvlfa.delta.add(0, 1, 1);
        lvlfa.delta.add(0, 2, 1);
        lvlfa.delta.add(0, 5, 1);
        lvlfa.delta.add(1, 3, 2);
        lvlfa.delta.add(2, 0, 1);
        lvlfa.delta.add(2, 0, 3);

        state_post = lvlfa.delta.state_post(0);
        expected_moves = std::vector<Move>{ { 1, 1 }, { 2, 1 }, { 5, 1 } };
        StatePost::Moves moves{ state_post.moves() };
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

        StatePost::Moves epsilon_moves{ state_post.moves_epsilons() };
        CHECK(std::vector<Move>{ epsilon_moves.begin(), epsilon_moves.end() }.empty());

        state_post = lvlfa.delta.state_post(1);
        moves = state_post.moves();
        StatePost::Moves moves_custom;
        moves_custom = moves;
        CHECK(std::vector<Move>{ moves.begin(), moves.end() }
              == std::vector<Move>{ moves_custom.begin(), moves_custom.end() });
        moves_custom = state_post.moves(state_post.begin(), state_post.end());
        CHECK(std::vector<Move>{ moves.begin(), moves.end() }
              == std::vector<Move>{ moves_custom.begin(), moves_custom.end() });
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
        epsilon_moves = state_post.moves_epsilons();
        CHECK(std::vector<Move>{ epsilon_moves.begin(), epsilon_moves.end() }.empty());

        state_post = lvlfa.delta.state_post(2);
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
        epsilon_moves = state_post.moves_epsilons();
        CHECK(std::vector<Move>{ epsilon_moves.begin(), epsilon_moves.end() }.empty());

        state_post = lvlfa.delta.state_post(3);
        moves = state_post.moves();
        iterated_moves.clear();
        for (auto move_it{ moves.begin() }; move_it != moves.end(); ++move_it) {
            iterated_moves.push_back(*move_it);
        }
        CHECK(iterated_moves.empty());
        CHECK(StatePost::Moves::const_iterator{ state_post } == moves.end());
        iterated_moves = { moves.begin(), moves.end() };
        CHECK(iterated_moves.empty());
        iterated_moves.clear();
        for (const Move& move: state_post.moves()) { iterated_moves.push_back(move); }
        CHECK(iterated_moves.empty());
        epsilon_moves = state_post.moves_epsilons();
        CHECK(std::vector<Move>{ epsilon_moves.begin(), epsilon_moves.end() }.empty());

        state_post = lvlfa.delta.state_post(4);
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
        epsilon_moves = state_post.moves_epsilons();
        CHECK(std::vector<Move>{ epsilon_moves.begin(), epsilon_moves.end() }.empty());

        lvlfa.delta.add(0, EPSILON, 2);
        state_post = lvlfa.delta.state_post(0);
        epsilon_moves = state_post.moves_epsilons();
        CHECK(std::vector<Move>{ epsilon_moves.begin(), epsilon_moves.end() } == std::vector<Move>{ { EPSILON, 2 } });
        lvlfa.delta.add(1, EPSILON, 3);
        state_post = lvlfa.delta.state_post(1);
        epsilon_moves = state_post.moves_epsilons();
        CHECK(std::vector<Move>{ epsilon_moves.begin(), epsilon_moves.end() } == std::vector<Move>{ { EPSILON, 3 } });
        lvlfa.delta.add(4, EPSILON, 4);
        state_post = lvlfa.delta.state_post(4);
        epsilon_moves = state_post.moves_epsilons();
        CHECK(std::vector<Move>{ epsilon_moves.begin(), epsilon_moves.end() } == std::vector<Move>{ { EPSILON, 4 } });

        state_post = lvlfa.delta.state_post(0);
        epsilon_moves = state_post.moves_epsilons(3);
        iterated_moves.clear();
        for (const Move& move: epsilon_moves) { iterated_moves.push_back(move); }
        CHECK(iterated_moves == std::vector<Move>{ { 5, 1 }, { EPSILON, 2 }});
        state_post = lvlfa.delta.state_post(1);
        epsilon_moves = state_post.moves_epsilons(3);
        CHECK(std::vector<Move>{ epsilon_moves.begin(), epsilon_moves.end() } == std::vector<Move>{ { 3, 2 }, { EPSILON, 3 } });

        state_post = lvlfa.delta.state_post(2);
        epsilon_moves = state_post.moves_epsilons(3);
        CHECK(std::vector<Move>{ epsilon_moves.begin(), epsilon_moves.end() }.empty());
        state_post = lvlfa.delta.state_post(4);
        epsilon_moves = state_post.moves_epsilons(3);
        CHECK(std::vector<Move>{ epsilon_moves.begin(), epsilon_moves.end() } == std::vector<Move>{ { EPSILON, 4 } });

        state_post = lvlfa.delta.state_post(0);
        StatePost::Moves symbol_moves = state_post.moves_symbols(3);
        iterated_moves.clear();
        for (const Move& move: symbol_moves) { iterated_moves.push_back(move); }
        CHECK(iterated_moves == std::vector<Move>{ { 1, 1 }, { 2, 1 } });
        symbol_moves = state_post.moves_symbols(0);
        iterated_moves.clear();
        for (const Move& move: symbol_moves) { iterated_moves.push_back(move); }
        CHECK(iterated_moves.empty());

        state_post = lvlfa.delta.state_post(1);
        symbol_moves = state_post.moves_symbols(3);
        CHECK(std::vector<Move>{ symbol_moves.begin(), symbol_moves.end() } == std::vector<Move>{ { 3, 2 } });
        state_post = lvlfa.delta.state_post(2);
        symbol_moves = state_post.moves_symbols(3);
        CHECK(std::vector<Move>{ symbol_moves.begin(), symbol_moves.end() } == std::vector<Move>{ { 0, 1 }, { 0 , 3 } });
        state_post = lvlfa.delta.state_post(4);
        symbol_moves = state_post.moves_symbols(3);
        CHECK(std::vector<Move>{ symbol_moves.begin(), symbol_moves.end() }.empty());

        // Create custom moves iterator.
        state_post = lvlfa.delta[0];
        moves = { state_post, state_post.cbegin(), state_post.cbegin() + 2 };
        iterated_moves = { moves.begin(), moves.end() };
        CHECK(iterated_moves == std::vector<Move>{ { 1, 1 }, { 2, 1 } });

        state_post = lvlfa.delta[20];
        moves = { state_post, state_post.cbegin(), state_post.cend() };
        iterated_moves = { moves.begin(), moves.end() };
        CHECK(iterated_moves.empty());
    }
}

TEST_CASE("mata::lvlfa::Delta iteration over transitions") {
    Lvlfa lvlfa;
    std::vector<Transition> iterated_transitions{};
    std::vector<Transition> expected_transitions{};

    SECTION("empty automaton") {
        Delta::Transitions transitions{ lvlfa.delta.transitions() };
        CHECK(transitions.begin() == transitions.end());
        Delta::Transitions::const_iterator transition_it{ lvlfa.delta };
        CHECK(transition_it == transitions.end());
        transition_it = { lvlfa.delta, 0 };
        CHECK(transition_it == transitions.end());
    }

    SECTION("Simple LVLFA") {
        lvlfa.initial.insert(0);
        lvlfa.final.insert(3);
        lvlfa.delta.add(0, 1, 1);
        lvlfa.delta.add(0, 2, 1);
        lvlfa.delta.add(0, 5, 1);
        lvlfa.delta.add(1, 3, 2);
        lvlfa.delta.add(2, 0, 1);
        lvlfa.delta.add(2, 0, 3);

        Delta::Transitions transitions{ lvlfa.delta.transitions() };
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
        for (const Transition& transition: lvlfa.delta.transitions()) { iterated_transitions.push_back(transition); }
        CHECK(iterated_transitions == expected_transitions);

        Delta::Transitions::const_iterator transitions_it{ lvlfa.delta.transitions().begin() };
        CHECK(*transitions_it == Transition{ 0, 1, 1 });
        transitions_it++;
        CHECK(*transitions_it == Transition{ 0, 2, 1 });
        transitions_it++;
        transitions_it++;
        CHECK(*transitions_it == Transition{ 1, 3, 2 });

        Delta::Transitions::const_iterator transitions_from_1_to_end_it{ lvlfa.delta, 1 };
        iterated_transitions.clear();
        while (transitions_from_1_to_end_it != lvlfa.delta.transitions().end()) {
            iterated_transitions.push_back(*transitions_from_1_to_end_it);
            transitions_from_1_to_end_it++;
        }
        expected_transitions = std::vector<Transition>{ { 1, 3, 2 }, { 2, 0, 1 }, { 2, 0, 3 } };
        CHECK(iterated_transitions == expected_transitions);
    }

    SECTION("Sparse automaton") {
        const size_t state_num = 'r'+1;
        lvlfa.delta.reserve(state_num);

        lvlfa.delta.add('q', 'a', 'r');
        lvlfa.delta.add('q', 'b', 'r');
        const Delta::Transitions transitions{ lvlfa.delta.transitions() };
        Delta::Transitions::const_iterator it{ transitions.begin() };
        Delta::Transitions::const_iterator jt{ transitions.begin() };
        CHECK(it == jt);
        ++it;
        CHECK(it != jt);
        CHECK((it != transitions.begin() && it != transitions.end()));
        CHECK(jt == transitions.begin());

        ++jt;
        CHECK(it == jt);
        CHECK((jt != transitions.begin() && jt != transitions.end()));

        jt = transitions.end();
        CHECK(it != jt);
        CHECK((jt != transitions.begin() && jt == transitions.end()));

        it = transitions.end();
        CHECK(it == jt);
        CHECK((it != transitions.begin() && it == transitions.end()));
    }
}

TEST_CASE("mata::lvlfa::Delta::operator=()") {
    Lvlfa lvlfa{};
    lvlfa.initial.insert(0);
    lvlfa.final.insert(1);
    lvlfa.delta.add(0, 'a', 1);

    Lvlfa copied_lvlfa{ lvlfa };
    lvlfa.delta.add(1, 'b', 0);
    CHECK(lvlfa.delta.num_of_transitions() == 2);
    CHECK(copied_lvlfa.delta.num_of_transitions() == 1);
}

TEST_CASE("mata::lvlfa::StatePost::Moves") {
    Lvlfa lvlfa{};
    lvlfa.initial.insert(0);
    lvlfa.final.insert(5);
    lvlfa.delta.add(0, 'a', 1);
    lvlfa.delta.add(1, 'b', 2);
    lvlfa.delta.add(1, 'c', 2);
    lvlfa.delta.add(1, 'd', 2);
    lvlfa.delta.add(2, 'e', 3);
    lvlfa.delta.add(3, 'e', 4);
    lvlfa.delta.add(4, 'f', 5);
    // TODO: rewrite in a check of moves.
    StatePost::Moves moves_from_source{ lvlfa.delta[0].moves() };

    CHECK(std::vector<Move>{ moves_from_source.begin(), moves_from_source.end() } == std::vector<Move>{ { 'a', 1 }});
    moves_from_source = lvlfa.delta[1].moves();
    CHECK(std::vector<Move>{ moves_from_source.begin(), moves_from_source.end() } ==
        std::vector<Move>{ { 'b', 2 }, { 'c', 2 }, { 'd', 2 } });
    StatePost::Moves::const_iterator move_incremented_it{ moves_from_source.begin() };
    move_incremented_it++;
    CHECK(*move_incremented_it == Move{ 'c', 2 });
    CHECK(*StatePost::Moves::const_iterator{ lvlfa.delta.state_post(1) } == Move{ 'b', 2 });
    CHECK(move_incremented_it != moves_from_source.begin());
    CHECK(move_incremented_it == ++moves_from_source.begin());
    StatePost::Moves moves_from_source_copy_constructed{ lvlfa.delta[12].moves() };
    CHECK(
        std::vector<Move>{ moves_from_source_copy_constructed.begin(), moves_from_source_copy_constructed.end() }
            .empty()
     );

}

TEST_CASE("mata::lvlfa::Delta::operator==()") {
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

TEST_CASE("mata::lvlfa::Delta::add_symbols_to()") {
    mata::OnTheFlyAlphabet empty_alphabet{};
    mata::OnTheFlyAlphabet alphabet{};
    Delta delta{};
    delta.add_symbols_to(alphabet);
    CHECK(alphabet.get_symbol_map().empty());
    delta.add(0, 0, 0);
    delta.add_symbols_to(alphabet);
    CHECK(alphabet.get_symbol_map().size() == 1);
    delta.add(0, 0, 0);
    delta.add_symbols_to(alphabet);
    CHECK(alphabet.get_symbol_map().size() == 1);
    delta.add(0, 1, 0);
    delta.add_symbols_to(alphabet);
    CHECK(alphabet.get_symbol_map().size() == 2);
    delta.add(0, 2, 0);
    delta.add(0, 3, 0);
    delta.add_symbols_to(alphabet);
    CHECK(alphabet.get_symbol_map().size() == 4);
    CHECK(alphabet.get_symbol_map() == std::unordered_map<std::string, mata::Symbol>{
        { "0", 0 }, { "1", 1 }, { "2", 2 }, { "3", 3 }
    });
}
