cimport pynfa
from libcpp.vector cimport vector
from cython.operator import dereference, postincrement as postinc, preincrement as preinc

cdef class Trans:
    """
    Wrapper over the transitions
    """
    cdef pynfa.CTrans *thisptr

    def __cinit__(self, State src=0, Symbol s=0, State tgt=0):
        self.thisptr = new pynfa.CTrans(src, s, tgt)

    def __dealloc__(self):
        if self.thisptr != NULL:
            del self.thisptr

    def __eq__(self, Trans other):
        return dereference(self.thisptr) == dereference(other.thisptr)

    def __neq__(self, Trans other):
        return dereference(self.thisptr) != dereference(other.thisptr)

    cdef copy_from(self, CTrans trans):
        """Copies the internals of trans into the wrapped pointer

        :param CTrans trans: copied transition
        """
        self.thisptr.src = trans.src
        self.thisptr.symb = trans.symb
        self.thisptr.tgt = trans.tgt

    def __str__(self):
        return f"{self.thisptr.src}-[{self.thisptr.symb}]\u2192{self.thisptr.tgt}"

    def __repr__(self):
        return str(self)

cdef class Nfa:
    """
    Wrapper over NFA
    """
    cdef pynfa.CNfa *thisptr

    def __cinit__(self):
        self.thisptr = new pynfa.CNfa()

    def __dealloc__(self):
        del self.thisptr

    def add_initial_state(self, State st):
        self.thisptr.add_initial(st)

    def add_initial_states(self, vector[State] states):
        self.thisptr.add_initial(states)

    def has_initial_state(self, State st):
        return self.thisptr.has_initial(st)

    def add_final_state(self, State st):
        self.thisptr.add_final(st)

    def has_final_state(self, State st):
        return self.thisptr.has_final(st)

    def add_trans(self, Trans tr):
        self.thisptr.add_trans(dereference(tr.thisptr))

    def add_trans_raw(self, State src, Symbol symb, State tgt):
        self.thisptr.add_trans(src, symb, tgt)

    def has_trans(self, Trans tr):
        return self.thisptr.has_trans(dereference(tr.thisptr))

    def has_trans_raw(self, State src, Symbol symb, State tgt):
        return self.thisptr.has_trans(src, symb, tgt)

    def trans_empty(self):
        return self.thisptr.trans_empty()

    def trans_size(self):
        return self.thisptr.trans_size()

    def iterate(self):
        iterator = self.thisptr.begin()
        while iterator != self.thisptr.end():
            trans = Trans()
            lhs = dereference(iterator)
            trans.copy_from(lhs)
            preinc(iterator)
            yield trans

    def post_map_of(self, State st):
        """Returns mapping of symbols to set of states.

        :param State st: source state
        :return: dictionary mapping symbols to set of reachable states from the symbol
        """
        return dereference(self.thisptr.post(st))

    def post_of(self, StateSet& states, Symbol symbol):
        """Returns sets of reachable states from set of states through a symbol

        :param StateSet states: set of states
        :param Symbol symbol: source symbol
        :return: set of reachable states
        """
        return self.thisptr.post(states, symbol)

    # Operations
    @classmethod
    def determinize(cls, Nfa lhs):
        """Determinize the lhs automaton

        :param Nfa lhs: non-deterministic finite automaton
        :return: deterministic finite automaton, subset map
        """
        result = Nfa()
        cdef SubsetMap subset_map
        cdef State last_state
        pynfa.determinize(result.thisptr, dereference(lhs.thisptr), &subset_map, &last_state)
        return result, {tuple(sorted(k)): v for k, v in subset_map}, last_state

    @classmethod
    def union(cls, Nfa lhs, Nfa rhs):
        result = Nfa()
        pynfa.union_rename(
            result.thisptr, dereference(lhs.thisptr), dereference(rhs.thisptr)
        )
        return result

    @classmethod
    def intersection(cls, Nfa lhs, Nfa rhs):
        result = Nfa()
        pynfa.intersection(
            result.thisptr, dereference(lhs.thisptr), dereference(rhs.thisptr), NULL
        )
        return result

    @classmethod
    def complement(cls, Nfa lhs, OnTheFlyAlphabet alphabet = None, params = None):
        if alphabet is None:
            alphabet = OnTheFlyAlphabet()
        result = Nfa()
        pynfa.complement(
            result.thisptr,
            dereference(lhs.thisptr),
            <CAlphabet&>dereference(alphabet.thisptr),
            params or {}
        )
        return result

    @classmethod
    def make_complete(cls, Nfa lhs, State state, OnTheFlyAlphabet alphabet = None):
        if alphabet is None:
            alphabet = OnTheFlyAlphabet()
        pynfa.make_complete(lhs.thisptr, <CAlphabet&>dereference(alphabet.thisptr), state)

    @classmethod
    def revert(cls, Nfa lhs):
        result = Nfa()
        pynfa.revert(result.thisptr, dereference(lhs.thisptr))
        return result

    @classmethod
    def remove_epsilon(cls, Nfa lhs, Symbol epsilon):
        result = Nfa()
        pynfa.remove_epsilon(
            result.thisptr, dereference(lhs.thisptr), epsilon
        )
        return result

    @classmethod
    def minimize(cls, Nfa lhs, params = None):
        result = Nfa()
        pynfa.minimize(
            result.thisptr, dereference(lhs.thisptr), params or {}
        )
        return result

    # Tests
    @classmethod
    def is_deterministic(cls, Nfa lhs):
        """Tests if the lhs is determinstic

        :param Nfa lhs: non-determinstic finite automaton
        :return: true if the lhs is deterministic
        """
        return pynfa.is_deterministic(dereference(lhs.thisptr))

    @classmethod
    def is_lang_empty_path_counterexample(cls, Nfa lhs):
        """Checks if language of automaton lhs is empty, if not, returns path of states as counter
        example.

        :param Nfa lhs:
        :return: true if the lhs is empty, counter example if lhs is not empty
        """
        cdef Path path
        result = pynfa.is_lang_empty(dereference(lhs.thisptr), &path)
        return result, path


    @classmethod
    def is_lang_empty_word_counterexample(cls, Nfa lhs):
        """Checks if language of automaton lhs is empty, if not, returns word as counter example.

        :param Nfa lhs:
        :return: true if the lhs is empty, counter example if lhs is not empty
        """
        cdef Word word
        result = pynfa.is_lang_empty_cex(dereference(lhs.thisptr), &word)
        return result, word

    @classmethod
    def is_universal(cls, Nfa lhs, OnTheFlyAlphabet alphabet, params = None):
        """Tests if lhs is universal wrt given alphabet

        :param Nfa lhs: automaton tested for universality
        :param OnTheFlyAlphabet alphabet: on the fly alphabet
        :param dict params: additional params to the function, currently supports key 'algo',
            which determines used universality test
        :return: true if lhs is universal
        """
        params = params or {'algo': 'antichains'}
        return pynfa.is_universal(
            dereference(lhs.thisptr),
            <CAlphabet&>dereference(alphabet.thisptr),
            {
                k.encode('utf-8'): v.encode('utf-8') if isinstance(v, str) else v
                for k, v in params.items()
            }
        )

    @classmethod
    def is_included(
            cls, Nfa lhs, Nfa rhs, OnTheFlyAlphabet alphabet, params = None
    ):
        """Test inclusion between two automata

        :param Nfa lhs: smaller automaton
        :param Nfa rhs: bigger automaton
        :param Alphabet alphabet: alpabet shared by two automata
        :param dict params: adtitional params
        :return: true if lhs is included by rhs, counter example word if not
        """
        cdef Word word
        params = params or {'algo': 'antichains'}
        result = pynfa.is_incl(
            dereference(lhs.thisptr),
            dereference(rhs.thisptr),
            <CAlphabet&>dereference(alphabet.thisptr),
            &word,
            {
                k.encode('utf-8'): v.encode('utf-8') if isinstance(v, str) else v
                for k, v in params.items()
            }
        )
        return result, word

    @classmethod
    def is_complete(cls, Nfa lhs, OnTheFlyAlphabet alphabet):
        """Test if automaton is complete

        :param Nf lhs: tested automaton
        :param OnTheFlyAlphabet alphabet: alphabet of the automaton
        :return: true if the automaton is complete
        """
        return pynfa.is_complete(
            dereference(lhs.thisptr),
            <CAlphabet&>dereference(alphabet.thisptr)
        )

    @classmethod
    def is_in_lang(cls, Nfa lhs, vector[Symbol] word):
        """Tests if word is in language

        :param Nfa lhs: tested automaton
        :param vector[Symbol] word: tested word
        :return: true if word is in language of automaton lhs
        """
        return pynfa.is_in_lang(dereference(lhs.thisptr), <Word> word)

    @classmethod
    def is_prefix_in_lang(cls, Nfa lhs, vector[Symbol] word):
        """Test if any prefix of the word is in the language

        :param Nfa lhs: tested automaton
        :param vector[Symbol] word: tested word
        :return: true if any prefix of word is in language of automaton lhs
        """
        return pynfa.is_prfx_in_lang(dereference(lhs.thisptr), <Word> word)

    @classmethod
    def accepts_epsilon(cls, Nfa lhs):
        """Tests if automaton accepts epsilon

        :param Nfa lhs: tested automaton
        :return: true if automaton accepts epsilon
        """
        return pynfa.accepts_epsilon(dereference(lhs.thisptr))

    # Helper functions
    @classmethod
    def get_forward_reachable_states(cls, Nfa lhs):
        """Returns list of reachable states from initial states

        >>> pynfa.Nfa.get_forward_reachable_states(lhs)
        {0, 1, 2}

        :param Nfa lhs: source automaton
        :return: set of reachable states
        """
        return pynfa.get_fwd_reach_states(dereference(lhs.thisptr))


    @classmethod
    def get_word_for_path(cls, Nfa lhs, path):
        """For a given path (set of states) returns a corresponding word

        >>> pynfa.Nfa.get_word_for_path(lhs, [0, 1, 2])
        ([1, 1], True)

        :param Nfa lhs: source automaton
        :param list path: list of states
        :return: pair of word (list of symbols) and true or false, whether the search was successful
        """
        return pynfa.get_word_for_path(dereference(lhs.thisptr), path)

    @classmethod
    def encode_word(cls, string_to_symbol, word):
        """Encodes word based on a string to symbol map

        >>> pynfa.Nfa.encode_word({'a': 1, 'b': 2, "c": 0}, "abca")
        [1, 2, 0, 1]

        :param dict string_to_symbol: dictionary of strings to integers
        :param word: list of strings representing a encoded word
        :return:
        """
        return pynfa.encode_word(
            {k.encode('utf-8'): v for (k, v) in string_to_symbol.items()},
            [s.encode('utf-8') for s in word]
        )

