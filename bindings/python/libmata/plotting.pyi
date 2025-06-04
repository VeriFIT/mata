from typing import Literal, Any

import libmata.nfa.nfa as mata_nfa
import graphviz

def plot(
        automata: mata_nfa.Nfa,
        with_scc: bool = False,
        node_highlight: list = None,
        edge_highlight: list = None,
        alphabet: alph.Alphabet = None
):
    """Plots the stream of automata

    :param bool with_scc: whether the SCC should be displayed
    :param list automata: stream of automata that will be plotted using graphviz
    :param list node_highlight: list of rules for changing style of nodes
    :param list edge_highlight: list of rules for changing style of edges
    :param alph.Alphabet alphabet: alphabet for printing the symbols
    """
    ...

def get_configuration_for(default: dict, rules: list, *args):
    """For given node or edge, processes the list of rules and applies them.

    :param dict default: default style of the primitive
    :param list rules: list of rules in form of condition and style
    """
    ...

def plot_using_graphviz(
        aut: mata_nfa.Nfa,
        with_scc: bool = False,
        node_highlight: list = None,
        edge_highlight: list = None,
        alphabet: alph.Alphabet = None
) -> graphviz.Digraph:
    """Plots automaton using graphviz

    :param list node_highlight: list of rules for changing style of nodes
    :param list edge_highlight: list of rules for changing style of edges
    :param mata_nfa.Nfa aut: plotted automaton
    :param bool with_scc: will plot with strongly connected components
    :param alph.Alphabet alphabet: alphabet for reverse translation of symbols
    :return: automaton in graphviz
    """
    ...

def get_interactive_mode() -> Literal['none'] | Literal['notebook'] | Literal['terminal']:
    """Checks and returns, which interactive mode (if any) the code is run in

    The function returns:
      1. 'none' if the code is not run in any interactive mode
      2. 'notebook' if the code is run in the jupyter notebook
      3. 'terminal' if the code is run in the interactive terminal

    :return: type of the interactive mode
    """
    ...

def display_inline(*args, per_row=None, show=None):
    """
    This is a wrapper around IPython's `display()` to display multiple
    elements in a row, without forced line break between them.

    Copyright (C) 2018 Laboratoire de Recherche et Développement de l'Epita
    (LRDE).

    This function is part of Spot, a model checking library.

    If the `per_row` argument is given, at most `per_row` arguments are
    displayed on each row, each one taking 1/per_row of the line width.
    """
    ...

def setup(**kwargs):
    """
    Provides the setup of the configuration of the mata library
    """
    ...

class Style:
    """
    Collection of helper styles for coloring nodes and edges in automata
    """
    @classmethod
    def filled(cls, fillcolor, edgecolor=None) -> dict[Literal['fillcolor'] | Literal['color'], Any]:
        """Style that fills the primitive with color"""
        ...
    @classmethod
    def colored(cls, color) -> dict[Literal['color'], Any]:
        """Style that make primitive colored"""
        ...
    @classmethod
    def dashed(cls, color=None) -> dict[Literal['style'] | Literal['color'], Any]:
        """Style that makes lines dashed"""
        ...
    @classmethod
    def hidden(cls) -> dict[Literal['style'], Literal['invis']]:
        """Style that hides the primitive"""
        ...

class Condition:
    """
    Collection of helper functions that can be used as conditions in highlighting rule
    """
    @classmethod
    def state_is_initial(cls, automaton, state) -> bool:
        """Tests if state in automaton is initial"""
        ...
    @classmethod
    def state_is_final(cls, automaton, state) -> bool:
        """Tests if state in automaton is final"""
        ...
    @classmethod
    def transition_is_cycle(cls, _, trans) -> bool:
        """Tests if transition is self cycle"""
