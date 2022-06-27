cimport pynfa
from libcpp cimport bool

cdef class Trans():
    cdef pynfa.CTrans *thisptr

    def __cinit__(self, int a, int b, int c):
        self.thisptr = new pynfa.CTrans(a, b, c)

    def __dealloc__(self):
        del self.thisptr

cdef class Nfa():
    cdef pynfa.CNfa *thisptr

    def __cinit__(self):
        self.thisptr = new pynfa.CNfa()

    def __dealloc__(self):
        del self.thisptr

    def add_trans(self, int src, int symb, int tgt):
        self.thisptr.add_trans(src, symb, tgt)

    def has_trans(self, int src, int symb, int tgt):
        return self.thisptr.has_trans(src, symb, tgt)

    def trans_empty(self):
        return self.thisptr.trans_empty()

    def trans_size(self):
        return self.thisptr.trans_size()

