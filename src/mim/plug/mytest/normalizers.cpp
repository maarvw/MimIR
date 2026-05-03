#include "mim/world.h"

#include "mim/plug/mytest/mytest.h"

namespace mim::plug::mytest {

const Def* normalize_const(const Def* type, const Def*, const Def* arg) {
    auto& world = type->world();
    return world.lit(world.type_idx(arg), 42);
}

MIM_mytest_NORMALIZER_IMPL

} // namespace mim::plug::mytest
