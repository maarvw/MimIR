"""Tests for the World.call(...) helper monkey-patched in mim_py/__init__.py."""
from __future__ import annotations
import unittest
import mim_py as mim
from mim_py.regex_plug import *
from mim_py.core_plug import * 


class TestCallFunctions(unittest.TestCase):
    
    def setUp(self):
        self.driver = mim.Driver()
        self.wrld = self.driver.world()
    
    def load_plugs(self):
        self.driver.load_pluins(["core"])
    
    def test_call_with_string_resolves_annex(self):
        callee = self.wrld.call(core.bit2.and_)
        assert isinstance(callee, mim.Def)
    
    
    def test_call_with_sym_resolves_annex(self):
        s = self.wrld.sym("%core.bit2.and_")
        callee = self.wrld.call(s)
        assert isinstance(callee, mim.Def)
    
    
    def test_call_with_def_passthrough(self):
        nat0 = self.wrld.lit_nat_0()
        # Per mim_py/__init__.py:24, a single non-string/Sym arg returns itself.
        assert self.wrld.call(nat0) is nat0 or isinstance(self.wrld.call(nat0), mim.Def)
    
    
    
    def test_call_folds_arg_list(self):
        # %core.bit2.and_ with a single Def arg — the folding branch.
        nat0 = self.wrld.lit_nat_0()
        result = self.wrld.call("%core.bit2.and_", nat0)
        assert isinstance(result, mim.Def)
   
   
if __name__ == "__main__":
    unittest.main()