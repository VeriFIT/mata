cimport mata
from libcpp.vector cimport vector
from libcpp.list cimport list as clist
from libcpp.set cimport set as cset
from libcpp.utility cimport pair
from libcpp.memory cimport shared_ptr, make_shared
from cython.operator import dereference, postincrement as postinc, preincrement as preinc
from libcpp.unordered_map cimport unordered_map as umap

import shlex
import subprocess
import tabulate

cdef Symbol EPSILON = CEPSILON

def epsilon():
    return EPSILON

cdef class Trans:
    """
    Wrapper over the transitions in NFA
    """
    cdef mata.CTrans *thisptr

    @property
    def src(self):
        """
        :return: source state of the transition
        """
        return self.thisptr.src

    @property
    def symb(self):
        """
        :return: symbol for the transition
        """
        return self.thisptr.symb

    @property
    def tgt(self):
        """
        :return: target state of the transition
        """
        return self.thisptr.tgt

    def __cinit__(self, State src=0, Symbol s=0, State tgt=0):
        """Constructor of the transition

        :param State src: source state
        :param Symbol s: symbol
        :param State tgt: target state
        """
        self.thisptr = new mata.CTrans(src, s, tgt)

    def __dealloc__(self):
        """
        Destructor
        """
        if self.thisptr != NULL:
            del self.thisptr

    cdef copy_from(self, CTrans trans):
        """Copies the internals of trans into the wrapped pointer

        :param CTrans trans: copied transition
        """
        self.thisptr.src = trans.src
        self.thisptr.symb = trans.symb
        self.thisptr.tgt = trans.tgt

    def __eq__(self, Trans other):
        return dereference(self.thisptr) == dereference(other.thisptr)

    def __neq__(self, Trans other):
        return dereference(self.thisptr) != dereference(other.thisptr)

    def __str__(self):
        return f"{self.thisptr.src}-[{self.thisptr.symb}]\u2192{self.thisptr.tgt}"

    def __repr__(self):
        return str(self)


cdef class Move:
    """
    Wrapper over pair of symbol and states for transitions
    """
    cdef mata.CTransSymbolStates *thisptr

    @property
    def symbol(self):
        return self.thisptr.symbol

    @symbol.setter
    def symbol(self, value):
        self.thisptr.symbol = value

    @property
    def states_to(self):
        cdef vector[State] states_as_vector = self.thisptr.states_to.ToVector()
        return [s for s in states_as_vector]

    @states_to.setter
    def states_to(self, value):
        cdef StateSet states_to = StateSet(value)
        self.thisptr.states_to = states_to

    def __cinit__(self, Symbol symbol, vector[State] states):
        cdef StateSet states_to = StateSet(states)
        self.thisptr = new mata.CTransSymbolStates(symbol, states_to)

    def __dealloc__(self):
        if self.thisptr != NULL:
            del self.thisptr

    def __lt__(self, Move other):
        return dereference(self.thisptr) < dereference(other.thisptr)

    def __gt__(self, Move other):
        return dereference(self.thisptr) > dereference(other.thisptr)

    def __le__(self, Move other):
        return dereference(self.thisptr) <= dereference(other.thisptr)

    def __ge__(self, Move other):
        return dereference(self.thisptr) >= dereference(other.thisptr)

    def __eq__(self, Move other):
        return self.symbol == other.symbol and self.states_to == other.states_to

    def __neq__(self, Move other):
        return self.symbol != other.symbol or self.states_to != other.states_to

    def __str__(self):
        trans = "{" + ",".join(map(str, self.states_to)) + "}"
        return f"[{self.symbol}]\u2192{trans}"

    def __repr__(self):
        return str(self)


