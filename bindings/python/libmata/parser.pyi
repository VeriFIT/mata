from collections.abc import Iterable
from typing import overload

import libmata.alphabets as alph
import libmata.nfa.nfa as mata_nfa

def from_regex(
    regex: str,
    encoding: str = "utf-8",
    reduce: bool = True,
    trim: bool = True,
    keep_epsilon: bool = False,
) -> mata_nfa.Nfa:
    """Creates automaton from the regular expression

    The format of the regex conforms to google RE2 regular expression library.
    By default, the automaton has no epsilon transitions, is trimmed and reduced.

    :param str regex: regular expression
    :param str encoding: encoding of the string
    :param bool reduce: whether to reduce the automaton using simulation reduction; costly for large regexes
    :param bool trim: whether to trim the automaton
    :param bool keep_epsilon: whether to keep epsilon transitions (labelled with EPSILON) in the automaton
    :return: Nfa automaton
    """

@overload
def from_mata(src: str, alphabet: alph.Alphabet | None = None) -> mata_nfa.Nfa:
    """Loads automata from single file.

    The file must hold exactly one automaton, i.e., a single @-section.

    :param src: single path
    :param alphabet: target alphabet; when omitted, the symbols are read as numbers
    :raises RuntimeError: when the file does not hold exactly one automaton
    :return: single automaton
    """

@overload
def from_mata(src: Iterable[str], alphabet: alph.Alphabet | None = None) -> list[mata_nfa.Nfa]:
    """Loads automata from list or other stream of files.

    Every file must hold exactly one automaton, i.e., a single @-section.
    All automata passed in sources are mintermized together. The sources can be a list or any other
    iterable of paths, such as a generator.

    :param src: iterable of paths to automata
    :param alphabet: target alphabet; when omitted, the symbols are read as numbers
    :raises ValueError: when @p src is an empty iterable
    :raises RuntimeError: when a file does not hold exactly one automaton
    :return: list of automata
    """
