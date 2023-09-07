cimport libmata.nfa.nfa as mata_nfa
cimport libmata.nfa.strings as mata_strings

from cython.operator import dereference, postincrement as postinc

from libmata.nfa.strings cimport CSegmentation
from libmata.nfa.nfa cimport CTrans

cdef class Segmentation:
    """Wrapper over Segmentation."""

    cdef CSegmentation* thisptr

    def __cinit__(self, mata_nfa.Nfa aut, cset[Symbol] symbols):
        """Compute segmentation.

        :param aut: Segment automaton to compute epsilon depths for.
        :param symbol: Symbol to execute segmentation for.
        """
        self.thisptr = new CSegmentation(dereference(aut.thisptr), symbols)

    def __dealloc__(self):
        del self.thisptr

    def get_epsilon_depths(self):
        """Get segmentation depths for ε-transitions.

        :return: Map of depths to lists of ε-transitions.
        """
        cdef umap[size_t, vector[CTrans]] c_epsilon_transitions = self.thisptr.get_epsilon_depths()
        result = {}
        for epsilon_depth_pair in c_epsilon_transitions:
            for trans in epsilon_depth_pair.second:
                if epsilon_depth_pair.first not in result:
                    result[epsilon_depth_pair.first] = []

                result[epsilon_depth_pair.first].append(mata_nfa.Transition(trans.source, trans.symbol, trans.target))

        return result

    def get_segments(self):
        """Get segment automata.

        :return: A vector of segments for the segment automaton in the order from the left (initial state in segment
                 automaton) to the right (final states of segment automaton).
        """
        segments = []
        cdef vector[CNfa] c_segments = self.thisptr.get_segments()
        for c_segment in c_segments:
            segment = mata_nfa.Nfa(c_segment.num_of_states())
            (<mata_nfa.Nfa>segment).thisptr.get().initial = c_segment.initial
            (<mata_nfa.Nfa>segment).thisptr.get().final = c_segment.final
            (<mata_nfa.Nfa>segment).thisptr.get().delta = c_segment.delta

            segments.append(segment)

        return segments


def noodlify(mata_nfa.Nfa aut, Symbol symbol, include_empty = False):
    """Create noodles from segment automaton.

    Segment automaton is a chain of finite automata (segments) connected via ε-transitions.
    A noodle is a copy of the segment automaton with exactly one ε-transition between each two consecutive segments.

    :param: mata_nfa.Nfa aut: Segment automaton to noodlify.
    :param: Symbol epsilon: Epsilon symbol to noodlify for.
    :param: bool include_empty: Whether to also include empty noodles.
    :return: List of automata: A list of all (non-empty) noodles.
    """
    noodle_segments = []
    cdef vector[vector[shared_ptr[CNfa]]] c_noodle_segments = mata_strings.c_noodlify(
        dereference(aut.thisptr.get()), symbol, include_empty
    )
    for c_noodle in c_noodle_segments:
        noodle = []
        for c_noodle_segment in c_noodle:
            noodle_segment = mata_nfa.Nfa()
            (<mata_nfa.Nfa>noodle_segment).thisptr = c_noodle_segment
            noodle.append(noodle_segment)

        noodle_segments.append(noodle)

    return noodle_segments

def get_shortest_words(mata_nfa.Nfa nfa):
    """Returns set of the shortest words accepted by the automaton."""
    cdef cset[vector[Symbol]] shortest
    shortest = mata_strings.c_get_shortest_words(dereference(nfa.thisptr.get()))
    result = []
    cdef cset[vector[Symbol]].iterator it = shortest.begin()
    cdef cset[vector[Symbol]].iterator end = shortest.end()
    while it != end:
        short = dereference(it)
        result.append(short)
        postinc(it)
    return result

def noodlify_for_equation(left_side_automata: list, mata_nfa.Nfa right_side_automaton, include_empty = False, params = None):
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
    cdef vector[CNfa*] c_left_side_automata
    for lhs_aut in left_side_automata:
        c_left_side_automata.push_back((<mata_nfa.Nfa>lhs_aut).thisptr.get())
    noodle_segments = []
    params = params or {}
    cdef vector[vector[shared_ptr[CNfa]]] c_noodle_segments = mata_strings.c_noodlify_for_equation(
        c_left_side_automata, dereference(right_side_automaton.thisptr.get()), include_empty,
        {
            k.encode('utf-8'): v.encode('utf-8') if isinstance(v, str) else v
            for k, v in params.items()
        },
    )
    for c_noodle in c_noodle_segments:
        noodle = []
        for c_noodle_segment in c_noodle:
            noodle_segment = mata_nfa.Nfa()
            (<mata_nfa.Nfa>noodle_segment).thisptr = c_noodle_segment
            noodle.append(noodle_segment)

        noodle_segments.append(noodle)

    return noodle_segments
