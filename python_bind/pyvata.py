#!/usr/bin/env python3
import ctypes
import ctypes.util

import unittest

VATALIB_NAME="../build/python_bind/libvata2-c-ifc"

# global variable carrying reference to the loaded DLL
g_vatalib = None

# do this when the module is loaded
vatalib_path = ctypes.util.find_library(VATALIB_NAME)
if not vatalib_path:
    raise Exception("Unable to find the library {}".format(VATALIB_NAME))
try:
    g_vatalib = ctypes.CDLL(vatalib_path)
except OSError:
    raise Exception("Unable to load the library {}".format(VATALIB_NAME))


###########################################
class NFA:
    """A Python wrapper over a VATA NFA."""

    ################ CLASS VARIABLE AND METHODS #################
    __symbDict = dict()    # translates symbols to numbers
    __numDict = dict()     # translates numbers to symbols
    __symbToNumCnt = 0     # counter for assigning symbols to unique numbers

    @classmethod
    def symbToNum(cls, symb):
        """Converts a symbol to a number to be used by VATA"""
        if symb in cls.__symbDict:
            return cls.__symbDict[symb]
        else:
            num = cls.__symbToNumCnt
            cls.__symbToNumCnt += 1
            cls.__symbDict[symb] = num
            cls.__numDict[num] = symb
            return num

    @staticmethod
    def clearLibrary():
        """Clears the automata library maintained by VATA"""
        g_vatalib.nfa_clear_library()

    ################ CONSTRUCTORS AND DESTRUCTORS #################
    def __init__(self):
        """The constructor"""
        self.aut = g_vatalib.nfa_init()

    def __del__(self):
        """The destructor"""
        g_vatalib.nfa_free(self.aut)

    def copy(self):
        """Copy operator (performs a deep copy, otherwise things might get crazy..."""
        tmp = NFA()
        g_vatalib.nfa_copy(tmp.aut, self.aut)
        return tmp


    ########################## AUXILIARY METHODS ##########################
    def getStringListFromVATAFunction(self, vataFuncName, *args):
        """Calls VATAFuncName TODO TODO TODO"""
        # TODO: improve
        buf_len = 128
        buf = ctypes.create_string_buffer(buf_len)
        rv = g_vatalib[vataFuncName](self.aut, buf, buf_len, *args)
        if rv < 0:
            raise Exception("returned error value from {}: {}".format(vataFuncName, rv))

        out_str = buf.raw[:rv].decode('ascii')
        if out_str == '':
            return list()
        out_list = out_str.split(',')
        return out_list


    ######################## INITIAL STATES ############################
    def addInitial(self, state):
        """Adds an initial state"""
        assert type(state) == int
        g_vatalib.nfa_add_initial(self.aut, state)

    def removeInitial(self, state):
        """Removes an initial state"""
        assert type(state) == int
        g_vatalib.nfa_remove_initial(self.aut, state)

    def isInitial(self, state):
        """Checks whether a state is initial"""
        assert type(state) == int
        return True if g_vatalib.nfa_is_initial(self.aut, state) else False

    def getInitial(self):
        """Gets initial states"""
        out_str_list = self.getStringListFromVATAFunction("nfa_get_initial")
        init_states = set([int(x) for x in out_str_list])
        return init_states


    ######################### FINAL STATES #############################
    def addFinal(self, state):
        """Adds a final state"""
        assert type(state) == int
        g_vatalib.nfa_add_final(self.aut, state)

    def removeFinal(self, state):
        """Removes a final state"""
        assert type(state) == int
        g_vatalib.nfa_remove_final(self.aut, state)

    def isFinal(self, state):
        """Checks whether a state is final"""
        assert type(state) == int
        return True if g_vatalib.nfa_is_final(self.aut, state) else False

    def getFinal(self):
        """Gets final states"""
        out_str_list = self.getStringListFromVATAFunction("nfa_get_final")
        fin_states = set([int(x) for x in out_str_list])
        return fin_states


    ######################### TRANSITIONS #############################
    def addTransition(self, src, symb, tgt):
        """Adds a transtion from src to tgt over symb"""
        assert type(src) == int and type(tgt) == int
        symb_num = NFA.symbToNum(symb)
        g_vatalib.nfa_add_trans(self.aut, src, symb_num, tgt)

    def hasTransition(self, src, symb, tgt):
        """Checks whether there is a transtion from src to tgt over symb"""
        assert type(src) == int and type(tgt) == int
        symb_num = NFA.symbToNum(symb)
        return True if g_vatalib.nfa_has_trans(self.aut, src, symb_num, tgt) else False

    def minimize(self):
        """Returns a minimizes automaton"""
        tmp = NFA()
        g_vatalib.nfa_minimize(tmp.aut, self.aut)
        return tmp

    #################### STATIC METHODS FOR AUTOMATA OPERATIONS ######################
    @classmethod
    def union_of_disjoint(cls, lhs, rhs):
        """Creates a union of lhs and rhs"""
        assert type(lhs) == NFA and type(rhs) == NFA
        tmp = NFA()
        g_vatalib.nfa_union(tmp.aut, lhs.aut, rhs.aut)
        return tmp