cdef class Nfa:
    """
    Wrapper over NFA
    """

    # TODO: Shared pointers are not ideal as they bring some overhead which could be substantial in theory. We are not
    #  sure whether the shared pointers will be a problem in this case, but it would be good to pay attention to this and
    #  potentially create some kind of Factory/Allocator/Pool class, that would take care of management of the pointers
    #  to optimize the shared pointers away if we find that the overhead is becoming too significant to ignore.
    cdef shared_ptr[mata.CNfa] thisptr
    cdef alphabet

    def __cinit__(self, state_number = 0, alphabet = None):
        """Constructor of the NFA

        :param int state_number: number of states in automaton
        :param Alphabet alphabet: alphabet corresponding to the automaton
        """
        self.thisptr = make_shared[CNfa](mata.CNfa(state_number))
        self.alphabet = alphabet

    @property
    def initial_states(self):
        cdef vector[State] initial_states = self.thisptr.get().initialstates.ToVector()
        return [initial_state for initial_state in initial_states]

    @initial_states.setter
    def initial_states(self, value):
        cdef StateSet initial_states = StateSet(value)
        self.thisptr.get().initialstates = initial_states

    @property
    def final_states(self):
        cdef vector[State] final_states = self.thisptr.get().finalstates.ToVector()
        return [final_state for final_state in final_states]

    @final_states.setter
    def final_states(self, value):
        cdef StateSet final_states = StateSet(value)
        self.thisptr.get().finalstates = final_states

    def is_state(self, state):
        """Tests if state is in the automaton

        :param int state: tested state
        :return: true if state is in the automaton
        """
        return self.thisptr.get().is_state(state)

    def add_new_state(self):
        """Adds new state to automaton

        :return: number of the state
        """
        return self.thisptr.get().add_new_state()

    def make_initial_state(self, State state):
        """
        Makes specified state from the automaton initial.

        :param State state: State to be made initial.
        """
        self.thisptr.get().make_initial(state)

    def make_initial_states(self, vector[State] states):
        """
        Makes specified states from the automaton initial.

        :param list states: List of states to be made initial.
        """
        self.thisptr.get().make_initial(states)

    def has_initial_state(self, State st):
        """Tests if automaton contains given state

        :param State st: tested state
        :return: true if automaton contains given state
        """
        return self.thisptr.get().has_initial(st)

    def remove_initial_state(self, State state):
        """
        Removes state from initial state set of the automaton.

        :param State state: State to be removed from initial states.
        """
        self.thisptr.get().remove_initial(state)

    def remove_initial_states(self, vector[State] states):
        """
        Removes states from initial state set of the automaton.

        :param list State states: States to be removed from initial states.
        """
        self.thisptr.get().remove_initial(states)

    def reset_initial_state(self, State state):
        """
        Resets initial state set of the automaton to the specified state.

        :param State state: State to be made initial.
        """
        self.thisptr.get().reset_initial(state)

    def reset_initial_states(self, vector[State] states):
        """
        Resets initial state set of the automaton to the specified states.

        :param list states: List of states to be made initial.
        """
        self.thisptr.get().reset_initial(states)

    def clear_initial(self):
        """Clears initial state set of the automaton."""
        self.thisptr.get().clear_initial()

    def make_final_state(self, State state):
        """
        Makes specified state from the automaton final.

        :param State state: State to be made final.
        """
        self.thisptr.get().make_final(state)

    def make_final_states(self, vector[State] states):
        """
        Makes specified states from the automaton final.

        :param vector[State] states: List of states to be made final.
        """
        self.thisptr.get().make_final(states)

    def has_final_state(self, State st):
        """Tests if automaton contains given state

        :param State st: tested state
        :return: true if automaton contains given state
        """
        return self.thisptr.get().has_final(st)

    def remove_final_state(self, State state):
        """
        Removes state from final state set of the automaton.

        :param State state: State to be removed from final states.
        """
        self.thisptr.get().remove_final(state)

    def remove_final_states(self, vector[State] states):
        """
        Removes states from final state set of the automaton.

        :param list State states: States to be removed from final states.
        """
        self.thisptr.get().remove_final(states)

    def reset_final_state(self, State state):
        """
        Resets final state set of the automaton to the specified state.

        :param State state: State to be made final.
        """
        self.thisptr.get().reset_final(state)

    def reset_final_states(self, vector[State] states):
        """
        Resets final state set of the automaton to the specified states.

        :param list states: List of states to be made final.
        """
        self.thisptr.get().reset_final(states)

    def clear_final(self):
        """Clears final state set of the automaton."""
        self.thisptr.get().clear_final()

    def unify_initial(self):
        """Unify initial states into a single new initial state."""
        self.thisptr.get().unify_initial()

    def unify_final(self):
        """Unify final states into a single new final state."""
        self.thisptr.get().unify_final()

    def get_num_of_states(self):
        """Returns number of states in automaton

        :return: number of states in automaton
        """
        return self.thisptr.get().get_num_of_states()

    def add_trans(self, Trans tr):
        """Adds transition to automaton

        :param Trans tr: added transition
        """
        self.thisptr.get().add_trans(dereference(tr.thisptr))

    def add_trans_raw(self, State src, Symbol symb, State tgt):
        """Constructs transition and adds it to automaton

        :param State src: source state
        :param Symbol symb: symbol
        :param State tgt: target state
        """
        self.thisptr.get().add_trans(src, symb, tgt)

    def remove_trans(self, Trans tr):
        """
        Removes transition from the automaton.

        :param Trans tr: Transition to be removed.
        """
        self.thisptr.get().remove_trans(dereference(tr.thisptr))

    def remove_trans_raw(self, State src, Symbol symb, State tgt):
        """
        Constructs transition and removes it from the automaton.

        :param State src: Source state of the transition to be removed.
        :param Symbol symb: Symbol of the transition to be removed.
        :param State tgt: Target state of the transition to be removed.
        """
        self.thisptr.get().remove_trans(src, symb, tgt)

    def has_trans(self, Trans tr):
        """Tests if automaton contains transition

        :param Trans tr: tested transition
        :return: true if automaton contains transition
        """
        return self.thisptr.get().has_trans(dereference(tr.thisptr))

    def has_trans_raw(self, State src, Symbol symb, State tgt):
        """Tests if automaton contains transition

        :param State src: source state
        :param Symbol symb: symbol
        :param State tgt: target state
        :return: true if automaton contains transition
        """
        return self.thisptr.get().has_trans(src, symb, tgt)

    def trans_empty(self):
        """Tests if there are no transitions

        :return: true if there are no transitions in automaton
        """
        return self.thisptr.get().nothing_in_trans()

    def get_num_of_trans(self):
        """Returns number of transitions in automaton

        :return: number of transitions in automaton
        """
        return self.thisptr.get().get_num_of_trans()

    def resize(self, size):
        """Increases the size of the automaton to size

        :param int size: new size of the
        """
        self.thisptr.get().increase_size(size)

    def resize_for_state(self, State state):
        """
        Increases the size of the automaton to include state.

        :param State state: State to resize for.
        """
        self.thisptr.get().increase_size_for_state(state)

    def iterate(self):
        """Iterates over all transitions

        :return: stream of transitions
        """
        iterator = self.thisptr.get().begin()
        while iterator != self.thisptr.get().end():
            trans = Trans()
            lhs = dereference(iterator)
            trans.copy_from(lhs)
            preinc(iterator)
            yield trans

    def get_transitions_from_state(self, State state):
        """Returns list of Move for the given state

        :param State state: state for which we are getting the transitions
        :return: Move
        """
        cdef MoveList transitions = self.thisptr.get().get_transitions_from(state)
        cdef vector[mata.CTransSymbolStates] transitions_list = transitions.ToVector()

        cdef vector[mata.CTransSymbolStates].iterator it = transitions_list.begin()
        cdef vector[mata.CTransSymbolStates].iterator end = transitions_list.end()
        transsymbols = []
        while it != end:
            t = Move(
                dereference(it).symbol,
                dereference(it).states_to.ToVector()
            )
            postinc(it)
            transsymbols.append(t)
        return transsymbols

    def get_transitions_to_state(self, State state_to):
        """
        Get transitions leading to state_to (state_to is their target state).

        :return: List mata.CTrans: List of transitions leading to state_to.
        """
        cdef vector[CTrans] c_transitions = self.thisptr.get().get_transitions_to(state_to)
        trans = []
        for c_transition in c_transitions:
            trans.append(Trans(c_transition.src, c_transition.symb, c_transition.tgt))
        return trans

    def get_trans_as_sequence(self):
        """
        Get automaton transitions as a sequence.
        :return: List of automaton transitions.
        """
        cdef vector[CTrans] c_transitions = self.thisptr.get().get_trans_as_sequence()
        transitions = []
        for c_transition in c_transitions:
            transitions.append(Trans(c_transition.src, c_transition.symb, c_transition.tgt))
        return transitions

    def get_trans_from_state_as_sequence(self, State state_from):
        """
        Get automaton transitions from state_from as a sequence.
        :return: List of automaton transitions.
        """
        cdef vector[CTrans] c_transitions = self.thisptr.get().get_trans_from_as_sequence(state_from)
        transitions = []
        for c_transition in c_transitions:
            transitions.append(Trans(c_transition.src, c_transition.symb, c_transition.tgt))
        return transitions

    def get_useful_states(self):
        """
        Get useful states (states which are reachable and terminating at the same time).
        :return: A set of useful states.
        """
        cdef vector[State] return_value = self.thisptr.get().get_useful_states().ToVector()
        return {state for state in return_value}

    def get_reachable_states(self):
        """
        Get reachable states.
        :return: A set of reachable states.
        """
        cdef vector[State] return_value = self.thisptr.get().get_reachable_states().ToVector()
        return {state for state in return_value}

    def get_terminating_states(self):
        """
        Get terminating states (states with a path from them leading to any final state).
        :return: A set of terminating states.
        """
        cdef vector[State] return_value = self.thisptr.get().get_terminating_states().ToVector()
        return {state for state in return_value}

    def trim(self):
        """
        Remove inaccessible (unreachable) and not co-accessible (non-terminating) states.

        Remove states which are not accessible (unreachable; state is accessible when the state is the endpoint of a path
         starting from an initial state) or not co-accessible (non-terminating; state is co-accessible when the state is
         the starting point of a path ending in a final state).
        """
        self.thisptr.get().trim()

    def get_digraph(self) -> Nfa:
        """
        Unify transitions to create a directed graph with at most a single transition between two states.

        :return: mata.Nfa: An automaton representing a directed graph.
        """
        cdef Nfa digraph = mata.Nfa()
        self.thisptr.get().get_digraph(dereference(digraph.thisptr.get()))
        return digraph

    def is_epsilon(self, Symbol symbol) -> bool:
        """
        Check whether passed symbol is epsilon symbol or not.
        :param Symbol symbol: The symbol to check.
        :return: True if the passed symbol is epsilon symbol, False otherwise.
        """
        return self.thisptr.get().is_epsilon(symbol)

    def __str__(self):
        """String representation of the automaton displays states, and transitions

        :return: string representation of the automaton
        """
        cdef vector[State] initial_states = self.thisptr.get().initialstates.ToVector()
        cdef vector[State] final_states = self.thisptr.get().finalstates.ToVector()
        result = "initial_states: {}\n".format([s for s in initial_states])
        result += "final_states: {}\n".format([s for s in final_states])
        result += "transitions:\n"
        for trans in self.iterate():
            symbol = trans.symb if self.alphabet is None \
                else self.alphabet.reverse_translate_symbol(trans.symb)
            result += f"{trans.src}-[{symbol}]\u2192{trans.tgt}\n"
        return result

    def to_dot_file(self, output_file='aut.dot', output_format='pdf'):
        """Transforms the automaton to dot format.

        By default, the result is saved to `aut.dot`, and further to `aut.dot.pdf`.

        One can choose other format that is supported by graphviz format (e.g. pdf or png).

        :param str output_file: name of the output file where the automaton will be stored
        :param str output_format: format of the output file (pdf/png/etc)
        """
        cdef mata.ofstream* output
        output = new mata.ofstream(output_file.encode('utf-8'))
        try:
            self.thisptr.get().print_to_DOT(dereference(output))
        finally:
            del output

        graphviz_command = f"dot -O -T{output_format} {output_file}"
        _, err = run_safely_external_command(graphviz_command)
        if err:
            print(f"error while dot file: {err}")

    def to_dot_str(self, encoding='utf-8'):
        """Transforms the automaton to dot format string

        :param str encoding: encoding of the dot string
        :return: string with dot representation of the automaton
        """
        cdef mata.stringstream* output_stream
        output_stream = new mata.stringstream("")
        cdef string result
        try:
            self.thisptr.get().print_to_DOT(dereference(output_stream))
            result = output_stream.str()
        finally:
            del output_stream
        return result.decode(encoding)

    def post_map_of(self, State st, Alphabet alphabet):
        """Returns mapping of symbols to set of states.

        :param State st: source state
        :param Alphabet alphabet: alphabet of the post
        :return: dictionary mapping symbols to set of reachable states from the symbol
        """
        symbol_map = {}
        for symbol in alphabet.get_symbols():
            symbol_map[symbol] = self.post_of({st}, symbol)
        return symbol_map

    def post_of(self, vector[State] states, Symbol symbol):
        """Returns sets of reachable states from set of states through a symbol

        :param StateSet states: set of states
        :param Symbol symbol: source symbol
        :return: set of reachable states
        """
        cdef vector[State] return_value
        cdef StateSet input_states = StateSet(states)
        return_value = self.thisptr.get().post(input_states, symbol).ToVector()
        return {v for v in return_value}

    def get_shortest_words(self):
        """Returns set of shortest words accepted by automaton

        :return: set of shortest words accepted by automaton
        """
        cdef WordSet shortest
        shortest = self.thisptr.get().get_shortest_words()
        result = []
        cdef cset[vector[Symbol]].iterator it = shortest.begin()
        cdef cset[vector[Symbol]].iterator end = shortest.end()
        while it != end:
            short = dereference(it)
            result.append(short)
            postinc(it)
        return result

    # External Constructors
    @classmethod
    def from_regex(cls, regex, encoding='utf-8'):
        """Creates automaton from the regular expression

        The format of the regex conforms to google RE2 regular expression library.

        :param str regex: regular expression
        :param str encoding: encoding of the string
        :return: Nfa automaton
        """
        result = Nfa()
        mata.create_nfa(result.thisptr.get(), regex.encode(encoding))
        return result

    # Operations
    @classmethod
    def determinize_with_subset_map(cls, Nfa lhs):
        """Determinize the lhs automaton

        :param Nfa lhs: non-deterministic finite automaton
        :return: deterministic finite automaton, subset map
        """
        result = Nfa()
        cdef SubsetMap subset_map
        mata.determinize(result.thisptr.get(), dereference(lhs.thisptr.get()), &subset_map)
        return result, subset_map_to_dictionary(subset_map)

    @classmethod
    def determinize(cls, Nfa lhs):
        """Determinize the lhs automaton

        :param Nfa lhs: non-deterministic finite automaton
        :return: deterministic finite automaton
        """
        result = Nfa()
        mata.determinize(result.thisptr.get(), dereference(lhs.thisptr.get()), NULL)
        return result

    @classmethod
    def union(cls, Nfa lhs, Nfa rhs):
        """Performs union of lhs and rhs

        :param Nfa lhs: first automaton
        :param Nfa rhs: second automaton
        :return: union of lhs and rhs
        """
        result = Nfa()
        mata.uni(
            result.thisptr.get(), dereference(lhs.thisptr.get()), dereference(rhs.thisptr.get())
        )
        return result

    @classmethod
    def intersection(cls, Nfa lhs, Nfa rhs):
        """Performs intersection of lhs and rhs.

        :param Nfa lhs: First automaton.
        :param Nfa rhs: Second automaton.
        :return: Intersection of lhs and rhs.
        """
        result = Nfa()
        mata.intersection(result.thisptr.get(), dereference(lhs.thisptr.get()), dereference(rhs.thisptr.get()))
        return result

    @classmethod
    def intersection_preserving_epsilon_transitions(cls, Nfa lhs, Nfa rhs, Symbol epsilon = CEPSILON):
        """
        Performs intersection of lhs and rhs preserving epsilon transitions.

        Create product of two NFAs, where both automata can contain ε-transitions. The product preserves the ε-transitions
         of both automata. This means that for each ε-transition of the form `s-ε->p` and each product state `(s,a)`,
         an ε-transition `(s,a)-ε->(p,a)` is created. Furthermore, for each ε-transition `s-ε->p` and `a-ε->b`,
         a product state `(s,a)-ε->(p,b)` is created.

        Automata must share alphabets.

        :param: Nfa lhs: First automaton with possible epsilon symbols.
        :param: Nfa rhs: Second automaton with possible epsilon symbols.
        :param: Symbol epsilon: Symbol to handle as an epsilon symbol.
        :return: Intersection of lhs and rhs with epsilon transitions preserved, product map of original pairs of states
         to new states.
        """
        result = Nfa()
        mata.intersection_preserving_epsilon_transitions(
            result.thisptr.get(), dereference(lhs.thisptr.get()), dereference(rhs.thisptr.get()), epsilon
        )
        return result

    @classmethod
    def intersection_with_product_map(cls, Nfa lhs, Nfa rhs):
        """Performs intersection of lhs and rhs.

        :param Nfa lhs: First automaton.
        :param Nfa rhs: Second automaton.
        :return: Intersection of lhs and rhs, product map of original pairs of states to new states.
        """
        result = Nfa()
        cdef ProductMap c_product_map
        mata.intersection(
            result.thisptr.get(), dereference(lhs.thisptr.get()), dereference(rhs.thisptr.get()), &c_product_map
        )
        return result, {tuple(k): v for k, v in c_product_map}

    @classmethod
    def intersection_pres_eps_trans_with_prod_map(cls, Nfa lhs, Nfa rhs, Symbol epsilon = CEPSILON):
        """
        Performs intersection of lhs and rhs preserving epsilon transitions.

        Create product of two NFAs, where both automata can contain ε-transitions. The product preserves the ε-transitions
         of both automata. This means that for each ε-transition of the form `s-ε->p` and each product state `(s,a)`,
         an ε-transition `(s,a)-ε->(p,a)` is created. Furthermore, for each ε-transition `s-ε->p` and `a-ε->b`,
         a product state `(s,a)-ε->(p,b)` is created.

        Automata must share alphabets.

        :param: Nfa lhs: First automaton with possible epsilon symbols.
        :param: Nfa rhs: Second automaton with possible epsilon symbols.
        :param: Symbol epsilon: Symbol to handle as an epsilon symbol.
        :return: Intersection of lhs and rhs with epsilon transitions preserved, product map of original pairs of states
         to new states.
        """
        result = Nfa()
        cdef ProductMap c_product_map
        mata.intersection_preserving_epsilon_transitions(
            result.thisptr.get(), dereference(lhs.thisptr.get()), dereference(rhs.thisptr.get()), epsilon, &c_product_map
        )
        return result, {tuple(k): v for k, v in c_product_map}

    @classmethod
    def concatenate(cls, Nfa lhs, Nfa rhs):
        """
        Concatenate two NFAs.

        :param Nfa lhs: First automaton to concatenate.
        :param Nfa rhs: Second automaton to concatenate.
        :return: Nfa: Concatenated automaton.
        """
        result = Nfa()
        mata.concatenate(result.thisptr.get(), dereference(lhs.thisptr.get()), dereference(rhs.thisptr.get()))
        return result

    @classmethod
    def concatenate_over_epsilon(cls, Nfa lhs, Nfa rhs, epsilon = CEPSILON):
        """
        Concatenate two NFAs over an epsilon symbol.

        :param Nfa lhs: First automaton to concatenate over epsilon.
        :param Nfa rhs: Second automaton to concatenate over epsilon.
        :param Symbol epsilon: Symbol to concatenate over (Default: Default Mata epsilon value).
        :return: Nfa: Concatenated automaton over epsilon.
        """
        result = Nfa()
        mata.concatenate_over_epsilon(result.thisptr.get(), dereference(lhs.thisptr.get()), dereference(rhs.thisptr.get()), epsilon)
        return result

    @classmethod
    def noodlify(cls, Nfa aut, Symbol symbol, include_empty = False):
        """
        Create noodles from segment automaton.

        Segment automaton is a chain of finite automata (segments) connected via ε-transitions.
        A noodle is a copy of the segment automaton with exactly one ε-transition between each two consecutive segments.

        :param: Nfa aut: Segment automaton to noodlify.
        :param: Symbol epsilon: Epsilon symbol to noodlify for.
        :param: bool include_empty: Whether to also include empty noodles.
        :return: List of automata: A list of all (non-empty) noodles.
        """
        noodle_segments = []
        cdef NoodleSequence c_noodle_segments = mata.noodlify(dereference(aut.thisptr.get()), symbol, include_empty)
        for c_noodle in c_noodle_segments:
            noodle = []
            for c_noodle_segment in c_noodle:
                noodle_segment = Nfa()
                noodle_segment.thisptr = c_noodle_segment
                noodle.append(noodle_segment)

            noodle_segments.append(noodle)

        return noodle_segments

    @classmethod
    def noodlify_for_equation(cls, left_side_automata: list[Nfa], Nfa right_side_automaton, include_empty = False, params = None):
        """
        Create noodles for equation.

        Segment automaton is a chain of finite automata (segments) connected via ε-transitions.
        A noodle is a copy of the segment automaton with exactly one ε-transition between each two consecutive segments.

        Mata cannot work with equations, queries etc. Hence, we compute the noodles for the equation, but represent
         the equation in a way that libMata understands. The left side automata represent the left side of the equation
         and the right automaton represents the right side of the equation. To create noodles, we need a segment automaton
         representing the intersection. That can be achieved by computing a product of both sides. First, the left side
         has to be concatenated over an epsilon transition into a single automaton to compute the intersection on, though.

        :param: list[Nfa] aut: Segment automata representing the left side of the equation to noodlify.
        :param: Nfa aut: Segment automaton representing the right side of the equation to noodlify.
        :param: bool include_empty: Whether to also include empty noodles.
        :param: dict params: Additional parameters for the noodlification:
            - "reduce": "false", "forward", "backward", "bidirectional"; Execute forward, backward or bidirectional simulation
                        minimization before noodlification.
        :return: List of automata: A list of all (non-empty) noodles.
        """
        cdef AutPtrSequence c_left_side_automata
        for lhs_aut in left_side_automata:
            c_left_side_automata.push_back((<Nfa>lhs_aut).thisptr.get())
        noodle_segments = []
        params = params or {}
        cdef NoodleSequence c_noodle_segments = mata.noodlify_for_equation(
            c_left_side_automata, dereference(right_side_automaton.thisptr.get()), include_empty,
            {
                k.encode('utf-8'): v.encode('utf-8') if isinstance(v, str) else v
                for k, v in params.items()
            },
        )
        for c_noodle in c_noodle_segments:
            noodle = []
            for c_noodle_segment in c_noodle:
                noodle_segment = Nfa()
                noodle_segment.thisptr = c_noodle_segment
                noodle.append(noodle_segment)

            noodle_segments.append(noodle)

        return noodle_segments

    @classmethod
    def complement_with_subset_map(cls, Nfa lhs, Alphabet alphabet, params = None):
        """Performs complement of lhs

        :param Nfa lhs: complemented automaton
        :param OnTheFlyAlphabet alphabet: alphabet of the lhs
        :param dict params: additional params
        :return: complemented automaton, map of subsets to states
        """
        result = Nfa()
        params = params or {'algo': 'classical'}
        cdef SubsetMap subset_map
        mata.complement(
            result.thisptr.get(),
            dereference(lhs.thisptr.get()),
            <CAlphabet&>dereference(alphabet.as_base()),
            {
                k.encode('utf-8'): v.encode('utf-8') if isinstance(v, str) else v
                for k, v in params.items()
            },
            &subset_map
        )
        return result, subset_map_to_dictionary(subset_map)

    @classmethod
    def complement(cls, Nfa lhs, Alphabet alphabet, params = None):
        """Performs complement of lhs

        :param Nfa lhs: complemented automaton
        :param OnTheFlyAlphabet alphabet: alphabet of the lhs
        :param dict params: additional params
        :return: complemented automaton
        """
        result = Nfa()
        params = params or {'algo': 'classical'}
        mata.complement(
            result.thisptr.get(),
            dereference(lhs.thisptr.get()),
            <CAlphabet&>dereference(alphabet.as_base()),
            {
                k.encode('utf-8'): v.encode('utf-8') if isinstance(v, str) else v
                for k, v in params.items()
            },
            NULL
        )
        return result

    @classmethod
    def make_complete(cls, Nfa lhs, State sink_state, Alphabet alphabet):
        """Makes lhs complete

        :param Nfa lhs: automaton that will be made complete
        :param Symbol sink_state: sink state of the automaton
        :param OnTheFlyAlphabet alphabet: alphabet of the
        """
        if not lhs.thisptr.get().is_state(sink_state):
            lhs.thisptr.get().increase_size(lhs.get_num_of_states() + 1)
        mata.make_complete(lhs.thisptr.get(), <CAlphabet&>dereference(alphabet.as_base()), sink_state)

    @classmethod
    def revert(cls, Nfa lhs):
        """Reverses transitions in the lhs

        :param Nfa lhs: source automaton
        :return: automaton with reversed transitions
        """
        result = Nfa()
        mata.revert(result.thisptr.get(), dereference(lhs.thisptr.get()))
        return result

    @classmethod
    def remove_epsilon(cls, Nfa lhs, Symbol epsilon = CEPSILON):
        """Removes transitions that contain epsilon symbol.

        :param Nfa lhs: Automaton, where epsilon transitions will be removed.
        :param Symbol epsilon: Symbol representing the epsilon.
        :return: Nfa: Automaton with epsilon transitions removed.
        """
        result = Nfa()
        mata.remove_epsilon(result.thisptr.get(), dereference(lhs.thisptr.get()), epsilon)
        return result

    def remove_epsilon(self, Symbol epsilon = CEPSILON):
        """
        Removes transitions which contain epsilon symbol.

        TODO: Possibly there may be issue with setting the size of the automaton beforehand?

        :param Symbol epsilon: Symbol representing the epsilon symbol.
        """
        self.thisptr.get().remove_epsilon(epsilon)

    def get_epsilon_transitions(self, State state, Symbol epsilon = CEPSILON) -> Move | None:
        """
        Get epsilon transitions for a state.
        :param state: State to get epsilon transitions for.
        :param epsilon: Epsilon symbol.
        :return: Epsilon transitions if there are any epsilon transitions for the passed state. None otherwise.
        """
        cdef COrdVector[CTransSymbolStates].const_iterator c_epsilon_transitions_iter = self.thisptr.get().get_epsilon_transitions(
            state, epsilon
        )
        if c_epsilon_transitions_iter == self.thisptr.get().get_transitions_from(state).cend():
            return None

        cdef CTransSymbolStates epsilon_transitions = dereference(c_epsilon_transitions_iter)
        return Move(epsilon_transitions.symbol, epsilon_transitions.states_to.ToVector())


    @classmethod
    def minimize(cls, Nfa lhs):
        """Minimizes the automaton lhs

        :param Nfa lhs: automaton to be minimized
        :return: minimized automaton
        """
        result = Nfa()
        mata.minimize(result.thisptr.get(), dereference(lhs.thisptr.get()))
        return result

    @classmethod
    def reduce_with_state_map(cls, Nfa aut, params = None):
        """
        Reduce the automaton.

        :param Nfa aut: Original automaton to reduce.
        :param Dict params: Additional parameters for the reduction algorithm:
            - "algorithm": "simulation"
        :return: (Reduced automaton, state map of original to new states)
        """
        params = params or {"algorithm": "simulation"}
        cdef StateToStateMap state_map
        result = Nfa()
        mata.reduce(result.thisptr.get(), dereference(aut.thisptr.get()), &state_map,
            {
                k.encode('utf-8'): v.encode('utf-8') for k, v in params.items()
            }
        )

        return result, {k: v for k, v in state_map}

    @classmethod
    def reduce(cls, Nfa aut, params = None):
        """
        Reduce the automaton.

        :param Nfa aut: Original automaton to reduce.
        :param Dict params: Additional parameters for the reduction algorithm:
            - "algorithm": "simulation"
        :return: Reduced automaton
        """
        params = params or {"algorithm": "simulation"}
        result = Nfa()
        mata.reduce(result.thisptr.get(), dereference(aut.thisptr.get()), NULL,
            {
                k.encode('utf-8'): v.encode('utf-8') for k, v in params.items()
            }
        )
        return result

    @classmethod
    def compute_relation(cls, Nfa lhs, params = None):
        """Computes the relation for the automaton

        :param Nfa lhs: automaton
        :param Dict params: parameters of the computed relation
        :return: computd relation
        """
        params = params or {'relation': 'simulation', 'direction': 'forward'}
        cdef mata.CBinaryRelation relation = mata.compute_relation(
            dereference(lhs.thisptr.get()),
            {
                k.encode('utf-8'): v.encode('utf-8') if isinstance(v, str) else v
                for k, v in params.items()
            }
        )
        result = BinaryRelation()
        cdef size_t relation_size = relation.size()
        result.resize(relation_size)
        for i in range(0, relation_size):
            for j in range(0, relation_size):
                val = relation.get(i, j)
                result.set(i, j, val)
        return result

    # Tests
    @classmethod
    def is_deterministic(cls, Nfa lhs):
        """Tests if the lhs is determinstic

        :param Nfa lhs: non-determinstic finite automaton
        :return: true if the lhs is deterministic
        """
        return mata.is_deterministic(dereference(lhs.thisptr.get()))

    @classmethod
    def is_lang_empty_path_counterexample(cls, Nfa lhs):
        """Checks if language of automaton lhs is empty, if not, returns path of states as counter
        example.

        :param Nfa lhs:
        :return: true if the lhs is empty, counter example if lhs is not empty
        """
        cdef Path path
        return mata.is_lang_empty(dereference(lhs.thisptr.get()), &path), path

    @classmethod
    def is_lang_empty(cls, Nfa lhs):
        """Checks if language of automaton lhs is empty, if not, returns path of states as counter
        example.

        :param Nfa lhs:
        :return: true if the lhs is empty, counter example if lhs is not empty
        """
        return mata.is_lang_empty(dereference(lhs.thisptr.get()))


    @classmethod
    def is_lang_empty_word_counterexample(cls, Nfa lhs):
        """Checks if language of automaton lhs is empty, if not, returns word as counter example.

        :param Nfa lhs:
        :return: true if the lhs is empty, counter example if lhs is not empty
        """
        cdef Word word
        return mata.is_lang_empty_cex(dereference(lhs.thisptr.get()), &word), word

    @classmethod
    def is_universal(cls, Nfa lhs, Alphabet alphabet, params = None):
        """Tests if lhs is universal wrt given alphabet

        :param Nfa lhs: automaton tested for universality
        :param OnTheFlyAlphabet alphabet: on the fly alphabet
        :param dict params: additional params to the function, currently supports key 'algo',
            which determines used universality test
        :return: true if lhs is universal
        """
        params = params or {'algo': 'antichains'}
        return mata.is_universal(
            dereference(lhs.thisptr.get()),
            <CAlphabet&>dereference(alphabet.as_base()),
            {
                k.encode('utf-8'): v.encode('utf-8') if isinstance(v, str) else v
                for k, v in params.items()
            }
        )

    @classmethod
    def is_included_with_cex(
            cls, Nfa lhs, Nfa rhs, Alphabet alphabet = None, params = None
    ):
        """Test inclusion between two automata

        :param Nfa lhs: smaller automaton
        :param Nfa rhs: bigger automaton
        :param Alphabet alphabet: alpabet shared by two automata
        :param dict params: additional params
        :return: true if lhs is included by rhs, counter example word if not
        """
        cdef Word word
        cdef CAlphabet* c_alphabet = NULL
        if alphabet:
            c_alphabet = alphabet.as_base()
        params = params or {'algo': 'antichains'}
        result = mata.is_incl(
            dereference(lhs.thisptr.get()),
            dereference(rhs.thisptr.get()),
            &word,
            c_alphabet,
            {
                k.encode('utf-8'): v.encode('utf-8') if isinstance(v, str) else v
                for k, v in params.items()
            }
        )
        return result, word

    @classmethod
    def is_included(
            cls, Nfa lhs, Nfa rhs, Alphabet alphabet = None, params = None
    ):
        """Test inclusion between two automata

        :param Nfa lhs: smaller automaton
        :param Nfa rhs: bigger automaton
        :param Alphabet alphabet: alpabet shared by two automata
        :param dict params: additional params
        :return: true if lhs is included by rhs, counter example word if not
        """
        cdef CAlphabet* c_alphabet = NULL
        if alphabet:
            c_alphabet = alphabet.as_base()
        params = params or {'algo': 'antichains'}
        result = mata.is_incl(
            dereference(lhs.thisptr.get()),
            dereference(rhs.thisptr.get()),
            NULL,
            c_alphabet,
            {
                k.encode('utf-8'): v.encode('utf-8') if isinstance(v, str) else v
                for k, v in params.items()
            }
        )
        return result

    @classmethod
    def equivalence_check(cls, Nfa lhs, Nfa rhs, Alphabet alphabet = None, params = None) -> bool:
        """
        Test equivalence of two automata.

        :param Nfa lhs: Smaller automaton.
        :param Nfa rhs: Bigger automaton.
        :param Alphabet alphabet: Alphabet shared by two automata.
        :param dict params: Additional params:
            - "algo": "antichains"
        :return: True if lhs is equivalent to rhs, False otherwise.
        """
        params = params or {'algo': 'antichains'}
        cdef CAlphabet * c_alphabet = NULL
        if alphabet:
            c_alphabet = alphabet.as_base()
            return mata.equivalence_check(
                dereference(lhs.thisptr.get()),
                dereference(rhs.thisptr.get()),
                c_alphabet,
                {
                    k.encode('utf-8'): v.encode('utf-8') if isinstance(v, str) else v
                    for k, v in params.items()
                }
            )
        else:
            return mata.equivalence_check(
                dereference(lhs.thisptr.get()),
                dereference(rhs.thisptr.get()),
                {
                    k.encode('utf-8'): v.encode('utf-8') if isinstance(v, str) else v
                    for k, v in params.items()
                }
            )



    @classmethod
    def is_complete(cls, Nfa lhs, Alphabet alphabet):
        """Test if automaton is complete

        :param Nf lhs: tested automaton
        :param OnTheFlyAlphabet alphabet: alphabet of the automaton
        :return: true if the automaton is complete
        """
        return mata.is_complete(
            dereference(lhs.thisptr.get()),
            <CAlphabet&>dereference(alphabet.as_base())
        )

    @classmethod
    def is_in_lang(cls, Nfa lhs, vector[Symbol] word):
        """Tests if word is in language

        :param Nfa lhs: tested automaton
        :param vector[Symbol] word: tested word
        :return: true if word is in language of automaton lhs
        """
        return mata.is_in_lang(dereference(lhs.thisptr.get()), <Word> word)

    @classmethod
    def is_prefix_in_lang(cls, Nfa lhs, vector[Symbol] word):
        """Test if any prefix of the word is in the language

        :param Nfa lhs: tested automaton
        :param vector[Symbol] word: tested word
        :return: true if any prefix of word is in language of automaton lhs
        """
        return mata.is_prfx_in_lang(dereference(lhs.thisptr.get()), <Word> word)

    @classmethod
    def accepts_epsilon(cls, Nfa lhs):
        """Tests if automaton accepts epsilon

        :param Nfa lhs: tested automaton
        :return: true if automaton accepts epsilon
        """
        cdef vector[State] initial_states = lhs.thisptr.get().initialstates.ToVector()
        for state in initial_states:
            if lhs.has_final_state(state):
                return True
        return False

    # Helper functions

    @classmethod
    def get_word_for_path(cls, Nfa lhs, path):
        """For a given path (set of states) returns a corresponding word

        >>> mata.Nfa.get_word_for_path(lhs, [0, 1, 2])
        ([1, 1], True)

        :param Nfa lhs: source automaton
        :param list path: list of states
        :return: pair of word (list of symbols) and true or false, whether the search was successful
        """
        return mata.get_word_for_path(dereference(lhs.thisptr.get()), path)

    @classmethod
    def encode_word(cls, string_to_symbol, word):
        """Encodes word based on a string to symbol map

        >>> mata.Nfa.encode_word({'a': 1, 'b': 2, "c": 0}, "abca")
        [1, 2, 0, 1]

        :param dict string_to_symbol: dictionary of strings to integers
        :param word: list of strings representing a encoded word
        :return:
        """
        return mata.encode_word(
            {k.encode('utf-8'): v for (k, v) in string_to_symbol.items()},
            [s.encode('utf-8') for s in word]
        )

