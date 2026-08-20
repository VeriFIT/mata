from enum import IntEnum

cimport libmata.nfa.nfa as mata_nfa
cimport libmata.nft.nft as mata_nft
cimport libmata.nfa.strings as mata_strings
cimport libmata.alphabets as alph

from libcpp.map cimport map as cmap
from libcpp.memory cimport shared_ptr, make_shared
from libcpp.optional cimport optional
from libcpp.pair cimport pair

from cython.operator import dereference, postincrement as postinc

from libmata.nfa.strings cimport CSegmentation, CShortestWordsMap
from libmata.nfa.nfa cimport CTrans, CNfa, State, StateSet
from libmata.nft.nft cimport CNft
from libmata.alphabets cimport CAlphabet

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
        return _wrap_segments(self.thisptr.get_segments())

    def get_untrimmed_segments(self):
        """Get raw (untrimmed) segment automata.

        :return: A vector of segments for the segment automaton in the order from the left (initial state in segment
                 automaton) to the right (final states of segment automaton) without trimming (the states are the
                 same as in the original automaton).
        """
        return _wrap_segments(self.thisptr.get_untrimmed_segments())


cdef _wrap_segments(vector[CNfa] c_segments):
    segments = []
    for c_segment in c_segments:
        segment = mata_nfa.Nfa(c_segment.num_of_states())
        (<mata_nfa.Nfa>segment).thisptr.get().initial = c_segment.initial
        (<mata_nfa.Nfa>segment).thisptr.get().final = c_segment.final
        (<mata_nfa.Nfa>segment).thisptr.get().delta = c_segment.delta
        segments.append(segment)
    return segments


cdef class ShortestWordsMap:
    """Class mapping states to the shortest words accepted by languages of the states."""

    cdef CShortestWordsMap* thisptr

    def __cinit__(self, mata_nfa.Nfa aut):
        """Maps states in `aut` to shortest words accepted by languages of the states.

        :param aut: Automaton to compute shortest words for.
        """
        self.thisptr = new CShortestWordsMap(dereference(aut.thisptr.get()))

    def __dealloc__(self):
        del self.thisptr

    def get_shortest_words_from(self, states):
        """Gets shortest words for the given `states` (a single state, or an iterable of states).

        :return: Set of shortest words.
        """
        cdef cset[vector[Symbol]] result
        cdef vector[State] c_states_vec
        if isinstance(states, int):
            result = self.thisptr.get_shortest_words_from(<State>states)
        else:
            c_states_vec = sorted(states)
            result = self.thisptr.get_shortest_words_from(StateSet(c_states_vec))
        return {tuple(word) for word in result}


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


cdef _wrap_noodle_with_epsilons_counter(mata_strings.CNoodleWithEpsilonsCounter c_noodle):
    noodle = []
    for c_segment_with_counter in c_noodle:
        segment = mata_nfa.Nfa()
        (<mata_nfa.Nfa>segment).thisptr = c_segment_with_counter.first
        noodle.append((segment, list(c_segment_with_counter.second)))
    return noodle


def noodlify_mult_eps(mata_nfa.Nfa aut, epsilons, include_empty = False):
    """Create noodles from segment automaton `aut`, for multiple epsilon symbols `epsilons`.

    :param mata_nfa.Nfa aut: Segment automaton to noodlify.
    :param epsilons: Epsilon symbols to noodlify for.
    :param bool include_empty: Whether to also include empty noodles.
    :return: A list of noodles, each a list of (segment, visited-epsilons-counter-vector) pairs.
    """
    cdef cset[Symbol] c_epsilons = epsilons
    cdef vector[mata_strings.CNoodleWithEpsilonsCounter] c_noodles = mata_strings.c_noodlify_mult_eps(
        dereference(aut.thisptr.get()), c_epsilons, include_empty
    )
    return [_wrap_noodle_with_epsilons_counter(c_noodle) for c_noodle in c_noodles]


def noodlify_for_equation_sides(left_side_automata, right_side_automata, include_empty = False, params = None):
    """Create noodles for an equation given as sequences of automata on both sides.

    :param left_side_automata: Segment automata representing the left side of the equation to noodlify.
    :param right_side_automata: Segment automata representing the right side of the equation to noodlify.
    :param bool include_empty: Whether to also include empty noodles.
    :param dict params: Additional parameters for the noodlification (see `noodlify_for_equation`).
    :return: A list of noodles, each a list of (segment, visited-epsilons-counter-vector) pairs.
    """
    cdef vector[shared_ptr[CNfa]] c_left_side_automata
    cdef vector[shared_ptr[CNfa]] c_right_side_automata
    for lhs_aut in left_side_automata:
        c_left_side_automata.push_back((<mata_nfa.Nfa>lhs_aut).thisptr)
    for rhs_aut in right_side_automata:
        c_right_side_automata.push_back((<mata_nfa.Nfa>rhs_aut).thisptr)
    params = params or {}
    cdef vector[mata_strings.CNoodleWithEpsilonsCounter] c_noodles = mata_strings.c_noodlify_for_equation_sides(
        c_left_side_automata, c_right_side_automata, include_empty,
        {
            k.encode('utf-8'): v.encode('utf-8') if isinstance(v, str) else v
            for k, v in params.items()
        },
    )
    return [_wrap_noodle_with_epsilons_counter(c_noodle) for c_noodle in c_noodles]


