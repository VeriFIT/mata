import libmata.nfa.nfa as mata_nfa
import libmata.nft.nft as mata_nft
import pytest
from libmata import alphabets


def test_nft_construction_and_states():
    nft = mata_nft.Nft(3, num_of_levels=2)
    assert nft.num_of_states() == 3
    assert list(nft.levels) == [0, 0, 0]
    assert nft.levels.num_of_levels == 2

    nft.make_initial_state(0)
    nft.make_final_state(2)
    assert nft.initial_states == [0]
    assert nft.final_states == [2]
    assert nft.has_initial_state(0)
    assert nft.has_final_state(2)

    new_state = nft.add_new_state()
    assert nft.is_state(new_state)


def test_nft_delta():
    nft = mata_nft.Nft(3)
    assert isinstance(nft.delta, mata_nfa.Delta)
    assert nft.delta.empty()

    nft.delta.add(0, 1, 1)
    assert nft.delta.contains(0, 1, 1)
    assert len(nft.delta) == 1
    assert nft.get_num_of_transitions() == 1


def test_nft_insert_word_and_is_in_lang():
    nft = mata_nft.Nft(1, num_of_levels=2)
    nft.make_initial_state(0)
    target = nft.insert_word(0, [1, 2])
    nft.make_final_state(target)

    assert nft.num_of_states() == 3
    assert nft.is_in_lang([1, 2])
    assert not nft.is_in_lang([1, 3])
    assert nft.is_in_lang_prefix([1, 2])


def test_nft_from_nfa():
    nfa = mata_nfa.Nfa(2)
    nfa.make_initial_state(0)
    nfa.make_final_state(1)
    nfa.add_transition(0, 5, 1)

    nft = mata_nft.Nft.from_nfa(nfa, num_of_levels=1)
    assert nft.num_of_states() == 2
    assert nft.levels.num_of_levels == 1

    nft_with_levels = mata_nft.Nft.from_nfa_with_levels(nfa, mata_nft.Levels(1, [0, 0]))
    assert list(nft_with_levels.levels) == [0, 0]


def test_nft_alphabet_inherited_from_source_nfa():
    """Nft.alphabet is copied as-is from a source Nfa's alphabet (from_nfa/from_nfa_with_levels); it is not always
    None, even though NFT operations themselves never read it (they use `alphabets` instead)."""
    alphabet = alphabets.OnTheFlyAlphabet.for_symbol_names(["a", "b"])
    nfa = mata_nfa.Nfa(2, alphabet)
    nfa.make_initial_state(0)
    nfa.make_final_state(1)
    nfa.add_transition(0, 0, 1)

    nft = mata_nft.Nft.from_nfa(nfa, num_of_levels=1)
    assert nft.alphabet is not None
    assert nft.alphabet.get_alphabet_symbols() == alphabet.get_alphabet_symbols()

    # A plain Nft constructor / with_levels never sets the inherited `alphabet` field.
    plain_nft = mata_nft.Nft(1, num_of_levels=2)
    assert plain_nft.alphabet is None


def _make_word_nft(word: list[int]) -> mata_nft.Nft:
    nft = mata_nft.Nft(1, num_of_levels=len(word))
    nft.make_initial_state(0)
    nft.make_final_state(nft.insert_word(0, word))
    return nft


def test_nft_union_and_determinize():
    a = _make_word_nft([1, 2])
    b = _make_word_nft([3, 4])
    u = mata_nft.union_nondet(a, b)
    assert u.is_in_lang([1, 2])
    assert u.is_in_lang([3, 4])
    assert not u.is_in_lang([1, 4])

    det = mata_nft.determinize(a)
    assert det.is_deterministic()
    assert det.is_in_lang([1, 2])


def test_nft_intersection_and_concatenate():
    a = _make_word_nft([1, 2])
    b = _make_word_nft([1, 2])
    c = _make_word_nft([3, 4])

    inter_ab = mata_nft.intersection(a, b)
    assert inter_ab.is_in_lang([1, 2])
    inter_ac = mata_nft.intersection(a, c)
    assert not inter_ac.is_in_lang([1, 2]) and not inter_ac.is_in_lang([3, 4])

    concatenated = mata_nft.concatenate(a, c)
    assert concatenated.is_in_lang([1, 2, 3, 4])


def test_nft_compose():
    lhs = mata_nft.Nft(1, num_of_levels=2)
    lhs.make_initial_state(0)
    lhs.make_final_state(lhs.insert_word(0, [10, 99]))

    rhs = mata_nft.Nft(1, num_of_levels=2)
    rhs.make_initial_state(0)
    rhs.make_final_state(rhs.insert_word(0, [99, 20]))

    composed = mata_nft.compose(lhs, rhs, 1, 0)
    assert composed.is_in_lang([10, 20])
    assert composed.levels.num_of_levels == 2


def test_nft_is_included_and_are_equivalent():
    smaller = _make_word_nft([1, 2])
    bigger = mata_nft.union_nondet(smaller, _make_word_nft([3, 4]))
    alphabet = alphabets.EnumAlphabet({1, 2, 3, 4})

    assert mata_nft.is_included(smaller, bigger, alphabet)
    assert not mata_nft.is_included(bigger, smaller, alphabet)
    assert mata_nft.are_equivalent(smaller, smaller, alphabet)
    assert not mata_nft.are_equivalent(smaller, bigger, alphabet)

    included, cex = mata_nft.is_included_with_cex(bigger, smaller, alphabet)
    assert not included
    assert cex.word


