from libc.stdint cimport uintptr_t
from libcpp cimport bool
from libcpp.memory cimport shared_ptr
from libcpp.set cimport set as cset
from libcpp.string cimport string
from libcpp.unordered_map cimport unordered_map as umap
from libcpp.vector cimport vector

from libmata.nfa.nfa cimport CNfa, CTrans
from libmata.alphabets cimport Symbol

cdef extern from "mata/nfa/nfa.hh" namespace "mata::nfa":
    ctypedef umap[string, string] ParameterMap

cdef extern from "mata/nfa/strings.hh" namespace "mata::strings":
    cdef cset[vector[Symbol]] c_get_shortest_words "mata::strings::get_shortest_words" (CNfa&)

cdef extern from "mata/nfa/strings.hh" namespace "mata::strings::seg_nfa":
    cdef cppclass CSegmentation "mata::strings::seg_nfa::Segmentation":
        CSegmentation(CNfa&, cset[Symbol]) except +

        ctypedef size_t EpsilonDepth
        ctypedef umap[EpsilonDepth, vector[CTrans]] EpsilonDepthTransitions

        EpsilonDepthTransitions get_epsilon_depths()
        vector[CNfa] get_segments()

    cdef vector[vector[shared_ptr[CNfa]]] c_noodlify "mata::strings::seg_nfa::noodlify" (CNfa&, Symbol, bool)
    cdef vector[vector[shared_ptr[CNfa]]] c_noodlify_for_equation "mata::strings::seg_nfa::noodlify_for_equation" \
        (const vector[CNfa*]&, CNfa&, bool, ParameterMap&)
