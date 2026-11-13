from libcpp cimport bool
from libcpp.optional cimport optional
from libcpp.string cimport string
from libcpp.unordered_map cimport unordered_map
from libcpp.vector cimport vector

cdef extern from "<filesystem>" namespace "std" nogil:
    cdef cppclass cpath "std::filesystem::path":
        cpath(string other)

from libmata.nfa.nfa cimport CNfa, State
from libmata.alphabets cimport CAlphabet, Symbol

cdef extern from "<iostream>" namespace "std":
    cdef cppclass istream:
        istream& write(const char*, int) except +

cdef extern from "<fstream>" namespace "std":
    cdef cppclass ifstream(istream):
        ifstream() except+
        ifstream(const char*) except +

cdef extern from "mata/parser/re2parser.hh" namespace "mata::parser":
    cdef cppclass CRe2Options "mata::parser::Re2Options":
        bool keep_epsilon
        Symbol epsilon
        bool trim
        bool reduce

    cdef void create_nfa(CNfa*, string, bool, Symbol, bool) except +
    cdef void create_nfa(CNfa*, string, const CRe2Options&) except +

cdef extern from "mata/parser/inter-aut.hh" namespace "mata":
    cdef struct CInterAut "mata::IntermediateAut":
        CInterAut() except +
        bool is_bitvector()


cdef extern from "mata/nfa/builder.hh" namespace "mata::nfa::builder":
    ctypedef unordered_map[string, State] NameStateMap

    cdef void construct(CNfa*, CInterAut&, CAlphabet*) except +
    cdef CNfa parse_from_mata(string, CAlphabet*, NameStateMap*, bool) except +
    cdef CNfa parse_from_mata_path "parse_from_mata" (cpath&, CAlphabet*, NameStateMap*, bool) except +
    cdef optional[CNfa] parse_explicit_from_mata(string, CAlphabet*, NameStateMap*, bool) except +

    cdef enum CRead "mata::nfa::builder::BitvectorNfaReader::Read":
        Rejected "mata::nfa::builder::BitvectorNfaReader::Read::Rejected"
        Directly "mata::nfa::builder::BitvectorNfaReader::Read::Directly"
        ByGeneralReader "mata::nfa::builder::BitvectorNfaReader::Read::ByGeneralReader"

    cdef cppclass CBitvectorNfaReader "mata::nfa::builder::BitvectorNfaReader":
        CBitvectorNfaReader() except +
        CRead add(string, bool, NameStateMap*) except +
        vector[CNfa] build(CAlphabet*) except +

cdef extern from "mata/parser/parser.hh" namespace "mata::parser":
    cdef struct CParsedSection "mata::parser::ParsedSection":
        CParsedSection() except +

    ctypedef vector[CParsedSection] Parsed

    cdef Parsed parse_mf(istream, bool) except +

cdef extern from "mata/parser/inter-aut.hh" namespace "mata::IntermediateAut":
    vector[CInterAut] parse_from_mf(Parsed&) except +
