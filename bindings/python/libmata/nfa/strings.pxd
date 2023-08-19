from libc.stdint cimport uintptr_t
from libcpp cimport bool
from libcpp.memory cimport shared_ptr
from libcpp.set cimport set as cset
from libcpp.string cimport string
from libcpp.unordered_map cimport unordered_map as umap
from libcpp.vector cimport vector

from libmata.nfa.nfa cimport CNfa, CTrans
from libmata.alphabets cimport Symbol

cdef extern from "mata/nfa/nfa.hh" namespace "Mata::Nfa":
    ctypedef umap[string, string] ParameterMap

cdef extern from "mata/nfa/strings.hh" namespace "Mata::Strings":
    cdef cset[vector[Symbol]] c_get_shortest_words "Mata::Strings::get_shortest_words" (CNfa&)

cdef extern from "mata/nfa/strings.hh" namespace "Mata::Strings::SegNfa":
    cdef cppclass CSegmentation "Mata::Strings::SegNfa::Segmentation":
        CSegmentation(CNfa&, cset[Symbol]) except +

        ctypedef size_t EpsilonDepth
        ctypedef umap[EpsilonDepth, vector[CTrans]] EpsilonDepthTransitions

        EpsilonDepthTransitions get_epsilon_depths()
        vector[CNfa] get_segments()

    cdef vector[vector[shared_ptr[CNfa]]] c_noodlify "Mata::Strings::SegNfa::noodlify" (CNfa&, Symbol, bool)
    cdef vector[vector[shared_ptr[CNfa]]] c_noodlify_for_equation "Mata::Strings::SegNfa::noodlify_for_equation" \
        (const vector[CNfa*]&, CNfa&, bool, ParameterMap&)
