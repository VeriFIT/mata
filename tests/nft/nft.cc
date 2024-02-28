// TODO: some header

#include <unordered_set>

#include <catch2/catch.hpp>

#include "utils.hh"

#include "mata/utils/sparse-set.hh"
#include "mata/nft/delta.hh"
#include "mata/nft/nft.hh"
#include "mata/nft/strings.hh"
#include "mata/nft/builder.hh"
#include "mata/nft/plumbing.hh"
#include "mata/nft/algorithms.hh"
#include "mata/parser/re2parser.hh"

using namespace mata;
using namespace mata::nft::algorithms;
using namespace mata::nft;
using namespace mata::strings;
using namespace mata::nft::plumbing;
using namespace mata::utils;
using namespace mata::parser;
using Symbol = mata::Symbol;
using Word = mata::Word;
using IntAlphabet = mata::IntAlphabet;
using OnTheFlyAlphabet = mata::OnTheFlyAlphabet;

TEST_CASE("mata::nft::Nft()") {
    Nft nft{};
    nft.levels.resize(3);
    nft.levels_cnt = 5;
    CHECK(nft.levels_cnt == 5);
    CHECK(nft.levels.size() == 3);
    nft.levels[0] = 0;
    nft.levels[1] = 3;
    nft.levels[2] = 1;
    CHECK(nft.levels[2] == 1);
    CHECK(nft.levels == std::vector<Level>{ 0, 3, 1 });
}

TEST_CASE("mata::nft::size()") {
    Nft nft{};
    CHECK(nft.num_of_states() == 0);

    nft.add_state(3);
    CHECK(nft.num_of_states() == 4);

    nft.clear();
    nft.add_state();
    CHECK(nft.num_of_states() == 1);

    nft.clear();
    FILL_WITH_AUT_A(nft);
    CHECK(nft.num_of_states() == 11);

    nft.clear();
    FILL_WITH_AUT_B(nft);
    CHECK(nft.num_of_states() == 15);

    nft = Nft{ 0, {}, {} };
    CHECK(nft.num_of_states() == 0);
}

TEST_CASE("mata::nft::Trans::operator<<") {
    Transition trans(1, 2, 3);
    REQUIRE(std::to_string(trans) == "(1, 2, 3)");
}

TEST_CASE("mata::nft::create_alphabet()") {
    Nft a{1};
    a.delta.add(0, 'a', 0);

    Nft b{1};
    b.delta.add(0, 'b', 0);
    b.delta.add(0, 'a', 0);
    Nft c{1};
    b.delta.add(0, 'c', 0);

    auto alphabet{ create_alphabet(a, b, c) };

    auto symbols{alphabet.get_alphabet_symbols() };
    CHECK(symbols == mata::utils::OrdVector<Symbol>{ 'c', 'b', 'a' });

    // create_alphabet(1, 3, 4); // Will not compile: '1', '3', '4' are not of the required type.
    // create_alphabet(a, b, 4); // Will not compile: '4' is not of the required type.
}

