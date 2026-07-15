// Tests for the tuple-oriented Nft::post(...) family and the Nft::is_in_lang(...) /
// Nft::is_prefix_in_lang(...) methods built on top of them.
//
// These methods interpret an NFT as a k-tape machine (k = num_of_levels): a single
// "transducer step" walks a path of zero-level -> level 1 -> ... -> level k-1 -> zero-level
// states, reading one symbol per level from the corresponding tape (word). An EPSILON
// transition on a level consumes nothing on that tape, a DONT_CARE transition matches any
// single real input symbol (but not epsilon), and a jump transition (target skipping levels)
// reads the same symbol on every skipped level (JumpMode::RepeatSymbol) or the symbol on the
// first level and DONT_CARE on the rest (JumpMode::AppendDontCares).

#include <catch2/catch_test_macros.hpp>

#include "mata/nft/nft.hh"

using namespace mata;
using namespace mata::nft;
using mata::Symbol;
using mata::Word;

namespace {

// Readable symbol names. Note: nft::EPSILON is the max symbol and nft::DONT_CARE == EPSILON - 1.
constexpr Symbol a{ 0 };
constexpr Symbol b{ 1 };
constexpr Symbol c{ 2 };
constexpr Symbol d{ 3 };
constexpr Symbol x{ 10 };
constexpr Symbol y{ 11 };
constexpr Symbol z{ 12 };

// A 2-level NFT accepting exactly the relation { ("ab", "c") }.
// Path: 0 --a(lvl0)--> 2 --c(lvl1)--> 3 --b(lvl0)--> 4 --eps(lvl1)--> 1(final).
Nft make_ab_c() {
    Nft n{ Nft::with_levels(2) };
    const State q0{ n.add_state_with_level(0) };
    const State qf{ n.add_state_with_level(0) };
    n.initial.insert(q0);
    n.final.insert(qf);
    n.insert_word_by_levels(q0, { Word{ a, b }, Word{ c } }, qf);
    return n;
}

} // namespace

TEST_CASE("mata::nft::Nft::post(symbol, level) — basic matching and other-level indifference") {
    // 0 --a(lvl0)--> 1 --c(lvl1)--> 2(final).
    Nft n{ Nft::with_levels(2) };
    const State q0{ n.add_state_with_level(0) };
    const State q1{ n.add_state_with_level(1) };
    const State q2{ n.add_state_with_level(0) };
    n.initial.insert(q0);
    n.final.insert(q2);
    n.delta.add(q0, a, q1);
    n.delta.add(q1, c, q2);

    SECTION("match on level 0, indifferent to level 1") {
        // 'a' matches on level 0; the level-1 symbol 'c' is walked over to the next zero-level state.
        CHECK(n.post(StateSet{ q0 }, a, 0) == StateSet{ q2 });
        CHECK(n.post(q0, a, 0) == StateSet{ q2 });          // single-state overload
    }

    SECTION("no match on level 0") {
        CHECK(n.post(StateSet{ q0 }, b, 0) == StateSet{});
    }

    SECTION("match on level 1, indifferent to level 0") {
        // The level-0 transition 'a' is walked over; 'c' must match on level 1.
        CHECK(n.post(StateSet{ q0 }, c, 1) == StateSet{ q2 });
        CHECK(n.post(StateSet{ q0 }, d, 1) == StateSet{});
    }

    SECTION("empty NFT (empty delta)") {
        Nft empty{ Nft::with_levels(2) };
        const State s{ empty.add_state_with_level(0) };
        CHECK(empty.post(StateSet{ s }, a, 0) == StateSet{});
        // With an epsilon closure requested and EPSILON symbol, the source state stays reachable.
        CHECK(empty.post(StateSet{ s }, EPSILON, 0, EpsilonClosureOpt::Before) == StateSet{ s });
    }
}

TEST_CASE("mata::nft::Nft::post(symbol, level) — DONT_CARE") {
    // 0 --DONT_CARE(lvl0)--> 1 --c(lvl1)--> 2(final).
    Nft n{ Nft::with_levels(2) };
    const State q0{ n.add_state_with_level(0) };
    const State q1{ n.add_state_with_level(1) };
    const State q2{ n.add_state_with_level(0) };
    n.initial.insert(q0);
    n.final.insert(q2);
    n.delta.add(q0, DONT_CARE, q1);
    n.delta.add(q1, c, q2);

    // DONT_CARE transition is matched by any real symbol.
    CHECK(n.post(StateSet{ q0 }, a, 0) == StateSet{ q2 });
    CHECK(n.post(StateSet{ q0 }, b, 0) == StateSet{ q2 });
    // ... but a DONT_CARE transition is not matched by an EPSILON query.
    CHECK(n.post(StateSet{ q0 }, EPSILON, 0) == StateSet{});
}