def noodlify_for_transducer(
    mata_nft.Nft nft, input_automata, output_automata, reduce_intersection = True, use_homomorphic_heuristic = True
):
    """Create noodles for a transducer applied between `input_automata` and `output_automata`.

    :param mata_nft.Nft nft: Transducer to noodlify.
    :param input_automata: Sequence of automata for the input tapes.
    :param output_automata: Sequence of automata for the output tapes.
    :return: A list of noodles, each a list of (transducer, input_aut, input_index, output_aut, output_index) tuples.
    """
    cdef shared_ptr[CNft] c_nft = (<mata_nft.Nft>nft).thisptr
    cdef vector[shared_ptr[CNfa]] c_input_automata
    cdef vector[shared_ptr[CNfa]] c_output_automata
    for input_aut in input_automata:
        c_input_automata.push_back((<mata_nfa.Nfa>input_aut).thisptr)
    for output_aut in output_automata:
        c_output_automata.push_back((<mata_nfa.Nfa>output_aut).thisptr)
    cdef vector[mata_strings.CTransducerNoodle] c_noodles = mata_strings.c_noodlify_for_transducer(
        c_nft, c_input_automata, c_output_automata, reduce_intersection, use_homomorphic_heuristic
    )
    noodles = []
    cdef mata_strings.CTransducerNoodle c_noodle
    cdef size_t i
    for c_noodle in c_noodles:
        noodle = []
        for i in range(c_noodle.size()):
            transducer = mata_nft.Nft.__new__(mata_nft.Nft)
            (<mata_nft.Nft>transducer).thisptr = c_noodle[i].transducer
            input_aut = mata_nfa.Nfa()
            (<mata_nfa.Nfa>input_aut).thisptr = c_noodle[i].input_aut
            output_aut = mata_nfa.Nfa()
            (<mata_nfa.Nfa>output_aut).thisptr = c_noodle[i].output_aut
            noodle.append((transducer, input_aut, c_noodle[i].input_index, output_aut, c_noodle[i].output_index))
        noodles.append(noodle)
    return noodles


def process_eps_map(eps_cnt: dict[int, int]) -> list[int]:
    """Process an epsilon count map to a sequence of values (sorted by key, descending)."""
    cdef cmap[Symbol, unsigned] c_eps_cnt = eps_cnt
    cdef vector[unsigned] result = mata_strings.c_process_eps_map(c_eps_cnt)
    return list(result)


def get_words_of_lengths(mata_nft.Nft nft, lengths: list[int]) -> list[list[Symbol]] | None:
    """Get the accepting words for each tape of `nft` with the specific `lengths`.

    :param mata_nft.Nft nft: Transducer whose accepting words we are looking for.
    :param lengths: The lengths of the words of each tape (size of `lengths` == number of levels of `nft`).
    :return: Either the resulting words of tapes, or `None` if such words of specific lengths do not exist.
    """
    cdef vector[unsigned] c_lengths = lengths
    cdef optional[vector[vector[Symbol]]] result = mata_strings.c_get_words_of_lengths(
        dereference((<mata_nft.Nft>nft).thisptr.get()), c_lengths
    )
    if not result.has_value():
        return None
    return [list(word) for word in result.value()]


def get_accepted_symbols(mata_nfa.Nfa nfa) -> set[Symbol]:
    """Get all the one-symbol words accepted by `nfa`."""
    cdef cset[Symbol] result = mata_strings.c_get_accepted_symbols(dereference(nfa.thisptr.get()))
    return {s for s in result}


def get_word_lengths(mata_nfa.Nfa aut) -> set[tuple[int, int]]:
    """Get the lengths of all words in `aut`.

    :return: Set of pairs `<u,v>`, where for each such a pair there is a word with length `u+k*v` for all `k`.
    """
    cdef cset[pair[int, int]] result = mata_strings.c_get_word_lengths(dereference(aut.thisptr.get()))
    return {(p.first, p.second) for p in result}


def is_lang_eps(mata_nfa.Nfa nfa) -> bool:
    """Checks if `nfa` accepts only the empty word."""
    return mata_strings.c_is_lang_eps(dereference(nfa.thisptr.get()))


class ReplaceMode(IntEnum):
    """How many occurrences of the regex/literal/symbol to replace, in order from left to right."""
    Single = <int>mata_strings.ReplaceModeSingle
    All = <int>mata_strings.ReplaceModeAll


cdef mata_strings.CReplaceMode _c_replace_mode(replace_mode):
    return <mata_strings.CReplaceMode><int>replace_mode


cdef mata_nft.Nft _wrap_nft(mata_nft.CNft c_nft):
    result = mata_nft.Nft.__new__(mata_nft.Nft)
    (<mata_nft.Nft>result).thisptr = make_shared[CNft](c_nft)
    return result


