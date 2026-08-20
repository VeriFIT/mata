from libc.stdint cimport uintptr_t
from libcpp cimport bool
from libcpp.map cimport map as cmap
from libcpp.memory cimport shared_ptr
from libcpp.optional cimport optional
from libcpp.pair cimport pair
from libcpp.set cimport set as cset
from libcpp.string cimport string
from libcpp.unordered_map cimport unordered_map as umap
from libcpp.vector cimport vector

from libmata.nfa.nfa cimport CNfa, CTrans, State, StateSet
from libmata.nft.nft cimport CNft
from libmata.alphabets cimport CAlphabet, Symbol

# Pure Cython-side aliases (no corresponding "using" declaration in the C++ header to bind against), so these must
#  live outside any `cdef extern from` block.
ctypedef vector[unsigned] CVisitedEpsilonsCounterVector
ctypedef cmap[Symbol, unsigned] CVisitedEpsilonsCounterMap
ctypedef pair[shared_ptr[CNfa], CVisitedEpsilonsCounterVector] CSegmentWithEpsilonsCounter
ctypedef vector[CSegmentWithEpsilonsCounter] CNoodleWithEpsilonsCounter

cdef extern from "mata/nfa/nfa.hh" namespace "mata::nfa":
    ctypedef umap[string, string] ParameterMap

cdef extern from "mata/applications/strings.hh" namespace "mata::applications::strings":
    cdef cppclass CShortestWordsMap "mata::applications::strings::ShortestWordsMap":
        CShortestWordsMap(CNfa&) except +
        cset[vector[Symbol]] get_shortest_words_from(StateSet&) except +
        cset[vector[Symbol]] get_shortest_words_from(State) except +

    cdef cset[vector[Symbol]] c_get_shortest_words "mata::applications::strings::get_shortest_words" (CNfa&)
    cdef optional[vector[vector[Symbol]]] c_get_words_of_lengths \
        "mata::applications::strings::get_words_of_lengths" (CNft&, vector[unsigned]) except +
    cdef cset[Symbol] c_get_accepted_symbols "mata::applications::strings::get_accepted_symbols" (CNfa&)
    cdef cset[pair[int, int]] c_get_word_lengths "mata::applications::strings::get_word_lengths" (CNfa&)
    cdef bool c_is_lang_eps "mata::applications::strings::is_lang_eps" (CNfa&)

cdef extern from "mata/applications/strings.hh" namespace "mata::applications::strings::seg_nfa":
    cdef cppclass CSegmentation "mata::applications::strings::seg_nfa::Segmentation":
        CSegmentation(CNfa&, cset[Symbol]) except +

        ctypedef size_t EpsilonDepth
        ctypedef umap[EpsilonDepth, vector[CTrans]] EpsilonDepthTransitions

        EpsilonDepthTransitions get_epsilon_depths()
        vector[CNfa] get_segments()
        vector[CNfa] get_untrimmed_segments()

    cdef vector[vector[shared_ptr[CNfa]]] c_noodlify "mata::applications::strings::seg_nfa::noodlify" (CNfa&, Symbol, bool)
    cdef vector[CNoodleWithEpsilonsCounter] c_noodlify_mult_eps \
        "mata::applications::strings::seg_nfa::noodlify_mult_eps" (CNfa&, cset[Symbol]&, bool) except +
    cdef vector[vector[shared_ptr[CNfa]]] c_noodlify_for_equation "mata::applications::strings::seg_nfa::noodlify_for_equation" \
        (const vector[CNfa*]&, CNfa&, bool, ParameterMap&)
    cdef vector[CNoodleWithEpsilonsCounter] c_noodlify_for_equation_sides \
        "mata::applications::strings::seg_nfa::noodlify_for_equation" \
        (vector[shared_ptr[CNfa]]&, vector[shared_ptr[CNfa]]&, bool, ParameterMap&) except +

    cdef cppclass CTransducerNoodleElement "mata::applications::strings::seg_nfa::TransducerNoodleElement":
        shared_ptr[CNft] transducer
        shared_ptr[CNfa] input_aut
        unsigned input_index
        shared_ptr[CNfa] output_aut
        unsigned output_index

# Pure Cython-side alias; see the note above CVisitedEpsilonsCounterVector for why this lives outside the extern block.
ctypedef vector[CTransducerNoodleElement] CTransducerNoodle

cdef extern from "mata/applications/strings.hh" namespace "mata::applications::strings::seg_nfa":
    cdef vector[CTransducerNoodle] c_noodlify_for_transducer \
        "mata::applications::strings::seg_nfa::noodlify_for_transducer" \
        (shared_ptr[CNft]&, vector[shared_ptr[CNfa]]&, vector[shared_ptr[CNfa]]&, bool, bool) except +

    cdef CVisitedEpsilonsCounterVector c_process_eps_map "mata::applications::strings::seg_nfa::process_eps_map" \
        (CVisitedEpsilonsCounterMap&)

cdef extern from "mata/applications/strings.hh" namespace "mata::applications::strings::replace":
    cdef enum CReplaceMode "mata::applications::strings::replace::ReplaceMode":
        ReplaceModeSingle "mata::applications::strings::replace::ReplaceMode::Single"
        ReplaceModeAll "mata::applications::strings::replace::ReplaceMode::All"

    cdef Symbol CBEGIN_MARKER "mata::applications::strings::replace::BEGIN_MARKER"
    cdef Symbol CEND_MARKER "mata::applications::strings::replace::END_MARKER"

    cdef CNfa c_reluctant_nfa "mata::applications::strings::replace::reluctant_nfa" (CNfa) except +

    cdef CNft c_create_identity "mata::applications::strings::replace::create_identity" (CAlphabet*, size_t) except +

    cdef CNft c_create_identity_with_single_symbol_replace_word \
        "mata::applications::strings::replace::create_identity_with_single_symbol_replace" \
        (CAlphabet*, Symbol, vector[Symbol]&, CReplaceMode) except +
    cdef CNft c_create_identity_with_single_symbol_replace_symbol \
        "mata::applications::strings::replace::create_identity_with_single_symbol_replace" \
        (CAlphabet*, Symbol, Symbol, CReplaceMode) except +

    cdef CNft c_replace_reluctant_regex_str "mata::applications::strings::replace::replace_reluctant_regex" \
        (string&, vector[Symbol]&, CAlphabet*, CReplaceMode, Symbol) except +
    cdef CNft c_replace_reluctant_regex_nfa "mata::applications::strings::replace::replace_reluctant_regex" \
        (CNfa, vector[Symbol]&, CAlphabet*, CReplaceMode, Symbol) except +

    cdef CNft c_replace_reluctant_literal "mata::applications::strings::replace::replace_reluctant_literal" \
        (vector[Symbol]&, vector[Symbol]&, CAlphabet*, CReplaceMode, Symbol) except +

    cdef CNft c_replace_reluctant_single_symbol_word \
        "mata::applications::strings::replace::replace_reluctant_single_symbol" \
        (Symbol, vector[Symbol]&, CAlphabet*, CReplaceMode) except +
    cdef CNft c_replace_reluctant_single_symbol_symbol \
        "mata::applications::strings::replace::replace_reluctant_single_symbol" \
        (Symbol, Symbol, CAlphabet*, CReplaceMode) except +