TEST_CASE("mata::nft::Nft::post(symbol, level) — epsilon closure") {
    // 0 --eps--> 1 (both zero-level), 1 --a(lvl0)--> 2 --c(lvl1)--> 3(final).
    Nft n{ Nft::with_levels(2) };
    const State q0{ n.add_state_with_level(0) };
    const State q1{ n.add_state_with_level(0) };
    const State q2{ n.add_state_with_level(1) };
    const State q3{ n.add_state_with_level(0) };
    n.initial.insert(q0);
    n.final.insert(q3);
    n.delta.add(q0, EPSILON, q1);
    n.delta.add(q1, a, q2);
    n.delta.add(q2, c, q3);

    SECTION("NONE: the epsilon move is not taken") {
        CHECK(n.post(StateSet{ q0 }, a, 0, EpsilonClosureOpt::None) == StateSet{});
    }

    SECTION("BEFORE: epsilon closure then the symbol step") {
        CHECK(n.post(StateSet{ q0 }, a, 0, EpsilonClosureOpt::Before) == StateSet{ q3 });
    }

    SECTION("EPSILON query: closure includes the source, NONE is a single epsilon step") {
        CHECK(n.post(StateSet{ q0 }, EPSILON, 0, EpsilonClosureOpt::Before) == StateSet{ q0, q1 });
        CHECK(n.post(StateSet{ q0 }, EPSILON, 0, EpsilonClosureOpt::None) == StateSet{ q1 });
    }

    SECTION("AFTER: symbol step then epsilon closure") {
        // 0 --a(lvl0)--> 1 --c(lvl1)--> 2(zero-level), and 2 --eps--> 3(final zero-level).
        Nft m{ Nft::with_levels(2) };
        const State s0{ m.add_state_with_level(0) };
        const State s1{ m.add_state_with_level(1) };
        const State s2{ m.add_state_with_level(0) };
        const State s3{ m.add_state_with_level(0) };
        m.initial.insert(s0);
        m.final.insert(s3);
        m.delta.add(s0, a, s1);
        m.delta.add(s1, c, s2);
        m.delta.add(s2, EPSILON, s3);
        CHECK(m.post(StateSet{ s0 }, a, 0, EpsilonClosureOpt::None) == StateSet{ s2 });
        CHECK(m.post(StateSet{ s0 }, a, 0, EpsilonClosureOpt::After) == StateSet{ s2, s3 });
    }
}

TEST_CASE("mata::nft::Nft::post(symbol, level) — jump transitions crossing the level") {
    SECTION("whole-cycle jump (level 0 -> level 0)") {
        // 0 --a(jump lvl0->lvl0)--> 1(final): consumes 'a' on level 0 and 'a' on level 1.
        Nft n{ Nft::with_levels(2) };
        const State q0{ n.add_state_with_level(0) };
        const State q1{ n.add_state_with_level(0) };
        n.initial.insert(q0);
        n.final.insert(q1);
        n.delta.add(q0, a, q1);

        // Query for a symbol on level 1 which the jump crosses.
        CHECK(n.post(StateSet{ q0 }, a, 1, EpsilonClosureOpt::None, JumpMode::RepeatSymbol) == StateSet{ q1 });
        CHECK(n.post(StateSet{ q0 }, b, 1, EpsilonClosureOpt::None, JumpMode::RepeatSymbol) == StateSet{});
        // AppendDontCares: the crossed level is a DONT_CARE, matched by any real symbol.
        CHECK(n.post(StateSet{ q0 }, b, 1, EpsilonClosureOpt::None, JumpMode::AppendDontCares) == StateSet{ q1 });
    }

    SECTION("3-level, not jumping over the queried level") {
        // 0 --a(lvl0)--> 1(lvl1) --b(lvl1)--> 2(lvl2): query on level 2 walks levels 0 and 1.
        Nft n{ Nft::with_levels(3) };
        const State q0{ n.add_state_with_level(0) };
        const State q1{ n.add_state_with_level(1) };
        const State q2{ n.add_state_with_level(2) };
        const State q3{ n.add_state_with_level(0) };
        n.initial.insert(q0);
        n.final.insert(q3);
        n.delta.add(q0, a, q1);
        n.delta.add(q1, b, q2);
        n.delta.add(q2, c, q3);
        CHECK(n.post(StateSet{ q0 }, c, 2) == StateSet{ q3 });
        CHECK(n.post(StateSet{ q0 }, d, 2) == StateSet{});
    }
}

