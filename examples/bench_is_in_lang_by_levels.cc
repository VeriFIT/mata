// Benchmark harness for mata::nft::Nft::is_in_lang_by_levels().
//
// Generates synthetic layered transducers with controllable word length (number of layers),
// branching factor (fan-out per layer), epsilon-closure chain depth (length of an epsilon-only
// detour inserted at each layer boundary), number of levels (tapes), and alphabet size.
// Then times is_in_lang_by_levels() on a handful of representative queries per scenario and
// prints one CSV row per (scenario, query, match_prefix) combination to stdout.
//
// This file is identical across the branches being compared (built independently against each
// branch's library) so the exact same automata and queries (same PRNG, same seed) are used on
// both sides, making the CSV outputs directly diffable.
//
// If MATA_BENCH_HAS_REPEAT_SYMBOL is defined (only true on the nft_post branch, which keeps the
// old hand-rolled worklist around as the free function mata::nft::is_in_lang_by_levels_repeat_symbol
// specifically for this kind of benchmarking), each query is additionally timed against that
// implementation in the same process, so the two can be compared without cross-run noise.

#include "mata/nft/nft.hh"

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace mata;
using namespace mata::nft;

namespace {

struct ScenarioParams {
    std::string name;
    size_t num_layers;
    size_t branching;
    size_t eps_chain_len;
    size_t num_levels;
    size_t alphabet_size;
    unsigned seed;
};

struct GeneratedCase {
    Nft nft;
    std::vector<Word> accept_words; // accept_words[level] = symbols along the designated accepting path
};

GeneratedCase generate(const ScenarioParams& p) {
    std::mt19937 rng(p.seed);
    std::uniform_int_distribution<Symbol> sym_dist(0, static_cast<Symbol>(p.alphabet_size - 1));

    Nft nft{ Nft::with_levels(p.num_levels) };
    const State start = nft.add_state();
    nft.initial.insert(start);

    std::vector<Word> accept_words(p.num_levels);

    std::vector<State> frontier{ start };
    size_t designated_idx = 0;

    for (size_t layer = 0; layer < p.num_layers; ++layer) {
        std::vector<State> next_frontier;
        next_frontier.reserve(p.branching);
        for (size_t i = 0; i < p.branching; ++i) { next_frontier.push_back(nft.add_state()); }

        std::vector<Symbol> correct_symbols(p.num_levels);
        for (size_t lvl = 0; lvl < p.num_levels; ++lvl) { correct_symbols[lvl] = sym_dist(rng); }
        const size_t next_designated_idx = std::uniform_int_distribution<size_t>(0, next_frontier.size() - 1)(rng);

        for (size_t fi = 0; fi < frontier.size(); ++fi) {
            for (size_t ti = 0; ti < next_frontier.size(); ++ti) {
                std::vector<Word> word_parts(p.num_levels);
                if (fi == designated_idx && ti == next_designated_idx) {
                    for (size_t lvl = 0; lvl < p.num_levels; ++lvl) { word_parts[lvl] = { correct_symbols[lvl] }; }
                } else {
                    for (size_t lvl = 0; lvl < p.num_levels; ++lvl) { word_parts[lvl] = { sym_dist(rng) }; }
                }
                nft.insert_word_by_levels(frontier[fi], word_parts, next_frontier[ti]);
            }
        }

        if (p.eps_chain_len > 0) {
            State prev = frontier[designated_idx];
            for (size_t k = 0; k < p.eps_chain_len; ++k) {
                const State mid = nft.add_state();
                nft.delta.add(prev, EPSILON, mid);
                prev = mid;
            }
            nft.delta.add(prev, EPSILON, next_frontier[next_designated_idx]);
        }

        for (size_t lvl = 0; lvl < p.num_levels; ++lvl) { accept_words[lvl].push_back(correct_symbols[lvl]); }

        frontier = std::move(next_frontier);
        designated_idx = next_designated_idx;
    }

    nft.final.insert(frontier[designated_idx]);

    return { std::move(nft), std::move(accept_words) };
}

struct Query {
    std::string type;
    std::vector<Word> level_words;
};

std::vector<Query> make_queries(const ScenarioParams& p, const std::vector<Word>& accept_words, std::mt19937& rng) {
    std::vector<Query> queries;
    std::uniform_int_distribution<Symbol> sym_dist(0, static_cast<Symbol>(p.alphabet_size - 1));

    queries.push_back({ "exact", accept_words });

    {
        std::vector<Word> prefix(p.num_levels);
        const size_t half = p.num_layers / 2;
        for (size_t lvl = 0; lvl < p.num_levels; ++lvl) {
            prefix[lvl] = Word(accept_words[lvl].begin(), accept_words[lvl].begin() + half);
        }
        queries.push_back({ "prefix_half", prefix });
    }

    if (p.num_layers > 0) {
        std::vector<Word> mutated = accept_words;
        const size_t pos = p.num_layers / 2;
        const size_t lvl = 0;
        Symbol orig = mutated[lvl][pos];
        Symbol replacement;
        do { replacement = sym_dist(rng); } while (replacement == orig);
        mutated[lvl][pos] = replacement;
        queries.push_back({ "mutate_middle", mutated });
    }

    {
        std::vector<Word> empty(p.num_levels);
        queries.push_back({ "empty", empty });
    }

    {
        std::vector<Word> too_long = accept_words;
        for (size_t lvl = 0; lvl < p.num_levels; ++lvl) { too_long[lvl].push_back(sym_dist(rng)); }
        queries.push_back({ "too_long", too_long });
    }

    return queries;
}

std::vector<ScenarioParams> make_scenarios() {
    std::vector<ScenarioParams> scenarios;
    unsigned seed_counter = 1000;

    const ScenarioParams base{ "base", 30, 4, 0, 2, 64, 0 };
    auto add = [&](ScenarioParams s) {
        s.seed = seed_counter++;
        scenarios.push_back(std::move(s));
    };

    add(base);

    for (size_t v : { (size_t)5, (size_t)30, (size_t)150, (size_t)600 }) {
        ScenarioParams s = base;
        s.num_layers = v;
        s.name = "word_len_" + std::to_string(v);
        add(s);
    }

    for (size_t v : { (size_t)1, (size_t)2, (size_t)4, (size_t)8, (size_t)16 }) {
        ScenarioParams s = base;
        s.branching = v;
        s.alphabet_size = std::max<size_t>(64, v * 4);
        s.name = "branching_" + std::to_string(v);
        add(s);
    }

    for (size_t v : { (size_t)0, (size_t)1, (size_t)5, (size_t)20, (size_t)100 }) {
        ScenarioParams s = base;
        s.eps_chain_len = v;
        s.name = "eps_len_" + std::to_string(v);
        add(s);
    }

    for (size_t v : { (size_t)1, (size_t)2, (size_t)3, (size_t)5 }) {
        ScenarioParams s = base;
        s.num_levels = v;
        s.name = "num_levels_" + std::to_string(v);
        add(s);
    }

    for (size_t v : { (size_t)4, (size_t)16, (size_t)64, (size_t)256 }) {
        ScenarioParams s = base;
        s.alphabet_size = v;
        s.name = "alphabet_" + std::to_string(v);
        add(s);
    }

    {
        ScenarioParams s{ "combined_large", 100, 8, 10, 2, 128, 0 };
        add(s);
    }
    {
        ScenarioParams s{ "combined_heavy_branch_eps", 50, 20, 50, 3, 256, 0 };
        add(s);
    }
    {
        ScenarioParams s{ "combined_long_thin", 800, 2, 3, 2, 32, 0 };
        add(s);
    }

    return scenarios;
}

// Adaptively picks a repeat count so the timed loop takes roughly target_ns.
struct TimingResult {
    double avg_ns;
    uint64_t repeats;
    bool result;
};

template <typename Fn>
TimingResult time_call(Fn&& fn) {
    using clock = std::chrono::steady_clock;
    constexpr double target_ns = 100'000'000.0; // ~100ms per measurement
    constexpr uint64_t max_repeats = 2'000'000;

    const bool result = fn();

    uint64_t repeats = 1;
    double elapsed_ns = 0.0;
    while (true) {
        const auto t0 = clock::now();
        for (uint64_t i = 0; i < repeats; ++i) {
            volatile bool r = fn();
            (void)r;
        }
        const auto t1 = clock::now();
        elapsed_ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
        if (elapsed_ns >= target_ns || repeats >= max_repeats) { break; }
        const double scale = target_ns / std::max(elapsed_ns, 1.0);
        uint64_t next_repeats = static_cast<uint64_t>(static_cast<double>(repeats) * std::min(scale, 64.0));
        if (next_repeats <= repeats) { next_repeats = repeats * 2; }
        repeats = std::min(next_repeats, max_repeats);
    }

    return { elapsed_ns / static_cast<double>(repeats), repeats, result };
}

} // namespace

