from libcpp cimport bool
from libcpp.set cimport set
from libcpp.unordered_set cimport unordered_set as uset
from libcpp.unordered_map cimport unordered_map as umap
from libcpp.vector cimport vector
from libcpp.string cimport string
from libcpp.list cimport list as clist
from libcpp.pair cimport pair
from libc.stdint cimport uintptr_t

cdef extern from "<iostream>" namespace "std":
    cdef cppclass ostream:
        ostream& write(const char*, int) except +

cdef extern from "<fstream>" namespace "std":
    cdef cppclass ofstream(ostream):
        ofstream(const char*) except +

cdef extern from "vata2/ord_vector.hh" namespace "Vata2::Util":
    cdef cppclass COrdVector "Vata2::Util::OrdVector" [T]:
        COrdVector() except+
        COrdVector(vector[T]) except+
        vector[T] ToVector()


cdef extern from "vata2/nfa.hh" namespace "Vata2::Nfa":
    # Typedefs
    ctypedef uintptr_t State
    ctypedef uintptr_t Symbol
    ctypedef COrdVector[State] StateSet
    ctypedef uset[State] UnorderedStateSet
    ctypedef umap[Symbol, StateSet] PostSymb
    ctypedef umap[State, PostSymb] StateToPostMap
    ctypedef umap[pair[State, State], State] ProductMap
    ctypedef umap[StateSet, State] SubsetMap
    ctypedef umap[string, State] StringSubsetMap
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
        StateSet initialstates
        StateSet finalstates

        # Constructor
        CNfa() except +
        CNfa(unsigned long) except +

        # Public Functions
        void add_initial(State)
        void add_initial(vector[State])
        bool has_initial(State)
        void remove_initial(State)
        void add_final(State)
        bool has_final(State)
        void remove_final(State)
        void add_trans(CTrans) except +
        void add_trans(State, Symbol, State) except +
        bool has_trans(CTrans)
        bool has_trans(State, Symbol, State)
        bool trans_empty()
        bool nothing_in_trans()
        bool is_state(State)
        size_t trans_size()
        StateSet post(StateSet&, Symbol)
        CNfa.const_iterator begin()
        CNfa.const_iterator end()
        size_t get_num_of_states()
        void increase_size(size_t)
        State add_new_state()
        void print_to_DOT(ostream)

    # Automata tests
    cdef bool is_deterministic(CNfa&)
    cdef bool is_lang_empty(CNfa&, Path*)
    cdef bool is_lang_empty_cex(CNfa&, Word*)
    cdef bool is_universal(CNfa&, CAlphabet&, StringDict&) except +
    cdef bool is_incl(CNfa&, CNfa&, CAlphabet&, StringDict&)
    cdef bool is_incl(CNfa&, CNfa&, CAlphabet&, Word*, StringDict&) except +
    cdef bool is_complete(CNfa&, CAlphabet&) except +
    cdef bool is_in_lang(CNfa&, Word&)
    cdef bool is_prfx_in_lang(CNfa&, Word&)

    # Automata operations
    cdef void determinize(CNfa*, CNfa&, SubsetMap*)
    cdef void uni(CNfa*, CNfa&, CNfa&)
    cdef void intersection(CNfa*, CNfa&, CNfa&, ProductMap*)
    cdef void complement(CNfa*, CNfa&, CAlphabet&, StringDict&, SubsetMap*) except +
    cdef void make_complete(CNfa*, CAlphabet&, State) except +
    cdef void revert(CNfa*, CNfa&)
    cdef void remove_epsilon(CNfa*, CNfa&, Symbol) except +
    cdef void minimize(CNfa*, CNfa&)

    # Helper functions
    cdef pair[Word, bool] get_word_for_path(CNfa&, Path&)
    cdef Word encode_word(StringToSymbolMap&, vector[string])

    # Alphabets
    cdef cppclass CAlphabet "Vata2::Nfa::Alphabet":
        CAlphabet() except +

    cdef cppclass CCharAlphabet "Vata2::Nfa::CharAlphabet":
        CCharAlphabet() except +
        Symbol translate_symb(string)
        clist[Symbol] get_symbols()

    cdef cppclass CDirectAlphabet "Vata2::Nfa::DirectAlphabet":
        CDirectAlphabet() except +
        Symbol translate_symb(string)
        clist[Symbol] get_symbols()

    cdef cppclass CEnumAlphabet "Vata2::Nfa::EnumAlphabet":
        CEnumAlphabet() except +
        CEnumAlphabet(vector[string].iterator, vector[string].iterator) except +
        Symbol translate_symb(string) except +
        clist[Symbol] get_symbols()

    cdef cppclass COnTheFlyAlphabet "Vata2::Nfa::OnTheFlyAlphabet":
        COnTheFlyAlphabet(StringToSymbolMap*, Symbol) except +
        Symbol translate_symb(string)
        clist[Symbol] get_symbols()
