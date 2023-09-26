from libcpp cimport bool
from libcpp.string cimport string
from libcpp.vector cimport vector

from libmata.nfa.nfa cimport CNfa
from libmata.alphabets cimport CAlphabet

cdef extern from "<iostream>" namespace "std":
    cdef cppclass istream:
        istream& write(const char*, int) except +

cdef extern from "<fstream>" namespace "std":
    cdef cppclass ifstream(istream):
        ifstream() except+
        ifstream(const char*) except +

cdef extern from "mata/parser/re2parser.hh" namespace "mata::parser":
    cdef void create_nfa(CNfa*, string) except +
    cdef void create_nfa(CNfa*, string, bool) except +

cdef extern from "mata/parser/inter-aut.hh" namespace "mata":
    cdef struct CInterAut "mata::IntermediateAut":
        CInterAut() except +
        bool is_bitvector()


cdef extern from "mata/nfa/builder.hh" namespace "mata::nfa::builder":
    cdef void construct(CNfa*, CInterAut&, CAlphabet*);

cdef extern from "mata/parser/parser.hh" namespace "mata::parser":
    cdef struct CParsedSection "mata::parser::ParsedSection":
        CParsedSection() except +

    ctypedef vector[CParsedSection] Parsed

    cdef Parsed parse_mf(istream, bool) except +

cdef extern from "mata/parser/inter-aut.hh" namespace "mata::IntermediateAut":
    vector[CInterAut] parse_from_mf(Parsed&)

cdef extern from "mata/parser/mintermization.hh" namespace "mata":
    cdef cppclass CMintermization "mata::Mintermization":
        CMintermization()
        CInterAut  c_mintermize "mata::Mintermization::mintermize" (CInterAut&);
        vector[CInterAut]  c_mintermize_vec "mata::Mintermization::mintermize" (vector[CInterAut]&);
