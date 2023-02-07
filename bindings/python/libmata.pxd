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

cdef extern from "mata/simlib/util/binary_relation.hh" namespace "Simlib::Util":
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


cdef extern from "mata/number-predicate.hh" namespace "Mata::Util":
    cdef cppclass CNumberPredicate "Mata::Util::NumberPredicate" [T]:
        vector[bool] predicate
        vector[T] elements
        bool elements_are_exact
        bool tracking_elements

        CNumberPredicate(bool)
        CNumberPredicate(vector[T], bool)

        void prune_elements()
        void update_elements()
        void compute_elements()

        void add(T)
        void remove(T)
        void track_elements()
        void dont_track_elements()
        bool operator[](T)
        T size()
        void clear()
        void reserve(T)
        void flip(T)
        void complement(T)
        vector[T] get_elements()
        bool are_disjoint(CNumberPredicate[T])
        bool empty()
        T domain_size()
        void truncate_domain()

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

cdef extern from "mata/ord-vector.hh" namespace "Mata::Util":
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

cdef extern from "mata/alphabet.hh" namespace "Mata":
    ctypedef uintptr_t Symbol
    ctypedef umap[string, Symbol] StringToSymbolMap

    cdef cppclass CAlphabet "Mata::Alphabet":
        CAlphabet() except +

        Symbol translate_symb(string)
        string reverse_translate_symbol(Symbol)

    cdef cppclass CIntAlphabet "Mata::IntAlphabet" (CAlphabet):
        COrdVector[Symbol] get_alphabet_symbols()

    cdef cppclass COnTheFlyAlphabet "Mata::OnTheFlyAlphabet" (CAlphabet):
        StringToSymbolMap symbol_map
        COnTheFlyAlphabet(StringToSymbolMap) except +
        COnTheFlyAlphabet(Symbol) except +
        COnTheFlyAlphabet(COnTheFlyAlphabet) except +
        COnTheFlyAlphabet(vector[string]) except +
        COrdVector[Symbol] get_alphabet_symbols()
        StringToSymbolMap get_symbol_map()
        StringToSymbolMap add_symbols_from(StringToSymbolMap)
        StringToSymbolMap add_symbols_from(vector[string])

cdef extern from "mata/nfa.hh" namespace "Mata::Nfa":
    # Typedefs
    ctypedef uintptr_t State
    ctypedef COrdVector[State] StateSet
    ctypedef uset[State] UnorderedStateSet
    ctypedef umap[Symbol, StateSet] PostSymb
    ctypedef umap[State, PostSymb] StateToPostMap
    ctypedef umap[string, State] StringSubsetMap
    ctypedef umap[string, State] StringToStateMap
    ctypedef umap[State, string] StateToStringMap
    ctypedef umap[State, State] StateToStateMap
    ctypedef umap[Symbol, string] SymbolToStringMap
    ctypedef umap[string, string] StringMap
    ctypedef vector[CNfa] AutSequence
    ctypedef vector[CNfa*] AutPtrSequence
    ctypedef vector[const CNfa*] ConstAutPtrSequence

    cdef const Symbol CEPSILON "Mata::Nfa::EPSILON"

    cdef cppclass CPost "Mata::Nfa::Post":
        void insert(CMove&)
        CMove& operator[](Symbol)
        CMove& back()
        void push_back(CMove&)
        void remove(CMove&)
        bool empty()
        size_t size()
        vector[CMove] ToVector()
        COrdVector[CMove].const_iterator cbegin()
        COrdVector[CMove].const_iterator cend()

    cdef cppclass CDelta "Mata::Nfa::Delta":
        vector[CPost] post

        void reserve(size_t)
        CPost& operator[](State)
        void emplace_back()
        void clear()
        bool empty()
        void resize(size_t)
        size_t post_size()
        void defragment()
        void add(CTrans) except +
        void add(State, Symbol, State) except +
        void remove(CTrans) except +
        void remove(State, Symbol, State) except +
        bool contains(State, Symbol, State)

    cdef cppclass CRun "Mata::Nfa::Run":
        # Public Attributes
        vector[Symbol] word
        vector[State] path

        # Constructor
        CRun() except +

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

    cdef cppclass CMove "Mata::Nfa::Move":
        # Public Attributes
        Symbol symbol
        StateSet targets

        # Constructors
        CMove() except +
        CMove(Symbol) except +
        CMove(Symbol, State) except +
        CMove(Symbol, StateSet) except +

        bool operator<(CMove)
        bool operator<=(CMove)
        bool operator>(CMove)
        bool operator>=(CMove)

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
        CNumberPredicate[State] initial
        CNumberPredicate[State] final
        CDelta delta
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
        void clear_initial()
        void make_final(State)
        void make_final(vector[State])
        bool has_final(State)
        void remove_final(State)
        void clear_final()
        void unify_initial()
        void unify_final()
        COrdVector[Symbol] get_used_symbols()
        bool is_state(State)
        size_t get_num_of_trans()
        StateSet post(StateSet&, Symbol)
        CNfa.const_iterator begin()
        CNfa.const_iterator end()
        State add_state()
        State add_state(State)
        void print_to_DOT(ostream)
        CPost get_moves_from(State)
        vector[CTrans] get_transitions_to(State)
        vector[CTrans] get_trans_as_sequence()
        vector[CTrans] get_trans_from_as_sequence(State)
        void trim(StateToStateMap*)
        void get_one_letter_aut(CNfa&)
        bool is_epsilon(Symbol)
        StateSet get_useful_states()
        StateSet get_reachable_states()
        StateSet get_terminating_states()
        void remove_epsilon(Symbol) except +
        COrdVector[CMove].const_iterator get_epsilon_transitions(State state, Symbol epsilon)
        COrdVector[CMove].const_iterator get_epsilon_transitions(CPost& state_transitions, Symbol epsilon)
        void clear()
        void defragment()
        size_t size()

    # Automata tests
    cdef bool is_deterministic(CNfa&)
    cdef bool is_lang_empty(CNfa&, CRun*)
    cdef bool is_universal(CNfa&, CAlphabet&, StringMap&) except +
    cdef bool is_included(CNfa&, CNfa&, CAlphabet*, StringMap&)
    cdef bool is_included(CNfa&, CNfa&, CRun*, CAlphabet*, StringMap&) except +
    cdef bool are_equivalent(CNfa&, CNfa&, CAlphabet*, StringMap&)
    cdef bool are_equivalent(CNfa&, CNfa&, StringMap&)
    cdef bool is_complete(CNfa&, CAlphabet&) except +
    cdef bool is_in_lang(CNfa&, CRun&)
    cdef bool is_prfx_in_lang(CNfa&, CRun&)

    # Automata operations
    cdef void compute_fw_direct_simulation(const CNfa&)

    # Helper functions
    cdef pair[CRun, bool] get_word_for_path(CNfa&, CRun&)
    cdef CRun encode_word(StringToSymbolMap&, vector[string])