TEST_CASE("mata::nft::Nft::delta.add()/delta.contains()")
{ // {{{
    Nft a(3);

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

TEST_CASE("mata::nft::Delta.transform/append")
{ // {{{
    Nft a(3);
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

TEST_CASE("mata::nft::is_lang_empty()")
{ // {{{
    Nft aut(14);
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

TEST_CASE("mata::nft::is_acyclic")
{ // {{{
    Nft aut(14);

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
        Nft aut(2);
        aut.initial = {0};
        aut.final = {1};
        aut.delta.add(0, 'c', 1);
        aut.delta.add(1, 'a', 1);
        REQUIRE(!aut.is_acyclic());
    }
} // }}}

TEST_CASE("mata::nft::get_word_for_path()")
{ // {{{
    Nft aut(5);
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


TEST_CASE("mata::nft::is_lang_empty_cex()")
{
    Nft aut(10);
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


TEST_CASE("mata::nft::determinize()")
{
    Nft aut(3);
    Nft result;
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
        Nft x{};
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

TEST_CASE("mata::nft::minimize() for profiling", "[.profiling],[minimize]") {
    Nft aut(4);
    Nft result;
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

TEST_CASE("mata::nft::construct() correct calls")
{ // {{{
    Nft aut(10);
    mata::parser::ParsedSection parsec;
    OnTheFlyAlphabet alphabet;

    SECTION("construct an empty automaton")
    {
        parsec.type = nft::TYPE_NFT;

        aut = builder::construct(parsec);

        REQUIRE(aut.is_lang_empty());
    }

    SECTION("construct a simple non-empty automaton accepting the empty word")
    {
        parsec.type = nft::TYPE_NFT;
        parsec.dict.insert({"Initial", {"q1"}});
        parsec.dict.insert({"Final", {"q1"}});

        aut = builder::construct(parsec);

        REQUIRE(!aut.is_lang_empty());
    }

    SECTION("construct an automaton with more than one initial/final states")
    {
        parsec.type = nft::TYPE_NFT;
        parsec.dict.insert({"Initial", {"q1", "q2"}});
        parsec.dict.insert({"Final", {"q1", "q2", "q3"}});

        aut = builder::construct(parsec);

        REQUIRE(aut.initial.size() == 2);
        REQUIRE(aut.final.size() == 3);
    }

    SECTION("construct a simple non-empty automaton accepting only the word 'a'")
    {
        parsec.type = nft::TYPE_NFT;
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
        parsec.type = nft::TYPE_NFT;
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

TEST_CASE("mata::nft::construct() invalid calls")
{ // {{{
    Nft aut;
    mata::parser::ParsedSection parsec;

    SECTION("construct() call with invalid ParsedSection object")
    {
        parsec.type = "FA";

        CHECK_THROWS_WITH(builder::construct(parsec),
                          Catch::Contains("expecting type"));
    }

    SECTION("construct() call with an epsilon transition")
    {
        parsec.type = nft::TYPE_NFT;
        parsec.body = { {"q1", "q2"} };

        CHECK_THROWS_WITH(builder::construct(parsec),
                          Catch::Contains("Epsilon transition"));
    }

    SECTION("construct() call with a nonsense transition")
    {
        parsec.type = nft::TYPE_NFT;
        parsec.body = { {"q1", "a", "q2", "q3"} };

        CHECK_THROWS_WITH(plumbing::construct(&aut, parsec),
                          Catch::Contains("Invalid transition"));
    }
} // }}}

TEST_CASE("mata::nft::construct() from IntermediateAut correct calls")
{ // {{{
    Nft aut;
    mata::IntermediateAut inter_aut;
    OnTheFlyAlphabet alphabet;

    SECTION("construct an empty automaton")
    {
        inter_aut.automaton_type = mata::IntermediateAut::AutomatonType::NFT;
        REQUIRE(aut.is_lang_empty());
        aut = builder::construct(inter_aut);
        REQUIRE(aut.is_lang_empty());
    }

    SECTION("construct a simple non-empty automaton accepting the empty word from intermediate automaton")
    {
        std::string file =
                "@NFT-explicit\n"
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
                "@NFT-explicit\n"
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
                "@NFT-explicit\n"
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
                "@NFT-explicit\n"
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
                "@NFT-explicit\n"
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
                "@NFT-explicit\n"
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
                "@NFT-bits\n"
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
                "@NFT-bits\n"
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

        nft::builder::NameStateMap state_map;
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
                "@NFT-bits\n"
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

        nft::builder::NameStateMap state_map;
        plumbing::construct(&aut, inter_aut, &alphabet, &state_map);
        CHECK(aut.final.empty());
    }
} // }}}

TEST_CASE("mata::nft::make_complete()")
{ // {{{
    Nft aut(11);

    SECTION("empty automaton, empty alphabet")
    {
        OnTheFlyAlphabet alph{};

        aut.make_complete(alph, 0);

        REQUIRE(aut.initial.empty());
        REQUIRE(aut.final.empty());
        REQUIRE(aut.delta.empty());
    }

    SECTION("empty automaton")
    {
        OnTheFlyAlphabet alph{ std::vector<std::string>{ "a", "b" } };

        aut.make_complete(alph, 0);

        REQUIRE(aut.initial.empty());
        REQUIRE(aut.final.empty());
        REQUIRE(aut.delta.contains(0, alph["a"], 0));
        REQUIRE(aut.delta.contains(0, alph["b"], 0));
    }

    SECTION("non-empty automaton, empty alphabet")
    {
        OnTheFlyAlphabet alphabet{};

        aut.initial = {1};

        aut.make_complete(alphabet, 0);

        REQUIRE(aut.initial.size() == 1);
        REQUIRE(*aut.initial.begin() == 1);
        REQUIRE(aut.final.empty());
        REQUIRE(aut.delta.empty());
    }

    SECTION("one-state automaton")
    {
        OnTheFlyAlphabet alph{ std::vector<std::string>{ "a", "b" } };
        const State SINK = 10;

        aut.initial = {1};

        aut.make_complete(alph, SINK);

        REQUIRE(aut.initial.size() == 1);
        REQUIRE(*aut.initial.begin() == 1);
        REQUIRE(aut.final.empty());
        REQUIRE(aut.delta.contains(1, alph["a"], SINK));
        REQUIRE(aut.delta.contains(1, alph["b"], SINK));
        REQUIRE(aut.delta.contains(SINK, alph["a"], SINK));
        REQUIRE(aut.delta.contains(SINK, alph["b"], SINK));
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

        aut.make_complete(alph, SINK);

        REQUIRE(aut.delta.contains(1, alph["a"], 2));
        REQUIRE(aut.delta.contains(1, alph["b"], SINK));
        REQUIRE(aut.delta.contains(1, alph["c"], SINK));
        REQUIRE(aut.delta.contains(2, alph["a"], 4));
        REQUIRE(aut.delta.contains(2, alph["c"], 1));
        REQUIRE(aut.delta.contains(2, alph["c"], 3));
        REQUIRE(aut.delta.contains(2, alph["b"], SINK));
        REQUIRE(aut.delta.contains(3, alph["b"], 5));
        REQUIRE(aut.delta.contains(3, alph["a"], SINK));
        REQUIRE(aut.delta.contains(3, alph["c"], SINK));
        REQUIRE(aut.delta.contains(4, alph["c"], 8));
        REQUIRE(aut.delta.contains(4, alph["a"], SINK));
        REQUIRE(aut.delta.contains(4, alph["b"], SINK));
        REQUIRE(aut.delta.contains(5, alph["a"], SINK));
        REQUIRE(aut.delta.contains(5, alph["b"], SINK));
        REQUIRE(aut.delta.contains(5, alph["c"], SINK));
        REQUIRE(aut.delta.contains(8, alph["a"], SINK));
        REQUIRE(aut.delta.contains(8, alph["b"], SINK));
        REQUIRE(aut.delta.contains(8, alph["c"], SINK));
        REQUIRE(aut.delta.contains(SINK, alph["a"], SINK));
        REQUIRE(aut.delta.contains(SINK, alph["b"], SINK));
        REQUIRE(aut.delta.contains(SINK, alph["c"], SINK));
    }
} // }}}

TEST_CASE("mata::nft::complement()")
{ // {{{
    Nft aut(3);
    Nft cmpl;

    SECTION("empty automaton, empty alphabet")
    {
        OnTheFlyAlphabet alph{};

        cmpl = complement(aut, alph, {{"algorithm", "classical"},
                                    {"minimize", "false"}});
        Nft empty_string_nft{ nft::builder::create_sigma_star_nft(&alph) };
        CHECK(are_equivalent(cmpl, empty_string_nft));
    }

    SECTION("empty automaton")
    {
        OnTheFlyAlphabet alph{ std::vector<std::string>{ "a", "b" } };

        cmpl = complement(aut, alph, {{"algorithm", "classical"},
                                    {"minimize", "false"}});

        REQUIRE(cmpl.is_in_lang({}));
        REQUIRE(cmpl.is_in_lang(Run{{ alph["a"] }, {}}));
        REQUIRE(cmpl.is_in_lang(Run{{ alph["b"] }, {}}));
        REQUIRE(cmpl.is_in_lang(Run{{ alph["a"], alph["a"]}, {}}));
        REQUIRE(cmpl.is_in_lang(Run{{ alph["a"], alph["b"], alph["b"], alph["a"] }, {}}));

        Nft sigma_star_nft{ nft::builder::create_sigma_star_nft(&alph) };
        CHECK(are_equivalent(cmpl, sigma_star_nft));
    }

    SECTION("empty automaton accepting epsilon, empty alphabet")
    {
        OnTheFlyAlphabet alph{};
        aut.initial = {1};
        aut.final = {1};

        cmpl = complement(aut, alph, {{"algorithm", "classical"},
                                    {"minimize", "false"}});

        CHECK(cmpl.is_lang_empty());
    }

    SECTION("empty automaton accepting epsilon")
    {
        OnTheFlyAlphabet alph{ std::vector<std::string>{ "a", "b" } };
        aut.initial = {1};
        aut.final = {1};

        cmpl = complement(aut, alph, {{"algorithm", "classical"},
                                    {"minimize", "false"}});

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

        cmpl = complement(aut, alph, {{"algorithm", "classical"},
                                    {"minimize", "false"}});

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

        cmpl = complement(aut, alph, {{"algorithm", "classical"},
                                    {"minimize", "true"}});
        Nft empty_string_nft{ nft::builder::create_sigma_star_nft(&alph) };
        CHECK(are_equivalent(empty_string_nft, cmpl));
    }

    SECTION("empty automaton, minimization")
    {
        OnTheFlyAlphabet alph{ std::vector<std::string>{ "a", "b" } };

        cmpl = complement(aut, alph, {{"algorithm", "classical"},
                                    {"minimize", "true"}});

        REQUIRE(cmpl.is_in_lang({}));
        REQUIRE(cmpl.is_in_lang(Run{{ alph["a"] }, {}}));
        REQUIRE(cmpl.is_in_lang(Run{{ alph["b"] }, {}}));
        REQUIRE(cmpl.is_in_lang(Run{{ alph["a"], alph["a"]}, {}}));
        REQUIRE(cmpl.is_in_lang(Run{{ alph["a"], alph["b"], alph["b"], alph["a"] }, {}}));

        Nft sigma_star_nft{ nft::builder::create_sigma_star_nft(&alph) };
        CHECK(are_equivalent(sigma_star_nft, cmpl));
    }

    SECTION("minimization vs no minimization")
    {
        OnTheFlyAlphabet alph{ std::vector<std::string>{ "a", "b" } };
        aut.initial = {0, 1};
        aut.final = {1, 2};

        aut.delta.add(1, alph["b"], 1);
        aut.delta.add(1, alph["a"], 2);
        aut.delta.add(2, alph["b"], 2);
        aut.delta.add(0, alph["a"], 1);
        aut.delta.add(0, alph["a"], 2);

        cmpl = complement(aut, alph, {{"algorithm", "classical"},
                                    {"minimize", "false"}});

        Nft cmpl_min = complement(aut, alph, {{"algorithm", "classical"},
                                    {"minimize", "true"}});

        CHECK(are_equivalent(cmpl, cmpl_min, &alph));
        CHECK(cmpl_min.num_of_states() == 4);
        CHECK(cmpl.num_of_states() == 5);
    }

} // }}}

TEST_CASE("mata::nft::is_universal()")
{ // {{{
    Nft aut(6);
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

TEST_CASE("mata::nft::is_included()")
{ // {{{
    Nft smaller(10);
    Nft bigger(16);
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

TEST_CASE("mata::nft::are_equivalent")
{
    Nft smaller(10);
    Nft bigger(16);
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
        Nft aut;
        mata::parser::create_nfa(&aut, "a*");
        Nft aut2;
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

TEST_CASE("mata::nft::revert()")
{ // {{{
    Nft aut(9);

    SECTION("empty automaton")
    {
        Nft result = revert(aut);

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

        Nft result = revert(aut);

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

        Nft result = revert(aut);

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

        Nft result = revert(aut);
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
        Nft nft{ 11 };
        FILL_WITH_AUT_A(nft);
        Nft res = revert(nft);
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
        Nft nft{ 15 };
        FILL_WITH_AUT_B(nft);
        Nft res = revert(nft);
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


TEST_CASE("mata::nft::Nft::is_deterministic()")
{ // {{{
    Nft aut('s'+1);

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

TEST_CASE("mata::nft::is_complete()")
{ // {{{
    Nft aut('q'+1);

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

        aut.make_complete(alph, 100);
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

TEST_CASE("mata::nft::is_prfx_in_lang()")
{ // {{{
    Nft aut('q'+1);

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

TEST_CASE("mata::nft::fw-direct-simulation()")
{ // {{{
    Nft aut;

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

    Nft aut_big(9);

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

TEST_CASE("mata::nft::reduce_size_by_simulation()")
{
    Nft aut;
    StateRenaming state_renaming;

    SECTION("empty automaton")
    {
        Nft result = reduce(aut, &state_renaming);

        REQUIRE(result.delta.empty());
        REQUIRE(result.initial.empty());
        REQUIRE(result.final.empty());
    }

    SECTION("simple automaton")
    {
        aut.add_state(2);
        aut.initial.insert(1);

        aut.final.insert(2);
        Nft result = reduce(aut, &state_renaming);

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


        Nft result = reduce(aut, &state_renaming);

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
        Nft result = reduce(aut.trim(), &state_renaming);
        CHECK(are_equivalent(result, aut));
    }
}

TEST_CASE("mata::nft::union_norename()") {
    Run one{{1},{}};
    Run zero{{0}, {}};

    Nft lhs(2);
    lhs.initial.insert(0);
    lhs.delta.add(0, 0, 1);
    lhs.final.insert(1);
    REQUIRE(!lhs.is_in_lang(one));
    REQUIRE(lhs.is_in_lang(zero));

    Nft rhs(2);
    rhs.initial.insert(0);
    rhs.delta.add(0, 1, 1);
    rhs.final.insert(1);
    REQUIRE(rhs.is_in_lang(one));
    REQUIRE(!rhs.is_in_lang(zero));

    SECTION("failing minimal scenario") {
        Nft result = uni(lhs, rhs);
        REQUIRE(result.is_in_lang(one));
        REQUIRE(result.is_in_lang(zero));
    }
}

TEST_CASE("mata::nft::union_inplace") {
    Run one{{1},{}};
    Run zero{{0}, {}};

    Nft lhs(2);
    lhs.initial.insert(0);
    lhs.delta.add(0, 0, 1);
    lhs.final.insert(1);
    REQUIRE(!lhs.is_in_lang(one));
    REQUIRE(lhs.is_in_lang(zero));

    Nft rhs(2);
    rhs.initial.insert(0);
    rhs.delta.add(0, 1, 1);
    rhs.final.insert(1);
    REQUIRE(rhs.is_in_lang(one));
    REQUIRE(!rhs.is_in_lang(zero));

    SECTION("failing minimal scenario") {
        Nft result = lhs.uni(rhs);
        REQUIRE(result.is_in_lang(one));
        REQUIRE(result.is_in_lang(zero));
    }

    SECTION("same automata") {
        size_t lhs_states = lhs.num_of_states();
        Nft result = lhs.uni(lhs);
        REQUIRE(result.num_of_states() == lhs_states * 2);
    }
}

TEST_CASE("mata::nft::remove_final()")
{
    Nft aut('q' + 1);

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

TEST_CASE("mata::nft::delta.remove()")
{
    Nft aut('q' + 1);

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

TEST_CASE("mata::nft::get_trans_as_sequence(}") {
    Nft aut('q' + 1);
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

TEST_CASE("mata::nft::remove_epsilon()")
{
    Nft aut{20};
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

TEST_CASE("Profile mata::nft::remove_epsilon()", "[.profiling]")
{
    for (size_t n{}; n < 100000; ++n) {
        Nft aut{20};
        FILL_WITH_AUT_A(aut);
        aut.remove_epsilon('c');
    }
}

TEST_CASE("mata::nft::get_num_of_trans()")
{
    Nft aut{20};
    FILL_WITH_AUT_A(aut);
    REQUIRE(aut.delta.num_of_transitions() == 15);
}

TEST_CASE("mata::nft::get_one_letter_aut()")
{
    Nft aut(11);
    Symbol abstract_symbol{'x'};
    FILL_WITH_AUT_A(aut);

    Nft digraph{aut.get_one_letter_aut() };

    REQUIRE(digraph.num_of_states() == aut.num_of_states());
    REQUIRE(digraph.delta.num_of_transitions() == 12);
    REQUIRE(digraph.delta.contains(1, abstract_symbol, 10));
    REQUIRE(digraph.delta.contains(10, abstract_symbol, 7));
    REQUIRE(!digraph.delta.contains(10, 'a', 7));
    REQUIRE(!digraph.delta.contains(10, 'b', 7));
    REQUIRE(!digraph.delta.contains(10, 'c', 7));
}

TEST_CASE("mata::nft::get_reachable_states()") {
    Nft aut{20};

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

TEST_CASE("mata::nft::trim() for profiling", "[.profiling],[trim]")
{
    Nft aut{20};
    FILL_WITH_AUT_A(aut);
    aut.delta.remove(1, 'a', 10);

    for (size_t i{ 0 }; i < 10000; ++i) {
        Nft new_aut{ aut };
        new_aut.trim();
    }
}

//TODO: make this a test for the new version
TEST_CASE("mata::nft::get_useful_states() for profiling", "[.profiling],[useful_states]")
{
    Nft aut{20};
    FILL_WITH_AUT_A(aut);
    aut.delta.remove(1, 'a', 10);

    for (size_t i{ 0 }; i < 10000; ++i) {
        aut.get_useful_states();
    }
}

TEST_CASE("mata::nft::trim() trivial") {
    Nft aut{1};
    aut.initial.insert(0);
    aut.final.insert(0);
    aut.trim();
}

TEST_CASE("mata::nft::trim()")
{
    Nft orig_aut{20};
    FILL_WITH_AUT_A(orig_aut);
    orig_aut.delta.remove(1, 'a', 10);


    SECTION("Without state map") {
        Nft aut{orig_aut};
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
        Nft aut{orig_aut};
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

TEST_CASE("mata::nft::Nft::delta.empty()")
{
    Nft aut{};

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

TEST_CASE("mata::nft::delta.operator[]")
{
    Nft aut{20};
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

    Nft aut1 = aut;
    aut1.delta.mutable_state_post(60);
    REQUIRE(aut1.num_of_states() == 61);
    REQUIRE(aut1.delta[60].empty());

    const Nft aut2 = aut;
    aut2.delta[60];
    REQUIRE(aut2.num_of_states() == 51);
    REQUIRE(aut2.delta[60].empty());
}

TEST_CASE("mata::nft::Nft::unify_(initial/final)()") {
    Nft nft{10};

    SECTION("No initial") {
        nft.unify_initial();
        CHECK(nft.num_of_states() == 10);
        CHECK(nft.initial.empty());
    }

    SECTION("initial==final unify final") {
        nft.initial.insert(0);
        nft.final.insert(0);
        nft.final.insert(1);
        nft.unify_final();
        REQUIRE(nft.num_of_states() == 11);
        CHECK(nft.final.size() == 1);
        CHECK(nft.final[10]);
        CHECK(nft.initial[10]);
    }

    SECTION("initial==final unify initial") {
        nft.initial.insert(0);
        nft.initial.insert(1);
        nft.final.insert(0);
        nft.unify_initial();
        REQUIRE(nft.num_of_states() == 11);
        CHECK(nft.initial.size() == 1);
        CHECK(nft.initial[10]);
        CHECK(nft.final[10]);
    }

    SECTION("Single initial") {
        nft.initial.insert(0);
        nft.unify_initial();
        CHECK(nft.num_of_states() == 10);
        CHECK(nft.initial.size() == 1);
        CHECK(nft.initial[0]);
    }

    SECTION("Multiple initial") {
        nft.initial.insert(0);
        nft.initial.insert(1);
        nft.unify_initial();
        CHECK(nft.num_of_states() == 11);
        CHECK(nft.initial.size() == 1);
        CHECK(nft.initial[10]);
    }

    SECTION("With transitions") {
        nft.initial.insert(0);
        nft.initial.insert(1);
        nft.delta.add(0, 'a', 3);
        nft.delta.add(1, 'b', 0);
        nft.delta.add(1, 'c', 1);
        nft.unify_initial();
        CHECK(nft.num_of_states() == 11);
        CHECK(nft.initial.size() == 1);
        CHECK(nft.initial[10]);
        CHECK(nft.delta.contains(10, 'a', 3));
        CHECK(nft.delta.contains(10, 'b', 0));
        CHECK(nft.delta.contains(10, 'c', 1));
        CHECK(nft.delta.contains(0, 'a', 3));
        CHECK(nft.delta.contains(1, 'b', 0));
        CHECK(nft.delta.contains(1, 'c', 1));
    }

    SECTION("No final") {
        nft.unify_final();
        CHECK(nft.num_of_states() == 10);
        CHECK(nft.final.empty());
    }

    SECTION("Single final") {
        nft.final.insert(0);
        nft.unify_final();
        CHECK(nft.num_of_states() == 10);
        CHECK(nft.final.size() == 1);
        CHECK(nft.final[0]);
    }

    SECTION("Multiple final") {
        nft.final.insert(0);
        nft.final.insert(1);
        nft.unify_final();
        CHECK(nft.num_of_states() == 11);
        CHECK(nft.final.size() == 1);
        CHECK(nft.final[10]);
    }

    SECTION("With transitions") {
        nft.final.insert(0);
        nft.final.insert(1);
        nft.delta.add(3, 'a', 0);
        nft.delta.add(4, 'b', 1);
        nft.delta.add(1, 'c', 1);
        nft.unify_final();
        CHECK(nft.num_of_states() == 11);
        CHECK(nft.final.size() == 1);
        CHECK(nft.final[10]);
        CHECK(nft.delta.contains(3, 'a', 10));
        CHECK(nft.delta.contains(4, 'b', 10));
        CHECK(nft.delta.contains(1, 'c', 10));
        CHECK(nft.delta.contains(3, 'a', 0));
        CHECK(nft.delta.contains(4, 'b', 1));
        CHECK(nft.delta.contains(1, 'c', 1));
    }

    SECTION("Bug: NFT with empty string unifying initial/final repeatedly") {
        Nft aut;
        mata::parser::create_nfa(&aut, "a*b*");
        for (size_t i{ 0 }; i < 8; ++i) {
            aut.unify_initial();
            aut.unify_final();
        }
        CHECK(true); // Check that the program does not seg fault.
    }
}

TEST_CASE("mata::nft::Nft::get_delta.epsilon_symbol_posts()") {
    Nft aut{20};
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

TEST_CASE("mata::nft::Nft::delta()") {
    Delta delta(6);
}

TEST_CASE("A segmentation fault in the nft::make_complement") {
    Nft r(1);
    OnTheFlyAlphabet alph{};
    alph["a"];
    alph["b"];

    r.initial = {0};
    r.delta.add(0, 0, 0);
    REQUIRE(not r.is_complete(&alph));
    r.make_complete(alph, 1);
    REQUIRE(r.is_complete(&alph));
}

TEST_CASE("mata::nft:: create simple automata") {
    Nft nft{ builder::create_empty_string_nft() };
    CHECK(nft.is_in_lang(Word{}));
    CHECK(get_word_lengths(nft) == std::set<std::pair<int, int>>{ std::make_pair(0, 0) });

    OnTheFlyAlphabet alphabet{ { "a", 0 }, { "b", 1 }, { "c", 2 } };
    nft = builder::create_sigma_star_nft(&alphabet);
    CHECK(nft.is_in_lang({ {}, {} }));
    CHECK(nft.is_in_lang({  0 , {} }));
    CHECK(nft.is_in_lang({  1 , {} }));
    CHECK(nft.is_in_lang({  2 , {} }));
    CHECK(nft.is_in_lang({ { 0, 1 }, {} }));
    CHECK(nft.is_in_lang({ { 1, 0 }, {} }));
    CHECK(nft.is_in_lang({ { 2, 2, 2 }, {} }));
    CHECK(nft.is_in_lang({ { 0, 1, 2, 2, 0, 1, 2, 1, 0, 0, 2, 1 }, {} }));
    CHECK(!nft.is_in_lang({  3 , {} }));
}

TEST_CASE("mata::nft::print_to_mata()") {
    Nft aut_big;
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
    Nft aut_big_from_mata = builder::construct(mata::IntermediateAut::parse_from_mf(parse_mf(aut_big_mata))[0], &int_alph);

    CHECK(are_equivalent(aut_big, aut_big_from_mata));
}

TEST_CASE("mata::nft::Nft::trim() bug") {
	Nft aut(5, {0}, {4});
	aut.delta.add(0, 122, 1);
	aut.delta.add(1, 98, 1);
	aut.delta.add(1, 122, 1);
	aut.delta.add(1, 97, 2);
	aut.delta.add(2, 122, 1);
	aut.delta.add(2, 97, 1);
	aut.delta.add(1, 97, 4);
	aut.delta.add(3, 97, 4);

	Nft aut_copy {aut};
	CHECK(are_equivalent(aut_copy.trim(), aut));
}

TEST_CASE("mata::nft::get_useful_states_tarjan") {
	SECTION("Nft 1") {
		Nft aut(5, {0}, {4});
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

	SECTION("Empty NFT") {
		Nft aut;
		mata::BoolVector bv = aut.get_useful_states();
		CHECK(bv == mata::BoolVector({}));
	}

	SECTION("Single-state NFT") {
		Nft aut(1, {0}, {});
		mata::BoolVector bv = aut.get_useful_states();
		CHECK(bv == mata::BoolVector({ 0}));
	}

	SECTION("Single-state NFT acc") {
		Nft aut(1, {0}, {0});
		mata::BoolVector bv = aut.get_useful_states();
		CHECK(bv == mata::BoolVector({ 1}));
	}

	SECTION("Nft 2") {
		Nft aut(5, {0, 1}, {2});
		aut.delta.add(0, 122, 2);
		aut.delta.add(2, 98, 3);
		aut.delta.add(1, 98, 4);
		aut.delta.add(4, 97, 3);

		mata::BoolVector bv = aut.get_useful_states();
		mata::BoolVector ref({ 1, 0, 1, 0, 0});
		CHECK(bv == ref);
	}

	SECTION("Nft 3") {
		Nft aut(2, {0, 1}, {0, 1});
		aut.delta.add(0, 122, 0);
		aut.delta.add(1, 98, 1);

		mata::BoolVector bv = aut.get_useful_states();
		mata::BoolVector ref({ 1, 1});
		CHECK(bv == ref);
	}

	SECTION("Nft no final") {
		Nft aut(5, {0}, {});
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
        Nft aut;
        mata::parser::create_nfa(&aut, "(a+b*a*)", false, EPSILON, false);

        mata::BoolVector bv = aut.get_useful_states();
        mata::BoolVector ref({ 1, 0, 1, 0, 1, 0, 1, 0, 0});
        CHECK(bv == ref);

        aut = reduce(aut.trim());
        bv = aut.get_useful_states();
        CHECK(bv == mata::BoolVector({ 1, 1, 1, 1}));
    }

    SECTION("more initials") {
        Nft aut(4, {0, 1, 2}, {0, 3});
        aut.delta.add(1, 48, 0);
        aut.delta.add(2, 53, 3);
        CHECK(aut.get_useful_states() == mata::BoolVector{ 1, 1, 1, 1});
    }
}

TEST_CASE("mata::nft::Nft::get_words") {
    SECTION("empty") {
        Nft aut;
        CHECK(aut.get_words(0) == std::set<mata::Word>());
        CHECK(aut.get_words(1) == std::set<mata::Word>());
        CHECK(aut.get_words(5) == std::set<mata::Word>());
    }

    SECTION("empty word") {
        Nft aut(1, {0}, {0});
        CHECK(aut.get_words(0) == std::set<mata::Word>{{}});
        CHECK(aut.get_words(1) == std::set<mata::Word>{{}});
        CHECK(aut.get_words(5) == std::set<mata::Word>{{}});
    }

    SECTION("noodle - one final") {
        Nft aut(3, {0}, {2});
        aut.delta.add(0, 0, 1);
        aut.delta.add(1, 1, 2);
        CHECK(aut.get_words(0) == std::set<mata::Word>{});
        CHECK(aut.get_words(1) == std::set<mata::Word>{});
        CHECK(aut.get_words(2) == std::set<mata::Word>{{0, 1}});
        CHECK(aut.get_words(3) == std::set<mata::Word>{{0, 1}});
        CHECK(aut.get_words(5) == std::set<mata::Word>{{0, 1}});
    }

    SECTION("noodle - two finals") {
        Nft aut(3, {0}, {1,2});
        aut.delta.add(0, 0, 1);
        aut.delta.add(1, 1, 2);
        CHECK(aut.get_words(0) == std::set<mata::Word>{});
        CHECK(aut.get_words(1) == std::set<mata::Word>{{0}});
        CHECK(aut.get_words(2) == std::set<mata::Word>{{0}, {0, 1}});
        CHECK(aut.get_words(3) == std::set<mata::Word>{{0}, {0, 1}});
        CHECK(aut.get_words(5) == std::set<mata::Word>{{0}, {0, 1}});
    }

    SECTION("noodle - three finals") {
        Nft aut(3, {0}, {0,1,2});
        aut.delta.add(0, 0, 1);
        aut.delta.add(1, 1, 2);
        CHECK(aut.get_words(0) == std::set<mata::Word>{{}});
        CHECK(aut.get_words(1) == std::set<mata::Word>{{}, {0}});
        CHECK(aut.get_words(2) == std::set<mata::Word>{{}, {0}, {0, 1}});
        CHECK(aut.get_words(3) == std::set<mata::Word>{{}, {0}, {0, 1}});
        CHECK(aut.get_words(5) == std::set<mata::Word>{{}, {0}, {0, 1}});
    }

    SECTION("more complex") {
        Nft aut(6, {0,1}, {1,3,4,5});
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
        Nft aut(6, {0,1}, {0,1});
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

TEST_CASE("mata::nft::Nft::get_one_level_aut") {
    #define REPLACE_DONT_CARE(delta, src, trg)\
                delta.add(src, 0, trg);\
                delta.add(src, 1, trg);\

    #define SPLIT_TRANSITION(delta, src, symbol, inter, trg)\
                ((symbol == DONT_CARE) ? (delta.add(src, 0, inter), delta.add(src, 1, inter)) : (delta.add(src, symbol, inter)));\
                REPLACE_DONT_CARE(delta, inter, trg);\

    SECTION("level_cnt == 1") {
        Nft aut(5, {0}, {3, 4});
        aut.delta.add(0, 0, 1);
        aut.delta.add(0, 1, 2);
        aut.delta.add(1, 0, 1);
        aut.delta.add(1, DONT_CARE, 3);
        aut.delta.add(2, DONT_CARE, 2);
        aut.delta.add(2, DONT_CARE, 4);
        aut.delta.add(3, 0, 1);
        aut.delta.add(3, DONT_CARE, 3);
        aut.delta.add(4, 1, 2);
        aut.delta.add(4, DONT_CARE, 4);

        Nft expected(5, {0}, {3, 4});
        expected.delta.add(0, 0, 1);
        expected.delta.add(0, 1, 2);
        expected.delta.add(1, 0, 1);
        REPLACE_DONT_CARE(expected.delta, 1, 3);
        REPLACE_DONT_CARE(expected.delta, 2, 2);
        REPLACE_DONT_CARE(expected.delta, 2, 4);
        expected.delta.add(3, 0, 1);
        REPLACE_DONT_CARE(expected.delta, 3, 3);
        expected.delta.add(4, 1, 2);
        REPLACE_DONT_CARE(expected.delta, 4, 4);

        CHECK(nfa::are_equivalent(aut.get_one_level_aut({0, 1}), expected));
        CHECK(nfa::are_equivalent(aut.get_one_level_aut().get_one_level_aut({0, 1}), expected));
        CHECK(nft::are_equivalent(aut, expected));
    }

    SECTION("level_cnt == 2") {
        Nft aut(7, {0}, {5, 6}, {0, 1, 1, 0, 0, 0, 0}, 2);
        aut.delta.add(0, 0, 1);
        aut.delta.add(0, 1, 2);
        aut.delta.add(1, DONT_CARE, 3);
        aut.delta.add(2, DONT_CARE, 4);
        aut.delta.add(3, 0, 3);
        aut.delta.add(3, 0, 5);
        aut.delta.add(4, DONT_CARE, 4);
        aut.delta.add(4, DONT_CARE, 6);
        aut.delta.add(5, DONT_CARE, 5);
        aut.delta.add(5, 0, 3);
        aut.delta.add(6, DONT_CARE, 6);
        aut.delta.add(6, 1, 4);

        Nft expected(15, {0}, {5, 6}, {0, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1}, 2);
        expected.delta.add(0, 0, 1);
        expected.delta.add(0, 1, 2);
        REPLACE_DONT_CARE(expected.delta, 1, 3);
        REPLACE_DONT_CARE(expected.delta, 2, 4);
        SPLIT_TRANSITION(expected.delta, 3, 0, 7, 3);
        SPLIT_TRANSITION(expected.delta, 3, 0, 8, 5);
        SPLIT_TRANSITION(expected.delta, 4, DONT_CARE, 10, 4);
        SPLIT_TRANSITION(expected.delta, 4, DONT_CARE, 12, 6);
        SPLIT_TRANSITION(expected.delta, 5, DONT_CARE, 13, 5);
        SPLIT_TRANSITION(expected.delta, 5, 0, 9, 3);
        SPLIT_TRANSITION(expected.delta, 6, DONT_CARE, 14, 6);
        SPLIT_TRANSITION(expected.delta, 6, 1, 11, 4);

        CHECK(nfa::are_equivalent(aut.get_one_level_aut({0, 1}), expected));
        CHECK(nfa::are_equivalent(aut.get_one_level_aut().get_one_level_aut({0, 1}), expected));
        CHECK(nft::are_equivalent(aut, expected));

    }

    SECTION("level_cnt == 4") {
        Nft aut(17, {0}, {15, 16}, {0, 1, 1, 3, 3, 0, 0, 2, 2, 0, 0, 1, 1, 2, 2, 0, 0}, 4);
        aut.delta.add(0, 0, 1);
        aut.delta.add(0, 1, 2);
        aut.delta.add(1, 0, 3);
        aut.delta.add(2, DONT_CARE, 4);
        aut.delta.add(3, 0, 5);
        aut.delta.add(4, DONT_CARE, 6);
        aut.delta.add(5, 0, 5);
        aut.delta.add(5, 0, 7);
        aut.delta.add(6, DONT_CARE, 6);
        aut.delta.add(6, DONT_CARE, 8);
        aut.delta.add(7, 0, 9);
        aut.delta.add(8, DONT_CARE, 10);
        aut.delta.add(9, 0, 11);
        aut.delta.add(10, DONT_CARE, 12);
        aut.delta.add(11, 0, 13);
        aut.delta.add(12, DONT_CARE, 14);
        aut.delta.add(13, 0, 15);
        aut.delta.add(14, DONT_CARE, 16);

        Nft expected(31, {0}, {15, 16}, {0, 1, 1, 3, 3, 0, 0, 2, 2, 0, 0, 1, 1, 2, 2, 0, 0, 2, 2, 2, 1, 1, 3, 3, 1, 2, 1, 3, 3, 3, 3}, 4);
        expected.delta.add(0, 0, 1);
        expected.delta.add(0, 1, 2);
        SPLIT_TRANSITION(expected.delta, 1, 0, 17, 3);
        SPLIT_TRANSITION(expected.delta, 2, DONT_CARE, 18, 4);
        expected.delta.add(3, 0, 5);
        REPLACE_DONT_CARE(expected.delta, 4, 6);
        expected.delta.add(5, 0, 20);
        REPLACE_DONT_CARE(expected.delta, 20, 19);
        REPLACE_DONT_CARE(expected.delta, 19, 29);
        REPLACE_DONT_CARE(expected.delta, 29, 5);
        SPLIT_TRANSITION(expected.delta, 5, 0, 21, 7);
        REPLACE_DONT_CARE(expected.delta, 6, 24);
        REPLACE_DONT_CARE(expected.delta, 24, 25);
        REPLACE_DONT_CARE(expected.delta, 25, 30);
        REPLACE_DONT_CARE(expected.delta, 30, 6);
        SPLIT_TRANSITION(expected.delta, 6, DONT_CARE, 26, 8);
        SPLIT_TRANSITION(expected.delta, 7, 0, 22, 9);
        SPLIT_TRANSITION(expected.delta, 8, DONT_CARE, 27, 10);
        expected.delta.add(9, 0, 11);
        REPLACE_DONT_CARE(expected.delta, 10, 12);
        expected.delta.add(11, 0, 13);
        REPLACE_DONT_CARE(expected.delta, 12, 14);
        SPLIT_TRANSITION(expected.delta, 13, 0, 23, 15);
        SPLIT_TRANSITION(expected.delta, 14, DONT_CARE, 28, 16);

        CHECK(nfa::are_equivalent(aut.get_one_level_aut({0, 1}), expected));
        CHECK(nfa::are_equivalent(aut.get_one_level_aut().get_one_level_aut({0, 1}), expected));
        CHECK(nft::are_equivalent(aut, expected));
    }

}

TEST_CASE("mata::nft::Nft::add_state()") {
    Nft nft{};
    State state{ nft.add_state() };
    CHECK(state == 0);
    CHECK(nft.levels[state] == 0);
    state = nft.add_state(4);
    CHECK(state == 4);
    CHECK(nft.levels[state] == 0);
    CHECK(nft.num_of_states() == 5);
    state = nft.add_state_with_level(3);
    CHECK(state == 5);
    CHECK(nft.levels[state] == 3);
    CHECK(nft.num_of_states() == 6);
    state = nft.add_state_with_level(12, 1);
    CHECK(state == 12);
    CHECK(nft.levels[state] == 1);
    CHECK(nft.num_of_states() == 13);
}


TEST_CASE("mata::nft::project_out(repeat_jump_symbol = false)") {

    SECTION("LINEAR") {
        Delta delta;
        delta.add(0, 0, 1);
        delta.add(1, 1, 2);
        delta.add(2, 2, 3);

        Nft atm(delta, { 0 }, { 3 }, { 0, 1, 2, 0 }, 3);

        SECTION("project 0") {
            Nft proj0 = project_out(atm, OrdVector<Level>{ 0 }, false);
            Nft proj0_expected(3, { 0 }, { 2 }, { 0, 1, 0 }, 2);
            proj0_expected.delta.add(0, 1, 1);
            proj0_expected.delta.add(1, 2, 2);
            CHECK(are_equivalent(proj0, proj0_expected));

        }

        SECTION("project 1") {
            Nft proj1 = project_out(atm, 1, false);
            Nft proj1_expected(3, { 0 }, { 2 }, { 0, 1, 0 }, 2);
            proj1_expected.delta.add(0, 0, 1);
            proj1_expected.delta.add(1, 2, 2);
            CHECK(are_equivalent(proj1, proj1_expected));
        }

        SECTION("project 2") {
            Nft proj2 = project_out(atm, 2, false);
            Nft proj2_expected(3, { 0 }, { 2 }, { 0, 1, 0 }, 2);
            proj2_expected.delta.add(0, 0, 1);
            proj2_expected.delta.add(1, 1, 2);
            CHECK(are_equivalent(proj2, proj2_expected));

        }
    }

    SECTION("LOOP") {
        Delta delta;
        delta.add(0, 0, 1);
        delta.add(1, 1, 2);
        delta.add(2, 2, 3);
        delta.add(0, 3, 0);
        delta.add(3, 4, 3);

        Nft atm_loop(delta, { 0 }, { 3 }, { 0, 1, 2, 0 }, 3);

        SECTION("project 0") {
            Nft proj0_loop = project_out(atm_loop, 0, false);
            Nft proj0_loop_expected(3, { 0 }, { 2 }, { 0, 1, 0 }, 2);
            proj0_loop_expected.delta.add(0, DONT_CARE, 0);
            proj0_loop_expected.delta.add(0, 1, 1);
            proj0_loop_expected.delta.add(1, 2, 2);
            proj0_loop_expected.delta.add(2, DONT_CARE, 2);
            CHECK(are_equivalent(proj0_loop, proj0_loop_expected));
        }

        SECTION("project 1") {
            Nft proj1_loop = project_out(atm_loop, 1, false);
            Nft proj1_loop_expected(3, { 0 }, { 2 }, { 0, 1, 0 }, 2);
            proj1_loop_expected.delta.add(0, 3, 0);
            proj1_loop_expected.delta.add(0, 0, 1);
            proj1_loop_expected.delta.add(1, 2, 2);
            proj1_loop_expected.delta.add(2, 4, 2);
            CHECK(are_equivalent(proj1_loop, proj1_loop_expected));
        }

        SECTION("project 2") {
            Nft proj2_loop = project_out(atm_loop, 2, false);
            Nft proj2_loop_expected(3, { 0 }, { 2 }, { 0, 1, 0 }, 2);
            proj2_loop_expected.delta.add(0, 3, 0);
            proj2_loop_expected.delta.add(0, 0, 1);
            proj2_loop_expected.delta.add(1, 1, 2);
            proj2_loop_expected.delta.add(2, 4, 2);
            CHECK(are_equivalent(proj2_loop, proj2_loop_expected));
        }

        SECTION("project 0, 1, 2") {
            Nft atm_empty(delta, { 0 }, {}, { 0, 1, 2, 0 }, 3);
            Nft proj012_empty = project_out(atm_empty, { 0, 1, 2 }, false);
            CHECK(are_equivalent(proj012_empty, Nft(1, {}, {}, {}, 0)));
        }
    }

    SECTION("COMPLEX") {
        Delta delta;
        delta.add(0, 0, 1);
        delta.add(1, 1, 2);
        delta.add(2, 2, 3);
        delta.add(0, 3, 3);
        delta.add(3, 4, 2);

        Nft nft_complex(delta, { 0 }, { 3 }, { 0, 1, 2, 0 }, 3);

        SECTION("project 0") {
            Nft proj0_complex = project_out(nft_complex, 0, false);
            Nft proj0_complex_expected(3, { 0 }, { 2 }, { 0, 1, 0 }, 2);
            proj0_complex_expected.delta.add(0, 1, 1);
            proj0_complex_expected.delta.add(0, DONT_CARE, 2);
            proj0_complex_expected.delta.add(1, 2, 2);
            proj0_complex_expected.delta.add(2, DONT_CARE, 1);
            CHECK(are_equivalent(proj0_complex, proj0_complex_expected));
        }

        SECTION("project 1") {
            Nft proj1_complex = project_out(nft_complex, 1, false);
            Nft proj1_complex_expected(3, { 0 }, { 2 }, { 0, 1, 0 }, 2);
            proj1_complex_expected.delta.add(0, 0, 1);
            proj1_complex_expected.delta.add(0, 3, 2);
            proj1_complex_expected.delta.add(1, 2, 2);
            proj1_complex_expected.delta.add(2, 4, 1);
            CHECK(are_equivalent(proj1_complex, proj1_complex_expected));
        }

        SECTION("project 2") {
            Nft proj2_complex = project_out(nft_complex, OrdVector<Level>{ 2 }, false);
            Nft proj2_complex_expected(3, { 0 }, { 2 }, { 0, 1, 0 }, 2);
            proj2_complex_expected.delta.add(0, 0, 1);
            proj2_complex_expected.delta.add(0, 3, 2);
            proj2_complex_expected.delta.add(1, 1, 2);
            proj2_complex_expected.delta.add(2, 4, 2);
            CHECK(are_equivalent(proj2_complex, proj2_complex_expected));

            proj2_complex = project_to(nft_complex, OrdVector<Level>{ 2 });
            proj2_complex_expected = Nft(2, { 0 }, { 1 }, { 0, 0 }, 1);
            proj2_complex_expected.delta.add(0, 2, 1);
            proj2_complex_expected.delta.add(0, 3, 1);
            proj2_complex_expected.delta.add(1, 2, 1);
            CHECK(are_equivalent(proj2_complex, proj2_complex_expected));
            proj2_complex_expected.delta.add(0, 0, 1);
            CHECK(!are_equivalent(proj2_complex, proj2_complex_expected));
        }

        SECTION("project 0, 1") {
            Nft proj01_complex = project_out(nft_complex, { 0, 1 }, false);
            Nft proj01_complex_expected(2, { 0 }, { 1 }, { 0, 0 }, 1);
            proj01_complex_expected.delta.add(0, 2, 1);
            proj01_complex_expected.delta.add(0, DONT_CARE, 1);
            proj01_complex_expected.delta.add(1, 2, 1);
            CHECK(are_equivalent(proj01_complex, proj01_complex_expected));
        }

        SECTION("project 0, 2") {
            Nft proj02_complex = project_out(nft_complex, { 0, 2 }, false);
            Nft proj02_complex_expected(2, { 0 }, { 1 }, { 0, 0 }, 1);
            proj02_complex_expected.delta.add(0, 1, 1);
            proj02_complex_expected.delta.add(0, DONT_CARE, 1);
            proj02_complex_expected.delta.add(1, DONT_CARE, 1);
            CHECK(are_equivalent(proj02_complex, proj02_complex_expected));
        }

        SECTION("project 1, 2") {
            Nft proj12_complex = project_out(nft_complex, { 1, 2 }, false);
            Nft proj12_complex_expected(2, { 0 }, { 1 }, { 0, 0 }, 1);
            proj12_complex_expected.delta.add(0, 0, 1);
            proj12_complex_expected.delta.add(0, 3, 1);
            proj12_complex_expected.delta.add(1, 4, 1);
            CHECK(are_equivalent(proj12_complex, proj12_complex_expected));
        }

        SECTION("project 0, 1, 2") {
            Nft proj012_complex = project_out(nft_complex, { 0, 1, 2 }, false);
            Nft proj012_complex_expected(1, { 0 }, { 0 }, {}, 0);
            CHECK(are_equivalent(proj012_complex, proj012_complex_expected));
        }
    }

    SECTION("HARD") {
        Nft atm_hard(Delta{}, { 0 , 2 }, { 7 }, { 0, 1, 0, 2, 3, 4, 5, 0 }, 6);
        atm_hard.delta.add(0, 1, 1);
        atm_hard.delta.add(2, 2, 1);
        atm_hard.delta.add(2, 3, 2);
        atm_hard.delta.add(1, 0, 3);
        atm_hard.delta.add(1, 10, 4);
        atm_hard.delta.add(3, 4, 4);
        atm_hard.delta.add(4, 5, 5);
        atm_hard.delta.add(5, 6, 6);
        atm_hard.delta.add(6, 7, 0);
        atm_hard.delta.add(6, 8, 7);
        atm_hard.delta.add(7, 9, 2);

        Nft proj_hard = project_out(atm_hard, { 0, 3, 4, 5 }, false);

        Nft proj_hard_expected(4, { 0, 1 }, { 3 }, { 0, 0, 1, 0 }, 2);
        proj_hard_expected.delta.add(0, 0, 2);
        proj_hard_expected.delta.add(0, 10, 3);
        proj_hard_expected.delta.add(1, 0, 2);
        proj_hard_expected.delta.add(1, 10, 3);
        proj_hard_expected.delta.add(1, DONT_CARE, 1);
        proj_hard_expected.delta.add(2, 4, 3);
        proj_hard_expected.delta.add(3, 10, 3);
        proj_hard_expected.delta.add(3, 0, 2);
        proj_hard_expected.delta.add(3, DONT_CARE, 1);
        CHECK(are_equivalent(proj_hard, proj_hard_expected));
    }
}

TEST_CASE("mata::nft::project_out(repeat_jump_symbol = true)") {

    SECTION("LINEAR") {
        Delta delta;
        delta.add(0, 0, 1);
        delta.add(1, 1, 2);
        delta.add(2, 2, 3);

        Nft atm(delta, { 0 }, { 3 }, { 0, 1, 2, 0 }, 3);

        SECTION("project 0") {
            Nft proj0 = project_out(atm, 0);
            Nft proj0_expected(3, { 0 }, { 2 }, { 0, 1, 0 }, 2);
            proj0_expected.delta.add(0, 1, 1);
            proj0_expected.delta.add(1, 2, 2);
            CHECK(are_equivalent(proj0, proj0_expected));

        }

        SECTION("project 1") {
            Nft proj1 = project_out(atm, 1);
            Nft proj1_expected(3, { 0 }, { 2 }, { 0, 1, 0 }, 2);
            proj1_expected.delta.add(0, 0, 1);
            proj1_expected.delta.add(1, 2, 2);
            CHECK(are_equivalent(proj1, proj1_expected));
        }

        SECTION("project 2") {
            Nft proj2 = project_out(atm, 2);
            Nft proj2_expected(3, { 0 }, { 2 }, { 0, 1, 0 }, 2);
            proj2_expected.delta.add(0, 0, 1);
            proj2_expected.delta.add(1, 1, 2);
            CHECK(are_equivalent(proj2, proj2_expected));

        }
    }

    SECTION("LOOP") {
        Delta delta;
        delta.add(0, 0, 1);
        delta.add(1, 1, 2);
        delta.add(2, 2, 3);
        delta.add(0, 3, 0);
        delta.add(3, 4, 3);

        Nft atm_loop(delta, { 0 }, { 3 }, { 0, 1, 2, 0 }, 3);

        SECTION("project 0") {
            Nft proj0_loop = project_out(atm_loop, 0);
            Nft proj0_loop_expected(3, { 0 }, { 2 }, { 0, 1, 0 }, 2);
            proj0_loop_expected.delta.add(0, 3, 0);
            proj0_loop_expected.delta.add(0, 1, 1);
            proj0_loop_expected.delta.add(1, 2, 2);
            proj0_loop_expected.delta.add(2, 4, 2);
            CHECK(are_equivalent(proj0_loop, proj0_loop_expected));
        }

        SECTION("project 1") {
            Nft proj1_loop = project_out(atm_loop, 1);
            Nft proj1_loop_expected(3, { 0 }, { 2 }, { 0, 1, 0 }, 2);
            proj1_loop_expected.delta.add(0, 3, 0);
            proj1_loop_expected.delta.add(0, 0, 1);
            proj1_loop_expected.delta.add(1, 2, 2);
            proj1_loop_expected.delta.add(2, 4, 2);
            CHECK(are_equivalent(proj1_loop, proj1_loop_expected));
        }

        SECTION("project 2") {
            Nft proj2_loop = project_out(atm_loop, 2);
            Nft proj2_loop_expected(3, { 0 }, { 2 }, { 0, 1, 0 }, 2);
            proj2_loop_expected.delta.add(0, 3, 0);
            proj2_loop_expected.delta.add(0, 0, 1);
            proj2_loop_expected.delta.add(1, 1, 2);
            proj2_loop_expected.delta.add(2, 4, 2);
            CHECK(are_equivalent(proj2_loop, proj2_loop_expected));
        }

        SECTION("project 0, 1, 2") {
            Nft atm_empty(delta, { 0 }, {}, { 0, 1, 2, 0 }, 3);
            Nft proj012_empty = project_out(atm_empty, { 0, 1, 2 });
            CHECK(are_equivalent(proj012_empty, Nft(1, {}, {}, {}, 0)));
        }
    }

    SECTION("COMPLEX") {
        Delta delta;
        delta.add(0, 0, 1);
        delta.add(1, 1, 2);
        delta.add(2, 2, 3);
        delta.add(0, 3, 3);
        delta.add(3, 4, 2);

        Nft atm_complex(delta, { 0 }, { 3 }, { 0, 1, 2, 0 }, 3);

        SECTION("project 0") {
            Nft proj0_complex = project_out(atm_complex, 0);
            Nft proj0_complex_expected(3, { 0 }, { 2 }, { 0, 1, 0 }, 2);
            proj0_complex_expected.delta.add(0, 1, 1);
            proj0_complex_expected.delta.add(0, 3, 2);
            proj0_complex_expected.delta.add(1, 2, 2);
            proj0_complex_expected.delta.add(2, 4, 1);
            CHECK(are_equivalent(proj0_complex, proj0_complex_expected));
        }

        SECTION("project 1") {
            Nft proj1_complex = project_out(atm_complex, 1);
            Nft proj1_complex_expected(3, { 0 }, { 2 }, { 0, 1, 0 }, 2);
            proj1_complex_expected.delta.add(0, 0, 1);
            proj1_complex_expected.delta.add(0, 3, 2);
            proj1_complex_expected.delta.add(1, 2, 2);
            proj1_complex_expected.delta.add(2, 4, 1);
            CHECK(are_equivalent(proj1_complex, proj1_complex_expected));
        }

        SECTION("project 2") {
            Nft proj2_complex = project_out(atm_complex, 2);
            Nft proj2_complex_expected(3, { 0 }, { 2 }, { 0, 1, 0 }, 2);
            proj2_complex_expected.delta.add(0, 0, 1);
            proj2_complex_expected.delta.add(0, 3, 2);
            proj2_complex_expected.delta.add(1, 1, 2);
            proj2_complex_expected.delta.add(2, 4, 2);
            CHECK(are_equivalent(proj2_complex, proj2_complex_expected));
        }

        SECTION("project 0, 1") {
            Nft proj01_complex = project_out(atm_complex, { 0, 1 });
            Nft proj01_complex_expected(2, { 0 }, { 1 }, { 0, 0 }, 1);
            proj01_complex_expected.delta.add(0, 2, 1);
            proj01_complex_expected.delta.add(0, 3, 1);
            proj01_complex_expected.delta.add(1, 2, 1);
            CHECK(are_equivalent(proj01_complex, proj01_complex_expected));
        }

        SECTION("project 0, 2") {
            Nft proj02_complex = project_out(atm_complex, { 0, 2 });
            Nft proj02_complex_expected(2, { 0 }, { 1 }, { 0, 0 }, 1);
            proj02_complex_expected.delta.add(0, 1, 1);
            proj02_complex_expected.delta.add(0, 3, 1);
            proj02_complex_expected.delta.add(1, 4, 1);
            CHECK(are_equivalent(proj02_complex, proj02_complex_expected));
        }

        SECTION("project 1, 2") {
            Nft proj12_complex = project_out(atm_complex, { 1, 2 });
            Nft proj12_complex_expected(2, { 0 }, { 1 }, { 0, 0 }, 1);
            proj12_complex_expected.delta.add(0, 0, 1);
            proj12_complex_expected.delta.add(0, 3, 1);
            proj12_complex_expected.delta.add(1, 4, 1);
            CHECK(are_equivalent(proj12_complex, proj12_complex_expected));
        }

        SECTION("project 0, 1, 2") {
            Nft proj012_complex = project_out(atm_complex, { 0, 1, 2 });
            Nft proj012_complex_expected(1, { 0 }, { 0 }, {}, 0);
            CHECK(are_equivalent(proj012_complex, proj012_complex_expected));
        }
    }

    SECTION("HARD") {
        Nft atm_hard(Delta{}, { 0 , 2 }, { 7 }, { 0, 1, 0, 2, 3, 4, 5, 0 }, 6);
        atm_hard.delta.add(0, 1, 1);
        atm_hard.delta.add(2, 2, 1);
        atm_hard.delta.add(2, 3, 2);
        atm_hard.delta.add(1, 0, 3);
        atm_hard.delta.add(1, 10, 4);
        atm_hard.delta.add(3, 4, 4);
        atm_hard.delta.add(4, 5, 5);
        atm_hard.delta.add(5, 6, 6);
        atm_hard.delta.add(6, 7, 0);
        atm_hard.delta.add(6, 8, 7);
        atm_hard.delta.add(7, 9, 2);

        Nft proj_hard = project_out(atm_hard, { 0, 3, 4, 5 });

        Nft proj_hard_expected(4, { 0, 1 }, { 3 }, { 0, 0, 1, 0 }, 2);
        proj_hard_expected.delta.add(0, 0, 2);
        proj_hard_expected.delta.add(0, 10, 3);
        proj_hard_expected.delta.add(1, 0, 2);
        proj_hard_expected.delta.add(1, 10, 3);
        proj_hard_expected.delta.add(1, 3, 1);
        proj_hard_expected.delta.add(2, 4, 3);
        proj_hard_expected.delta.add(3, 10, 3);
        proj_hard_expected.delta.add(3, 0, 2);
        proj_hard_expected.delta.add(3, 9, 1);
        CHECK(are_equivalent(proj_hard, proj_hard_expected));
    }
}

TEST_CASE("mata::nft::project_to()") {
    Nft nft;
    Nft projection;
    Nft expected;

    SECTION("linear") {
        nft = Nft{ {}, { 0 }, { 3 }, { 0, 1, 2, 0 }, 3 };
        nft.delta.add(0, 0, 1);
        nft.delta.add(1, 1, 2);
        nft.delta.add(2, 2, 3);
        projection = project_to(nft, 2);
        expected = Nft{ 2, { 0 }, { 1 }, { 0, 0 }, 1 };
        expected.delta.add(0, 2, 1);
        CHECK(nft::are_equivalent(projection, expected));
    }

    SECTION("linear longer") {
        nft = Nft{ {}, { 0 }, { 6 }, { 0, 1, 2, 0, 1, 2, 0 }, 3 };
        nft.delta.add(0, 0, 1);
        nft.delta.add(1, 1, 2);
        nft.delta.add(2, 2, 3);
        nft.delta.add(3, 3, 4);
        nft.delta.add(4, 4, 5);
        nft.delta.add(5, 5, 6);
        projection = project_to(nft, 2);
        expected = Nft{ {}, { 0 }, { 2 }, { 0, 0, 0 }, 1 };
        expected.delta.add(0, 2, 1);
        expected.delta.add(1, 5, 2);
        CHECK(nft::are_equivalent(projection, expected));
    }

    SECTION("linear longer symbol long jump") {
        nft = Nft{ {}, { 0 }, { 6 }, { 0, 1, 2, 0, 1, 2, 0 }, 3 };
        nft.delta.add(0, 0, 1);
        nft.delta.add(1, 1, 2);
        nft.delta.add(2, 2, 3);
        nft.delta.add(3, 3, 4);
        nft.delta.add(4, 4, 5);
        nft.delta.add(5, 5, 6);
        nft.delta.add(0, 'j', 6);
        projection = project_to(nft, 2);
        expected = Nft{ {}, { 0 }, { 2 }, { 0, 0, 0 }, 1 };
        expected.delta.add(0, 2, 1);
        expected.delta.add(1, 5, 2);
        expected.delta.add(0, 'j', 2);
        CHECK(nft::are_equivalent(projection, expected));
        expected.delta.add(0, 'b', 2);
        CHECK(!nft::are_equivalent(projection, expected));
    }

    SECTION("cycle longer") {
        nft = Nft{ {}, { 0 }, { 6 }, { 0, 1, 2, 0, 1, 2, 0, 1, 2, 1, 2 }, 3 };
        nft.delta.add(0, 0, 1);
        nft.delta.add(1, 1, 2);
        nft.delta.add(2, 2, 3);
        nft.delta.add(3, 3, 4);
        nft.delta.add(4, 4, 5);
        nft.delta.add(5, 5, 6);
        nft.delta.add(3, 6, 7);
        nft.delta.add(7, 7, 8);
        nft.delta.add(8, 8, 0);
        nft.delta.add(6, 9, 9);
        nft.delta.add(9, 9, 10);
        nft.delta.add(10, 10, 0);
        projection = project_to(nft, 2);
        expected = Nft{ {}, { 0 }, { 2 }, { 0, 0, 0 }, 1 };
        expected.delta.add(0, 2, 1);
        expected.delta.add(1, 8, 0);
        expected.delta.add(1, 5, 2);
        expected.delta.add(2, 10, 0);
        CHECK(nft::are_equivalent(projection, expected));
    }

    SECTION("cycle longer project to { 0, 2 }") {
        nft = Nft{ {}, { 0 }, { 6 }, { 0, 1, 2, 0, 1, 2, 0, 1, 2, 1, 2 }, 3 };
        nft.delta.add(0, 0, 1);
        nft.delta.add(1, 1, 2);
        nft.delta.add(2, 2, 3);
        nft.delta.add(3, 3, 4);
        nft.delta.add(4, 4, 5);
        nft.delta.add(5, 5, 6);
        nft.delta.add(3, 6, 7);
        nft.delta.add(7, 7, 8);
        nft.delta.add(8, 8, 0);
        nft.delta.add(6, 9, 9);
        nft.delta.add(9, 9, 10);
        nft.delta.add(10, 10, 0);
        projection = project_to(nft, { 0, 2 });
        expected = Nft{ {}, { 0 }, { 6 }, { 0, 1, 0, 1, 0, 1, 0 }, 2 };
        expected.delta.add(0, 0, 1);
        expected.delta.add(1, 2, 3);
        expected.delta.add(3, 6, 4);
        expected.delta.add(3, 3, 5);
        expected.delta.add(4, 8, 0);
        expected.delta.add(5, 5, 6);
        expected.delta.add(6, 9, 7);
        expected.delta.add(7, 10, 0);
        CHECK(nft::are_equivalent(projection, expected));
    }

    SECTION("cycle longer project to { 0, 2 } with epsilon and dont care symbols") {
        nft = Nft{ {}, { 0 }, { 6 }, { 0, 1, 2, 0, 1, 2, 0, 1, 2, 1, 2 }, 3 };
        nft.delta.add(0, EPSILON, 2);
        nft.delta.add(2, 2, 3);
        nft.delta.add(3, 3, 4);
        nft.delta.add(4, 4, 5);
        nft.delta.add(5, 5, 6);
        nft.delta.add(3, EPSILON, 7);
        nft.delta.add(7, 7, 8);
        nft.delta.add(8, DONT_CARE, 0);
        nft.delta.add(6, 9, 9);
        nft.delta.add(9, 10, 10);
        nft.delta.add(10, 11, 0);
        projection = project_to(nft, { 0, 2 });
        expected = Nft{ {}, { 0 }, { 6 }, { 0, 1, 0, 0, 1, 1, 0, 1 }, 2 };
        expected.delta.add(0, EPSILON, 1);
        expected.delta.add(1, 2, 3);
        expected.delta.add(3, EPSILON, 4);
        expected.delta.add(3, 3, 5);
        expected.delta.add(4, DONT_CARE, 0);
        expected.delta.add(5, 5, 6);
        expected.delta.add(6, 9, 7);
        expected.delta.add(7, 11, 0);
        CHECK(nft::are_equivalent(projection, expected));
    }
}

TEST_CASE("mata::nft::insert_level() and mata::nft::insert_levels()") {
    Delta delta;
    Nft input_nft, output_nft, expected_nft;

    SECTION("Linear - default_symbol = DONT_CARE, repeat_jump_symbol = false") {
        delta.add(0, 0, 1);
        delta.add(1, 1, 2);
        delta.add(2, 2, 3);

        input_nft = Nft(delta, { 0 }, { 3 }, { 0, 1, 2, 0 }, 3);

        SECTION("add level 0") {
            output_nft = insert_level(input_nft, 0, DONT_CARE, false);
            expected_nft = Nft(5, { 0 }, { 4 }, { 0, 1, 2, 3, 0 }, 4);
            expected_nft.delta.add(0, DONT_CARE, 1);
            expected_nft.delta.add(1, 0, 2);
            expected_nft.delta.add(2, 1, 3);
            expected_nft.delta.add(3, 2, 4);
            CHECK(are_equivalent(output_nft, expected_nft));
        }

        SECTION("add level 1") {
            output_nft = insert_level(input_nft, 1, DONT_CARE, false);
            expected_nft = Nft(5, { 0 }, { 4 }, { 0, 1, 2, 3, 0 }, 4);
            expected_nft.delta.add(0, 0, 1);
            expected_nft.delta.add(1, DONT_CARE, 2);
            expected_nft.delta.add(2, 1, 3);
            expected_nft.delta.add(3, 2, 4);
            CHECK(are_equivalent(output_nft, expected_nft));
        }

        SECTION("add level 2") {
            output_nft = insert_level(input_nft, 2, DONT_CARE, false);
            expected_nft = Nft(5, { 0 }, { 4 }, { 0, 1, 2, 3, 0 }, 4);
            expected_nft.delta.add(0, 0, 1);
            expected_nft.delta.add(1, 1, 2);
            expected_nft.delta.add(2, DONT_CARE, 3);
            expected_nft.delta.add(3, 2, 4);
            CHECK(are_equivalent(output_nft, expected_nft));
        }

        SECTION("add level 3") {
            output_nft = insert_level(input_nft, 3, DONT_CARE, false);
            expected_nft = Nft(5, { 0 }, { 4 }, { 0, 1, 2, 3, 0 }, 4);
            expected_nft.delta.add(0, 0, 1);
            expected_nft.delta.add(1, 1, 2);
            expected_nft.delta.add(2, 2, 3);
            expected_nft.delta.add(3, DONT_CARE, 4);
            CHECK(are_equivalent(output_nft, expected_nft));
        }

        SECTION("add level 4") {
            output_nft = insert_level(input_nft, 4, DONT_CARE, false);
            expected_nft = Nft(6, { 0 }, { 5 }, { 0, 1, 2, 3, 4, 0 }, 5);
            expected_nft.delta.add(0, 0, 1);
            expected_nft.delta.add(1, 1, 2);
            expected_nft.delta.add(2, 2, 3);
            expected_nft.delta.add(3, DONT_CARE, 4);
            expected_nft.delta.add(4, DONT_CARE, 5);
            CHECK(are_equivalent(output_nft, expected_nft));
        }

        SECTION("add levels according to the mask 100011") {
            output_nft = insert_levels(input_nft, { 1, 0, 0, 0, 1, 1 }, DONT_CARE, false);
            expected_nft = Nft(7, { 0 }, { 6 }, { 0, 1, 2, 3, 4, 5, 0 }, 6);
            expected_nft.delta.add(0, DONT_CARE, 1);
            expected_nft.delta.add(1, 0, 2);
            expected_nft.delta.add(2, 1, 3);
            expected_nft.delta.add(3, 2, 4);
            expected_nft.delta.add(4, DONT_CARE, 5);
            expected_nft.delta.add(5, DONT_CARE, 6);
            CHECK(are_equivalent(output_nft, expected_nft));
        }
    }

    SECTION("Linear - default_symbol = DONT_CARE, repeat_jump_symbol = true") {
        delta.add(0, 0, 1);
        delta.add(1, 1, 2);
        delta.add(2, 2, 3);

        input_nft = Nft(delta, { 0 }, { 3 }, { 0, 1, 2, 0 }, 3);

        SECTION("add level 0") {
            output_nft = insert_level(input_nft, 0);
            expected_nft = Nft(5, { 0 }, { 4 }, { 0, 1, 2, 3, 0 }, 4);
            expected_nft.delta.add(0, DONT_CARE, 1);
            expected_nft.delta.add(1, 0, 2);
            expected_nft.delta.add(2, 1, 3);
            expected_nft.delta.add(3, 2, 4);
            CHECK(are_equivalent(output_nft, expected_nft));
        }

        SECTION("add level 1") {
            output_nft = insert_level(input_nft, 1);
            expected_nft = Nft(5, { 0 }, { 4 }, { 0, 1, 2, 3, 0 }, 4);
            expected_nft.delta.add(0, 0, 1);
            expected_nft.delta.add(1, DONT_CARE, 2);
            expected_nft.delta.add(2, 1, 3);
            expected_nft.delta.add(3, 2, 4);
            CHECK(are_equivalent(output_nft, expected_nft));
        }

        SECTION("add level 2") {
            output_nft = insert_level(input_nft, 2);
            expected_nft = Nft(5, { 0 }, { 4 }, { 0, 1, 2, 3, 0 }, 4);
            expected_nft.delta.add(0, 0, 1);
            expected_nft.delta.add(1, 1, 2);
            expected_nft.delta.add(2, DONT_CARE, 3);
            expected_nft.delta.add(3, 2, 4);
            CHECK(are_equivalent(output_nft, expected_nft));
        }

        SECTION("add level 3") {
            output_nft = insert_level(input_nft, 3);
            expected_nft = Nft(5, { 0 }, { 4 }, { 0, 1, 2, 3, 0 }, 4);
            expected_nft.delta.add(0, 0, 1);
            expected_nft.delta.add(1, 1, 2);
            expected_nft.delta.add(2, 2, 3);
            expected_nft.delta.add(3, DONT_CARE, 4);
            CHECK(are_equivalent(output_nft, expected_nft));
        }

        SECTION("add level 4") {
            output_nft = insert_level(input_nft, 4);
            expected_nft = Nft(6, { 0 }, { 5 }, { 0, 1, 2, 3, 4, 0 }, 5);
            expected_nft.delta.add(0, 0, 1);
            expected_nft.delta.add(1, 1, 2);
            expected_nft.delta.add(2, 2, 3);
            expected_nft.delta.add(3, DONT_CARE, 4);
            expected_nft.delta.add(4, DONT_CARE, 5);
            CHECK(are_equivalent(output_nft, expected_nft));
        }

        SECTION("add levels according to the mask 100011") {
            output_nft = insert_levels(input_nft, { 1, 0, 0, 0, 1, 1 });
            expected_nft = Nft(7, { 0 }, { 6 }, { 0, 1, 2, 3, 4, 5, 0 }, 6);
            expected_nft.delta.add(0, DONT_CARE, 1);
            expected_nft.delta.add(1, 0, 2);
            expected_nft.delta.add(2, 1, 3);
            expected_nft.delta.add(3, 2, 4);
            expected_nft.delta.add(4, DONT_CARE, 5);
            expected_nft.delta.add(5, DONT_CARE, 6);
            CHECK(are_equivalent(output_nft, expected_nft));
        }
    }

    SECTION("Linear - default_symbol = 42, repeat_jump_symbol = false") {
        delta.clear();
        delta.add(0, 0, 1);
        delta.add(1, 1, 2);
        delta.add(2, 2, 3);

        input_nft = Nft(delta, { 0 }, { 3 }, { 0, 1, 2, 0 }, 3);

        SECTION("add level 0") {
            output_nft = insert_level(input_nft, 0, 42);
            expected_nft = Nft(5, { 0 }, { 4 }, { 0, 1, 2, 3, 0 }, 4);
            expected_nft.delta.add(0, 42, 1);
            expected_nft.delta.add(1, 0, 2);
            expected_nft.delta.add(2, 1, 3);
            expected_nft.delta.add(3, 2, 4);
            CHECK(are_equivalent(output_nft, expected_nft));
        }

        SECTION("add level 1") {
            output_nft = insert_level(input_nft, 1, 42);
            expected_nft = Nft(5, { 0 }, { 4 }, { 0, 1, 2, 3, 0 }, 4);
            expected_nft.delta.add(0, 0, 1);
            expected_nft.delta.add(1, 42, 2);
            expected_nft.delta.add(2, 1, 3);
            expected_nft.delta.add(3, 2, 4);
            CHECK(are_equivalent(output_nft, expected_nft));
        }

        SECTION("add level 2") {
            output_nft = insert_level(input_nft, 2, 42);
            expected_nft = Nft(5, { 0 }, { 4 }, { 0, 1, 2, 3, 0 }, 4);
            expected_nft.delta.add(0, 0, 1);
            expected_nft.delta.add(1, 1, 2);
            expected_nft.delta.add(2, 42, 3);
            expected_nft.delta.add(3, 2, 4);
            CHECK(are_equivalent(output_nft, expected_nft));
        }

        SECTION("add level 3") {
            output_nft = insert_level(input_nft, 3, 42);
            expected_nft = Nft(5, { 0 }, { 4 }, { 0, 1, 2, 3, 0 }, 4);
            expected_nft.delta.add(0, 0, 1);
            expected_nft.delta.add(1, 1, 2);
            expected_nft.delta.add(2, 2, 3);
            expected_nft.delta.add(3, 42, 4);
            CHECK(are_equivalent(output_nft, expected_nft));
        }

        SECTION("add level 4") {
            output_nft = insert_level(input_nft, 4, 42);
            expected_nft = Nft(6, { 0 }, { 5 }, { 0, 1, 2, 3, 4, 0 }, 5);
            expected_nft.delta.add(0, 0, 1);
            expected_nft.delta.add(1, 1, 2);
            expected_nft.delta.add(2, 2, 3);
            expected_nft.delta.add(3, 42, 4);
            expected_nft.delta.add(4, 42, 5);
            CHECK(are_equivalent(output_nft, expected_nft));
        }

        SECTION("add levels according to the mask 100011") {
            output_nft = insert_levels(input_nft, { 1, 0, 0, 0, 1, 1 }, 42);
            expected_nft = Nft(7, { 0 }, { 6 }, { 0, 1, 2, 3, 4, 5, 0 }, 6);
            expected_nft.delta.add(0, 42, 1);
            expected_nft.delta.add(1, 0, 2);
            expected_nft.delta.add(2, 1, 3);
            expected_nft.delta.add(3, 2, 4);
            expected_nft.delta.add(4, 42, 5);
            expected_nft.delta.add(5, 42, 6);
            CHECK(are_equivalent(output_nft, expected_nft));
        }
    }

    SECTION("loop - default_symbol = DONT_CARE, repeat_jump_symbol = false") {
        delta.clear();
        delta.add(0, 4, 0);
        delta.add(0, 0, 1);
        delta.add(1, 1, 2);
        delta.add(2, 2, 3);
        delta.add(3, 5, 3);

        input_nft = Nft(delta, { 0 }, { 3 }, { 0, 1, 2, 0 }, 3);

        SECTION("add level 0") {
            output_nft = insert_level(input_nft, 0, DONT_CARE, false);
            expected_nft = Nft(7, { 0 }, { 4 }, { 0, 1, 2, 3, 0, 1, 1 }, 4);
            expected_nft.delta.add(0, DONT_CARE, 5);
            expected_nft.delta.add(5, 4, 0);
            expected_nft.delta.add(0, DONT_CARE, 1);
            expected_nft.delta.add(1, 0, 2);
            expected_nft.delta.add(2, 1, 3);
            expected_nft.delta.add(3, 2, 4);
            expected_nft.delta.add(4, DONT_CARE, 6);
            expected_nft.delta.add(6, 5, 4);
            CHECK(are_equivalent(output_nft, expected_nft));
        }

        SECTION("add level 1") {
            output_nft = insert_level(input_nft, 1, DONT_CARE, false);
            expected_nft = Nft(11, { 0 }, { 4 }, { 0, 1, 2, 3, 0, 1, 1, 2, 3, 2, 3}, 4);
            expected_nft.delta.add(0, 4, 5);
            expected_nft.delta.add(5, DONT_CARE, 7);
            expected_nft.delta.add(7, DONT_CARE, 8);
            expected_nft.delta.add(8, DONT_CARE, 0);
            expected_nft.delta.add(0, 0, 1);
            expected_nft.delta.add(1, DONT_CARE, 2);
            expected_nft.delta.add(2, 1, 3);
            expected_nft.delta.add(3, 2, 4);
            expected_nft.delta.add(4, 5, 6);
            expected_nft.delta.add(6, DONT_CARE, 9);
            expected_nft.delta.add(9, DONT_CARE, 10);
            expected_nft.delta.add(10, DONT_CARE, 4);
            CHECK(are_equivalent(output_nft, expected_nft));
        }

        SECTION("add level 2") {
            output_nft = insert_level(input_nft, 2, DONT_CARE, false);
            expected_nft = Nft(9, { 0 }, { 4 }, { 0, 1, 2, 3, 0, 2, 3, 2, 3 }, 4);
            expected_nft.delta.add(0, 4, 7);
            expected_nft.delta.add(7, DONT_CARE, 8);
            expected_nft.delta.add(8, DONT_CARE, 0);
            expected_nft.delta.add(0, 0, 1);
            expected_nft.delta.add(1, 1, 2);
            expected_nft.delta.add(2, DONT_CARE, 3);
            expected_nft.delta.add(3, 2, 4);
            expected_nft.delta.add(4, 5, 5);
            expected_nft.delta.add(5, DONT_CARE, 6);
            expected_nft.delta.add(6, DONT_CARE, 4);
            CHECK(are_equivalent(output_nft, expected_nft));
        }

        SECTION("add level 3") {
            output_nft = insert_level(input_nft, 3, DONT_CARE, false);
            expected_nft = Nft(7, { 0 }, { 4 }, { 0, 1, 2, 3, 0, 3, 3 }, 4);
            expected_nft.delta.add(0, 4, 5);
            expected_nft.delta.add(5, DONT_CARE, 0);
            expected_nft.delta.add(0, 0, 1);
            expected_nft.delta.add(1, 1, 2);
            expected_nft.delta.add(2, 2, 3);
            expected_nft.delta.add(3, DONT_CARE, 4);
            expected_nft.delta.add(4, 5, 6);
            expected_nft.delta.add(6, DONT_CARE, 4);
            CHECK(are_equivalent(output_nft, expected_nft));
        }

        SECTION("add levels according to the mask 1010011") {
            output_nft = insert_levels(input_nft, { 1, 0, 1, 0, 0, 1, 1 }, DONT_CARE, false);
            expected_nft = Nft(20, { 0 }, { 7 }, { 0, 1, 2, 3, 4, 5, 6, 0, 1, 2, 3, 4, 5, 6, 1, 2, 3, 4, 5, 6 }, 7);
            expected_nft.delta.add(0, DONT_CARE, 8);
            expected_nft.delta.add(8, 4, 9);
            expected_nft.delta.add(9, DONT_CARE, 10);
            expected_nft.delta.add(10, DONT_CARE, 11);
            expected_nft.delta.add(11, DONT_CARE, 12);
            expected_nft.delta.add(12, DONT_CARE, 13);
            expected_nft.delta.add(13, DONT_CARE, 0);
            expected_nft.delta.add(0, DONT_CARE, 1);
            expected_nft.delta.add(1, 0, 2);
            expected_nft.delta.add(2, DONT_CARE, 3);
            expected_nft.delta.add(3, 1, 4);
            expected_nft.delta.add(4, 2, 5);
            expected_nft.delta.add(5, DONT_CARE, 6);
            expected_nft.delta.add(6, DONT_CARE, 7);
            expected_nft.delta.add(7, DONT_CARE, 14);
            expected_nft.delta.add(14, 5, 15);
            expected_nft.delta.add(15, DONT_CARE, 16);
            expected_nft.delta.add(16, DONT_CARE, 17);
            expected_nft.delta.add(17, DONT_CARE, 18);
            expected_nft.delta.add(18, DONT_CARE, 19);
            expected_nft.delta.add(19, DONT_CARE, 7);
            CHECK(are_equivalent(output_nft, expected_nft));
        }
    }

    SECTION("loop - default_symbol = 42, repeat_jump_symbol = true") {
        delta.clear();
        delta.add(0, 4, 0);
        delta.add(0, 0, 1);
        delta.add(1, 1, 2);
        delta.add(2, 2, 3);
        delta.add(3, 5, 3);

        input_nft = Nft(delta, { 0 }, { 3 }, { 0, 1, 2, 0 }, 3);

        SECTION("add level 0") {
            output_nft = insert_level(input_nft, 0, 42);
            expected_nft = Nft(11, { 0 }, { 4 }, { 0, 1, 2, 3, 0, 3, 3, 1, 1, 2, 2, 3, 3 }, 4);
            expected_nft.delta.add(0, 42, 5);
            expected_nft.delta.add(5, 4, 7);
            expected_nft.delta.add(7, 4, 10);
            expected_nft.delta.add(10, 4, 0);
            expected_nft.delta.add(0, 42, 1);
            expected_nft.delta.add(1, 0, 2);
            expected_nft.delta.add(2, 1, 3);
            expected_nft.delta.add(3, 2, 4);
            expected_nft.delta.add(4, 42, 6);
            expected_nft.delta.add(6, 5, 8);
            expected_nft.delta.add(8, 5, 9);
            expected_nft.delta.add(9, 5, 4);
            CHECK(are_equivalent(output_nft, expected_nft));
        }

        SECTION("add level 1") {
            output_nft = insert_level(input_nft, 1, 42);
            expected_nft = Nft(11, { 0 }, { 4 }, { 0, 1, 2, 3, 0, 1, 1, 2, 3, 2, 3}, 4);
            expected_nft.delta.add(0, 4, 5);
            expected_nft.delta.add(5, 42, 7);
            expected_nft.delta.add(7, 4, 8);
            expected_nft.delta.add(8, 4, 0);
            expected_nft.delta.add(0, 0, 1);
            expected_nft.delta.add(1, 42, 2);
            expected_nft.delta.add(2, 1, 3);
            expected_nft.delta.add(3, 2, 4);
            expected_nft.delta.add(4, 5, 6);
            expected_nft.delta.add(6, 42, 9);
            expected_nft.delta.add(9, 5, 10);
            expected_nft.delta.add(10, 5, 4);
            CHECK(are_equivalent(output_nft, expected_nft));
        }

        SECTION("add level 2") {
            output_nft = insert_level(input_nft, 2, 42);
            expected_nft = Nft(11, { 0 }, { 4 }, { 0, 1, 2, 3, 0, 1, 3, 1, 3, 2, 2 }, 4);
            expected_nft.delta.add(0, 4, 7);
            expected_nft.delta.add(7, 4, 10);
            expected_nft.delta.add(10, 42, 8);
            expected_nft.delta.add(8, 4, 0);
            expected_nft.delta.add(0, 0, 1);
            expected_nft.delta.add(1, 1, 2);
            expected_nft.delta.add(2, 42, 3);
            expected_nft.delta.add(3, 2, 4);
            expected_nft.delta.add(4, 5, 5);
            expected_nft.delta.add(5, 5, 9);
            expected_nft.delta.add(9, 42, 6);
            expected_nft.delta.add(6, 5, 4);
            CHECK(are_equivalent(output_nft, expected_nft));
        }

        SECTION("add level 3") {
            output_nft = insert_level(input_nft, 3, 42);
            expected_nft = Nft(11, { 0 }, { 4 }, { 0, 1, 2, 3, 0, 1, 2, 3, 1, 2, 3 }, 4);
            expected_nft.delta.add(0, 4, 5);
            expected_nft.delta.add(5, 4, 6);
            expected_nft.delta.add(6, 4, 7);
            expected_nft.delta.add(7, 42, 0);
            expected_nft.delta.add(0, 0, 1);
            expected_nft.delta.add(1, 1, 2);
            expected_nft.delta.add(2, 2, 3);
            expected_nft.delta.add(3, 42, 4);
            expected_nft.delta.add(4, 5, 8);
            expected_nft.delta.add(8, 5, 9);
            expected_nft.delta.add(9, 5, 10);
            expected_nft.delta.add(10, 42, 4);
            CHECK(are_equivalent(output_nft, expected_nft));
        }

        SECTION("add levels according to the mask 1010011") {
            output_nft = insert_levels(input_nft, { 1, 0, 1, 0, 0, 1, 1 }, 42);
            expected_nft = Nft(20, { 0 }, { 7 }, { 0, 1, 2, 3, 4, 5, 6, 0, 1, 2, 3, 4, 5, 6, 1, 2, 3, 4, 5, 6 }, 7);
            expected_nft.delta.add(0, 42, 8);
            expected_nft.delta.add(8, 4, 9);
            expected_nft.delta.add(9, 42, 10);
            expected_nft.delta.add(10, 4, 11);
            expected_nft.delta.add(11, 4, 12);
            expected_nft.delta.add(12, 42, 13);
            expected_nft.delta.add(13, 42, 0);
            expected_nft.delta.add(0, 42, 1);
            expected_nft.delta.add(1, 0, 2);
            expected_nft.delta.add(2, 42, 3);
            expected_nft.delta.add(3, 1, 4);
            expected_nft.delta.add(4, 2, 5);
            expected_nft.delta.add(5, 42, 6);
            expected_nft.delta.add(6, 42, 7);
            expected_nft.delta.add(7, 42, 14);
            expected_nft.delta.add(14, 5, 15);
            expected_nft.delta.add(15, 42, 16);
            expected_nft.delta.add(16, 5, 17);
            expected_nft.delta.add(17, 5, 18);
            expected_nft.delta.add(18, 42, 19);
            expected_nft.delta.add(19, 42, 7);
            CHECK(are_equivalent(output_nft, expected_nft));
        }
    }

    SECTION("complex - default_symbol = DONT_CARE, repeat_jump_symbol = false") {
        delta.clear();
        delta.add(0, 0, 1);
        delta.add(0, 4, 2);
        delta.add(1, 1, 2);
        delta.add(1, 5, 3);
        delta.add(2, 2, 3);
        delta.add(3, 3, 0);

        input_nft = Nft(delta, { 0 }, { 3 }, { 0, 1, 2, 0 }, 3);

        SECTION("add level 0") {
            output_nft = insert_level(input_nft, 0, DONT_CARE, false);
            expected_nft = Nft(7, { 0 }, { 3 }, { 0, 2, 3, 0, 1, 1, 1 }, 4);
            expected_nft.delta.add(0, DONT_CARE, 4);
            expected_nft.delta.add(0, DONT_CARE, 5);
            expected_nft.delta.add(4, 0, 1);
            expected_nft.delta.add(5, 4, 2);
            expected_nft.delta.add(1, 5, 3);
            expected_nft.delta.add(1, 1, 2);
            expected_nft.delta.add(2, 2, 3);
            expected_nft.delta.add(3, DONT_CARE, 6);
            expected_nft.delta.add(6, 3, 0);
            CHECK(are_equivalent(output_nft, expected_nft));
        }

        SECTION("add level 1") {
            output_nft = insert_level(input_nft, 1, DONT_CARE, false);
            expected_nft = Nft(8, { 0 }, { 3 }, { 0, 1, 3, 0, 2, 2, 2, 2 }, 4);
            expected_nft.delta.add(0, 0, 1);
            expected_nft.delta.add(0, 4, 6);
            expected_nft.delta.add(1, DONT_CARE, 4);
            expected_nft.delta.add(1, DONT_CARE, 5);
            expected_nft.delta.add(4, 1, 2);
            expected_nft.delta.add(5, 5, 3);
            expected_nft.delta.add(6, DONT_CARE, 2);
            expected_nft.delta.add(2, 2, 3);
            expected_nft.delta.add(3, 3, 7);
            expected_nft.delta.add(7, DONT_CARE, 0);
            CHECK(are_equivalent(output_nft, expected_nft));
        }

        SECTION("add level 2") {
            output_nft = insert_level(input_nft, 2, DONT_CARE, false);
            expected_nft = Nft(7, { 0 }, { 3 }, {0, 1, 2, 0, 2, 3, 2 }, 4 );
            expected_nft.delta.add(0, 0, 1);
            expected_nft.delta.add(0, 4, 2);
            expected_nft.delta.add(1, 1, 2);
            expected_nft.delta.add(1, 5, 4);
            expected_nft.delta.add(2, DONT_CARE, 5);
            expected_nft.delta.add(5, 2, 3);
            expected_nft.delta.add(4, DONT_CARE, 3);
            expected_nft.delta.add(3, 3, 6);
            expected_nft.delta.add(6, DONT_CARE, 0);
            CHECK(are_equivalent(output_nft, expected_nft));
        }

        SECTION("add level 3") {
            output_nft = insert_level(input_nft, 3, DONT_CARE, false);
            expected_nft = Nft(7, { 0 }, { 3 }, {0, 1, 2, 0, 3, 3, 3 }, 4 );
            expected_nft.delta.add(0, 0, 1);
            expected_nft.delta.add(0, 4, 2);
            expected_nft.delta.add(1, 1, 2);
            expected_nft.delta.add(1, 5, 4);
            expected_nft.delta.add(2, 2, 5);
            expected_nft.delta.add(5, DONT_CARE, 3);
            expected_nft.delta.add(4, DONT_CARE, 3);
            expected_nft.delta.add(3, 3, 6);
            expected_nft.delta.add(6, DONT_CARE, 0);
            CHECK(are_equivalent(output_nft, expected_nft));
        }

        SECTION("add levels according to the mask 1010011") {
            output_nft = insert_levels(input_nft, { 1, 0, 1, 0, 0, 1, 1 }, DONT_CARE, false);
            expected_nft = Nft(21, { 0 }, { 3 }, { 0, 2, 4, 0, 1, 1, 3, 3, 3, 5, 5, 6, 6, 1, 5, 6, 3, 2, 4, 2, 4 }, 7);
            expected_nft.delta.add(0, DONT_CARE, 5);
            expected_nft.delta.add(5, 0, 1);
            expected_nft.delta.add(0, DONT_CARE, 4);
            expected_nft.delta.add(4, 4, 17);
            expected_nft.delta.add(17, DONT_CARE, 8);
            expected_nft.delta.add(8, DONT_CARE, 2);
            expected_nft.delta.add(1, DONT_CARE, 6);
            expected_nft.delta.add(1, DONT_CARE, 7);
            expected_nft.delta.add(6, 5, 18);
            expected_nft.delta.add(18, DONT_CARE, 9);
            expected_nft.delta.add(9, DONT_CARE, 11);
            expected_nft.delta.add(11, DONT_CARE, 3);
            expected_nft.delta.add(7, 1, 2);
            expected_nft.delta.add(2, 2, 10);
            expected_nft.delta.add(10, DONT_CARE, 12);
            expected_nft.delta.add(12, DONT_CARE, 3);
            expected_nft.delta.add(3, DONT_CARE, 13);
            expected_nft.delta.add(13, 3, 19);
            expected_nft.delta.add(19, DONT_CARE, 16);
            expected_nft.delta.add(16, DONT_CARE, 20);
            expected_nft.delta.add(20, DONT_CARE, 14);
            expected_nft.delta.add(14, DONT_CARE, 15);
            expected_nft.delta.add(15, DONT_CARE, 0);
            CHECK(are_equivalent(output_nft, expected_nft));
        }
    }

    SECTION("Complex - default_symbol = 42, repeat_jump_symbol = false") {
        delta.clear();
        delta.add(0, 0, 1);
        delta.add(0, 4, 2);
        delta.add(1, 1, 2);
        delta.add(1, 5, 3);
        delta.add(2, 2, 3);
        delta.add(3, 3, 0);

        input_nft = Nft(delta, { 0 }, { 3 }, { 0, 1, 2, 0 }, 3);

        output_nft = insert_levels(input_nft, { 1, 0, 1, 0, 0, 1, 1 }, 42, false);
        expected_nft = Nft(21, { 0 }, { 3 }, { 0, 2, 4, 0, 1, 1, 3, 3, 3, 5, 5, 6, 6, 1, 5, 6, 3, 2, 4, 2, 4 }, 7);
        expected_nft.delta.add(0, 42, 5);
        expected_nft.delta.add(5, 0, 1);
        expected_nft.delta.add(0, 42, 4);
        expected_nft.delta.add(4, 4, 17);
        expected_nft.delta.add(17, 42, 8);
        expected_nft.delta.add(8, DONT_CARE, 2);
        expected_nft.delta.add(1, 42, 6);
        expected_nft.delta.add(1, 42, 7);
        expected_nft.delta.add(6, 5, 18);
        expected_nft.delta.add(18, DONT_CARE, 9);
        expected_nft.delta.add(9, 42, 11);
        expected_nft.delta.add(11, 42, 3);
        expected_nft.delta.add(7, 1, 2);
        expected_nft.delta.add(2, 2, 10);
        expected_nft.delta.add(10, 42, 12);
        expected_nft.delta.add(12, 42, 3);
        expected_nft.delta.add(3, 42, 13);
        expected_nft.delta.add(13, 3, 19);
        expected_nft.delta.add(19, 42, 16);
        expected_nft.delta.add(16, DONT_CARE, 20);
        expected_nft.delta.add(20, DONT_CARE, 14);
        expected_nft.delta.add(14, 42, 15);
        expected_nft.delta.add(15, 42, 0);
        CHECK(are_equivalent(output_nft, expected_nft));
    }

    SECTION("Complex - default_symbol = 42, repeat_jump_symbol = true") {
        delta.clear();
        delta.add(0, 0, 1);
        delta.add(0, 4, 2);
        delta.add(1, 1, 2);
        delta.add(1, 5, 3);
        delta.add(2, 2, 3);
        delta.add(3, 3, 0);

        input_nft = Nft(delta, { 0 }, { 3 }, { 0, 1, 2, 0 }, 3);

        output_nft = insert_levels(input_nft, { 1, 0, 1, 0, 0, 1, 1 }, 42);
        expected_nft = Nft(21, { 0 }, { 3 }, { 0, 2, 4, 0, 1, 1, 3, 3, 3, 5, 5, 6, 6, 1, 5, 6, 3, 2, 4, 2, 4 }, 7);
        expected_nft.delta.add(0, 42, 5);
        expected_nft.delta.add(5, 0, 1);
        expected_nft.delta.add(0, 42, 4);
        expected_nft.delta.add(4, 4, 17);
        expected_nft.delta.add(17, 42, 8);
        expected_nft.delta.add(8, 4, 2);
        expected_nft.delta.add(1, 42, 6);
        expected_nft.delta.add(1, 42, 7);
        expected_nft.delta.add(6, 5, 18);
        expected_nft.delta.add(18, 5, 9);
        expected_nft.delta.add(9, 42, 11);
        expected_nft.delta.add(11, 42, 3);
        expected_nft.delta.add(7, 1, 2);
        expected_nft.delta.add(2, 2, 10);
        expected_nft.delta.add(10, 42, 12);
        expected_nft.delta.add(12, 42, 3);
        expected_nft.delta.add(3, 42, 13);
        expected_nft.delta.add(13, 3, 19);
        expected_nft.delta.add(19, 42, 16);
        expected_nft.delta.add(16, 3, 20);
        expected_nft.delta.add(20, 3, 14);
        expected_nft.delta.add(14, 42, 15);
        expected_nft.delta.add(15, 42, 0);
        CHECK(are_equivalent(output_nft, expected_nft));
    }
}

TEST_CASE("mata::nft::Nft::insert_word()") {
    Delta delta;
    delta.add(0, 0, 1);
    delta.add(0, 4, 0);
    delta.add(1, 1, 2);
    delta.add(1, 5, 1);
    delta.add(2, 2, 3);
    delta.add(2, 6, 2);
    delta.add(3, 3, 4);
    delta.add(3, 7, 3);

    Nft nft, expected;

    SECTION("Insert 'a'") {
        SECTION("levels_cnt == 1") {
            nft = Nft(delta, { 0 }, { 4 }, { 0, 0, 0, 0, 0 }, 1);
            nft.insert_word(1, {'a'}, 3);

            expected = Nft(delta, { 0 }, { 4 }, { 0, 0, 0, 0, 0 }, 1);
            expected.delta.add(1, 'a', 3);

            CHECK(are_equivalent(nft, expected));
        }

        SECTION("levels_cnt == 3") {
            nft = Nft(delta, { 0 }, { 4 }, { 0, 0, 0, 0, 0 }, 3);
            nft.insert_word(1, {'a'}, 3);

            expected = Nft(delta, { 0 }, { 4 }, { 0, 0, 0, 0, 0 }, 3);
            expected.delta.add(1, 'a', 3);

            CHECK(are_equivalent(nft, expected));
        }

        SECTION("self-loop, levels_cnt == 1") {
            nft = Nft(delta, { 0 }, { 4 }, { 0, 0, 0, 0, 0 }, 1);
            nft.insert_word(3, {'a'}, 3);

            expected = Nft(delta, { 0 }, { 4 }, { 0, 0, 0, 0, 0 }, 1);
            expected.delta.add(3, 'a', 3);

            CHECK(are_equivalent(nft, expected));
        }

        SECTION("self-loop, levels_cnt == 3") {
            nft = Nft(delta, { 0 }, { 4 }, { 0, 0, 0, 0, 0 }, 3);
            nft.insert_word(3, {'a'}, 3);

            expected = Nft(delta, { 0 }, { 4 }, { 0, 0, 0, 0, 0 }, 3);
            expected.delta.add(3, 'a', 3);

            CHECK(are_equivalent(nft, expected));
        }

    }

    SECTION("Insert 'ab'") {
        SECTION("levels_cnt == 1") {
            nft = Nft(delta, { 0 }, { 4 }, { 0, 0, 0, 0, 0 }, 1);
            nft.insert_word(1, {'a', 'b'}, 3);

            expected = Nft(delta, { 0 }, { 4 }, { 0, 0, 0, 0, 0, 0 }, 1);
            expected.delta.add(1, 'a', 5);
            expected.delta.add(5, 'b', 3);

            CHECK(are_equivalent(nft, expected));
        }

        SECTION("levels_cnt == 3") {
            nft = Nft(delta, { 0 }, { 4 }, { 0, 0, 0, 0, 0 }, 3);
            nft.insert_word(1, {'a', 'b'}, 3);

            expected = Nft(delta, { 0 }, { 4 }, { 0, 0, 0, 0, 0, 1 }, 3);
            expected.delta.add(1, 'a', 5);
            expected.delta.add(5, 'b', 3);

            CHECK(are_equivalent(nft, expected));
        }

        SECTION("self-loop, levels_cnt == 1") {
            nft = Nft(delta, { 0 }, { 4 }, { 0, 0, 0, 0, 0 }, 1);
            nft.insert_word(3, {'a', 'b'}, 3);

            expected = Nft(delta, { 0 }, { 4 }, { 0, 0, 0, 0, 0, 0 }, 1);
            expected.delta.add(3, 'a', 5);
            expected.delta.add(5, 'b', 3);

            CHECK(are_equivalent(nft, expected));
        }

        SECTION("self-loop, levels_cnt == 3") {
            nft = Nft(delta, { 0 }, { 4 }, { 0, 0, 0, 0, 0 }, 3);
            nft.insert_word(3, {'a', 'b'}, 3);

            expected = Nft(delta, { 0 }, { 4 }, { 0, 0, 0, 0, 0, 1 }, 3);
            expected.delta.add(3, 'a', 5);
            expected.delta.add(5, 'b', 3);

            CHECK(are_equivalent(nft, expected));
        }
    }

    SECTION("Insert 'abcd'") {
        SECTION("levels_cnt == 1") {
            nft = Nft(delta, { 0 }, { 4 }, { 0, 0, 0, 0, 0}, 1);
            nft.insert_word(1, {'a', 'b', 'c', 'd'}, 3);

            expected = Nft(delta, { 0 }, { 4 }, { 0, 0, 0, 0, 0, 0, 0, 0}, 1);
            expected.delta.add(1, 'a', 5);
            expected.delta.add(5, 'b', 6);
            expected.delta.add(6, 'c', 7);
            expected.delta.add(7, 'd', 3);

            CHECK(are_equivalent(nft, expected));
        }

        SECTION("levels_cnt == 3") {
            nft = Nft(delta, { 0 }, { 4 }, { 0, 0, 0, 0, 0}, 3);
            nft.insert_word(1, {'a', 'b', 'c', 'd'}, 3);

            expected = Nft(delta, { 0 }, { 4 }, { 0, 0, 0, 0, 0, 1, 2, 0}, 3);
            expected.delta.add(1, 'a', 5);
            expected.delta.add(5, 'b', 6);
            expected.delta.add(6, 'c', 7);
            expected.delta.add(7, 'd', 3);

            CHECK(are_equivalent(nft, expected));
        }

        SECTION("self-loop, levels_cnt == 1") {
            nft = Nft(delta, { 0 }, { 4 }, { 0, 0, 0, 0, 0}, 1);
            nft.insert_word(3, {'a', 'b', 'c', 'd'}, 3);

            expected = Nft(delta, { 0 }, { 4 }, { 0, 0, 0, 0, 0, 0, 0, 0}, 1);
            expected.delta.add(3, 'a', 5);
            expected.delta.add(5, 'b', 6);
            expected.delta.add(6, 'c', 7);
            expected.delta.add(7, 'd', 3);

            CHECK(are_equivalent(nft, expected));
        }

        SECTION("self-loop, levels_cnt == 3") {
            nft = Nft(delta, { 0 }, { 4 }, { 0, 0, 0, 0, 0}, 3);
            nft.insert_word(3, {'a', 'b', 'c', 'd'}, 3);

            expected = Nft(delta, { 0 }, { 4 }, { 0, 0, 0, 0, 0, 1, 2, 0}, 3);
            expected.delta.add(3, 'a', 5);
            expected.delta.add(5, 'b', 6);
            expected.delta.add(6, 'c', 7);
            expected.delta.add(7, 'd', 3);

            CHECK(are_equivalent(nft, expected));
        }
    }
}

TEST_CASE("mata::nft::Nft::insert_identity()") {
    Nft nft, expected;
    SECTION("Creating an identity on two states (both initial and final) with empty delta.") {
        Delta delta{};
        SECTION("levels_cnt == 1") {
            nft = Nft(delta, { 0, 1 }, { 0, 1 }, { 0, 0 }, 1);
            nft.insert_identity(0, 'a');
            nft.insert_identity(1, 'b');

            expected = Nft(2, { 0, 1 }, { 0, 1 }, { 0, 0 }, 1);
            expected.delta.add(0, 'a', 0);
            expected.delta.add(1, 'b', 1);

            CHECK(are_equivalent(nft, expected));
        }

        SECTION("levels_cnt == 2") {
            nft = Nft(delta, { 0, 1 }, { 0, 1 }, { 0, 0 }, 2);
            nft.insert_identity(0, 'a');
            nft.insert_identity(1, 'b');

            expected = Nft(4, { 0, 1 }, { 0, 1 }, { 0, 0, 1, 1 }, 2);
            expected.delta.add(0, 'a', 2);
            expected.delta.add(2, 'a', 0);
            expected.delta.add(1, 'b', 3);
            expected.delta.add(3, 'b', 1);

            CHECK(are_equivalent(nft, expected));
        }

        SECTION("levels_cnt == 4") {
            nft = Nft(delta, { 0, 1 }, { 0, 1 }, { 0, 0 }, 4);
            nft.insert_identity(0, 'a');
            nft.insert_identity(1, 'b');

            expected = Nft(8, { 0, 1 }, { 0, 1 }, { 0, 0, 1, 1, 2, 2, 3, 3 }, 4);
            expected.delta.add(0, 'a', 2);
            expected.delta.add(2, 'a', 4);
            expected.delta.add(4, 'a', 6);
            expected.delta.add(6, 'a', 0);
            expected.delta.add(1, 'b', 3);
            expected.delta.add(3, 'b', 5);
            expected.delta.add(5, 'b', 7);
            expected.delta.add(7, 'b', 1);

            CHECK(are_equivalent(nft, expected));
        }
    }

    SECTION("Creating an identity on a state with incoming and oncoming transitions.") {
        Delta delta{};
        delta.add(0, 'a', 1);
        delta.add(1, 'b', 2);

        SECTION("levels_cnt == 1") {
            nft = Nft(delta, { 0 }, { 2 }, { 0, 0, 0 }, 1);
            nft.insert_identity(1, 'c');

            expected = Nft(delta, { 0 }, { 2 }, { 0, 0, 0 }, 1);
            expected.delta.add(1, 'c', 1);

            CHECK(are_equivalent(nft, expected));
        }

        SECTION("levels_cnt == 2") {
            nft = Nft(delta, { 0 }, { 2 }, { 0, 0, 0 }, 2);
            nft.insert_identity(1, 'c');

            expected = Nft(delta, { 0 }, { 2 }, { 0, 0, 0, 1 }, 2);
            expected.delta.add(1, 'c', 3);
            expected.delta.add(3, 'c', 1);

            CHECK(are_equivalent(nft, expected));
        }

        SECTION("levels_cnt == 4") {
            nft = Nft(delta, { 0 }, { 2 }, { 0, 0, 0 }, 4);
            nft.insert_identity(1, 'c');

            expected = Nft(delta, { 0 }, { 2 }, { 0, 0, 0, 1, 2, 3 }, 4);
            expected.delta.add(1, 'c', 3);
            expected.delta.add(3, 'c', 4);
            expected.delta.add(4, 'c', 5);
            expected.delta.add(5, 'c', 1);

            CHECK(are_equivalent(nft, expected));
        }
    }

    SECTION("Creating an identity on a state with only incoming transitions.") {
        Delta delta{};
        delta.add(0, 'a', 1);
        delta.add(1, 'b', 2);

        SECTION("levels_cnt == 1") {
            nft = Nft(delta, { 0 }, { 2 }, { 0, 0, 0 }, 1);
            nft.insert_identity(2, 'c');

            expected = Nft(delta, { 0 }, { 2 }, { 0, 0, 0 }, 1);
            expected.delta.add(2, 'c', 2);

            CHECK(are_equivalent(nft, expected));
        }

        SECTION("levels_cnt == 2") {
            nft = Nft(delta, { 0 }, { 2 }, { 0, 0, 0 }, 2);
            nft.insert_identity(2, 'c');

            expected = Nft(delta, { 0 }, { 2 }, { 0, 0, 0, 1 }, 2);
            expected.delta.add(2, 'c', 3);
            expected.delta.add(3, 'c', 2);

            CHECK(are_equivalent(nft, expected));
        }

        SECTION("levels_cnt == 4") {
            nft = Nft(delta, { 0 }, { 2 }, { 0, 0, 0 }, 4);
            nft.insert_identity(2, 'c');

            expected = Nft(delta, { 0 }, { 2 }, { 0, 0, 0, 1, 2, 3 }, 4);
            expected.delta.add(2, 'c', 3);
            expected.delta.add(3, 'c', 4);
            expected.delta.add(4, 'c', 5);
            expected.delta.add(5, 'c', 2);

            CHECK(are_equivalent(nft, expected));
        }
    }
}