TEST_CASE("mata::nft::Nft::post(words) — exact tuple matching") {
    Nft n{ make_ab_c() };
    const State q0{ *n.initial.begin() };
    const State qf{ *n.final.begin() };

    CHECK(n.post(StateSet{ q0 }, std::vector<Word>{ Word{ a, b }, Word{ c } }) == StateSet{ qf });
    CHECK(n.post(q0, std::vector<Word>{ Word{ a, b }, Word{ c } }) == StateSet{ qf }); // single-state overload
    CHECK(n.post(StateSet{ q0 }, std::vector<Word>{ Word{ a, b }, Word{ d } }) == StateSet{});
    // ("a","c") is a proper prefix: it reaches an intermediate zero-level state but not the final one.
    CHECK_FALSE(n.post(StateSet{ q0 }, std::vector<Word>{ Word{ a }, Word{ c } }).contains(qf));
    CHECK(n.post(StateSet{ q0 }, std::vector<Word>{ Word{ a, b }, Word{ c, d } }) == StateSet{});
}

TEST_CASE("mata::nft::Nft::post(words) — epsilon track consumes nothing") {
    // Relation ("a", ""): 0 --a(lvl0)--> 1 --eps(lvl1)--> 2(final).
    // Regression: an EPSILON transition must never advance a (possibly already exhausted) tape head.
    Nft n{ Nft::with_levels(2) };
    const State q0{ n.add_state_with_level(0) };
    const State q1{ n.add_state_with_level(1) };
    const State q2{ n.add_state_with_level(0) };
    n.initial.insert(q0);
    n.final.insert(q2);
    n.delta.add(q0, a, q1);
    n.delta.add(q1, EPSILON, q2);

    CHECK(n.post(StateSet{ q0 }, std::vector<Word>{ Word{ a }, Word{} }) == StateSet{ q2 });
    CHECK(n.post(StateSet{ q0 }, std::vector<Word>{ Word{ a }, Word{ c } }) == StateSet{});
    // Empty words consume nothing, so only the source zero-level state is reached.
    CHECK(n.post(StateSet{ q0 }, std::vector<Word>{ Word{}, Word{} }) == StateSet{ q0 });
}

TEST_CASE("mata::nft::Nft::post(words) — DONT_CARE track") {
    // Relation (x, "c") for any single symbol x: 0 --DONT_CARE(lvl0)--> 1 --c(lvl1)--> 2(final).
    Nft n{ Nft::with_levels(2) };
    const State q0{ n.add_state_with_level(0) };
    const State q1{ n.add_state_with_level(1) };
    const State q2{ n.add_state_with_level(0) };
    n.initial.insert(q0);
    n.final.insert(q2);
    n.delta.add(q0, DONT_CARE, q1);
    n.delta.add(q1, c, q2);

    CHECK(n.post(StateSet{ q0 }, std::vector<Word>{ Word{ a }, Word{ c } }) == StateSet{ q2 });
    CHECK(n.post(StateSet{ q0 }, std::vector<Word>{ Word{ b }, Word{ c } }) == StateSet{ q2 });
    CHECK(n.post(StateSet{ q0 }, std::vector<Word>{ Word{ a }, Word{ d } }) == StateSet{});
    // DONT_CARE requires a real input symbol; it does not match an empty (epsilon) tape.
    CHECK(n.post(StateSet{ q0 }, std::vector<Word>{ Word{}, Word{ c } }) == StateSet{});
}

TEST_CASE("mata::nft::Nft::post(words) — jump transition and jump modes") {
    // 0 --a(jump lvl0->lvl0)--> 1(final).
    Nft n{ Nft::with_levels(2) };
    const State q0{ n.add_state_with_level(0) };
    const State q1{ n.add_state_with_level(0) };
    n.initial.insert(q0);
    n.final.insert(q1);
    n.delta.add(q0, a, q1);

    SECTION("RepeatSymbol: same symbol on every crossed level") {
        CHECK(n.post(StateSet{ q0 }, std::vector<Word>{ Word{ a }, Word{ a } }, nullptr, true, JumpMode::RepeatSymbol) == StateSet{ q1 });
        CHECK(n.post(StateSet{ q0 }, std::vector<Word>{ Word{ a }, Word{ b } }, nullptr, true, JumpMode::RepeatSymbol) == StateSet{});
    }
    SECTION("AppendDontCares: DONT_CARE on the crossed levels") {
        CHECK(n.post(StateSet{ q0 }, std::vector<Word>{ Word{ a }, Word{ a } }, nullptr, true, JumpMode::AppendDontCares) == StateSet{ q1 });
        CHECK(n.post(StateSet{ q0 }, std::vector<Word>{ Word{ a }, Word{ b } }, nullptr, true, JumpMode::AppendDontCares) == StateSet{ q1 });
        // The appended DONT_CARE still needs a real symbol on level 1.
        CHECK(n.post(StateSet{ q0 }, std::vector<Word>{ Word{ a }, Word{} }, nullptr, true, JumpMode::AppendDontCares) == StateSet{});
    }
}