cdef extern from "mata/nfa-algorithms.hh" namespace "Mata::Nfa::Algorithms":
    cdef CBinaryRelation& compute_relation(CNfa&, StringMap&)

cdef extern from "mata/nfa-plumbing.hh" namespace "Mata::Nfa::Plumbing":
    cdef void determinize(CNfa*, CNfa&, umap[StateSet, State]*)
    cdef void uni(CNfa*, CNfa&, CNfa&)
    cdef void intersection(CNfa*, CNfa&, CNfa&, bool, umap[pair[State, State], State]*)
    cdef void concatenate(CNfa*, CNfa&, CNfa&, bool, StateToStateMap*, StateToStateMap*)
    cdef void complement(CNfa*, CNfa&, CAlphabet&, StringMap&, umap[StateSet, State]*) except +
    cdef void make_complete(CNfa*, CAlphabet&, State) except +
    cdef void revert(CNfa*, CNfa&)
    cdef void remove_epsilon(CNfa*, CNfa&, Symbol) except +
    cdef void minimize(CNfa*, CNfa&)
    cdef void reduce(CNfa*, CNfa&, bool, StateToStateMap*, StringMap&)


cdef extern from "mata/nfa-strings.hh" namespace "Mata::Strings":
    cdef cset[vector[Symbol]] get_shortest_words(CNfa&)


cdef extern from "mata/nfa-strings.hh" namespace "Mata::Strings::SegNfa":
    cdef cppclass CSegmentation "Mata::Strings::SegNfa::Segmentation":
        CSegmentation(CNfa&, cset[Symbol]) except +

        ctypedef unsigned EpsilonDepth
        ctypedef umap[EpsilonDepth, TransitionSequence] EpsilonDepthTransitions

        EpsilonDepthTransitions get_epsilon_depths()
        AutSequence get_segments()

    ctypedef vector[vector[shared_ptr[CNfa]]] NoodleSequence

    cdef NoodleSequence noodlify(CNfa&, Symbol, bool)
    cdef NoodleSequence noodlify_for_equation(const AutPtrSequence&, CNfa&, bool, StringMap&)


cdef extern from "mata/re2parser.hh" namespace "Mata::RE2Parser":
    cdef void create_nfa(CNfa*, string) except +
    cdef void create_nfa(CNfa*, string, bool) except +