int main() {
#ifdef MATA_BENCH_HAS_REPEAT_SYMBOL
    std::cout << "scenario,num_layers,branching,eps_chain_len,num_levels,alphabet_size,"
                 "num_states,num_transitions,query_type,match_prefix,"
                 "result_post,repeats_post,avg_ns_post,"
                 "result_repeat,repeats_repeat,avg_ns_repeat\n";
#else
    std::cout << "scenario,num_layers,branching,eps_chain_len,num_levels,alphabet_size,"
                 "num_states,num_transitions,query_type,match_prefix,result,repeats,avg_ns\n";
#endif

    for (const ScenarioParams& p : make_scenarios()) {
        GeneratedCase gc = generate(p);
        std::mt19937 query_rng(p.seed * 7919u + 17);
        std::vector<Query> queries = make_queries(p, gc.accept_words, query_rng);
        const size_t num_states = gc.nft.num_of_states();
        const size_t num_transitions = gc.nft.delta.num_of_transitions();

        for (const Query& q : queries) {
            for (bool match_prefix : { false, true }) {
                // Called with 2 args (no explicit jump_mode) so this compiles unchanged against master's
                // older signature too; nft_post defaults the 3rd param to JumpMode::RepeatSymbol anyway,
                // which is what is_in_lang_by_levels_repeat_symbol below is restricted to.
                TimingResult post_result = time_call([&] {
                    return gc.nft.is_in_lang_by_levels(q.level_words, match_prefix);
                });
#ifdef MATA_BENCH_HAS_REPEAT_SYMBOL
                TimingResult repeat_result = time_call([&] {
                    return mata::nft::is_in_lang_by_levels_repeat_symbol(gc.nft, q.level_words, match_prefix);
                });
                std::cout << p.name << ',' << p.num_layers << ',' << p.branching << ',' << p.eps_chain_len << ','
                          << p.num_levels << ',' << p.alphabet_size << ',' << num_states << ',' << num_transitions
                          << ',' << q.type << ',' << (match_prefix ? 1 : 0) << ','
                          << (post_result.result ? 1 : 0) << ',' << post_result.repeats << ',' << std::fixed
                          << std::setprecision(2) << post_result.avg_ns << ','
                          << (repeat_result.result ? 1 : 0) << ',' << repeat_result.repeats << ','
                          << std::fixed << std::setprecision(2) << repeat_result.avg_ns << '\n';
#else
                std::cout << p.name << ',' << p.num_layers << ',' << p.branching << ',' << p.eps_chain_len << ','
                          << p.num_levels << ',' << p.alphabet_size << ',' << num_states << ',' << num_transitions
                          << ',' << q.type << ',' << (match_prefix ? 1 : 0) << ',' << (post_result.result ? 1 : 0)
                          << ',' << post_result.repeats << ',' << std::fixed << std::setprecision(2)
                          << post_result.avg_ns << '\n';
#endif
            }
        }
    }

    return 0;
}
