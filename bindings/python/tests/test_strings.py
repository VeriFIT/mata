import sys
import threading

import libmata.nfa.nfa as mata_nfa
import libmata.nfa.strings as mata_strings
import libmata.nft.nft as mata_nft
import libmata.utils as mata_utils
import pytest
from libmata import alphabets


def test_shortest_words_map():
    nfa = mata_nfa.Nfa(3)
    nfa.make_initial_state(0)
    nfa.make_final_state(2)
    nfa.add_transition(0, 1, 1)
    nfa.add_transition(1, 2, 2)

    swm = mata_strings.ShortestWordsMap(nfa)
    assert swm.get_shortest_words_from(0) == {(1, 2)}
    assert swm.get_shortest_words_from({0}) == {(1, 2)}


def test_get_accepted_symbols_and_word_lengths_and_is_lang_eps():
    nfa = mata_nfa.Nfa(2)
    nfa.make_initial_state(0)
    nfa.make_final_state(1)
    nfa.add_transition(0, 5, 1)

    assert mata_strings.get_accepted_symbols(nfa) == {5}
    assert mata_strings.get_word_lengths(nfa) == {(1, 0)}
    assert not mata_strings.is_lang_eps(nfa)

    eps_nfa = mata_nfa.Nfa(1)
    eps_nfa.make_initial_state(0)
    eps_nfa.make_final_state(0)
    assert mata_strings.is_lang_eps(eps_nfa)


def test_reluctant_nfa():
    nfa = mata_nfa.Nfa(2)
    nfa.make_initial_state(0)
    nfa.make_final_state(1)
    nfa.add_transition(0, 5, 1)
    nfa.add_transition(1, 6, 0)

    reluctant = mata_strings.reluctant_nfa(nfa)
    # Outgoing transitions from final states are removed.
    assert not reluctant.is_in_lang([5, 6])
    assert reluctant.is_in_lang([5])


def test_segmentation_untrimmed_segments():
    epsilon = mata_nfa.epsilon()
    nfa = mata_nfa.Nfa(4)
    nfa.make_initial_state(0)
    nfa.make_final_state(3)
    nfa.add_transition(0, 1, 1)
    nfa.add_transition(1, epsilon, 2)
    nfa.add_transition(2, 2, 3)

    segmentation = mata_strings.Segmentation(nfa, {epsilon})
    segments = segmentation.get_segments()
    untrimmed_segments = segmentation.get_untrimmed_segments()
    assert len(segments) == 2
    assert len(untrimmed_segments) == 2
    # Untrimmed segments keep the original (larger) state space.
    assert untrimmed_segments[0].num_of_states() >= segments[0].num_of_states()


def test_segments_keep_the_alphabet_and_are_independent_copies():
    alphabet = alphabets.OnTheFlyAlphabet.from_symbol_map({"a": 1, "b": 2})
    epsilon = mata_nfa.epsilon()
    nfa = mata_nfa.Nfa(4, alphabet=alphabet)
    nfa.make_initial_state(0)
    nfa.make_final_state(3)
    nfa.add_transition(0, 1, 1)
    nfa.add_transition(1, epsilon, 2)
    nfa.add_transition(2, 2, 3)

    segmentation = mata_strings.Segmentation(nfa, {epsilon})
    for segments in (segmentation.get_segments(), segmentation.get_untrimmed_segments()):
        for segment in segments:
            assert segment.alphabet is not None
            assert segment.alphabet.translate_symbol("b") == 2
    first = segmentation.get_segments()[0]
    first.add_transition(0, 2, 0)
    assert not segmentation.get_segments()[0].has_transition(0, 2, 0)


def test_noodlify_mult_eps():
    e1, e2 = 100, 101
    nfa = mata_nfa.Nfa(4)
    nfa.make_initial_state(0)
    nfa.make_final_state(3)
    nfa.add_transition(0, 1, 1)
    nfa.add_transition(1, e1, 2)
    nfa.add_transition(2, 2, 3)

    noodles = mata_strings.noodlify_mult_eps(nfa, {e1, e2})
    assert len(noodles) == 1
    noodle = noodles[0]
    assert len(noodle) == 2
    for segment, counters in noodle:
        assert isinstance(segment, mata_nfa.Nfa)
        assert isinstance(counters, list)


