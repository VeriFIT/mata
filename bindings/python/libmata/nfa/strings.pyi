from collections.abc import Iterable
from enum import IntEnum
from typing import Literal

import libmata.alphabets as alph
import libmata.nfa.nfa as mata_nfa
import libmata.nft.nft as mata_nft
from libmata.alphabets import Symbol

class Segmentation:
    """Wrapper over Segmentation."""
    def __init__(self, aut: mata_nfa.Nfa, symbols: set[Symbol]) -> None:
        """Compute segmentation.

        :param aut: Segment automaton to compute epsilon depths for.
        :param symbol: Symbol to execute segmentation for.
        """
    def get_epsilon_depths(self) -> dict[int, list[mata_nfa.Transition]]:
        """Get segmentation depths for ε-transitions.

        :return: Map of depths to lists of ε-transitions.
        """
    def get_segments(self) -> list[mata_nfa.Nfa]:
        """Get segment automata.

        :return: A vector of segments for the segment automaton in the order from the left (initial state in segment
                 automaton) to the right (final states of segment automaton).
        """
    def get_untrimmed_segments(self) -> list[mata_nfa.Nfa]:
        """Get raw (untrimmed) segment automata."""

class ShortestWordsMap:
    """Class mapping states to the shortest words accepted by languages of the states."""
    def __init__(self, aut: mata_nfa.Nfa) -> None: ...
    def get_shortest_words_from(self, states: mata_nfa.State | Iterable[mata_nfa.State]) -> set[tuple[Symbol, ...]]:
        """Gets shortest words for the given `states` (a single state, or an iterable of states)."""

class ReplaceMode(IntEnum):
    """How many occurrences of the regex/literal/symbol to replace, in order from left to right."""

    Single = 0
    All = 1

def noodlify(aut: mata_nfa.Nfa, symbol: Symbol, include_empty: bool = False) -> list[mata_nfa.Nfa]:
    """Create noodles from segment automaton.

    Segment automaton is a chain of finite automata (segments) connected via ε-transitions.
    A noodle is a copy of the segment automaton with exactly one ε-transition between each two consecutive segments.

    :param: mata_nfa.Nfa aut: Segment automaton to noodlify.
    :param: Symbol epsilon: Epsilon symbol to noodlify for.
    :param: bool include_empty: Whether to also include empty noodles.
    :return: List of automata: A list of all (non-empty) noodles.
    """

def get_shortest_words(nfa: mata_nfa.Nfa) -> list[list[Symbol]]:
    """Returns set of the shortest words accepted by the automaton."""

def noodlify_for_equation(
    left_side_automata: list[mata_nfa.Nfa],
    right_side_automaton: mata_nfa.Nfa,
    include_empty: bool = False,
    params: dict[Literal["reduce"], Literal["false", "forward", "backward", "bidirectional"]] = None,
) -> None:
    """Create noodles for equation.

    Segment automaton is a chain of finite automata (segments) connected via ε-transitions.
    A noodle is a copy of the segment automaton with exactly one ε-transition between each two consecutive segments.

    Mata cannot work with equations, queries etc. Hence, we compute the noodles for the equation, but represent
     the equation in a way that libMata understands. The left side automata represent the left side of the equation
     and the right automaton represents the right side of the equation. To create noodles, we need a segment automaton
     representing the intersection. That can be achieved by computing a product of both sides. First, the left side
     has to be concatenated over an epsilon transition into a single automaton to compute the intersection on, though.

    :param: list[mata_nfa.Nfa] aut: Segment automata representing the left side of the equation to noodlify.
    :param: mata_nfa.Nfa aut: Segment automaton representing the right side of the equation to noodlify.
    :param: bool include_empty: Whether to also include empty noodles.
    :param: dict params: Additional parameters for the noodlification:
        - "reduce": "false", "forward", "backward", "bidirectional"; Execute forward, backward or bidirectional simulation
                    minimization before noodlification.
    :return: List of automata: A list of all (non-empty) noodles.
    """