cdef class Alphabet:
    """
    Base class for alphabets
    """
    def __cinit__(self):
        pass

    def __dealloc__(self):
        pass

    def translate_symbol(self, str symbol):
        pass

    def reverse_translate_symbol(self, Symbol symbol):
        pass

    cdef get_symbols(self):
        pass

    cdef mata.CAlphabet* as_base(self):
        pass


cdef class OnTheFlyAlphabet(Alphabet):
    """
    OnTheFlyAlphabet represents alphabet that is not known before hand and is constructed on-the-fly
    """
    cdef mata.COnTheFlyAlphabet *thisptr

    def __cinit__(self, State initial_symbol = 0):
        self.thisptr = new mata.COnTheFlyAlphabet(initial_symbol)

    @classmethod
    def from_symbol_map(cls, symbol_map: dict[str, int]) -> OnTheFlyAlphabet:
        """
        Create on the fly alphabet filled with symbol_map.
        :param symbol_map: Map mapping symbol names to symbol values.
        :return: On the fly alphabet.
        """
        alphabet = cls()
        alphabet.add_symbols_from_symbol_map(symbol_map)
        return alphabet

    @classmethod
    def for_symbol_names(cls, symbol_map: list[str]) -> OnTheFlyAlphabet:
        alphabet = cls()
        alphabet.add_symbols_for_names(symbol_map)
        return alphabet

    def add_symbols_from_symbol_map(self, symbol_map: dict[str, int]) -> None:
        """
        Add symbols from symbol_map to the current alphabet.
        :param symbol_map: Map mapping strings to symbols.
        """
        cdef StringToSymbolMap c_symbol_map
        for symbol, value in symbol_map.items():
            c_symbol_map[symbol.encode('utf-8')] = value
        self.thisptr.add_symbols_from(c_symbol_map)

    def add_symbols_for_names(self, symbol_names: list[str]) -> None:
        """
        Add symbols for symbol names to the current alphabet.
        :param symbol_names: Vector of symbol names.
        """
        cdef vector[string] c_symbol_names
        for symbol_name in symbol_names:
            c_symbol_names.push_back(symbol_name.encode('utf-8'))
        self.thisptr.add_symbols_from(c_symbol_names)

    def __dealloc__(self):
        del self.thisptr

    def get_symbol_map(self) -> dict[str, int]:
        """
        Get map mapping strings to symbols.

        :return: Map of strings to symbols.
        """
        cdef umap[string, Symbol] c_symbol_map = self.thisptr.get_symbol_map()
        symbol_map = {}
        for symbol, value in c_symbol_map:
            symbol_map[symbol.encode('utf-8')] = value
        return symbol_map

    def translate_symbol(self, str symbol):
        """Translates symbol to the position of the seen values

        :param str symbol: translated symbol
        :return: order of the symbol as was seen during the construction
        """
        return self.thisptr.translate_symb(symbol.encode('utf-8'))

    def reverse_translate_symbol(self, Symbol symbol):
        """Translates symbol back to its string representation

        :param Symbol symbol: integer symbol
        :return: original string
        """
        cdef StringToSymbolMap c_symbol_map = self.thisptr.get_symbol_map()
        cdef umap[string, Symbol].iterator it = c_symbol_map.begin()
        cdef umap[string, Symbol].iterator end = c_symbol_map.end()
        while it != end:
            key = dereference(it).first
            value = dereference(it).second
            if value == symbol:
                return key
            postinc(it)
        raise IndexError(f"{symbol} is out of range of enumeration")


    cpdef get_symbols(self):
        """Returns list of supported symbols

        :return: list of supported symbols
        """
        cdef clist[Symbol] symbols = self.thisptr.get_symbols()
        return [s for s in symbols]

    cdef mata.CAlphabet* as_base(self):
        """Retypes the alphabet to its base class

        :return: alphabet as CAlphabet*
        """
        return <mata.CAlphabet*> self.thisptr