TEST_CASE("mata::nft::Nft::post(words) — projection (use_level bitmask and word_levels)") {
    Nft n{ make_ab_c() };   // relation ("ab", "c")
    const State q0{ *n.initial.begin() };
    const State qf{ *n.final.begin() };

    SECTION("word_levels projection ignores the unspecified level") {
        // Project onto level 0: level 1 ("c") is ignored, so "ab" is enough to reach the final state.
        CHECK(n.post(StateSet{ q0 }, std::vector<Word>{ Word{ a, b } }, std::vector<Level>{ 0 }).contains(qf));
        // Project onto level 1: level 0 ("ab") is ignored.
        CHECK(n.post(StateSet{ q0 }, std::vector<Word>{ Word{ c } }, std::vector<Level>{ 1 }).contains(qf));
        CHECK(n.post(StateSet{ q0 }, std::vector<Word>{ Word{ d } }, std::vector<Level>{ 1 }) == StateSet{});
    }

    SECTION("use_level bitmask overload matches word_levels projection") {
        BoolVector use_level_0{ true, false };
        CHECK(n.post(StateSet{ q0 }, std::vector<Word>{ Word{ a, b }, Word{} }, use_level_0).contains(qf));
        BoolVector use_level_1{ false, true };
        CHECK(n.post(StateSet{ q0 }, std::vector<Word>{ Word{}, Word{ c } }, use_level_1).contains(qf));
    }
}

TEST_CASE("mata::nft::Nft::post(words) — empty delta, empty words, and visited states") {
    SECTION("empty delta: all-empty words keep the source states, otherwise empty") {
        Nft n{ Nft::with_levels(2) };
        const State s{ n.add_state_with_level(0) };
        CHECK(n.post(StateSet{ s }, std::vector<Word>{ Word{}, Word{} }) == StateSet{ s });
        CHECK(n.post(StateSet{ s }, std::vector<Word>{ Word{ a }, Word{} }) == StateSet{});
    }

    SECTION("visited_zero_level_states is populated") {
        // Relation ("a","x") then ("b","y") chained through zero-level state qmid.
        Nft n{ Nft::with_levels(2) };
        const State q0{ n.add_state_with_level(0) };
        const State qmid{ n.add_state_with_level(0) };
        const State qend{ n.add_state_with_level(0) };
        n.initial.insert(q0);
        n.final.insert(qend);
        n.insert_word_by_levels(q0, { Word{ a }, Word{ x } }, qmid);
        n.insert_word_by_levels(qmid, { Word{ b }, Word{ y } }, qend);

        StateSet visited{};
        const StateSet targets{ n.post(StateSet{ q0 }, std::vector<Word>{ Word{ a, b }, Word{ x, y } }, &visited) };
        CHECK(targets == StateSet{ qend });
        // Both the intermediate and final zero-level states are visited while consuming the tuple.
        CHECK(visited.contains(q0));
        CHECK(visited.contains(qmid));
        CHECK(visited.contains(qend));
    }
}

TEST_CASE("mata::nft::Nft::post(words) — epsilon_closure_after flag") {
    // Consuming ("a","c") reaches zero-level state q2, and q2 --eps--> q3 (final zero-level).
    Nft n{ Nft::with_levels(2) };
    const State q0{ n.add_state_with_level(0) };
    const State q1{ n.add_state_with_level(1) };
    const State q2{ n.add_state_with_level(0) };
    const State q3{ n.add_state_with_level(0) };
    n.initial.insert(q0);
    n.final.insert(q3);
    n.delta.add(q0, a, q1);
    n.delta.add(q1, c, q2);
    n.delta.add(q2, EPSILON, q3);

    // With the closure (default) the trailing epsilon move to q3 is followed.
    CHECK(n.post(StateSet{ q0 }, std::vector<Word>{ Word{ a }, Word{ c } }, nullptr, /*epsilon_closure_after=*/true) == StateSet{ q2, q3 });
    // Without it, exploration stops at the first zero-level state reached at the end position.
    CHECK(n.post(StateSet{ q0 }, std::vector<Word>{ Word{ a }, Word{ c } }, nullptr, /*epsilon_closure_after=*/false) == StateSet{ q2 });
    // is_in_lang_by_levels always closes after, so it still reaches the final state.
    CHECK(n.is_in_lang_by_levels(std::vector<Word>{ Word{ a }, Word{ c } }));
}
