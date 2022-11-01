from libcpp cimport bool
from libcpp.set cimport set as cset
from libcpp.unordered_set cimport unordered_set as uset
from libcpp.unordered_map cimport unordered_map as umap
from libcpp.vector cimport vector
from libcpp.memory cimport shared_ptr
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

cdef extern from "<sstream>" namespace "std":
    cdef cppclass stringstream(ostream):
        stringstream(string) except +
        string str()

cdef extern from "simlib/util/binary_relation.hh" namespace "Simlib::Util":
    ctypedef vector[size_t] ivector
    ctypedef vector[bool] bvector
    ctypedef vector[ivector] IndexType

    cdef cppclass CBinaryRelation "Simlib::Util::BinaryRelation":
        CBinaryRelation()
        CBinaryRelation(size_t, bool, size_t)
        CBinaryRelation(vector[bvector])

        void reset(bool)
        void resize(size_t, bool)
        size_t alloc()
        size_t split(size_t, bool)
        bool get(size_t, size_t)
        bool set(size_t, size_t, bool)
        size_t size()
        bool sym(size_t, size_t)
        void build_equivalence_classes(ivector&)
        void build_equivalence_classes(ivector&, ivector&)
        CBinaryRelation transposed(CBinaryRelation&)
        void build_index(IndexType&)
        void build_inv_index(IndexType&)
        void build_index(IndexType&, IndexType&)
        void restrict_to_symmetric()
        void get_quotient_projection(ivector&)


cdef extern from "mata/ord_vector.hh" namespace "Mata::Util":
    cdef cppclass COrdVector "Mata::Util::OrdVector" [T]:
        COrdVector() except+
        COrdVector(vector[T]) except+
        vector[T] ToVector()

        cppclass const_iterator:
            const T operator *()
            const_iterator operator++()
            bint operator ==(const_iterator)
            bint operator !=(const_iterator)
        const_iterator cbegin()
        const_iterator cend()

        cppclass iterator:
            T operator *()
            iterator operator++()
            bint operator ==(iterator)
            bint operator !=(iterator)
        iterator begin()
        iterator end()


