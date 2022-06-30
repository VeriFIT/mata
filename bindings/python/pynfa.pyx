cimport pynfa
from libcpp cimport bool
from libcpp.vector cimport vector
from libc.stdint cimport uintptr_t
from cython.operator import dereference

cdef class Trans():
    """
    Wrapper over the transitions
    """
    cdef pynfa.CTrans *thisptr

    def __cinit__(self, State src, Symbol s, State tgt):
        self.thisptr = new pynfa.CTrans(src, s, tgt)
        
    def __dealloc__(self):
        del self.thisptr

    def __eq__(self, Trans other):
        return dereference(self.thisptr) == dereference(other.thisptr)

    def __neq__(self, Trans other):
        return dereference(self.thisptr) != dereference(other.thisptr)

cdef class Nfa():
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
