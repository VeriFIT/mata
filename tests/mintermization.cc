/* tests-mintermization.cc -- tests of Mintermization
 */

#include <catch2/catch.hpp>

#include "mata/parser/inter-aut.hh"
#include "mata/parser/mintermization.hh"
#include "mata/parser/bdd-domain.hh"

using namespace mata::parser;

TEST_CASE("mata::Mintermization::trans_to_vars_nfa")
{
    Parsed parsed;
    mata::Mintermization<mata::BDDDomain> mintermization{};

    SECTION("Empty trans")
    {
        std::string file =
                "@NFA-explicit\n"
                "%States-enum q r s t \"(r,s)\"\n"
                "%Alphabet-auto\n"
                "%Initial q & r\n"
                "%Final q | r\n"
                "q a r\n";

        parsed = parse_mf(file);
        std::vector<mata::IntermediateAut> auts = mata::IntermediateAut::parse_from_mf(parsed);
        const auto& aut= auts[0];
        REQUIRE(aut.transitions[0].first.is_operand());
        REQUIRE(aut.transitions[0].second.children[0].node.is_operand());
        const mata::BDDDomain alg = mintermization.graph_to_vars_nfa(aut.transitions[0].second.children[0]);
        REQUIRE(alg.val.nodeCount() == 2);
    }

    SECTION("Small bitvector transition")
    {
        std::string file =
                "@NFA-bits\n"
                "%States-enum q r s t \"(r,s)\"\n"
                "%Alphabet-auto\n"
                "%Initial q & r\n"
                "%Final q | r\n"
                "q (a1 | !a2)  r\n";

        parsed = parse_mf(file);
        std::vector<mata::IntermediateAut> auts = mata::IntermediateAut::parse_from_mf(parsed);
        const auto& aut= auts[0];
        REQUIRE(aut.transitions[0].second.children[0].node.is_operator());
        REQUIRE(aut.transitions[0].second.children[1].node.is_operand());
        const mata::BDDDomain alg  = mintermization.graph_to_vars_nfa(aut.transitions[0].second.children[0]);
        REQUIRE(alg.val.nodeCount() == 3);
    }

    SECTION("Complex bitvector transition")
    {
        std::string file =
                "@NFA-bits\n"
                "%States-enum q r s t \"(r,s)\"\n"
                "%Alphabet-auto\n"
                "%Initial q & r\n"
                "%Final q | r\n"
                "q ((a1 | !a2) | (!a1 & a3 | (a4 & !a2)))  r\n";

        parsed = parse_mf(file);
        std::vector<mata::IntermediateAut> auts = mata::IntermediateAut::parse_from_mf(parsed);
        const auto& aut= auts[0];
        REQUIRE(aut.transitions[0].second.children[0].node.is_operator());
        REQUIRE(aut.transitions[0].second.children[1].node.is_operand());
        const mata::BDDDomain alg = mintermization.graph_to_vars_nfa(aut.transitions[0].second.children[0]);
        REQUIRE(alg.val.nodeCount() == 4);
        int inputs[] = {0,0,0,0};
        REQUIRE(alg.val.Eval(inputs).IsOne());
        int inputs_false[] = {0,1,0,0};
        REQUIRE(alg.val.Eval(inputs_false).IsZero());
    }
} // trans to bdd section

TEST_CASE("mata::Mintermization::compute_minterms")
{
    Parsed parsed;
    mata::Mintermization<mata::BDDDomain> mintermization{};

    SECTION("Minterm from trans no elimination")
    {
        std::string file =
                "@NFA-bits\n"
                "%States-enum q r s t \"(r,s)\"\n"
                "%Alphabet-auto\n"
                "%Initial q & r\n"
                "%Final q | r\n"
                "q (a1 | !a2) r\n"
                "q (a3 & a4) r\n";

        parsed = parse_mf(file);
        std::vector<mata::IntermediateAut> auts = mata::IntermediateAut::parse_from_mf(parsed);
        const auto& aut= auts[0];
        REQUIRE(aut.transitions[0].second.children[0].node.is_operator());
        REQUIRE(aut.transitions[0].second.children[1].node.is_operand());
        std::unordered_set<mata::BDDDomain> vars;
        vars.insert(mintermization.graph_to_vars_nfa(aut.transitions[0].second.children[0]));
        vars.insert(mintermization.graph_to_vars_nfa(aut.transitions[1].second.children[0]));
        auto res = mintermization.compute_minterms(vars);
        REQUIRE(res.size() == 4);
    }

    SECTION("Minterm from trans with minterm tree elimination")
    {
        std::string file =
                "@NFA-bits\n"
                "%States-enum q r s t \"(r,s)\"\n"
                "%Alphabet-auto\n"
                "%Initial q & r\n"
                "%Final q | r\n"
                "q (a1 | a2) r\n"
                "q (a1 & a4) r\n";

        parsed = parse_mf(file);
        std::vector<mata::IntermediateAut> auts = mata::IntermediateAut::parse_from_mf(parsed);
        const auto& aut= auts[0];
        REQUIRE(aut.transitions[0].second.children[0].node.is_operator());
        REQUIRE(aut.transitions[0].second.children[1].node.is_operand());
        std::unordered_set<struct mata::BDDDomain> vars;
        vars.insert(mintermization.graph_to_vars_nfa(aut.transitions[0].second.children[0]));
        vars.insert(mintermization.graph_to_vars_nfa(aut.transitions[1].second.children[0]));
        auto res = mintermization.compute_minterms(vars);
        REQUIRE(res.size() == 3);
    }
} // compute_minterms