def test_noodles_share_the_wrapped_segments():
    nfa = mata_nfa.Nfa(5)
    nfa.make_initial_state(0)
    nfa.make_final_state(4)
    nfa.add_transition(0, 1, 1)
    nfa.add_transition(1, 200, 2)
    nfa.add_transition(1, 200, 3)
    nfa.add_transition(2, 2, 4)
    nfa.add_transition(3, 3, 4)

    noodles = mata_strings.noodlify(nfa, 200)
    mult_eps_noodles = [[segment for segment, _ in noodle] for noodle in mata_strings.noodlify_mult_eps(nfa, {200})]
    for result in (noodles, mult_eps_noodles):
        assert len(result) == 2
        # Both noodles take the first segment up to state 1: one wrapped copy serves both.
        assert result[0][0] is result[1][0]
        assert result[0][0].is_in_lang([1])
        assert {(noodle[1].is_in_lang([2]), noodle[1].is_in_lang([3])) for noodle in result} == {
            (True, False),
            (False, True),
        }


def test_noodlify_for_equation_sides():
    left = mata_nfa.Nfa(2)
    left.make_initial_state(0)
    left.make_final_state(1)
    left.add_transition(0, 1, 1)

    right = mata_nfa.Nfa(2)
    right.make_initial_state(0)
    right.make_final_state(1)
    right.add_transition(0, 1, 1)

    noodles = mata_strings.noodlify_for_equation_sides([left], [right])
    assert len(noodles) == 1


def test_noodlify_for_transducer():
    identity_nft = mata_nft.Nft(1, num_of_levels=2)
    identity_nft.make_initial_state(0)
    identity_nft.make_final_state(0)
    identity_nft.insert_identity(0, [1, 2, 3], mata_nft.JumpMode.NoJump)

    in_aut = mata_nfa.Nfa(2)
    in_aut.make_initial_state(0)
    in_aut.make_final_state(1)
    in_aut.add_transition(0, 1, 1)

    out_aut = mata_nfa.Nfa(2)
    out_aut.make_initial_state(0)
    out_aut.make_final_state(1)
    out_aut.add_transition(0, 1, 1)

    noodles = mata_strings.noodlify_for_transducer(identity_nft, [in_aut], [out_aut])
    assert len(noodles) == 1
    transducer, input_aut, input_index, output_aut, output_index = noodles[0][0]
    assert isinstance(transducer, mata_nft.Nft)
    assert isinstance(input_aut, mata_nfa.Nfa)
    assert isinstance(output_aut, mata_nfa.Nfa)
    assert input_index == 0
    assert output_index == 0


def _snapshot(value):
    """Describe a noodlification result by value: states, initial/final states, transitions (and levels for NFTs)."""
    if isinstance(value, (mata_nfa.Nfa, mata_nft.Nft)):
        description = (
            value.num_of_states(),
            sorted(value.initial_states),
            sorted(value.final_states),
            sorted((t.source, t.symbol, t.target) for t in value.get_trans_as_sequence()),
        )
        return description + (list(value.levels),) if isinstance(value, mata_nft.Nft) else description
    if isinstance(value, (list, tuple)):
        return [_snapshot(item) for item in value]
    return value


def _two_initial_two_final_nfa():
    """Unifying its initial and final states changes the automaton."""
    aut = mata_nfa.Nfa(4)
    aut.make_initial_states([0, 1])
    aut.make_final_states([2, 3])
    aut.add_transition(0, 1, 2)
    aut.add_transition(1, 2, 3)
    return aut


def test_noodlify_for_equation_sides_and_transducer_do_not_mutate_their_inputs():
    left = _two_initial_two_final_nfa()
    right = _two_initial_two_final_nfa()
    identity_nft = mata_nft.Nft(1, num_of_levels=2)
    identity_nft.make_initial_state(0)
    identity_nft.make_final_state(0)
    identity_nft.insert_identity(0, [1, 2, 3], mata_nft.JumpMode.NoJump)
    expected = _snapshot([left, right, identity_nft])

    assert mata_strings.noodlify_for_equation_sides([left], [right])
    assert mata_strings.noodlify_for_transducer(identity_nft, [left], [right])
    assert _snapshot([left, right, identity_nft]) == expected


