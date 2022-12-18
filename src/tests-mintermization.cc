/* tests-mintermization.cc -- tests of Mintermization
 *
 * Copyright (c) 2022 Martin Hruska
 *
 * This file is a part of libmata.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "../3rdparty/catch.hpp"

#include <mata/inter-aut.hh>
#include <mata/mintermization.hh>
#include <mata/parser.hh>

using namespace Mata::Parser;

TEST_CASE("Mata::Mintermization::trans_to_bdd_nfa")
{
    Parsed parsed;
    Mata::Mintermization mintermization{};

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
        std::vector<Mata::IntermediateAut> auts = Mata::IntermediateAut::parse_from_mf(parsed);
        const auto& aut= auts[0];
        REQUIRE(aut.transitions[0].first.is_operand());
        REQUIRE(aut.transitions[0].second.children[0].node.is_operand());
        const BDD bdd = mintermization.graph_to_bdd_nfa(aut.transitions[0].second.children[0]);
        REQUIRE(bdd.nodeCount() == 2);
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
        std::vector<Mata::IntermediateAut> auts = Mata::IntermediateAut::parse_from_mf(parsed);
        const auto& aut= auts[0];
        REQUIRE(aut.transitions[0].second.children[0].node.is_operator());
        REQUIRE(aut.transitions[0].second.children[1].node.is_operand());
        const BDD bdd = mintermization.graph_to_bdd_nfa(aut.transitions[0].second.children[0]);
        REQUIRE(bdd.nodeCount() == 3);
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
        std::vector<Mata::IntermediateAut> auts = Mata::IntermediateAut::parse_from_mf(parsed);
        const auto& aut= auts[0];
        REQUIRE(aut.transitions[0].second.children[0].node.is_operator());
        REQUIRE(aut.transitions[0].second.children[1].node.is_operand());
        const BDD bdd = mintermization.graph_to_bdd_nfa(aut.transitions[0].second.children[0]);
        REQUIRE(bdd.nodeCount() == 4);
        int inputs[] = {0,0,0,0};
        REQUIRE(bdd.Eval(inputs).IsOne());
        int inputs_false[] = {0,1,0,0};
        REQUIRE(bdd.Eval(inputs_false).IsZero());
    }
} // trans to bdd section

TEST_CASE("Mata::Mintermization::compute_minterms")
{
    Parsed parsed;
    Mata::Mintermization mintermization{};

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
        std::vector<Mata::IntermediateAut> auts = Mata::IntermediateAut::parse_from_mf(parsed);
        const auto& aut= auts[0];
        REQUIRE(aut.transitions[0].second.children[0].node.is_operator());
        REQUIRE(aut.transitions[0].second.children[1].node.is_operand());
        std::vector<BDD> bdds;
        bdds.push_back(mintermization.graph_to_bdd_nfa(aut.transitions[0].second.children[0]));
        bdds.push_back(mintermization.graph_to_bdd_nfa(aut.transitions[1].second.children[0]));
        std::vector<BDD> res = mintermization.compute_minterms(bdds);
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
        std::vector<Mata::IntermediateAut> auts = Mata::IntermediateAut::parse_from_mf(parsed);
        const auto& aut= auts[0];
        REQUIRE(aut.transitions[0].second.children[0].node.is_operator());
        REQUIRE(aut.transitions[0].second.children[1].node.is_operand());
        std::vector<BDD> bdds;
        bdds.push_back(mintermization.graph_to_bdd_nfa(aut.transitions[0].second.children[0]));
        bdds.push_back(mintermization.graph_to_bdd_nfa(aut.transitions[1].second.children[0]));
        std::vector<BDD> res = mintermization.compute_minterms(bdds);
        REQUIRE(res.size() == 3);
    }
} // compute_minterms

TEST_CASE("Mata::Mintermization::mintermization")
{
    Parsed parsed;
    Mata::Mintermization mintermization{};

    SECTION("Mintermization small")
    {
        std::string file =
                "@NFA-bits\n"
                "%States-enum q r s t \"(r,s)\"\n"
                "%Alphabet-auto\n"
                "%Initial q & r\n"
                "%Final q | r\n"
                "q (a1 | !a2) r\n"
                "s (a3 & a4) t\n";

        parsed = parse_mf(file);
        std::vector<Mata::IntermediateAut> auts = Mata::IntermediateAut::parse_from_mf(parsed);
        const auto& aut= auts[0];
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

    SECTION("Mintermization AFA small")
    {
        Parsed parsed;
        Mata::Mintermization mintermization{};
        std::string file =
            "@AFA-bits\n"
            "%Initial (q0) & ((q1 & q1' & q3 & q3'))\n"
            "%Final true & (!q3' | (!q1))\n"
            "q1 (!a0 & !a1 & (q2))\n"
            "q1 (a1 & !a2 & (q3))\n"
            "q1' q1'\n"
            "q3' q3'\n";

        parsed = parse_mf(file);
        std::vector<Mata::IntermediateAut> auts = Mata::IntermediateAut::parse_from_mf(parsed);
        const auto &aut = auts[0];
        REQUIRE(aut.transitions[0].second.children[0].node.is_operator());
        REQUIRE(aut.transitions[0].second.children[1].node.is_operator());

        const auto res = mintermization.mintermize(aut);
        REQUIRE(res.transitions.size() == 8);
        REQUIRE(res.transitions[0].first.name == "1");
        REQUIRE(res.transitions[1].first.name == "1");
        REQUIRE(res.transitions[2].first.name == "1'");
        REQUIRE(res.transitions[3].first.name == "1'");
        REQUIRE(res.transitions[4].first.name == "1'");
        REQUIRE(res.transitions[5].first.name == "3'");
        REQUIRE(res.transitions[6].first.name == "3'");
        REQUIRE(res.transitions[7].first.name == "3'");
        REQUIRE(res.transitions[0].second.children[1].node.name == "2");
        REQUIRE(res.transitions[1].second.children[1].node.name == "3");
        REQUIRE(res.transitions[2].second.children[1].node.name == "1'");
        REQUIRE(res.transitions[3].second.children[1].node.name == "1'");
        REQUIRE(res.transitions[4].second.children[1].node.name == "1'");
        REQUIRE(res.transitions[5].second.children[1].node.name == "3'");
        REQUIRE(res.transitions[6].second.children[1].node.name == "3'");
        REQUIRE(res.transitions[7].second.children[1].node.name == "3'");
    }

    SECTION("Mintermization AFA normal")
    {
        Parsed parsed;
        Mata::Mintermization mintermization{};

        std::string file =
                "@AFA-bits\n"
                "%Initial (q0) & ((q1 & q1' & q3 & q3'))\n"
                "%Final true & (!q3' | (!q1))\n"
                "q1 (!a0 & !a1 & !a2 & !a3 & (q2))\n"
                "q0 (a4 & !a5 & !a6 & !a7 & (q0)) | (!a4 & a5 & !a6 & !a7 & (q1)) | (a4 & a5 & !a6 & !a7 & (q2))\n"
                "q1' q1'\n"
                "q3' q3'\n";

        parsed = parse_mf(file);
        std::vector<Mata::IntermediateAut> auts = Mata::IntermediateAut::parse_from_mf(parsed);
        const auto &aut = auts[0];
        REQUIRE(aut.transitions[0].second.children[0].node.is_operator());
        REQUIRE(aut.transitions[0].second.children[1].node.is_operator());

        const auto res = mintermization.mintermize(aut);
        REQUIRE(res.transitions.size() == 26);
        REQUIRE(res.transitions[0].first.name == "1");
        REQUIRE(res.transitions[1].first.name == "1");
        REQUIRE(res.transitions[2].first.name == "1");
        REQUIRE(res.transitions[3].first.name == "1");
        REQUIRE(res.transitions[4].first.name == "0");
        REQUIRE(res.transitions[5].first.name == "0");
        REQUIRE(res.transitions[6].first.name == "0");
        REQUIRE(res.transitions[7].first.name == "0");
        REQUIRE(res.transitions[8].first.name == "0");
        REQUIRE(res.transitions[9].first.name == "0");
        REQUIRE(res.transitions[0].second.children[0].node.name == "0");
        REQUIRE(res.transitions[0].second.children[1].node.name == "2");
        REQUIRE(res.transitions[1].second.children[0].node.name == "1");
        REQUIRE(res.transitions[1].second.children[1].node.name == "2");
        REQUIRE(res.transitions[2].second.children[0].node.name == "2");
        REQUIRE(res.transitions[2].second.children[1].node.name == "2");
        REQUIRE(res.transitions[3].second.children[0].node.name == "3");
        REQUIRE(res.transitions[3].second.children[1].node.name == "2");
    }

    SECTION("Mintermization AFA complex")
    {
        Parsed parsed;
        Mata::Mintermization mintermization{};

        std::string file =
                "@AFA-bits\n"
                "%Initial (q0) & ((q1 & q1' & q3 & q3'))\n"
                "%Final true & (!q3' | (!q1))\n"
                "q1 (!a0 & !a1 & !a2 & !a3 & (q2))\n"
                "q0 (a4 & !a5 & !a6 & !a7 & (q0)) | (!a4 & a5 & !a6 & !a7 & (q0)) | (a4 & a5 & !a6 & !a7 & (q0)) | (!a4 & !a5 & a6 & !a7 & (q0)) | (a4 & !a5 & a6 & !a7 & (q0)) | (!a4 & a5 & a6 & !a7 & (q0)) | (a4 & a5 & a6 & !a7 & (q0)) | (!a4 & !a5 & !a6 & a7 & (q0)) | (a4 & !a5 & !a6 & a7 & (q0)) | (!a4 & a5 & !a6 & a7 & (q0)) | (a4 & a5 & !a6 & a7 & (q0)) | (!a4 & !a5 & a6 & a7 & (q0)) | (!a4 & !a5 & !a6 & !a7 & (q0)) | (a4 & !a5 & a6 & a7 & (q0))\n"
                "q3 (a8 & !a9 & !a10 & !a11 & (q3)) | (!a8 & a9 & !a10 & !a11 & (q3)) | (a8 & a9 & !a10 & !a11 & (q3)) | (!a8 & !a9 & a10 & !a11 & (q3)) | (a8 & !a9 & a10 & !a11 & (q3)) | (!a8 & a9 & a10 & !a11 & (q3)) | (a8 & a9 & a10 & !a11 & (q3)) | (!a8 & !a9 & !a10 & a11 & (q3)) | (a8 & !a9 & !a10 & a11 & (q3)) | (!a8 & a9 & !a10 & a11 & (q3)) | (a8 & a9 & !a10 & a11 & (q3)) | (!a8 & !a9 & a10 & a11 & (q3)) | (!a8 & !a9 & !a10 & !a11 & (q3)) | (a8 & !a9 & a10 & a11 & (q3))\n"
                "q1' q1'\n"
                "q3' q3'\n";

        parsed = parse_mf(file);
        std::vector<Mata::IntermediateAut> auts = Mata::IntermediateAut::parse_from_mf(parsed);
        const auto &aut = auts[0];
        REQUIRE(aut.transitions[0].second.children[0].node.is_operator());
        REQUIRE(aut.transitions[0].second.children[1].node.is_operator());

        const auto res = mintermization.mintermize(aut);
        REQUIRE(res.transitions.size() == 1965);
    }

    SECTION("Mintermization AFA state conjunction")
    {
        Parsed parsed;
        Mata::Mintermization mintermization{};

        std::string file =
                "@AFA-bits\n"
                "%Initial (q0) & ((q1 & q1' & q3 & q3'))\n"
                "%Final true & (!q3' | (!q1))\n"
                "q1 (!a0 & !a1 & !a2 & !a3 & (q2 & q3 & q0))\n"
                "q0 (a4 & !a5 & !a6 & !a7 & (q0 & q1 & q1')) | (!a4 & a5 & !a6 & !a7 & (q1)) | (a4 & a5 & !a6 & !a7 & q2 & q1')\n"
                "q1' q1'\n"
                "q3' q3'\n";

        parsed = parse_mf(file);
        std::vector<Mata::IntermediateAut> auts = Mata::IntermediateAut::parse_from_mf(parsed);
        const auto &aut = auts[0];
        REQUIRE(aut.transitions[0].second.children[0].node.is_operator());
        REQUIRE(aut.transitions[0].second.children[1].node.is_operator());

        const auto res = mintermization.mintermize(aut);
        REQUIRE(res.transitions.size() == 26);
        REQUIRE(res.transitions[0].first.name == "1");
        REQUIRE(res.transitions[1].first.name == "1");
        REQUIRE(res.transitions[2].first.name == "1");
        REQUIRE(res.transitions[3].first.name == "1");
        REQUIRE(res.transitions[4].first.name == "0");
        REQUIRE(res.transitions[5].first.name == "0");
        REQUIRE(res.transitions[6].first.name == "0");
        REQUIRE(res.transitions[7].first.name == "0");
        REQUIRE(res.transitions[8].first.name == "0");
        REQUIRE(res.transitions[9].first.name == "0");
        REQUIRE(res.transitions[0].second.children[0].node.name == "0");
        REQUIRE(res.transitions[0].second.children[1].node.name == "&");
        REQUIRE(res.transitions[1].second.children[0].node.name == "1");
        REQUIRE(res.transitions[1].second.children[1].node.name == "&");
        REQUIRE(res.transitions[2].second.children[0].node.name == "2");
        REQUIRE(res.transitions[2].second.children[1].node.name == "&");
        REQUIRE(res.transitions[3].second.children[0].node.name == "3");
        REQUIRE(res.transitions[3].second.children[1].node.name == "&");
        REQUIRE(res.transitions[4].second.children[1].node.name == "&");
        REQUIRE(res.transitions[5].second.children[1].node.name == "&");
    }

    SECTION("Mintermization AFA difficult")
    {
        Parsed parsed;
        Mata::Mintermization mintermization{};

        std::string file =
                            "@AFA-bits\n"
                            "%Initial q11\n"
                            "%Final !q0 & !q1 & !q2 & !q3 & !q4 & !q5 & !q6 & !q7 & !q8 & !q9 & !q10 & !q11\n"
                            "q10 (a1 & !a0 & a0 & q9) | (a1 & !a0 & q9 & q10)\n"
                            "q6 a3\n"
                            "q8 !a2 | (a3 & !a0 & q8)\n"
                            "q4 !a0 & q3\n"
                            "q3 !a0 & q2\n"
                            "q1 (!a1 & a0) | (!a1 & q1) | (!a0 & a0 & q0) | (!a0 & q0 & q1)\n"
                            "q11 (!a1 & a0 & a4 & !a0 & !a0 & !a0 & a0 & q2 & q3 & q4) | (!a1 & a0 & a4 & !a0 & !a0 & !a0 & q2 & q3 & q4 & q5) | (!a1 & a0 & a3 & !a0 & a0 & q6) | (!a1 & a0 & a3 & !a0 & q6 & q7) | (!a1 & a0 & a1 & !a0 & a0 & q9) | (!a1 & a0 & a1 & !a0 & q9 & q10) | (!a1 & a4 & !a0 & !a0 & !a0 & a0 & q1 & q2 & q3 & q4) | (!a1 & a4 & !a0 & !a0 & !a0 & q1 & q2 & q3 & q4 & q5) | (!a1 & a3 & !a0 & a0 & q1 & q6) | (!a1 & a3 & !a0 & q1 & q6 & q7) | (!a1 & a1 & !a0 & a0 & q1 & q9) | (!a1 & a1 & !a0 & q1 & q9 & q10) | (!a0 & a0 & a4 & !a0 & !a0 & !a0 & a0 & q0 & q2 & q3 & q4) | (!a0 & a0 & a4 & !a0 & !a0 & !a0 & q0 & q2 & q3 & q4 & q5) | (!a0 & a0 & a3 & !a0 & a0 & q0 & q6) | (!a0 & a0 & a3 & !a0 & q0 & q6 & q7) | (!a0 & a0 & a1 & !a0 & a0 & q0 & q9) | (!a0 & a0 & a1 & !a0 & q0 & q9 & q10) | (!a0 & a4 & !a0 & !a0 & !a0 & a0 & q0 & q1 & q2 & q3 & q4) | (!a0 & a4 & !a0 & !a0 & !a0 & q0 & q1 & q2 & q3 & q4 & q5) | (!a0 & a3 & !a0 & a0 & q0 & q1 & q6) | (!a0 & a3 & !a0 & q0 & q1 & q6 & q7) | (!a0 & a1 & !a0 & a0 & q0 & q1 & q9) | (!a0 & a1 & !a0 & q0 & q1 & q9 & q10)\n"
                            "q7 (a3 & !a0 & a0 & q6) | (a3 & !a0 & q6 & q7)\n"
                            "q2 !a3\n"
                            "q9 q8\n"
                            "q5 (a4 & !a0 & !a0 & !a0 & a0 & q2 & q3 & q4) | (a4 & !a0 & !a0 & !a0 & q2 & q3 & q4 & q5)\n"
                            "q0 a2\n";

        parsed = parse_mf(file);
        std::vector<Mata::IntermediateAut> auts = Mata::IntermediateAut::parse_from_mf(parsed);
        const auto &aut = auts[0];
        REQUIRE(aut.transitions[0].second.children[0].node.is_operator());
        REQUIRE(aut.transitions[0].second.children[1].node.is_operator());

        const auto res = mintermization.mintermize(aut);
    }

    SECTION("Mintermization NFA true and false")
    {
        std::string file =
            "@NFA-bits\n"
            "%States-enum q r s\n"
            "%Alphabet-auto\n"
            "%Initial q\n"
            "%Final r\n"
            "q true r\n"
            "r a1 & a2 s\n"
            "s false s\n";

        parsed = parse_mf(file);
        std::vector<Mata::IntermediateAut> auts = Mata::IntermediateAut::parse_from_mf(parsed);
        const auto& aut= auts[0];
        REQUIRE(aut.transitions[0].second.children[0].node.is_operand());
        REQUIRE(aut.transitions[0].second.children[0].node.raw == "true");
        REQUIRE(aut.transitions[0].second.children[1].node.is_operand());
        REQUIRE(aut.transitions[0].second.children[1].node.raw == "r");

        const auto res = mintermization.mintermize(aut);
        REQUIRE(res.transitions.size() == 3);
        REQUIRE(res.transitions[0].first.name == "q");
        REQUIRE(res.transitions[1].first.name == "q");
        REQUIRE(res.transitions[2].first.name == "r");
    }

    SECTION("Mintermization AFA true and false")
    {
        std::string file =
            "@AFA-bits\n"
            "%Initial q0\n"
            "%Final q3\n"
            "q0 (true & q2 & q3 & q0) | (a4 & !a5 & !a6 & !a7 & q0 & q1 & q2)\n"
            "q1 false\n"
            "q2 q1\n"
            "q3 true\n";

        parsed = parse_mf(file);
        std::vector<Mata::IntermediateAut> auts = Mata::IntermediateAut::parse_from_mf(parsed);
        const auto& aut= auts[0];
        REQUIRE(aut.transitions[0].second.node.is_operator());
        REQUIRE(aut.transitions[0].second.node.raw == "|");
        REQUIRE(aut.transitions[0].second.children[1].node.is_operator());
        REQUIRE(aut.transitions[0].second.children[1].node.raw == "&");

        const auto res = mintermization.mintermize(aut);
        REQUIRE(res.transitions.size() == 7);
        REQUIRE(res.transitions[0].first.raw == "q0");
        REQUIRE(res.transitions[1].first.raw == "q0");
        REQUIRE(res.transitions[2].first.raw == "q0");
        REQUIRE(res.transitions[3].first.raw == "q2");
        REQUIRE(res.transitions[4].first.raw == "q2");
        REQUIRE(res.transitions[5].first.raw == "q3");
        REQUIRE(res.transitions[6].first.raw == "q3");
    }

    SECTION("Mintermization NFA multiple")
    {
        Parsed parsed;
        Mata::Mintermization mintermization{};

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
        std::vector<Mata::IntermediateAut> auts = Mata::IntermediateAut::parse_from_mf(parsed);

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
} // mintermization
