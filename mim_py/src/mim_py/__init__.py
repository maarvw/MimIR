from enum import IntEnum
from mim_py.mim import * 
from mim_py.mim import World, Sym, Def # for explicit use below
from mim_py.regex_plug import regex as r
from typing import List
from mim_py.callable import MimCallable
from mim_py.jit import JIT
from mim_py.plugin import MimPlugin
from mim_py.mim_regex import MimRegex

def call(self, *args) -> Def:
    callee = args[0]
    if isinstance(args[0], IntEnum):
        callee = self.annex_by_id(args[0].value)
    if isinstance(callee, str):
        callee = self.sym(callee)
        callee = self.annex(callee)
    if isinstance(callee, Sym):
        callee = self.annex(callee)
        if len(args) == 1:
            return callee

    if len(args) == 1:
        return args[0]

    if len(args) >= 3:
        if isinstance(args[1], List):
            return self.call(self.implicit_app(callee, args[1]), args[2::])
        else:
            return self.call(self.implicit_app(callee, [args[1]]), args[2::])
    else:
        if isinstance(args[1], List):
            return self.implicit_app(callee, args[1])
        else:
            # print(f"first argument: {type(args[1])}")
            if isinstance(args[1], tuple):
                tmp = list(args[1])
                return self.implicit_app(callee, tmp[0])
            return self.implicit_app(callee, [args[1]])
    raise TypeError("The given arguments dont match the expected types")

World.call = call 