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

    @classmethod
    def is_deterministic(cls, Nfa lhs):
        """Tests if the lhs is determinstic

        :param Nfa lhs: non-determinstic finite automaton
        :return: true if the lhs is deterministic
        """
        return pynfa.is_deterministic(dereference(lhs.thisptr))

    @classmethod
    def determinize(cls, Nfa lhs):
        """Determinize the lhs automaton

        TODO: Add support for SubsetMap and State (no idea what that is currently)?

        :param Nfa lhs: non-deterministic finite automaton
        :return: deterministic finite automaton
        """
        result = Nfa()
        pynfa.determinize(result.thisptr, dereference(lhs.thisptr), NULL, NULL)
        return result

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
