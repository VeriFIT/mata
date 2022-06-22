from libcpp cimport bool

cdef extern from "vata2/nfa.hh" namespace "Vata2::Nfa":
    cdef cppclass Trans:
        int src
        int symb
        int tgt
        Trans(int, int, int) except +

    cdef cppclass Nfa:
        Nfa() except +
        void add_trans(int, int, int)
        bool has_trans(int, int, int)
        bool trans_empty()
        size_t trans_size()
