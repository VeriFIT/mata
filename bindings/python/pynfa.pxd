from libcpp cimport bool
from libcpp.set cimport set
from libcpp.vector cimport vector
from libc.stdint cimport uintptr_t

cdef extern from "vata2/nfa.hh" namespace "Vata2::Nfa":
    # Typedefs
    ctypedef uintptr_t State
    ctypedef uintptr_t Symbol

    cdef cppclass CTrans "Vata2::Nfa::Trans":
        # Public Attributes
        State src
        Symbol symb
        State tgt

        # Constructor
        CTrans(State, Symbol, State) except +

        # Public Functions
        bool operator==(CTrans)
        bool operator!=(CTrans)

    cdef cppclass CNfa "Vata2::Nfa::Nfa":
        # Public Attributes
        set[State] initialstates
        set[State] finalstates

        # Constructor
        CNfa() except +

        # Public Functions
        void add_initial(State)
        void add_initial(vector[State])
        bool has_initial(State)
        void add_final(State)
        bool has_final(State)
        void add_trans(CTrans)
        void add_trans(State, Symbol, State)
        bool has_trans(CTrans)
        bool has_trans(State, Symbol, State)
        bool trans_empty()
        size_t trans_size()
