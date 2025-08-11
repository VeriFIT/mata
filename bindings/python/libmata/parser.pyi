from typing import overload

import libmata.nfa.nfa as mata_nfa
import libmata.alphabets as alph

def from_regex(regex: str, encoding: str = 'utf-8') -> mata_nfa.Nfa:
    """Creates automaton from the regular expression

    The format of the regex conforms to google RE2 regular expression library.

    :param str regex: regular expression
    :param str encoding: encoding of the string
    :return: Nfa automaton
    """
    ...

@overload
def from_mata(src: str, alphabet: alph.Alphabet) -> mata_nfa.Nfa:
    """Loads automata from single file.
    
    All automata passed in sources are mintermized together.

    :param src: single path
    :param alphabet: target alphabet
    :return: single automaton
    """
    ...
@overload
def from_mata(src: list[str], alphabet: alph.Alphabet) -> list[mata_nfa.Nfa]:
    """Loads automata from list or other stream of files.
    
    All automata passed in sources are mintermized together. If you wish to load 
    multiple automata and mintermize them, then you have to pass them as a list.

    :param src: list of paths to automata
    :param alphabet: target alphabet
    :return: list of automata
    """
    ...
