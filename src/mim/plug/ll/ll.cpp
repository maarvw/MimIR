#include "mim/plug/ll/ll.h"

#include <fstream>
#include <string>

#include <mim/config.h>
#include <mim/phase.h>

#include "mim/plugin.h"

#include "mim/util/sys.h"

#include "mim/plug/ll/autogen.h"

namespace mim::ll {

using namespace std::string_literals;

void emit(World& world, std::ostream& ostream) {
    Emitter emitter(world, ostream);
    emitter.run();
}

int compile(World& world, std::string name) {
#ifdef _WIN32
    auto exe = name + ".exe"s;
#else
    auto exe = name;
#endif
    return compile(world, name + ".ll"s, exe);
}

int compile(World& world, std::string ll, std::string out) {
    std::ofstream ofs(ll);
    emit(world, ofs);
    ofs.close();
    auto cmd = std::format("clang \"{}\" -o \"{}\" -Wno-override-module", ll, out);
    return sys::system(cmd);
}

int compile_and_run(World& world, std::string name, std::string args) {
    if (compile(world, name) == 0) return sys::run(name, args);
    error("compilation failed");
}

namespace {
/// Pipeline phase for `%ll.emit`.
/// Writes the LLVM IR of the fully lowered world to `<world>.ll`.
class Emit : public Phase {
public:
    Emit(World& world, flags_t annex)
        : Phase(world, annex) {}

    void start() override {
        auto name = world().name() ? std::string(world().name().view()) : "a"s;
        std::ofstream ofs(name + ".ll"s);
        ll::emit(world(), ofs);
    }
};
} // namespace

} // namespace mim::ll

using namespace mim;

static void reg_stages(Flags2Stages& stages) { Stage::hook<plug::ll::emit, ll::Emit>(stages); }

extern "C" MIM_EXPORT Plugin mim_get_plugin() { return {"ll", MIM_VERSION, nullptr, reg_stages}; }
