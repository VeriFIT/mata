#include "mata/nfa/nfa.hh"
#include "mata/nfa/learning.hh"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

TEST_CASE("mata::nfa::residual_automaton_learning"){
    Nfa aut;
    aut.initial = {0};
    aut.final = {3};
    aut.delta.add(0, 'a', 0);
    aut.delta.add(0, 'b', 0);
    aut.delta.add(0, 'a', 1);
    aut.delta.add(1, 'a', 1);
    aut.delta.add(1, 'a', 0);
    aut.delta.add(1, 'b', 0);
    aut.delta.add(1, 'a', 2);
    aut.delta.add(1, 'b', 2);
    aut.delta.add(2, 'a', 1);
    aut.delta.add(2, 'a', 0);
    aut.delta.add(2, 'b', 0);
    aut.delta.add(2, 'a', 3);
    aut.delta.add(2, 'b', 3);
    aut.delta.add(3, 'a', 1);
    aut.delta.add(3, 'a', 0);
    aut.delta.add(3, 'b', 0);

    StateRenaming* state_renaming = new StateRenaming();
    ParameterMap params_nlstar;
    params_nlstar["algorithm"] = "nlstar";
    ParameterMap params_residuals;
    params_residuals["algorithm"] = "residual";
    params_residuals["type"] = "with";
    params_residuals["direction"] = "forward";
    Nfa conjecture1 = learn(aut, params_nlstar);
    Nfa conjecture2 = reduce(aut, state_renaming, params_residuals);
    // should have only 4 states, while minimal DFA has 8
    // when rfsa_consistency was not fixed, this wasnt equal
    REQUIRE(conjecture1.num_of_states() == conjecture2.num_of_states());
    REQUIRE(are_equivalent(conjecture1, conjecture2));

    ParameterMap params_lstar;
    params_lstar["algorithm"] = "lstar";
    Nfa conjecture3 = learn(aut, params_lstar);
    Nfa conjecture4 = minimize_brzozowski(aut);

    REQUIRE(are_equivalent(conjecture3, conjecture4));
    REQUIRE(are_equivalent(conjecture1, conjecture3));
    REQUIRE(conjecture3.num_of_states() == conjecture4.num_of_states());
    REQUIRE(conjecture3.num_of_states() == 8);
    REQUIRE(conjecture1.num_of_states() == 4);
}