cdef class BinaryRelation:
    """
    Wrapper for binary relation
    """
    cdef mata.CBinaryRelation *thisptr

    def __cinit__(self, size_t size=0, bool defVal=False, size_t rowSize=16):
        self.thisptr = new mata.CBinaryRelation(size, defVal, rowSize)

    def __dealloc__(self):
        if self.thisptr != NULL:
            del self.thisptr

    def size(self):
        """Returns the size of the relation

        :return: size of the relation
        """
        return self.thisptr.size()

    def resize(self, size_t size, bool defValue=False):
        """Resizes the binary relation to size

        :param size_t size: new size of the binary relation
        :param bool defValue: default value that is set after resize
        """
        self.thisptr.resize(size, defValue)

    def get(self, size_t row, size_t col):
        """Gets the value of the relation at [row, col]

        :param size_t row: row of the relation
        :param size_t col: col of the relation
        :return: value of the binary relation at [row, col]
        """
        return self.thisptr.get(row, col)

    def set(self, size_t row, size_t col, bool value):
        """Sets the value of the relation at [row, col]

        :param size_t row: row of the relation
        :param size_t col: col of the relation
        :param bool value: value that is set
        """
        self.thisptr.set(row, col, value)

    def to_matrix(self):
        """Converts the relation to list of lists of booleans

        :return: relation of list of lists to booleans
        """
        size = self.size()
        result = []
        for i in range(0, size):
            sub_result = []
            for j in range(0, size):
                sub_result.append(self.get(i, j))
            result.append(sub_result)
        return result

    def reset(self, bool defValue = False):
        """Resets the relation to defValue

        :param bool defValue: value to which the relation will be reset
        """
        self.thisptr.reset(defValue)

    def split(self, size_t at, bool reflexive=True):
        """Creates new row corresponding to the row/col at given index (i think)

        :param size_t at: where the splitting will commence
        :param bool reflexive: whether the relation should stay reflexive
        """
        self.thisptr.split(at, reflexive)

    def alloc(self):
        """Increases the size of the relation by one

        :return: previsous size of the relation
        """
        return self.thisptr.alloc()

    def is_symmetric_at(self, size_t row, size_t col):
        """Checks if the relation is symmetric at [row, col] and [col, row]

        :param size_t row: checked row
        :param size_t col: checked col
        :return: true if [row, col] and [col, row] are symmetric
        """
        return self.thisptr.sym(row, col)

    def restrict_to_symmetric(self):
        """Restricts the relation to its symmetric fragment"""
        self.thisptr.restrict_to_symmetric()

    def build_equivalence_classes(self):
        """Builds equivalence classes w.r.t relation

        :return: mapping of state to its equivalence class,
            first states corresponding to a equivalence class?
        """
        cdef vector[size_t] index
        cdef vector[size_t] head
        self.thisptr.build_equivalence_classes(index, head)
        return index, head

    def build_index(self):
        """Builds index mapping states to their images

        :return: index mapping states to their images, i.e. x -> {y | xRy}
        """
        cdef vector[vector[size_t]] index
        self.thisptr.build_index(index)
        return [[v for v in i] for i in index]

    def build_inverse_index(self):
        """Builds index mapping states to their co-images

        :return: index mapping states to their co-images, i.e. x -> {y | yRx}
        """
        cdef vector[vector[size_t]] index
        self.thisptr.build_inv_index(index)
        return [[v for v in i] for i in index]

    def build_indexes(self):
        """Builds index mapping states to their images/co-images

        :return: index mapping states to their images/co-images, i.e. x -> {y | yRx}
        """
        cdef vector[vector[size_t]] index
        cdef vector[vector[size_t]] inv_index
        self.thisptr.build_index(index, inv_index)
        return [[v for v in i] for i in index], [[v for v in i] for i in inv_index]

    def transpose(self):
        """Transpose the relation

        :return: transposed relation
        """
        result = BinaryRelation()
        self.thisptr.transposed(dereference(result.thisptr))
        return result

    def get_quotient_projection(self):
        """Gets quotient projection of the relation

        :return: quotient projection
        """
        cdef vector[size_t] projection
        self.thisptr.get_quotient_projection(projection)
        return projection

    def __str__(self):
        return str(tabulate.tabulate(self.to_matrix()))


