from libc.stdint cimport uint64_t, uintptr_t
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

cdef extern from "mata/applications/strings.hh" namespace "mata::applications::strings::seg_nfa":
    cdef cppclass CNoodleSegment "mata::applications::strings::seg_nfa::NoodleSegment":
        shared_ptr[const CNfa] automaton
        State initial
        CNfa to_nfa() except +

# Pure Cython-side aliases (no corresponding "using" declaration in the C++ header to bind against), so these must
#  live outside any `cdef extern from` block.
ctypedef vector[unsigned] CVisitedEpsilonsCounterVector
ctypedef cmap[Symbol, unsigned] CVisitedEpsilonsCounterMap
ctypedef pair[CNoodleSegment, CVisitedEpsilonsCounterVector] CSegmentWithEpsilonsCounter
ctypedef vector[CSegmentWithEpsilonsCounter] CNoodleWithEpsilonsCounter
ctypedef vector[CNoodleSegment] CNoodle

cdef extern from "mata/nfa/nfa.hh" namespace "mata::nfa":
    ctypedef umap[string, string] ParameterMap

cdef extern from "mata/applications/strings.hh" namespace "mata::applications::strings":
    cdef cppclass CShortestWordsMap "mata::applications::strings::ShortestWordsMap":
        CShortestWordsMap(CNfa&) except +
        cset[vector[Symbol]] get_shortest_words_from(StateSet&) except +
        cset[vector[Symbol]] get_shortest_words_from(State) except +

    cdef cset[vector[Symbol]] c_get_shortest_words "mata::applications::strings::get_shortest_words" (CNfa&) except +
    cdef optional[vector[vector[Symbol]]] c_get_words_of_lengths \
        "mata::applications::strings::get_words_of_lengths" (CNft&, const vector[unsigned]&) except +
    cdef cset[Symbol] c_get_accepted_symbols "mata::applications::strings::get_accepted_symbols" (CNfa&) except +
    cdef cset[pair[int, int]] c_get_word_lengths "mata::applications::strings::get_word_lengths" (CNfa&) except +
    cdef bool c_is_lang_eps "mata::applications::strings::is_lang_eps" (CNfa&) except +

cdef extern from "mata/applications/strings.hh" namespace "mata::applications::strings::seg_nfa":
    cdef cppclass CSegmentation "mata::applications::strings::seg_nfa::Segmentation":
        CSegmentation(CNfa&, cset[Symbol]) except +

        ctypedef size_t EpsilonDepth
        ctypedef umap[EpsilonDepth, vector[CTrans]] EpsilonDepthTransitions

        EpsilonDepthTransitions get_epsilon_depths() except +
        const vector[CNfa]& get_segments() except +
        const vector[CNfa]& get_untrimmed_segments() except +

    # The noodlify entry points run without the GIL; see the note on the plumbing externs in nfa.pxd.
    cdef vector[CNoodle] c_noodlify "mata::applications::strings::seg_nfa::noodlify" \
        (CNfa&, Symbol, bool) except + nogil
    cdef vector[CNoodleWithEpsilonsCounter] c_noodlify_mult_eps \
        "mata::applications::strings::seg_nfa::noodlify_mult_eps" (CNfa&, cset[Symbol]&, bool) except + nogil
    cdef vector[CNoodle] c_noodlify_for_equation "mata::applications::strings::seg_nfa::noodlify_for_equation" \
        (const vector[CNfa*]&, CNfa&, bool, ParameterMap&) except +
    cdef vector[CNoodleWithEpsilonsCounter] c_noodlify_for_equation_sides \
        "mata::applications::strings::seg_nfa::noodlify_for_equation" \
        (vector[shared_ptr[CNfa]]&, vector[shared_ptr[CNfa]]&, bool, ParameterMap&) except + nogil

    cdef enum CNoodleOrderKind "mata::applications::strings::seg_nfa::NoodleOrder::Kind":
        NoodleOrderForward "mata::applications::strings::seg_nfa::NoodleOrder::Kind::Forward"
        NoodleOrderReverse "mata::applications::strings::seg_nfa::NoodleOrder::Kind::Reverse"
        NoodleOrderSmallestFirst "mata::applications::strings::seg_nfa::NoodleOrder::Kind::SmallestFirst"
        NoodleOrderLargestFirst "mata::applications::strings::seg_nfa::NoodleOrder::Kind::LargestFirst"
        NoodleOrderRandom "mata::applications::strings::seg_nfa::NoodleOrder::Kind::Random"

    cdef cppclass CNoodleOrder "mata::applications::strings::seg_nfa::NoodleOrder":
        CNoodleOrderKind kind
        uint64_t seed

    cdef cppclass CTransducerNoodleElement "mata::applications::strings::seg_nfa::TransducerNoodleElement":
        shared_ptr[CNft] transducer
        shared_ptr[CNfa] input_aut
        unsigned input_index
        shared_ptr[CNfa] output_aut
        unsigned output_index