def reluctant_nfa(mata_nfa.Nfa nfa) -> mata_nfa.Nfa:
    """Modify a copy of `nfa` to remove outgoing transitions from final states.

    If `nfa` accepts the empty string, the returned NFA will accept only the empty string.
    """
    result = mata_nfa.Nfa()
    (<mata_nfa.Nfa>result).thisptr = make_shared[CNfa](
        mata_strings.c_reluctant_nfa(dereference(nfa.thisptr.get()))
    )
    return result


def create_identity(alph.Alphabet alphabet, num_of_levels: int = 2) -> mata_nft.Nft:
    """Create an identity transducer over `alphabet` with `num_of_levels` levels."""
    return _wrap_nft(mata_strings.c_create_identity(alphabet.as_base().get(), <size_t>num_of_levels))


def create_identity_with_single_symbol_replace(
    alph.Alphabet alphabet, Symbol from_symbol, replacement, replace_mode = ReplaceMode.All
) -> mata_nft.Nft:
    """Create an identity input/output transducer over `alphabet` with `from_symbol` replaced with `replacement`.

    :param replacement: Either a single symbol, or a word (list of symbols) to replace `from_symbol` with.
    """
    cdef CAlphabet* c_alphabet = alphabet.as_base().get()
    cdef vector[Symbol] c_word
    if isinstance(replacement, int):
        return _wrap_nft(
            mata_strings.c_create_identity_with_single_symbol_replace_symbol(
                c_alphabet, from_symbol, <Symbol>replacement, _c_replace_mode(replace_mode)
            )
        )
    c_word = replacement
    return _wrap_nft(
        mata_strings.c_create_identity_with_single_symbol_replace_word(
            c_alphabet, from_symbol, c_word, _c_replace_mode(replace_mode)
        )
    )


def replace_reluctant_regex(
    regex_or_aut, replacement, alph.Alphabet alphabet, replace_mode = ReplaceMode.All, begin_marker = None
) -> mata_nft.Nft:
    """Create an NFT modelling a reluctant leftmost replace of `regex_or_aut` with `replacement`.

    :param regex_or_aut: Either a regex string, or a deterministic `mata_nfa.Nfa` representing the regex to replace.
    :param replacement: The word (list of symbols) to replace with.
    :param alph.Alphabet alphabet: Alphabet over which to create the NFT.
    :param replace_mode: Whether to replace all, or just the leftmost, occurrence of `regex_or_aut`.
    :param begin_marker: Symbol to use internally as a begin marker. Defaults to `replace.BEGIN_MARKER`.
    """
    cdef CAlphabet* c_alphabet = alphabet.as_base().get()
    cdef vector[Symbol] c_replacement = replacement
    cdef Symbol c_begin_marker = mata_strings.CBEGIN_MARKER if begin_marker is None else <Symbol>begin_marker
    if isinstance(regex_or_aut, str):
        return _wrap_nft(
            mata_strings.c_replace_reluctant_regex_str(
                regex_or_aut.encode('utf-8'), c_replacement, c_alphabet, _c_replace_mode(replace_mode), c_begin_marker
            )
        )
    return _wrap_nft(
        mata_strings.c_replace_reluctant_regex_nfa(
            dereference((<mata_nfa.Nfa>regex_or_aut).thisptr.get()), c_replacement, c_alphabet,
            _c_replace_mode(replace_mode), c_begin_marker
        )
    )


def replace_reluctant_literal(
    literal, replacement, alph.Alphabet alphabet, replace_mode = ReplaceMode.All, end_marker = None
) -> mata_nft.Nft:
    """Create an NFT modelling a reluctant leftmost replace of `literal` with `replacement`."""
    cdef vector[Symbol] c_literal = literal
    cdef vector[Symbol] c_replacement = replacement
    cdef Symbol c_end_marker = mata_strings.CEND_MARKER if end_marker is None else <Symbol>end_marker
    return _wrap_nft(
        mata_strings.c_replace_reluctant_literal(
            c_literal, c_replacement, alphabet.as_base().get(), _c_replace_mode(replace_mode), c_end_marker
        )
    )


def replace_reluctant_single_symbol(
    Symbol from_symbol, replacement, alph.Alphabet alphabet, replace_mode = ReplaceMode.All
) -> mata_nft.Nft:
    """Create an NFT modelling a reluctant leftmost replace of `from_symbol` with `replacement`.

    :param replacement: Either a single symbol, or a word (list of symbols) to replace `from_symbol` with.
    """
    cdef CAlphabet* c_alphabet = alphabet.as_base().get()
    cdef vector[Symbol] c_word
    if isinstance(replacement, int):
        return _wrap_nft(
            mata_strings.c_replace_reluctant_single_symbol_symbol(
                from_symbol, <Symbol>replacement, c_alphabet, _c_replace_mode(replace_mode)
            )
        )
    c_word = replacement
    return _wrap_nft(
        mata_strings.c_replace_reluctant_single_symbol_word(
            from_symbol, c_word, c_alphabet, _c_replace_mode(replace_mode)
        )
    )
