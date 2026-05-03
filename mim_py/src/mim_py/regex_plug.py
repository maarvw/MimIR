from enum import IntEnum

class regex(IntEnum):
    ID = 0x5503968721166663680
    conj = 0x4c62066400000000
    disj = 0x4c62066400000100
    range = 0x4c62066400000200
    lit = 0x4c62066400000300
    not_ = 0x4c62066400000400
    neg_lookahead = 0x4c62066400000500
    any = 0x4c62066400000700
    empty = 0x4c62066400000800
    lower_regex = 0x4c62066400000a00
    
    class cls(IntEnum):
        d = 0x4c62066400000600,
        D = 0x4c62066400000601,
        w = 0x4c62066400000602,
        W = 0x4c62066400000603,
        s = 0x4c62066400000604,
        S = 0x4c62066400000605,
    

    class quant(IntEnum):
        optional = 0x4c62066400000900,
        star = 0x4c62066400000901,
        plus = 0x4c62066400000902,
    