TEST_CASE("mata::Mintermization::mintermization") {
    Parsed parsed;
    mata::Mintermization<mata::BDDDomain> mintermization{};

    SECTION("Mintermization small") {
        std::string file =
                "@NFA-bits\n"
                "%States-enum q r s t \"(r,s)\"\n"
                "%Alphabet-auto\n"
                "%Initial q & r\n"
                "%Final q | r\n"
                "q (a1 | !a2) r\n"
                "s (a3 & a4) t\n";

        parsed = parse_mf(file);
        std::vector<mata::IntermediateAut> auts = mata::IntermediateAut::parse_from_mf(parsed);
        const auto &aut = auts[0];
        REQUIRE(aut.transitions[0].second.children[0].node.is_operator());
        REQUIRE(aut.transitions[0].second.children[1].node.is_operand());

        const auto res = mintermization.mintermize(aut);
        REQUIRE(res.transitions.size() == 4);
        REQUIRE(res.transitions[0].first.name == "q");
        REQUIRE(res.transitions[1].first.name == "q");
        REQUIRE(res.transitions[2].first.name == "s");
        REQUIRE(res.transitions[3].first.name == "s");
        REQUIRE(res.transitions[0].second.children[1].node.name == "r");
        REQUIRE(res.transitions[1].second.children[1].node.name == "r");
        REQUIRE(res.transitions[2].second.children[1].node.name == "t");
        REQUIRE(res.transitions[3].second.children[1].node.name == "t");
    }

    SECTION("Mintermization NFA true and false") {
        std::string file =
            "@NFA-bits\n"
            "%States-enum q r s\n"
            "%Alphabet-auto\n"
            "%Initial q\n"
            "%Final r\n"
            "q \\true r\n"
            "r a1 & a2 s\n"
            "s \\false s\n";

        parsed = parse_mf(file);
        std::vector<mata::IntermediateAut> auts = mata::IntermediateAut::parse_from_mf(parsed);
        const auto &aut = auts[0];
        REQUIRE(aut.transitions[0].second.children[0].node.is_operand());
        REQUIRE(aut.transitions[0].second.children[0].node.raw == "\\true");
        REQUIRE(aut.transitions[0].second.children[1].node.is_operand());
        REQUIRE(aut.transitions[0].second.children[1].node.raw == "r");

        const auto res = mintermization.mintermize(aut);
        REQUIRE(res.transitions.size() == 3);
        REQUIRE(res.transitions[0].first.name == "q");
        REQUIRE(res.transitions[1].first.name == "q");
        REQUIRE(res.transitions[2].first.name == "r");
    }

    SECTION("Mintermization NFA multiple") {
        Parsed parsed;
        mata::Mintermization<mata::BDDDomain> mintermization{};

        std::string file =
                "@NFA-bits\n"
                "%States-enum q r s t\n"
                "%Alphabet-auto\n"
                "%Initial q\n"
                "%Final q | r\n"
                "q (a1 | a2) r\n"
                "s (a3 & a4) t\n"
                "@NFA-bits\n"
                "%States-enum q r\n"
                "%Alphabet-auto\n"
                "%Initial q\n"
                "%Final q | r\n"
                "q (a1 & a4) r\n";

        parsed = parse_mf(file);
        std::vector<mata::IntermediateAut> auts = mata::IntermediateAut::parse_from_mf(parsed);

        const auto res = mintermization.mintermize(auts);
        REQUIRE(res.size() == 2);
        REQUIRE(res[0].transitions.size() == 7);
        REQUIRE(res[0].transitions[0].first.name == "q");
        REQUIRE(res[0].transitions[1].first.name == "q");
        REQUIRE(res[0].transitions[2].first.name == "q");
        REQUIRE(res[0].transitions[3].first.name == "q");
        REQUIRE(res[0].transitions[4].first.name == "s");
        REQUIRE(res[0].transitions[5].first.name == "s");
        REQUIRE(res[0].transitions[6].first.name == "s");
        REQUIRE(res[0].transitions[0].second.children[1].node.name == "r");
        REQUIRE(res[0].transitions[1].second.children[1].node.name == "r");
        REQUIRE(res[0].transitions[2].second.children[1].node.name == "r");
        REQUIRE(res[0].transitions[3].second.children[1].node.name == "r");
        REQUIRE(res[0].transitions[4].second.children[1].node.name == "t");
        REQUIRE(res[0].transitions[5].second.children[1].node.name == "t");
        REQUIRE(res[0].transitions[6].second.children[1].node.name == "t");
        REQUIRE(res[1].transitions.size() == 2);
        REQUIRE(res[1].transitions[0].first.name == "q");
        REQUIRE(res[1].transitions[1].first.name == "q");
        REQUIRE(res[1].transitions[0].second.children[1].node.name == "r");
        REQUIRE(res[1].transitions[1].second.children[1].node.name == "r");
    }
} // TEST_CASE("mata::Mintermization::mintermization")
