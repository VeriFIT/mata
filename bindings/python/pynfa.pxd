from libcpp cimport bool

cdef extern from "vata2/nfa.hh" namespace "Vata2::Nfa":
    cdef cppclass CTrans "Vata2::Nfa::Trans":
        int src
        int symb
        int tgt
        CTrans(int, int, int) except +

    cdef cppclass CNfa "Vata2::Nfa::Nfa":
        CNfa() except +
        void add_trans(int, int, int)
        bool has_trans(int, int, int)
        bool trans_empty()
        size_t trans_size()
