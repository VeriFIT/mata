__author__ = "Tomas Fiedor"

import libmata.nfa.nfa as mata_nfa


def test_conversions(fa_one_divisible_by_two):
    """Tests conversions of automata to various formats"""
    # Test transforming automaton to dataframe
    df = fa_one_divisible_by_two.to_dataframe()

    assert not df.empty
    assert len(df) == 6
    assert list(df["source"]) == [0, 0, 1, 1, 2, 2]
    assert list(df["symbol"]) == [0, 1, 0, 1, 0, 1]
    assert list(df["target"]) == [0, 1, 1, 2, 2, 1]

    # Test transforming automaton to nx.Graph
    G = fa_one_divisible_by_two.to_networkx_graph()

    assert len(G) == 3
    assert list(G.nodes) == [0, 1, 2]
    assert list(G.edges) == [(0, 0), (0, 1), (1, 1), (1, 2), (2, 2), (2, 1)]

    # Test empty automaton
    empty = mata_nfa.Nfa()
    df = empty.to_dataframe()
    assert df.empty

    G = empty.to_networkx_graph()
    assert len(G) == 0
    assert list(G.nodes) == []
    assert list(G.edges) == []


def test_to_networkx_graph_keeps_parallel_symbols():
    """All symbols between a pair of states have to survive the conversion."""
    nfa = mata_nfa.Nfa(2)
    for symbol in (2, 0, 1):
        nfa.add_transition(0, symbol, 1)
    nfa.add_transition(1, 7, 1)

    G = nfa.to_networkx_graph()

    # Plain (source, target) edges, so that unpacking `G.edges` keeps working.
    assert list(G.edges) == [(0, 1), (1, 1)]
    assert G[0][1]["symbols"] == [0, 1, 2]
    # A single-symbol edge keeps the scalar attribute; an ambiguous one must not.
    assert "symbol" not in G[0][1]
    assert G[1][1]["symbols"] == [7]
    assert G[1][1]["symbol"] == 7


def test_plotting_edge_rules_receive_a_transition():
    """Edge rules are given a Transition of the plotted automaton and can restyle the edge."""
    from libmata import plotting

    nfa = mata_nfa.Nfa(3)
    nfa.add_transition(0, 5, 1)
    nfa.add_transition(1, 6, 2)

    seen = []

    def matches(aut, trans):
        seen.append((trans.source, trans.symbol, trans.target))
        return trans == mata_nfa.Transition(0, 5, 1)

    dot = plotting.plot_using_graphviz(nfa, edge_highlight=[(matches, {"color": "red"})])

    assert seen == [(0, 5, 1), (1, 6, 2)]
    source = dot.source
    assert "0 -> 1 [label=<5> color=red" in source
    assert "color=red" not in source.split("1 -> 2", 1)[1]