################################## UNIT TESTS ##################################
class NFATest(unittest.TestCase):

    def setUp(self):
        NFA.clearLibrary()

    def test_init_free(self):
        """Testing construction and destruction"""
        self.assertEqual(g_vatalib.nfa_library_size(), 0)
        aut = NFA()
        self.assertEqual(g_vatalib.nfa_library_size(), 1)
        del aut
        self.assertEqual(g_vatalib.nfa_library_size(), 0)

    def test_initial_states(self):
        """Testing adding and checking of initial states"""
        aut = NFA()
        self.assertFalse(aut.isInitial(42))
        aut.addInitial(42)
        self.assertTrue(aut.isInitial(42))
        aut.removeInitial(42)
        self.assertFalse(aut.isInitial(42))
        self.assertEqual(aut.getInitial(), set())
        aut.addInitial(42)
        self.assertEqual(aut.getInitial(), {42})
        aut.addInitial(43)
        self.assertEqual(aut.getInitial(), {42, 43})

    def test_final_states(self):
        """Testing adding and checking of final states"""
        aut = NFA()
        self.assertFalse(aut.isFinal(42))
        aut.addFinal(42)
        self.assertTrue(aut.isFinal(42))
        aut.removeFinal(42)
        self.assertFalse(aut.isFinal(42))
        self.assertEqual(aut.getFinal(), set())
        aut.addFinal(42)
        self.assertEqual(aut.getFinal(), {42})
        aut.addFinal(43)
        self.assertEqual(aut.getFinal(), {42, 43})

    def test_transitions_simple(self):
        """Testing adding and checking of transitions"""
        aut = NFA()
        self.assertFalse(aut.hasTransition(41, "hello", 42))
        aut.addTransition(41, "hello", 42)
        self.assertTrue(aut.hasTransition(41, "hello", 42))

    def test_copy(self):
        """Testing copying"""
        aut1 = NFA()
        aut1.addFinal(42)
        aut2 = aut1.copy()
        aut1.removeFinal(42)
        self.assertTrue(aut2.isFinal(42))

    def test_union(self):
        """Testing union"""
        aut1 = NFA()
        aut1.addInitial(41)
        aut2 = NFA()
        aut2.addFinal(42)
        aut3 = NFA.union_of_disjoint(aut1, aut2)
        self.assertTrue(aut3.isInitial(41))
        self.assertTrue(aut3.isFinal(42))

    def test_minimization(self):
        """Testing minimization"""
        aut1 = NFA()
        aut2 = aut1.minimize()

        assert False



###########################################
if __name__ == '__main__':
    unittest.main()