def test_noodlify_variants_threaded_match_single_threaded_results():
    """The noodlify entry points run without the GIL; concurrent results must not depend on interleaving."""
    import concurrent.futures
    import threading

    aut = mata_nfa.Nfa(6)
    aut.make_initial_state(0)
    aut.make_final_state(5)
    aut.add_transition(0, 1, 1)
    aut.add_transition(1, 200, 2)
    aut.add_transition(2, 1, 3)
    aut.add_transition(3, 200, 4)
    aut.add_transition(4, 1, 5)

    # Shared by all threads; the C++ noodlification unifies the initial and final states of its inputs in place.
    left = _two_initial_two_final_nfa()
    right = _two_initial_two_final_nfa()

    identity_nft = mata_nft.Nft(1, num_of_levels=2)
    identity_nft.make_initial_state(0)
    identity_nft.make_final_state(0)
    identity_nft.insert_identity(0, [1, 2, 3], mata_nft.JumpMode.NoJump)

    expected_noodles = _snapshot(mata_strings.noodlify(aut, 200))
    expected_mult_eps = _snapshot(mata_strings.noodlify_mult_eps(aut, {200}))
    expected_sides = _snapshot(mata_strings.noodlify_for_equation_sides([left], [right]))
    expected_transducer = _snapshot(mata_strings.noodlify_for_transducer(identity_nft, [left], [right]))
    assert expected_noodles and expected_mult_eps and expected_sides and expected_transducer

    barrier = threading.Barrier(3)

    def run():
        barrier.wait()
        return (
            _snapshot(mata_strings.noodlify(aut, 200)),
            _snapshot(mata_strings.noodlify_mult_eps(aut, {200})),
            _snapshot(mata_strings.noodlify_for_equation_sides([left], [right])),
            _snapshot(mata_strings.noodlify_for_transducer(identity_nft, [left], [right])),
        )

    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as executor:
        results = [future.result() for future in [executor.submit(run) for _ in range(3)]]
    for result in results:
        assert result == (expected_noodles, expected_mult_eps, expected_sides, expected_transducer)


def _random_nfa(seed, num_states):
    import random

    rng = random.Random(seed)
    aut = mata_nfa.Nfa(num_states)
    for _ in range(4 * num_states):
        aut.add_transition(rng.randrange(num_states), rng.randrange(2), rng.randrange(num_states))
    aut.make_initial_state(0)
    aut.make_final_state(num_states - 1)
    return aut


def test_noodlify_for_equation_sides_lets_other_threads_run(runs_without_the_gil):
    left, right = _random_nfa(1, 100), _random_nfa(2, 100)
    assert runs_without_the_gil(lambda: mata_strings.noodlify_for_equation_sides([left], [right]))


def test_noodlify_for_transducer_lets_other_threads_run(runs_without_the_gil):
    identity_nft = mata_nft.Nft(1, num_of_levels=2)
    identity_nft.make_initial_state(0)
    identity_nft.make_final_state(0)
    identity_nft.insert_identity(0, [0, 1], mata_nft.JumpMode.NoJump)
    left, right = _random_nfa(1, 22), _random_nfa(2, 22)
    assert runs_without_the_gil(lambda: mata_strings.noodlify_for_transducer(identity_nft, [left], [right]))


needs_generator_support = pytest.mark.skipif(
    not mata_utils.HAS_GENERATOR_SUPPORT,
    reason="the noodlify_*_lazy() functions need a standard library implementing std::generator",
)


def _two_level_identity():
    identity_nft = mata_nft.Nft(1, num_of_levels=2)
    identity_nft.make_initial_state(0)
    identity_nft.make_final_state(0)
    identity_nft.insert_identity(0, [0, 1], mata_nft.JumpMode.NoJump)
    return identity_nft


def _eager_and_lazy_noodlifications():
    """Pairs of a list-returning noodlification and its lazy counterpart, each taking an order and a seed."""
    aut = _random_nfa(3, 8)
    for source, target in ((2, 3), (2, 4), (5, 6), (4, 6)):
        aut.add_transition(source, 200, target)
    identity_nft = _two_level_identity()
    return [
        (
            lambda: mata_strings.noodlify(aut, 200),
            lambda order, seed: mata_strings.noodlify_lazy(aut, 200, order=order, seed=seed),
        ),
        (
            lambda: mata_strings.noodlify_mult_eps(aut, {200}),
            lambda order, seed: mata_strings.noodlify_mult_eps_lazy(aut, {200}, order=order, seed=seed),
        ),
        (
            # Fresh left sides: the noodlification unifies their initial and final states in place.
            lambda: mata_strings.noodlify_for_equation([_random_nfa(4, 6), _random_nfa(5, 6)], _random_nfa(6, 9)),
            lambda order, seed: mata_strings.noodlify_for_equation_lazy(
                [_random_nfa(4, 6), _random_nfa(5, 6)], _random_nfa(6, 9), order=order, seed=seed
            ),
        ),
        (
            lambda: mata_strings.noodlify_for_equation_sides(
                [_random_nfa(4, 6), _random_nfa(5, 6)], [_random_nfa(6, 9)]
            ),
            lambda order, seed: mata_strings.noodlify_for_equation_sides_lazy(
                [_random_nfa(4, 6), _random_nfa(5, 6)], [_random_nfa(6, 9)], order=order, seed=seed
            ),
        ),
        (
            lambda: mata_strings.noodlify_for_transducer(
                identity_nft, [_random_nfa(4, 6), _random_nfa(5, 6)], [_random_nfa(6, 9)]
            ),
            lambda order, seed: mata_strings.noodlify_for_transducer_lazy(
                identity_nft, [_random_nfa(4, 6), _random_nfa(5, 6)], [_random_nfa(6, 9)], order=order, seed=seed
            ),
        ),
    ]