def noodlify_mult_eps(
    aut: mata_nfa.Nfa, epsilons: set[Symbol], include_empty: bool = False
) -> list[list[tuple[mata_nfa.Nfa, list[int]]]]:
    """Create noodles from segment automaton `aut`, for multiple epsilon symbols `epsilons`."""

def noodlify_for_equation_sides(
    left_side_automata: list[mata_nfa.Nfa],
    right_side_automata: list[mata_nfa.Nfa],
    include_empty: bool = False,
    params: dict[str, str] | None = None,
) -> list[list[tuple[mata_nfa.Nfa, list[int]]]]:
    """Create noodles for an equation given as sequences of automata on both sides."""

def noodlify_for_transducer(
    nft: mata_nft.Nft,
    input_automata: list[mata_nfa.Nfa],
    output_automata: list[mata_nfa.Nfa],
    reduce_intersection: bool = True,
    use_homomorphic_heuristic: bool = True,
) -> list[list[tuple[mata_nft.Nft, mata_nfa.Nfa, int, mata_nfa.Nfa, int]]]:
    """Create noodles for a transducer applied between `input_automata` and `output_automata`."""

def process_eps_map(eps_cnt: dict[int, int]) -> list[int]:
    """Process an epsilon count map to a sequence of values (sorted by key, descending)."""

def get_words_of_lengths(nft: mata_nft.Nft, lengths: list[int]) -> list[list[Symbol]] | None:
    """Get the accepting words for each tape of `nft` with the specific `lengths`."""

def get_accepted_symbols(nfa: mata_nfa.Nfa) -> set[Symbol]:
    """Get all the one-symbol words accepted by `nfa`."""

def get_word_lengths(aut: mata_nfa.Nfa) -> set[tuple[int, int]]:
    """Get the lengths of all words in `aut`."""

def is_lang_eps(nfa: mata_nfa.Nfa) -> bool:
    """Checks if `nfa` accepts only the empty word."""

def reluctant_nfa(nfa: mata_nfa.Nfa) -> mata_nfa.Nfa:
    """Modify a copy of `nfa` to remove outgoing transitions from final states."""

def create_identity(alphabet: alph.Alphabet, num_of_levels: int = 2) -> mata_nft.Nft:
    """Create an identity transducer over `alphabet` with `num_of_levels` levels."""

def create_identity_with_single_symbol_replace(
    alphabet: alph.Alphabet,
    from_symbol: Symbol,
    replacement: Symbol | list[Symbol],
    replace_mode: ReplaceMode = ReplaceMode.All,
) -> mata_nft.Nft:
    """Create an identity input/output transducer over `alphabet` with `from_symbol` replaced with `replacement`."""

def replace_reluctant_regex(
    regex_or_aut: str | mata_nfa.Nfa,
    replacement: list[Symbol],
    alphabet: alph.Alphabet,
    replace_mode: ReplaceMode = ReplaceMode.All,
    begin_marker: Symbol | None = None,
) -> mata_nft.Nft:
    """Create an NFT modelling a reluctant leftmost replace of `regex_or_aut` with `replacement`."""

def replace_reluctant_literal(
    literal: list[Symbol],
    replacement: list[Symbol],
    alphabet: alph.Alphabet,
    replace_mode: ReplaceMode = ReplaceMode.All,
    end_marker: Symbol | None = None,
) -> mata_nft.Nft:
    """Create an NFT modelling a reluctant leftmost replace of `literal` with `replacement`."""

def replace_reluctant_single_symbol(
    from_symbol: Symbol,
    replacement: Symbol | list[Symbol],
    alphabet: alph.Alphabet,
    replace_mode: ReplaceMode = ReplaceMode.All,
) -> mata_nft.Nft:
    """Create an NFT modelling a reluctant leftmost replace of `from_symbol` with `replacement`."""
