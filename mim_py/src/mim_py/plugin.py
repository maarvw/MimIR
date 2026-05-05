from pathlib import Path

from abc import ABC, abstractmethod

from mim_py.jit import JIT
from mim_py import Driver, Level
from mim_py.callable import MimCallable

class MimPlugin(ABC):
    
    def __init__(self, driver: Driver, log_level: Level, libname):
        self.driver = driver
        self.driver.log().set_stdout().set(log_level)
        self._registered_functions = {}
        self.libname = libname
        
    @abstractmethod
    def build(self):
        raise MemoryError("Needs to be implemented by plugin author")
        
    def register_func(self, name: str, input_types: list, return_type):
        self._registered_functions[name] = (input_types, return_type)

    def jit(self, ) -> dict[str, MimCallable]:
        self.build()
        
        self.driver.world().optimize()
        self.driver.backend("ll", self.libname + ".ll", self.driver.world())
        
        jit = JIT(self.libname)          # ← erzeugt, nicht geerbt
        lib = jit.compile_and_load()
        
        callables = {}
        for name, (in_types, ret_type) in self._registered_functions.items():
            callables[name] = MimCallable(name, in_types, ret_type, lib[name])

        return callables