#pragma once

#include <mim/phase.h>

namespace mim::plug::tensor::phase {

/// Lowers the high-level tensor axioms (`broadcast_in_dim`, `product_2d`) into the
/// low-level tensor axioms (`map_reduce`, `broadcast`, …). The resulting low-level
/// axioms are then expected to be lowered to primitives by `LowerMapReduce`.
class Lower : public RWPhase {
public:
    Lower(World& world, flags_t annex)
        : RWPhase(world, annex) {}

private:
    const Def* rewrite_imm_App(const App*) final;

    const Def* lower_broadcast_in_dim(const App*);
    const Def* lower_product_2d(const App*);
};

} // namespace mim::plug::tensor::phase
