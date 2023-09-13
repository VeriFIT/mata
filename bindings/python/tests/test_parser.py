import os
import libmata.parser as parser
import libmata.alphabets as alph

AUTOMATA_DIR = os.path.join(
    os.path.dirname(os.path.realpath(__file__)),
    "automata"
)


def test_parse_mf():
    src_automata = sorted([os.path.join(AUTOMATA_DIR, src) for src in os.listdir(AUTOMATA_DIR)])
    alphabet = alph.OnTheFlyAlphabet()
    
    aut = parser.from_mata(src_automata[0], alphabet)
    assert aut.size() == 23

    alphabet = alph.OnTheFlyAlphabet()
    list_aut = parser.from_mata(src_automata, alphabet)
    assert len(list_aut) == 3
    assert list_aut[0].size() == 23
    assert list_aut[1].size() == 4
    assert list_aut[2].size() == 256


def test_parse_intermediate_mf():
    # TODO
    pass


def test_mintermization_mf():
    # TODO
    pass