cdef subset_map_to_dictionary(SubsetMap subset_map):
    """Helper function that translates the unordered map to dictionary

    :param SubsetMap subset_map: map of state sets to states
    :return: subset_map as dictionary
    """
    result = {}
    cdef umap[StateSet, State].iterator it = subset_map.begin()
    while it != subset_map.end():
        key = dereference(it).first.ToVector()
        value = dereference(it).second
        result[tuple(sorted(key))] = value
        postinc(it)
    return result


# Temporary for testing
def divisible_by(k: int):
    """
    Constructs automaton accepting strings containing ones divisible by "k"
    """
    assert k > 1
    lhs = Nfa(k+1)
    lhs.make_initial_state(0)
    lhs.add_trans_raw(0, 0, 0)
    for i in range(1, k + 1):
        lhs.add_trans_raw(i - 1, 1, i)
        lhs.add_trans_raw(i, 0, i)
    lhs.add_trans_raw(k, 1, 1)
    lhs.make_final_state(k)
    return lhs


def run_safely_external_command(cmd: str, check_results=True, quiet=True, timeout=None, **kwargs):
    """Safely runs the piped command, without executing of the shell

    Courtesy of: https://blog.avinetworks.com/tech/python-best-practices

    :param str cmd: string with command that we are executing
    :param bool check_results: check correct command exit code and raise exception in case of fail
    :param bool quiet: if set to False, then it will print the output of the command
    :param int timeout: timeout of the command
    :param dict kwargs: additional args to subprocess call
    :return: returned standard output and error
    :raises subprocess.CalledProcessError: when any of the piped commands fails
    """
    # Split
    unpiped_commands = list(map(str.strip, cmd.split(" | ")))
    cmd_no = len(unpiped_commands)

    # Run the command through pipes
    objects: List[subprocess.Popen] = []
    for i in range(cmd_no):
        executed_command = shlex.split(unpiped_commands[i])

        # set streams
        stdin = None if i == 0 else objects[i-1].stdout
        stderr = subprocess.STDOUT if i < (cmd_no - 1) else subprocess.PIPE

        # run the piped command and close the previous one
        piped_command = subprocess.Popen(
            executed_command,
            shell=False, stdin=stdin, stdout=subprocess.PIPE, stderr=stderr, **kwargs
        )
        if i != 0:
            objects[i-1].stdout.close()  # type: ignore
        objects.append(piped_command)

    try:
        # communicate with the last piped object
        cmdout, cmderr = objects[-1].communicate(timeout=timeout)

        for i in range(len(objects) - 1):
            objects[i].wait(timeout=timeout)

    except subprocess.TimeoutExpired:
        for process in objects:
            process.terminate()
        raise

    # collect the return codes
    if check_results:
        for i in range(cmd_no):
            if objects[i].returncode:
                if not quiet and (cmdout or cmderr):
                    print(f"captured stdout: {cmdout.decode('utf-8')}", 'red')
                    print(f"captured stderr: {cmderr.decode('utf-8')}", 'red')
                raise subprocess.CalledProcessError(
                    objects[i].returncode, unpiped_commands[i]
                )

    return cmdout, cmderr

