import networkx as nx
import graphviz
import IPython
import sys

from IPython.display import display, HTML

cimport libmata.alphabets as alph
cimport libmata.nfa.nfa as mata_nfa

from libmata.nfa.nfa import store, _store


def plot(
        *automata: mata_nfa.Nfa,
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
    dots = []
    for aut in automata:
        dot = plot_using_graphviz(aut, with_scc, node_highlight, edge_highlight, alphabet)
        if get_interactive_mode() == 'notebook':
            dots.append(dot)
        else:
            dot.view()
    if get_interactive_mode() == 'notebook':
        display_inline(*dots)


def _plot_state(aut, dot, state, configuration):
    """Plots the state

    :param aut: base automaton
    :param dot: output digraph
    :param state: plotted state
    :param configuration: configuration of the state
    """
    if aut.has_initial_state(state):
        dot.node(f"q{state}", "", shape="plaintext", fontsize="1")
    dot.node(
        f"{state}", label=f"{state}", **configuration,
        shape='doublecircle' if aut.has_final_state(state) else 'circle',
    )


def get_configuration_for(default, rules, *args):
    """For given node or edge, processes the list of rules and applies them.

    :param dict default: default style of the primitive
    :param list rules: list of rules in form of condition and style
    """
    conf = {}
    conf.update(default)
    for rule, style in rules or []:
        if rule(*args):
            conf.update(style)
    return conf


def plot_using_graphviz(
        aut: mata_nfa.Nfa,
        with_scc: bool = False,
        node_highlight: list = None,
        edge_highlight: list = None,
        alphabet: alph.Alphabet = None
):
    """Plots automaton using graphviz

    :param list node_highlight: list of rules for changing style of nodes
    :param list edge_highlight: list of rules for changing style of edges
    :param mata_nfa.Nfa aut: plotted automaton
    :param bool with_scc: will plot with strongly connected components
    :param alph.Alphabet alphabet: alphabet for reverse translation of symbols
    :return: automaton in graphviz
    """
    # Configuration
    base_configuration = store()['node_style']
    edge_configuration = store()['edge_style']
    alphabet = alphabet or store()['alphabet']
    if not alphabet:
        print("warning: missing alphabet necessary to translate the symbols")
    dot = graphviz.Digraph("dot")
    if aut.label:
        dot.attr(
            label=aut.label, labelloc="t", kw="graph",
            fontname="Helvetica", fontsize="14"
        )

    if with_scc:
        G = aut.to_networkx_graph()
        for i, scc in enumerate(nx.strongly_connected_components(G)):
            with dot.subgraph(name=f"cluster_{i}") as c:
                c.attr(color='black', style='filled', fillcolor="lightgray", label="")
                for state in scc:
                    _plot_state(
                        aut, c, state,
                        get_configuration_for(base_configuration, node_highlight, aut, state)
                    )
    else:
        # Only print reachable states
        for state in range(0, aut.num_of_states()):
            # Helper node to simulate initial automaton
            _plot_state(
                aut, dot, state,
                get_configuration_for(base_configuration, node_highlight, aut, state)
            )

    # Plot edges
    for state in aut.initial_states:
        dot.edge(f"q{state}", f"{state}", **edge_configuration)
    edges = {}
    for trans in aut.iterate():
        key = f"{trans.source},{trans.target}"
        if key not in edges.keys():
            edges[key] = []
        symbol = "{}".format(
            alphabet.reverse_translate_symbol(trans.symbol) if alphabet else trans.symbol
        )
        edges[key].append((
            f"{trans.source}", f"{trans.target}", symbol,
            get_configuration_for(
                edge_configuration, edge_highlight, aut, trans
            )
        ))
    for edge in edges.values():
        source = edge[0][0]
        target = edge[0][1]
        label = "<" + " | ".join(sorted(t[2] for t in edge)) + ">"
        style = {}
        for val in edge:
            style.update(val[3])
        dot.edge(source, target, label=label, **style)

    return dot


def get_interactive_mode() -> str:
    """Checks and returns, which interactive mode (if any) the code is run in

    The function returns:
      1. 'none' if the code is not run in any interactive mode
      2. 'notebook' if the code is run in the jupyter notebook
      3. 'terminal' if the code is run in the interactive terminal

    :return: type of the interactive mode
    """
    if 'ipykernel' in sys.modules:
        return 'notebook'
    elif 'IPython' in sys.modules:
        return 'terminal'
    else:
        return 'none'


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
    width = res = ''
    if per_row:
        width = 'width:{}%;'.format(100//per_row)
    for arg in args:
        dpy = 'inline-block'
        if show is not None and hasattr(arg, 'show'):
            rep = arg.show(show)._repr_svg_()
        elif hasattr(arg, '_repr_image_svg_xml'):
            rep = arg._repr_image_svg_xml()
        elif hasattr(arg, '_repr_svg_'):
            rep = arg._repr_svg_()
        elif hasattr(arg, '_repr_html_'):
            rep = arg._repr_html_()
        elif hasattr(arg, '_repr_latex_'):
            rep = arg._repr_latex_()
            if not per_row:
                dpy = 'inline'
        else:
            rep = str(arg)
        res += ("<div style='vertical-align:text-top;display:{};{}'>{}</div>"
                .format(dpy, width, rep))
    display(HTML(res))




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
        return trans.source == trans.target
