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

    @classmethod
    def numToSymb(cls, num):
        """Converts a number used by VATA to a symbol"""
        if num in cls.__numDict:
            return cls.__numDict[num]
        else:
            raise Exception("no translation for {} in {}".format(num, cls.__numDict))

    @staticmethod
    def clearLibrary():
        """Clears the automata library maintained by VATA"""
        g_vatalib.nfa_clear_library()

    @staticmethod
    def setDebugLevel(level):
        """Sets VATA's debug level"""
        assert type(level) == int
        g_vatalib.nfa_set_debug_level(level)

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
    def __getStringListFromVATAFunction(self, vataFuncName, *args):
        """Calls VATAFuncName TODO TODO TODO"""
        def f(aut, buf_len, *args):
            buf = ctypes.create_string_buffer(buf_len)
            rv = g_vatalib[vataFuncName](self.aut, buf, buf_len, *args)
            return rv, buf

        INIT_BUF_LEN = 1024
        rv, buf = f(self.aut, INIT_BUF_LEN, *args)   # first attempt
        if rv < 0:          # if there was some problem
            if rv == -1:    # other error
                raise Exception("error while communicating with VATA: " +
                    "returned error value from {}: {}".format(vataFuncName, rv))
            else:           # memory too small
                rv, buf = f(self.aut, -rv, *args)   # call again with larger memory
                if rv < 0:
                    raise Exception("error while communicating with VATA " +
                        "(second attempt): " +
                        "returned error value from {}: {}".format(vataFuncName, rv))

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
        out_str_list = self.__getStringListFromVATAFunction("nfa_get_initial")
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
        out_str_list = self.__getStringListFromVATAFunction("nfa_get_final")
        fin_states = set([int(x) for x in out_str_list])
        return fin_states


    ######################### TRANSITIONS #############################
    def addTransition(self, src, symb, tgt):
        """Adds a transtion from src to tgt over symb"""
        assert type(src) == int and type(tgt) == int
        symb_num = NFA.symbToNum(symb)
        g_vatalib.nfa_add_trans(self.aut, src, symb_num, tgt)

    def hasTransition(self, src, symb, tgt):
        """Checks whether there is a transition from src to tgt over symb"""
        assert type(src) == int and type(tgt) == int
        symb_num = NFA.symbToNum(symb)
        return True if g_vatalib.nfa_has_trans(self.aut, src, symb_num, tgt) else False

    def getTransitions(self):
        """Gets all transitions of the automaton"""
        out_str_list = self.__getStringListFromVATAFunction("nfa_get_transitions")
        trans_set = set()
        for trans_str in out_str_list:
            trans_split = trans_str.split(' ')
            assert len(trans_split) == 3
            symb = NFA.numToSymb(int(trans_split[1]))
            trans = (int(trans_split[0]), symb, int(trans_split[2]))
            trans_set.add(trans)

        return trans_set


    ################################# FOR DEBUGGING ############################
    def __str__(self):
        initial = self.getInitial()
        final = self.getFinal()
        trans = self.getTransitions()
        trans_str = ["{} -({})-> {}".format(src, symb, tgt) for (src, symb, tgt) in trans]
        return "<init: {}, final: {}, trans: {}>".format(initial, final, trans_str)

    ############################## AUXILIARY OPERATIONS ########################
    def getFwdReachStates(self):
        """Gets states reachable from initial states"""
        out_str_list = self.__getStringListFromVATAFunction("nfa_get_fwd_reach_states")
        fwd_states = set([int(x) for x in out_str_list])
        return fwd_states


    ############################### AUTOMATA OPERATIONS ########################
    def minimize(self):
        """Returns a minimized automaton"""
        tmp = NFA()
        g_vatalib.nfa_minimize(tmp.aut, self.aut)
        return tmp

    def removeEpsilon(self, epsilon):
        """Removes epsilon transitions from the automaton (preserving language)"""
        tmp = NFA()
        eps_num = NFA.symbToNum(epsilon)
        g_vatalib.nfa_remove_epsilon(tmp.aut, self.aut, eps_num)
        return tmp


    ################################## LANGUAGE TESTS ##########################
    def acceptsEpsilon(self):
        """Checkes whether the automaton accepts epsilon"""
        return True if g_vatalib.nfa_accepts_epsilon(self.aut) == 1 else False


    # TODO: trim (odstran zbytecne prechody/stavy)


    ################# STATIC METHODS FOR AUTOMATA OPERATIONS ###################
    @classmethod
    def union(cls, lhs, rhs):
        """Creates a union of lhs and rhs.  The states will be renamed to avoid collision."""
        assert type(lhs) == NFA and type(rhs) == NFA
        tmp = NFA()
        g_vatalib.nfa_union(tmp.aut, lhs.aut, rhs.aut)
        return tmp

    # TODO: test inkluze

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
        self.assertEqual(aut.getTransitions(), set())
        aut.addTransition(41, "hello", 42)
        self.assertTrue(aut.hasTransition(41, "hello", 42))
        self.assertEqual(aut.getTransitions(), {(41, "hello", 42)})

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
        aut2.addInitial(41)
        aut2.addFinal(41)
        aut3 = NFA.union(aut1, aut2)
        self.assertEqual(len(aut3.getInitial()), 2)
        self.assertEqual(len(aut3.getFinal()), 1)


    def test_accepts_epsilon(self):
        """Testing accepts epsilon"""
        aut = NFA()
        self.assertFalse(aut.acceptsEpsilon())
        aut.addInitial(1)
        self.assertFalse(aut.acceptsEpsilon())
        aut.addFinal(2)
        self.assertFalse(aut.acceptsEpsilon())
        aut.addInitial(2)
        self.assertTrue(aut.acceptsEpsilon())

    def test_fwd_reach_states(self):
        """Testing collection of fwd reachable states"""
        aut = NFA()
        aut.addInitial(1)
        aut.addTransition(1, "a", 1)
        aut.addTransition(1, "b", 2)
        aut.addTransition(2, "a", 3)
        aut.addTransition(3, "a", 4)
        aut.addTransition(8, "b", 4)
        aut.addInitial(7)
        aut.addFinal(42)
        self.assertEqual(aut.getFwdReachStates(), {1, 2, 3, 4, 7})


    def test_minimization(self):
        """Testing minimization"""
        aut1 = NFA()
        aut1.addInitial(1)
        aut1.addTransition(1, "a", 1)
        aut1.addTransition(1, "b", 1)

        aut1.addTransition(1, "a", 2)

        aut1.addTransition(2, "a", 3)
        aut1.addTransition(2, "b", 3)

        aut1.addTransition(3, "a", 4)
        aut1.addTransition(3, "b", 4)

        aut1.addTransition(4, "a", 5)
        aut1.addTransition(4, "b", 5)

        aut1.addFinal(5)

        aut2 = aut1.minimize()
        aut2_reach = aut2.getFwdReachStates()

        self.assertEqual(len(aut2_reach), 16)

    def test_removeEpsilon(self):
        """Testing epsilon removing"""
        aut1 = NFA()

        aut1.addInitial(1)
        aut1.addTransition(1, "a", 1)
        aut1.addTransition(1, "b", 2)
        aut1.addTransition(2, "a", 3)
        aut1.addTransition(3, "a", 4)
        aut1.addTransition(3, "a", 5)
        aut1.addTransition(5, "b", 4)
        aut1.addFinal(4)

        aut2 = aut1.removeEpsilon("a")

        self.assertTrue(aut2.isInitial(1))
        self.assertTrue(aut2.hasTransition(1, "b", 2))
        self.assertTrue(aut2.hasTransition(2, "b", 4))
        self.assertTrue(aut2.isFinal(2))
        self.assertTrue(aut2.isFinal(4))

        self.assertFalse(aut2.hasTransition(1, "b", 1))
        self.assertFalse(aut2.hasTransition(2, "b", 2))
        self.assertFalse(aut2.hasTransition(4, "b", 4))
        self.assertFalse(aut2.isFinal(1))
        self.assertFalse(aut2.isInitial(2))
        self.assertFalse(aut2.isInitial(3))
        self.assertFalse(aut2.isInitial(4))


###########################################
if __name__ == '__main__':
    NFA.setDebugLevel(0)
    unittest.main()