cdef class Segmentation:
    """Wrapper over Segmentation."""
    cdef mata.CSegmentation* thisptr

    def __cinit__(self, Nfa aut, Symbol symbol = CEPSILON):
        """
        Compute segmentation.

        :param aut: Segment automaton to compute epsilon depths for.
        :param symbol: Symbol to execute segmentation for.
        """
        self.thisptr = new mata.CSegmentation(dereference(aut.thisptr), symbol)

    def __dealloc__(self):
        del self.thisptr

    def get_epsilon_depths(self):
        """
        Get segmentation depths for ε-transitions.

        :return: Map of depths to lists of ε-transitions.
        """
        cdef umap[unsigned, vector[CTrans]] c_epsilon_transitions = self.thisptr.get_epsilon_depths()
        result = {}
        for epsilon_depth_pair in c_epsilon_transitions:
            for trans in epsilon_depth_pair.second:
                if epsilon_depth_pair.first not in result:
                    result[epsilon_depth_pair.first] = []

                result[epsilon_depth_pair.first].append(Trans(trans.src, trans.symb, trans.tgt))

        return result

    def get_segments(self):
        """
        Get segment automata.

        :return: A vector of segments for the segment automaton in the order from the left (initial state in segment
                 automaton) to the right (final states of segment automaton).
        """
        segments = []
        cdef AutSequence c_segments = self.thisptr.get_segments()
        for c_segment in c_segments:
            segment = Nfa(c_segment.get_num_of_states())
            segment.thisptr.get().initialstates = c_segment.initialstates
            segment.thisptr.get().finalstates = c_segment.finalstates
            segment.thisptr.get().transitionrelation = c_segment.transitionrelation

            segments.append(segment)

        return segments