cdef class CharAlphabet:
    cdef pynfa.CCharAlphabet *thisptr

    def __cinit__(self):
        self.thisptr = new pynfa.CCharAlphabet()

    def __dealloc__(self):
        del self.thisptr

    def translate_symbol(self, str symbol):
        return self.thisptr.translate_symb(symbol.encode('utf-8'))


cdef class EnumAlphabet:
    cdef pynfa.CEnumAlphabet *thisptr

    def __cinit__(self, enums):
        cdef vector[string] enums_as_strings = [e.encode('utf-8') for e in enums]
        self.thisptr = new pynfa.CEnumAlphabet(enums_as_strings.begin(), enums_as_strings.end())

    def __dealloc__(self):
        del self.thisptr

    def translate_symbol(self, str symbol):
        return self.thisptr.translate_symb(symbol.encode('utf-8'))


cdef class DirectAlphabet:
    cdef pynfa.CDirectAlphabet *thisptr

    def __cinit__(self):
        self.thisptr = new pynfa.CDirectAlphabet()

    def __dealloc__(self):
        del self.thisptr

    def translate_symbol(self, str symbol):
        return self.thisptr.translate_symb(symbol.encode('utf-8'))


cdef class OnTheFlyAlphabet:
    cdef pynfa.COnTheFlyAlphabet *thisptr
    cdef StringToSymbolMap string_to_symbol_map

    def __cinit__(self, State initial_symbol = 0):
        self.thisptr = new pynfa.COnTheFlyAlphabet(&self.string_to_symbol_map, initial_symbol)

    def __dealloc__(self):
        del self.thisptr

    def translate_symbol(self, str symbol):
        return self.thisptr.translate_symb(symbol.encode('utf-8'))


# Temporary for testing
def divisible_by(k: int):
    """
    Constructs automaton accepting strings containing ones divisible by "k"
    """
    assert k > 1
    lhs = Nfa()
    lhs.add_initial_state(0)
    lhs.add_trans_raw(0, 0, 0)
    for i in range(1, k + 1):
        lhs.add_trans_raw(i - 1, 1, i)
        lhs.add_trans_raw(i, 0, i)
    lhs.add_trans_raw(k, 1, 1)
    lhs.add_final_state(k)
    return lhs
