import libmata.nfa.nfa as mata_nfa
import libmata.nfa.strings as mata_strings
import libmata.nft.nft as mata_nft
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
    identity_nft.insert_identity(0, [1, 2, 3])

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
