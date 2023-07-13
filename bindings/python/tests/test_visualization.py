__author__ = 'Tomas Fiedor'

import libmata.nfa.nfa as mata_nfa


def test_conversions(fa_one_divisible_by_two):
    """Tests conversions of automata to various formats"""
    # Test transforming automaton to dataframe
    df = fa_one_divisible_by_two.to_dataframe()

    assert not df.empty
    assert len(df) == 6
    assert list(df['source']) == [0, 0, 1, 1, 2, 2]
    assert list(df['symbol']) == [0, 1, 0, 1, 0, 1]
    assert list(df['target']) == [0, 1, 1, 2, 2, 1]

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


