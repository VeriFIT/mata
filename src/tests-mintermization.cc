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

    SECTION("Mintermization AFA small 2")
    {
        Parsed parsed;
        Mata::Mintermization mintermization{};
        std::string file =
                "@AFA-bits\n"
                "%Initial q1\n"
                "%Final q2\n"
                "q1 a2 | q2\n";

        parsed = parse_mf(file);
        std::vector<Mata::IntermediateAut> auts = Mata::IntermediateAut::parse_from_mf(parsed);
        const auto &aut = auts[0];
        const auto res = mintermization.mintermize(aut);
        REQUIRE(res.transitions.size() == 3);
        REQUIRE(res.transitions[0].first.name == "1");
        REQUIRE(res.transitions[1].first.name == "1");
        REQUIRE(res.transitions[2].first.name == "1");
        REQUIRE(res.transitions[2].second.children.empty());
        REQUIRE(res.transitions[0].second.children[1].node.name == "2");
        REQUIRE(res.transitions[1].second.children[1].node.name == "2");
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

    SECTION("AFA big")
    {
        std::string file =
                "@AFA-bits\n"
                "%Initial qQC0_0 & qQC1_0\n"
                "%Final !qQC0_39 & !qQC0_5 & !qQC1_12 & !qQC0_20 & !qQC1_22 & !qQC0_10 & !qQC1_36 & !qQC0_40 & !qQC1_2 & !qQC1_31 & !qQC0_47 & !qQC1_5 & !qQC1_28 & !qQC0_35 & !qQC1_43 & !qQC0_9 & !qQC1_51 & !qQC1_48 & !qQC0_2 & !qQC1_15 & !qQC0_27 & !qQC0_7 & !qQC1_10 & !qQC0_22 & !qQC1_24 & !qQC0_52 & !qQC0_16 & !qQC1_9 & !qQC0_13 & !qQC1_38 & !qQC1_21 & !qQC0_18 & !qQC1_33 & !qQC0_45 & !qQC1_7 & !qQC0_37 & !qQC1_41 & !qQC0_30 & !qQC1_46 & !qQC0_29 & !qQC1_52 & !qQC0_1 & !qQC1_16 & !qQC0_24 & !qQC0_14 & !qQC0_49 & !qQC1_26 & !qQC0_50 & !qQC0_11 & !qQC1_23 & !qQC1_35 & !qQC0_43 & !qQC1_1 & !qQC1_4 & !qQC1_29 & !qQC1_30 & !qQC0_46 & !qQC0_32 & !qQC1_44 & !qQC1_19 & !qQC1_50 & !qQC1_49 & !qQC0_3 & !qQC1_14 & !qQC0_26 & !qQC0_4 & !qQC1_13 & !qQC0_21 & !qQC0_38 & !qQC1_8 & !qQC1_25 & !qQC0_53 & !qQC0_17 & !qQC1_3 & !qQC1_37 & !qQC0_41 & !qQC1_6 & !qQC0_19 & !qQC1_32 & !qQC0_44 & !qQC0_34 & !qQC1_42 & !qQC0_8 & !qQC0_28 & !qQC0_31 & !qQC1_47 & !qQC1_11 & !qQC0_23 & !qQC0_6 & !qQC1_27 & !qQC0_51 & !qQC0_15 & !qQC0_48 & !qQC1_20 & !qQC0_12 & !qQC1_39 & !qQC1_0 & !qQC1_34 & !qQC0_42 & !qQC0_36 & !qQC1_40 & !qQC1_18 & !qQC0_33 & !qQC1_45 & !qQC0_25 & !qQC1_53 & !qQC0_0 & !qQC1_17\n"
                "qQC1_34 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_35\n"
                "qQC1_1 aF | ((aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | !aV0) & (aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0))) | ((aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | !aV0) & qQC1_1) | ((aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0)) & qQC1_2) | (qQC1_2 & qQC1_1)\n"
                "qQC0_42 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_43\n"; // I cut it here to make test time feasible.
                /*
                "qQC0_3 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_4\n"
                "qQC1_40 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_41\n"
                "qQC0_36 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_37\n"
                "qQC1_9 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_10\n"
                "qQC1_10 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_11\n"
                "qQC1_4 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_5\n"
                "qQC0_22 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_23\n"
                "qQC1_3 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_4\n"
                "qQC0_25 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_26\n"
                "qQC1_53 !aF\n"
                "qQC1_17 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_18\n"
                "qQC1_38 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_39\n"
                "qQC1_21 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_22\n"
                "qQC0_13 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_14\n"
                "qQC1_33 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_34\n"
                "qQC0_18 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_19\n"
                "qQC0_45 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_46\n"
                "qQC0_4 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_5\n"
                "qQC1_36 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_37\n"
                "qQC0_40 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_41\n"
                "qQC0_1 (!aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & aV0 & qQC0_2) | (!aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_1)\n"
                "qQC0_30 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_31\n"
                "qQC0_29 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_30\n"
                "qQC1_46 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_47\n"
                "qQC0_35 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_36\n"
                "qQC1_43 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_44\n"
                "qQC0_27 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_28\n"
                "qQC1_51 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_52\n"
                "qQC1_48 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_49\n"
                "qQC1_15 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_16\n"
                "qQC1_23 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_24\n"
                "qQC0_11 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_12\n"
                "qQC1_24 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_25\n"
                "qQC0_16 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_17\n"
                "qQC0_52 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_53\n"
                "qQC0_46 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_47\n"
                "qQC0_7 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_8\n"
                "qQC1_30 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_31\n"
                "qQC1_29 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_30\n"
                "qQC0_32 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_33\n"
                "qQC1_44 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_45\n"
                "qQC1_19 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_20\n"
                "qQC1_8 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_9\n"
                "qQC1_41 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_42\n"
                "qQC0_37 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_38\n"
                "qQC0_21 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_22\n"
                "qQC1_13 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_14\n"
                "qQC0_38 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_39\n"
                "qQC1_7 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_8\n"
                "qQC0_24 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_25\n"
                "qQC1_52 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_53\n"
                "qQC1_16 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_17\n"
                "qQC1_2 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_3\n"
                "qQC0_14 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_15\n"
                "qQC0_49 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_50\n"
                "qQC0_50 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_51\n"
                "qQC0_8 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_9\n"
                "qQC1_26 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_27\n"
                "qQC0_19 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_20\n"
                "qQC0_44 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_45\n"
                "qQC0_5 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_6\n"
                "qQC1_32 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_33\n"
                "qQC0_43 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_44\n"
                "qQC0_2 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & aV0 & qQC0_3\n"
                "qQC1_35 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_36\n"
                "qQC0_28 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_29\n"
                "qQC1_47 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_48\n"
                "qQC0_31 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_32\n"
                "qQC1_11 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_12\n"
                "qQC1_5 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_6\n"
                "qQC0_23 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_24\n"
                "qQC1_50 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_51\n"
                "qQC1_49 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_50\n"
                "qQC1_14 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_15\n"
                "qQC1_0 aF | ((aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0)) & (aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | !aV0)) | ((aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0)) & qQC1_2) | ((aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | !aV0) & qQC1_1) | (qQC1_1 & qQC1_2)\n"
                "qQC0_26 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_27\n"
                "qQC0_12 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_13\n"
                "qQC1_39 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_40\n"
                "qQC1_20 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_21\n"
                "qQC0_53 aF\n"
                "qQC1_25 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_26\n"
                "qQC0_17 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_18\n"
                "qQC0_41 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_42\n"
                "qQC0_0 (!aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_1) | (!aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & aV0 & qQC0_2)\n"
                "qQC1_37 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_38\n"
                "qQC1_45 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_46\n"
                "qQC1_18 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_19\n"
                "qQC0_33 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_34\n"
                "qQC1_42 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_43\n"
                "qQC0_34 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_35\n"
                "qQC1_12 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_13\n"
                "qQC0_39 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_40\n"
                "qQC1_6 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_7\n"
                "qQC0_20 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_21\n"
                "qQC1_22 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_23\n"
                "qQC0_10 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_11\n"
                "qQC0_51 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_52\n"
                "qQC0_9 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_10\n"
                "qQC1_27 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_28\n"
                "qQC0_15 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_16\n"
                "qQC0_48 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_49\n"
                "qQC1_31 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_32\n"
                "qQC1_28 aF | aV15 | aV14 | aV13 | aV12 | aV11 | aV10 | aV9 | aV8 | aV7 | aV6 | !aV5 | !aV4 | aV3 | aV2 | aV1 | (aV0 & !aV0) | qQC1_29\n"
                "qQC0_47 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_48\n"
                "qQC0_6 !aF & !aV15 & !aV14 & !aV13 & !aV12 & !aV11 & !aV10 & !aV9 & !aV8 & !aV7 & !aV6 & aV5 & aV4 & !aV3 & !aV2 & !aV1 & (!aV0 | aV0) & qQC0_7\n";
                 */

        parsed = parse_mf(file);
        std::vector<Mata::IntermediateAut> auts = Mata::IntermediateAut::parse_from_mf(parsed);
        const auto& aut= auts[0];
        const auto res = mintermization.mintermize(aut);
    }
} // mintermization