# Pure Cython-side alias; see the note above CVisitedEpsilonsCounterVector for why this lives outside the extern block.
ctypedef vector[CTransducerNoodleElement] CTransducerNoodle

# Pull adaptors over the noodlify_*_lazy() generators, created only by the *_lazy_ptr() functions below; see
#  CLazyWordGenerator in alphabets.pxd. `done()`/`next()` run the search to the noodle they return, so they run
#  without the GIL.
cdef extern from "mata/utils/lazy-generator.hh" namespace "mata::utils":
    cdef cppclass CLazyNoodleGenerator \
        "mata::utils::LazyGenerator<mata::applications::strings::seg_nfa::Noodle>":
        bool done() except + nogil
        CNoodle next() except + nogil

    cdef cppclass CLazyNoodleWithEpsilonsCounterGenerator \
        "mata::utils::LazyGenerator<mata::applications::strings::seg_nfa::NoodleWithEpsilonsCounter>":
        bool done() except + nogil
        CNoodleWithEpsilonsCounter next() except + nogil

    cdef cppclass CLazyTransducerNoodleGenerator \
        "mata::utils::LazyGenerator<mata::applications::strings::seg_nfa::TransducerNoodle>":
        bool done() except + nogil
        CTransducerNoodle next() except + nogil

cdef extern from "mata/applications/strings.hh" namespace "mata::applications::strings::seg_nfa":
    cdef CLazyNoodleGenerator* c_noodlify_lazy_ptr "mata::applications::strings::seg_nfa::noodlify_lazy_ptr" \
        (const CNfa&, Symbol, bool, CNoodleOrder) except + nogil
    cdef CLazyNoodleWithEpsilonsCounterGenerator* c_noodlify_mult_eps_lazy_ptr \
        "mata::applications::strings::seg_nfa::noodlify_mult_eps_lazy_ptr" \
        (const CNfa&, const cset[Symbol]&, bool, CNoodleOrder) except + nogil
    # Copies the automata into the frame of a coroutine that owns them, so the wrapper of the iterator holds nothing
    #  alive; it still unifies the left side automata in place when reducing, like c_noodlify_for_equation.
    cdef CLazyNoodleGenerator* c_noodlify_for_equation_lazy_ptr \
        "mata::applications::strings::seg_nfa::noodlify_for_equation_lazy_ptr" \
        (const vector[shared_ptr[CNfa]]&, const shared_ptr[CNfa]&, bool, ParameterMap&, CNoodleOrder) except + nogil
    cdef CLazyNoodleWithEpsilonsCounterGenerator* c_noodlify_for_equation_sides_lazy_ptr \
        "mata::applications::strings::seg_nfa::noodlify_for_equation_lazy_ptr" \
        (const vector[shared_ptr[CNfa]]&, const vector[shared_ptr[CNfa]]&, bool, ParameterMap&, CNoodleOrder) \
        except + nogil
    cdef CLazyTransducerNoodleGenerator* c_noodlify_for_transducer_lazy_ptr \
        "mata::applications::strings::seg_nfa::noodlify_for_transducer_lazy_ptr" \
        (const shared_ptr[CNft]&, const vector[shared_ptr[CNfa]]&, const vector[shared_ptr[CNfa]]&, bool, bool, \
         CNoodleOrder) except + nogil

cdef extern from "mata/applications/strings.hh" namespace "mata::applications::strings::seg_nfa":
    cdef vector[CTransducerNoodle] c_noodlify_for_transducer \
        "mata::applications::strings::seg_nfa::noodlify_for_transducer" \
        (shared_ptr[CNft]&, vector[shared_ptr[CNfa]]&, vector[shared_ptr[CNfa]]&, bool, bool) except + nogil

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