@needs_generator_support
def test_lazy_noodlifications_emit_the_noodles_of_the_eager_ones():
    for eager, lazy in _eager_and_lazy_noodlifications():
        expected = _snapshot(eager())
        assert len(expected) > 1
        assert _snapshot(list(lazy(mata_strings.NoodleOrder.Forward, 0))) == expected
        assert _snapshot(list(lazy(mata_strings.NoodleOrder.Reverse, 0))) == expected[::-1]
        for order in (mata_strings.NoodleOrder.SmallestFirst, mata_strings.NoodleOrder.LargestFirst):
            assert sorted(_snapshot(list(lazy(order, 0)))) == sorted(expected)
        random_order = _snapshot(list(lazy(mata_strings.NoodleOrder.Random, 7)))
        assert sorted(random_order) == sorted(expected)
        assert _snapshot(list(lazy(mata_strings.NoodleOrder.Random, 7))) == random_order
        # Stopping early takes the first noodle only.
        assert _snapshot(next(lazy(mata_strings.NoodleOrder.Forward, 0))) == expected[0]


@needs_generator_support
def test_consecutive_lazy_noodles_share_the_wrapped_segments():
    nfa = mata_nfa.Nfa(5)
    nfa.make_initial_state(0)
    nfa.make_final_state(4)
    nfa.add_transition(0, 1, 1)
    nfa.add_transition(1, 200, 2)
    nfa.add_transition(1, 200, 3)
    nfa.add_transition(2, 2, 4)
    nfa.add_transition(3, 3, 4)

    noodles = list(mata_strings.noodlify_lazy(nfa, 200))
    mult_eps_noodles = [
        [segment for segment, _ in noodle] for noodle in mata_strings.noodlify_mult_eps_lazy(nfa, {200})
    ]
    for result in (noodles, mult_eps_noodles):
        assert len(result) == 2
        assert result[0][0] is result[1][0]
        assert result[0][1] is not result[1][1]


@needs_generator_support
def test_lazy_noodles_are_taken_without_the_gil(runs_without_the_gil):
    left, right = _random_nfa(1, 100), _random_nfa(2, 100)
    assert runs_without_the_gil(lambda: list(mata_strings.noodlify_for_equation_sides_lazy([left], [right])))
    small_left, small_right = _random_nfa(1, 22), _random_nfa(2, 22)
    identity_nft = _two_level_identity()
    assert runs_without_the_gil(
        lambda: list(mata_strings.noodlify_for_transducer_lazy(identity_nft, [small_left], [small_right]))
    )


@pytest.mark.skipif(
    mata_utils.HAS_GENERATOR_SUPPORT,
    reason="the noodlify_*_lazy() functions only fail without a standard library implementing std::generator",
)
def test_lazy_noodlify_without_generator_support():
    with pytest.raises(RuntimeError):
        mata_strings.noodlify_lazy(_random_nfa(1, 4), 200)


def _epsilon_lattice(depth):
    """An 'a'-loop segment, `depth` levels of two ε-only states each with an ε-transition from every state of the
    previous level, and a final 'b'-loop segment: all 2**depth routes give the one noodle [a*, b*], which
    noodlify_mult_eps*() leaves the ε-only segments out of, so the search past that noodle walks every route."""
    nfa = mata_nfa.Nfa()
    first = nfa.add_new_state()
    nfa.make_initial_state(first)
    nfa.add_transition(first, ord("a"), first)
    level = [first]
    for _ in range(depth):
        next_level = [nfa.add_new_state(), nfa.add_new_state()]
        for source in level:
            for target in next_level:
                nfa.add_transition(source, 200, target)
        level = next_level
    last = nfa.add_new_state()
    nfa.add_transition(last, ord("b"), last)
    nfa.make_final_state(last)
    for source in level:
        nfa.add_transition(source, 200, last)
    return nfa


