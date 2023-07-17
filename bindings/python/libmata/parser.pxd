from libcpp cimport bool
from libcpp.string cimport string

from libmata.nfa.nfa cimport CNfa

cdef extern from "mata/parser/re2parser.hh" namespace "Mata::Parser":
    cdef void create_nfa(CNfa*, string) except +
    cdef void create_nfa(CNfa*, string, bool) except +