cdef extern from "mata/nfa.hh" namespace "Mata::Nfa":
    # Typedefs
    ctypedef uintptr_t State
    ctypedef uintptr_t Symbol
    ctypedef COrdVector[Symbol] SymbolSet
    ctypedef COrdVector[State] StateSet
    ctypedef uset[State] UnorderedStateSet
    ctypedef umap[Symbol, StateSet] PostSymb
    ctypedef umap[State, PostSymb] StateToPostMap
    ctypedef umap[pair[State, State], State] ProductMap
    ctypedef umap[StateSet, State] SubsetMap
    ctypedef umap[string, State] StringSubsetMap
    ctypedef vector[State] Path
    ctypedef vector[Symbol] Word
    ctypedef cset[Word] WordSet
    ctypedef umap[string, State] StringToStateMap
    ctypedef umap[string, Symbol] StringToSymbolMap
    ctypedef umap[State, string] StateToStringMap
    ctypedef umap[State, State] StateToStateMap
    ctypedef umap[Symbol, string] SymbolToStringMap
    ctypedef umap[string, string] StringDict
    ctypedef COrdVector[CTransSymbolStates] TransitionList
    ctypedef vector[TransitionList] TransitionRelation
    ctypedef vector[CNfa] AutSequence
    ctypedef vector[CNfa*] AutPtrSequence
    ctypedef vector[const CNfa*] ConstAutPtrSequence

    cdef const Symbol CEPSILON "Mata::Nfa::EPSILON"

    cdef cppclass CTrans "Mata::Nfa::Trans":
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

    ctypedef vector[CTrans] TransitionSequence

    cdef cppclass CTransSymbolStates "Mata::Nfa::TransSymbolStates":
        # Public Attributes
        Symbol symbol
        StateSet states_to

        # Constructors
        CTransSymbolStates() except +
        CTransSymbolStates(Symbol) except +
        CTransSymbolStates(Symbol, State) except +
        CTransSymbolStates(Symbol, StateSet) except +

        bool operator<(CTransSymbolStates)
        bool operator<=(CTransSymbolStates)
        bool operator>(CTransSymbolStates)
        bool operator>=(CTransSymbolStates)

    cdef cppclass CNfa "Mata::Nfa::Nfa":
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
        TransitionRelation transitionrelation
        umap[string, void*] attributes
        CAlphabet* alphabet

        # Constructor
        CNfa() except +
        CNfa(unsigned long) except +
        CNfa(unsigned long, StateSet, StateSet, CAlphabet*)

        # Public Functions
        void make_initial(State)
        void make_initial(vector[State])
        bool has_initial(State)
        void remove_initial(State)
        void remove_initial(vector[State])
        void reset_initial(State)
        void reset_initial(vector[State])
        void clear_initial()
        void make_final(State)
        void make_final(vector[State])
        bool has_final(State)
        void remove_final(State)
        void remove_final(vector[State])
        void reset_final(State)
        void reset_final(vector[State])
        void clear_final()
        void unify_initial()
        void unify_final()
        SymbolSet get_symbols()
        void add_trans(CTrans) except +
        void add_trans(State, Symbol, State) except +
        void remove_trans(CTrans) except +
        void remove_trans(State, Symbol, State) except +
        bool has_trans(CTrans)
        bool has_trans(State, Symbol, State)
        bool trans_empty()
        bool nothing_in_trans()
        bool is_state(State)
        size_t get_num_of_trans()
        StateSet post(StateSet&, Symbol)
        CNfa.const_iterator begin()
        CNfa.const_iterator end()
        size_t get_num_of_states()
        void increase_size(size_t)
        void increase_size_for_state(State)
        State add_new_state()
        void print_to_DOT(ostream)
        WordSet get_shortest_words()
        TransitionList get_transitions_from(State)
        vector[CTrans] get_transitions_to(State)
        vector[CTrans] get_trans_as_sequence()
        vector[CTrans] get_trans_from_as_sequence(State)
        void trim()
        void get_digraph(CNfa&)
        bool is_epsilon(Symbol)
        StateSet get_useful_states()
        StateSet get_reachable_states()
        StateSet get_terminating_states()
        void remove_epsilon(Symbol) except +
        COrdVector[CTransSymbolStates].const_iterator get_epsilon_transitions(State state, Symbol epsilon)
        COrdVector[CTransSymbolStates].const_iterator get_epsilon_transitions(TransitionList& state_transitions, Symbol epsilon)


    # Automata tests
    cdef bool is_deterministic(CNfa&)
    cdef bool is_lang_empty(CNfa&)
    cdef bool is_lang_empty(CNfa&, Path*)
    cdef bool is_lang_empty_cex(CNfa&, Word*)
    cdef bool is_universal(CNfa&, CAlphabet&, StringDict&) except +
    cdef bool is_incl(CNfa&, CNfa&, CAlphabet*, StringDict&)
    cdef bool is_incl(CNfa&, CNfa&, Word*, CAlphabet*, StringDict&) except +
    cdef bool equivalence_check(CNfa&, CNfa&, CAlphabet*, StringDict&)
    cdef bool equivalence_check(CNfa&, CNfa&, StringDict&)
    cdef bool is_complete(CNfa&, CAlphabet&) except +
    cdef bool is_in_lang(CNfa&, Word&)
    cdef bool is_prfx_in_lang(CNfa&, Word&)

    # Automata operations
    cdef void determinize(CNfa*, CNfa&, SubsetMap*)
    cdef void uni(CNfa*, CNfa&, CNfa&)
    cdef void intersection(CNfa*, CNfa&, CNfa&, bool, ProductMap*)
    cdef void concatenate(CNfa*, CNfa&, CNfa&, bool, StateToStateMap*, StateToStateMap*)
    cdef void complement(CNfa*, CNfa&, CAlphabet&, StringDict&, SubsetMap*) except +
    cdef void make_complete(CNfa*, CAlphabet&, State) except +
    cdef void revert(CNfa*, CNfa&)
    cdef void remove_epsilon(CNfa*, CNfa&, Symbol) except +
    cdef void minimize(CNfa*, CNfa&)
    cdef void reduce(CNfa*, CNfa&, StateToStateMap*, StringDict&)
    cdef CBinaryRelation& compute_relation(CNfa&, StringDict&)

    # Helper functions
    cdef pair[Word, bool] get_word_for_path(CNfa&, Path&)
    cdef Word encode_word(StringToSymbolMap&, vector[string])

    # Alphabets
    cdef cppclass CAlphabet "Mata::Nfa::Alphabet":
        CAlphabet() except +

        Symbol translate_symb(string)
        string reverse_translate_symbol(Symbol)

    cdef cppclass CIntAlphabet "Mata::Nfa::IntAlphabet" (CAlphabet):
        SymbolSet get_alphabet_symbols()

    cdef cppclass COnTheFlyAlphabet "Mata::Nfa::OnTheFlyAlphabet" (CAlphabet):
        StringToSymbolMap symbol_map
        COnTheFlyAlphabet(StringToSymbolMap) except +
        COnTheFlyAlphabet(Symbol) except +
        COnTheFlyAlphabet(COnTheFlyAlphabet) except +
        COnTheFlyAlphabet(vector[string]) except +
        SymbolSet get_alphabet_symbols()
        StringToSymbolMap get_symbol_map()
        StringToSymbolMap add_symbols_from(StringToSymbolMap)
        StringToSymbolMap add_symbols_from(vector[string])

    cdef cppclass CSegmentation "Mata::Nfa::SegNfa::Segmentation":
        CSegmentation(CNfa&, Symbol) except +

        ctypedef unsigned EpsilonDepth
        ctypedef umap[EpsilonDepth, TransitionSequence] EpsilonDepthTransitions

        EpsilonDepthTransitions get_epsilon_depths()
        AutSequence get_segments()


cdef extern from "mata/noodlify.hh" namespace "Mata::Nfa::SegNfa":
    ctypedef vector[vector[shared_ptr[CNfa]]] NoodleSequence

    cdef NoodleSequence noodlify(CNfa&, Symbol, bool)
    cdef NoodleSequence noodlify_for_equation(const AutPtrSequence&, CNfa&, bool, StringDict&)


cdef extern from "mata/re2parser.hh" namespace "Mata::RE2Parser":
    cdef void create_nfa(CNfa*, string) except +
    cdef void create_nfa(CNfa*, string, bool) except +


cdef extern from "mata/nfa.hh" namespace "Mata::Nfa":
    cdef void compute_fw_direct_simulation(const CNfa&)