def test_nft_revert_project_and_insert_level():
    nft = _make_word_nft([1, 2, 3])
    reverted = mata_nft.revert(nft)
    assert reverted.num_of_states() == nft.num_of_states()

    projected_out = mata_nft.project_out(nft, 1)
    assert projected_out.levels.num_of_levels == 2

    projected_to = mata_nft.project_to(nft, [0, 2])
    assert projected_to.levels.num_of_levels == 2

    with_level = mata_nft.insert_level(nft, 1)
    assert with_level.levels.num_of_levels == 4

    # Mask must contain exactly num_of_levels (3) `False` entries; each `True` inserts a new level.
    with_levels = mata_nft.insert_levels(nft, [False, True, False, True, False])
    assert with_levels.levels.num_of_levels == 5


def test_nft_alphabets_property_and_resolve_alphabet():
    nft = mata_nft.Nft(1, num_of_levels=2)
    assert nft.alphabets is None

    level0 = alphabets.IntAlphabet()
    level1 = alphabets.EnumAlphabet({1, 2, 3})
    nft.alphabets = alphabets.AlphabetLevels([level0, level1])
    assert nft.alphabets.mode == alphabets.AlphabetLevelsMode.MultiLevel

    with pytest.raises(RuntimeError, match="requires an explicit level"):
        nft.resolve_alphabet()

    resolved = nft.resolve_alphabet(level=1)
    assert isinstance(resolved, alphabets.EnumAlphabet)
    assert nft.get_symbols_to_work_with(level=1) == {1, 2, 3}

    nft.alphabets = None
    assert nft.alphabets is None


def test_nft_insert_identity():
    nft = mata_nft.Nft(1, num_of_levels=2)
    nft.make_initial_state(0)
    nft.make_final_state(0)
    nft.insert_identity(0, alphabets.EnumAlphabet({1, 2}))
    assert nft.is_in_lang([1, 1])
    assert nft.is_in_lang([2, 2])
    assert not nft.is_in_lang([1, 2])

    nft2 = mata_nft.Nft(1, num_of_levels=2)
    nft2.make_initial_state(0)
    nft2.make_final_state(0)
    nft2.insert_identity(0, [5, 6])
    assert nft2.is_in_lang([5, 5])
    assert nft2.is_in_lang([6, 6])


def test_nft_make_complete():
    nft = mata_nft.Nft(1, num_of_levels=1)
    nft.make_initial_state(0)
    nft.make_final_state(0)
    changed = nft.make_complete(alphabets.EnumAlphabet({1, 2}))
    assert changed
    assert nft.is_complete()


def test_nft_apply_and_to_nfa():
    base = mata_nft.Nft(1, num_of_levels=2)
    base.make_initial_state(0)
    base.make_final_state(base.insert_word(0, [1, 2]))

    applied = base.apply([1], level_to_apply_on=0)
    assert applied.is_in_lang([2])

    nfa = base.to_nfa_copy()
    assert isinstance(nfa, mata_nfa.Nfa)


def test_nft_complement_and_reduce():
    base = _make_word_nft([1, 2])
    alphabet = alphabets.EnumAlphabet({1, 2})
    complemented = mata_nft.complement(base, alphabet)
    assert not complemented.is_in_lang([1, 2])

    reduced = mata_nft.reduce(base)
    assert reduced.is_in_lang([1, 2])


def test_nft_get_words():
    nft = _make_word_nft([7, 8])
    assert nft.get_words() == {(7, 8)}


def test_nft_builder_functions():
    word_nft = mata_nft.create_single_word_nft([1, 2])
    assert word_nft.is_in_lang([1, 2])

    empty_nft = mata_nft.create_empty_string_nft(2)
    assert empty_nft.is_in_lang([])

    sigma_star = mata_nft.create_sigma_star_nft(2)
    assert sigma_star.num_of_states() >= 1


def test_nft_parse_from_mata_string_roundtrip():
    nft = _make_word_nft([7, 8])
    mata_str = nft.to_mata_str()
    parsed = mata_nft.parse_from_mata_string(mata_str)
    assert parsed.is_in_lang([7, 8])


def test_levels_basic():
    levels = mata_nft.Levels(2, [0, 1, 0])
    assert levels.num_of_levels == 2
    assert list(levels) == [0, 1, 0]
    assert levels[1] == 1
    levels[0] = 1
    assert levels[0] == 1

    assert levels.count(1) == 2
    assert levels.next_level_after(0) == 1
    assert levels.next_level_after(1) == 0

    assert mata_nft.Levels.can_follow(0, 1)
    assert mata_nft.Levels.can_follow(1, 0)  # target level 0 is always reachable
    assert not mata_nft.Levels.can_follow(1, 1)


def test_levels_get_minimal_level_of():
    levels = mata_nft.Levels(3, [0, 1, 2])
    assert levels.get_minimal_level_of({0, 1, 2}) == 0
    assert levels.get_minimal_level_of(set()) is None