@needs_generator_support
def test_lazy_noodlify_takes_the_first_noodle_without_the_rest_of_the_search():
    # 2**64 routes after the first noodle: taking it must not search for the next one, which would not end.
    noodles = mata_strings.noodlify_mult_eps_lazy(_epsilon_lattice(64), {200})
    assert len(next(noodles)) == 2


@needs_generator_support
def test_lazy_noodlify_iterator_rejects_a_second_thread():
    if not getattr(sys, "_is_gil_enabled", lambda: True)():
        pytest.skip("the interpreter has no GIL")
    # 2**22 routes take the second next() about a second, far longer than the main thread needs to wake up.
    noodles = mata_strings.noodlify_mult_eps_lazy(_epsilon_lattice(22), {200})
    assert len(next(noodles)) == 2

    entered = threading.Event()
    outcome = []

    def run():
        entered.set()
        try:
            next(noodles)
        except StopIteration:
            outcome.append(StopIteration)

    switch_interval = sys.getswitchinterval()
    # As in `runs_without_the_gil`: the worker holds the GIL from `entered.set()` until the search releases it inside
    #  next(), so the main thread wakes only once that next() is running.
    sys.setswitchinterval(60)
    worker = threading.Thread(target=run)
    try:
        worker.start()
        entered.wait()
        with pytest.raises(RuntimeError, match="from two threads at once"):
            next(noodles)
    finally:
        worker.join()
        sys.setswitchinterval(switch_interval)
    assert outcome == [StopIteration]
    with pytest.raises(StopIteration):
        next(noodles)


def test_process_eps_map():
    assert mata_strings.process_eps_map({3: 2, 1: 5}) == [2, 5]


def test_get_words_of_lengths():
    nft = mata_nft.Nft(1, num_of_levels=2)
    nft.make_initial_state(0)
    nft.make_final_state(nft.insert_word(0, [1, 2]))

    assert mata_strings.get_words_of_lengths(nft, [1, 1]) == [[1], [2]]
    assert mata_strings.get_words_of_lengths(nft, [5, 5]) is None


def test_create_identity():
    alphabet = alphabets.EnumAlphabet({ord("a"), ord("b"), ord("c")})
    identity = mata_strings.create_identity(alphabet, num_of_levels=2)
    assert identity.is_in_lang([ord("a"), ord("a")])
    assert not identity.is_in_lang([ord("a"), ord("b")])


def test_create_identity_with_single_symbol_replace():
    alphabet = alphabets.EnumAlphabet({ord("a"), ord("b"), ord("c")})

    replaced_symbol = mata_strings.create_identity_with_single_symbol_replace(alphabet, ord("a"), ord("b"))
    assert replaced_symbol.is_in_lang([ord("a"), ord("b")])
    assert replaced_symbol.is_in_lang([ord("b"), ord("b")])

    replaced_word = mata_strings.create_identity_with_single_symbol_replace(alphabet, ord("a"), [ord("b"), ord("c")])
    assert isinstance(replaced_word, mata_nft.Nft)


def test_replace_reluctant_literal_and_single_symbol():
    alphabet = alphabets.EnumAlphabet({ord("a"), ord("b"), ord("x"), ord("z")})

    literal_nft = mata_strings.replace_reluctant_literal([ord("a"), ord("b")], [ord("x")], alphabet)
    assert literal_nft.apply([ord("a"), ord("b")]).is_in_lang([ord("x")])

    symbol_nft = mata_strings.replace_reluctant_single_symbol(ord("a"), ord("z"), alphabet)
    assert symbol_nft.apply([ord("a")]).is_in_lang([ord("z")])

    symbol_word_nft = mata_strings.replace_reluctant_single_symbol(ord("a"), [ord("z"), ord("z")], alphabet)
    assert isinstance(symbol_word_nft, mata_nft.Nft)


def test_replace_reluctant_regex():
    alphabet = alphabets.EnumAlphabet({ord("a"), ord("x")})
    regex_nft = mata_strings.replace_reluctant_regex("a", [ord("x")], alphabet)
    assert regex_nft.apply([ord("a")]).is_in_lang([ord("x")])


def test_replace_mode_enum():
    assert mata_strings.ReplaceMode.Single != mata_strings.ReplaceMode.All
    alphabet = alphabets.EnumAlphabet({ord("a"), ord("b")})
    single_mode_nft = mata_strings.create_identity_with_single_symbol_replace(
        alphabet, ord("a"), ord("b"), mata_strings.ReplaceMode.Single
    )
    assert isinstance(single_mode_nft, mata_nft.Nft)
