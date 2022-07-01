from libcpp cimport bool
from libcpp.set cimport set
from libcpp.unordered_set cimport unordered_set as uset
from libcpp.unordered_map cimport unordered_map as umap
from libcpp.vector cimport vector
from libcpp.string cimport string
from libcpp.pair cimport pair
from libc.stdint cimport uintptr_t

cdef extern from "vata2/nfa.hh" namespace "Vata2::Nfa":
    # Typedefs
    ctypedef uintptr_t State
    ctypedef uintptr_t Symbol
    ctypedef set[State] StateSet
    ctypedef uset[State] UnorderedStateSet
    ctypedef pair[State, State] StatePair
    ctypedef umap[Symbol, StateSet] PostSymb
    ctypedef umap[State, PostSymb] StateToPostMap
    ctypedef umap[StatePair, State] ProductMap
    ctypedef umap[StateSet, State] SubsetMap
    ctypedef vector[State] Path
    ctypedef vector[Symbol] Word
    ctypedef umap[string, State] StringToStateMap
    ctypedef umap[string, Symbol] StringToSymbolMap
    ctypedef umap[State, string] StateToStringMap
    ctypedef umap[Symbol, string] SymbolToStringMap
    ctypedef umap[string, string] StringDict

    cdef cppclass CTrans "Vata2::Nfa::Trans":
        # Public Attributes
        State src
        Symbol symb
        State tgt

        # Constructor
        CTrans() except +
        CTrans(State, Symbol, State) except +

        # Public Functions
        bool operator==(CTrans)
        bool operator!=(CTrans)

    cdef cppclass CNfa "Vata2::Nfa::Nfa":
        # Nested iterator
        cppclass const_iterator:
            const_iterator()
            CTrans operator*()
            const_iterator& operator++()
            bool operator==(const_iterator&)
            bool operator!=(const_iterator&)
            void refresh_trans()

        # Public Attributes
        set[State] initialstates
        set[State] finalstates

        # Constructor
        CNfa() except +

        # Public Functions
        void add_initial(State)
        void add_initial(vector[State])
        bool has_initial(State)
        void add_final(State)
        bool has_final(State)
        void add_trans(CTrans)
        void add_trans(State, Symbol, State)
        bool has_trans(CTrans)
        bool has_trans(State, Symbol, State)
        bool trans_empty()
        size_t trans_size()
        PostSymb* post(State)
        StateSet post(StateSet&, Symbol)
        CNfa.const_iterator begin()
        CNfa.const_iterator end()

    # Automata tests
    cdef bool is_deterministic(CNfa&)
    cdef bool is_lang_empty(CNfa&, Path*)
    cdef bool is_lang_empty_cex(CNfa&, Word*)
    cdef bool is_universal(CNfa&, CAlphabet&, StringDict&)
    cdef bool is_incl(CNfa&, CNfa&, CAlphabet&, StringDict&)
    cdef bool is_incl(CNfa&, CNfa&, CAlphabet&, Word*, StringDict&)
    cdef bool is_complete(CNfa&, CAlphabet&)
    cdef bool is_in_lang(CNfa&, Word&)
    cdef bool is_prfx_in_lang(CNfa&, Word&)
    cdef bool accepts_epsilon(CNfa&)

    # Automata operations
    cdef void determinize(CNfa*, CNfa&, SubsetMap*, State*)
    cdef CNfa union_rename(CNfa&, CNfa&)
    cdef void union_no_rename(CNfa*, CNfa&, CNfa&)
    cdef void intersection(CNfa*, CNfa&, CNfa&, ProductMap*)
    cdef void complement(CNfa*, CNfa&, CAlphabet&, StringDict&)
    cdef void make_complete(CNfa*, CAlphabet&, State)
    cdef void revert(CNfa*, CNfa&)
    cdef void remove_epsilon(CNfa*, CNfa&, Symbol)
    cdef void minimize(CNfa*, CNfa&, StringDict&)

    # Helper functions
    cdef UnorderedStateSet get_fwd_reach_states(CNfa&)
    cdef pair[Word, bool] get_word_for_path(CNfa&, Path&)
    cdef Word encode_word(StringToSymbolMap&, vector[string])

    # Alphabets
    cdef cppclass CAlphabet "Vata2::Nfa::Alphabet":
        CAlphabet() except +

    cdef cppclass CCharAlphabet "Vata2::Nfa::CharAlphabet":
        CCharAlphabet() except +
        Symbol translate_symb(string)

    cdef cppclass CDirectAlphabet "Vata2::Nfa::DirectAlphabet":
        CDirectAlphabet() except +
        Symbol translate_symb(string)

    cdef cppclass CEnumAlphabet "Vata2::Nfa::EnumAlphabet":
        CEnumAlphabet() except +
        CEnumAlphabet(vector[string].iterator, vector[string].iterator) except +
        Symbol translate_symb(string) except +

    cdef cppclass COnTheFlyAlphabet "Vata2::Nfa::OnTheFlyAlphabet":
        COnTheFlyAlphabet(StringToSymbolMap*, Symbol) except +
        Symbol translate_symb(string)
