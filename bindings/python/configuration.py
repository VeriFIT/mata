# cython: language_level=3

import alphabets

__author__ = 'Tomas Fiedor'

_store = {
    'node_style': {
        "width": "0.3",
        "height": "0.3",
        "fontsize": "10",
        "fixedsize": "true",
        "penwidth": "1.5",
    },
    'alphabet': alphabets.OnTheFlyAlphabet(),
}


def store():
    """
    Returns the configuration of the library
    """
    return _store


def setup(**kwargs):
    """
    Provides the setup of the configuration of the mata library
    """
    _store.update(kwargs)


class Style:
    """
    Collection of helper styles for coloring nodes and edges in automata
    """
    @classmethod
    def filled(cls, fillcolor, edgecolor=None):
        """Style that fills the primitive with color"""
        style = {'fillcolor': fillcolor}
        if edgecolor:
            style['color'] = edgecolor
        return style

    @classmethod
    def colored(cls, color):
        """Style that make primitive colored"""
        return {'color': color}

    @classmethod
    def dashed(cls, color=None):
        """Style that makes lines dashed"""
        style = {'style': 'dashed'}
        if color:
            style['color'] = color
        return style

    @classmethod
    def hidden(cls):
        """Style that hides the primitive"""
        return {'style': 'invis'}


class Condition:
    """
    Collection of helper functions that can be used as conditions in highlighting rule
    """
    @classmethod
    def state_is_initial(cls, automaton, state):
        """Tests if state in automaton is initial"""
        return automaton.has_initial_state(state)

    @classmethod
    def state_is_final(cls, automaton, state):
        """Tests if state in automaton is final"""
        return automaton.has_final_state(state)

    @classmethod
    def transition_is_cycle(cls, _, trans):
        """Tests if transition is self cycle"""
        return trans.src == trans.tgt
