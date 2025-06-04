from typing import Literal

import libmata.nfa.nfa as mata_nfa
from libmata.alphabets import Symbol

class Segmentation:
    """Wrapper over Segmentation."""
    def __init__(self, aut: mata_nfa.Nfa, symbols: set[Symbol]) -> None:
        """Compute segmentation.

        :param aut: Segment automaton to compute epsilon depths for.
        :param symbol: Symbol to execute segmentation for.
        """
        ...
    def get_epsilon_depths(self) -> dict[int, list[mata_nfa.Transition]]:
        """Get segmentation depths for ε-transitions.

        :return: Map of depths to lists of ε-transitions.
        """
        ...
    def get_segments(self) -> list[mata_nfa.Nfa]:
        """Get segment automata.

        :return: A vector of segments for the segment automaton in the order from the left (initial state in segment
                 automaton) to the right (final states of segment automaton).
        """
        ...

def noodlify(aut: mata_nfa.Nfa, symbol: Symbol, include_empty: bool = False) -> list[mata_nfa.Nfa]:
    """Create noodles from segment automaton.

    Segment automaton is a chain of finite automata (segments) connected via ε-transitions.
    A noodle is a copy of the segment automaton with exactly one ε-transition between each two consecutive segments.

    :param: mata_nfa.Nfa aut: Segment automaton to noodlify.
    :param: Symbol epsilon: Epsilon symbol to noodlify for.
    :param: bool include_empty: Whether to also include empty noodles.
    :return: List of automata: A list of all (non-empty) noodles.
    """
    ...

def get_shortest_words(nfa: mata_nfa.Nfa) -> list[list[Symbol]]:
    """Returns set of the shortest words accepted by the automaton."""
    ...

def noodlify_for_equation(left_side_automata: list[mata_nfa.Nfa], right_side_automaton: mata_nfa.Nfa, include_empty: bool = False, params: dict[Literal['reduce'], Literal['false', 'forward', 'backward', 'bidirectional']] = None) -> None:
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
    ...
