#include <cstdint>

#include <fe/sym.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/vector.h>

#include <mim/def.h>
#include <mim/lam.h>
#include <mim/world.h>

#include <mim/pass/optimize.h>

namespace nb = nanobind;

namespace mim {

void init_world(nb::module_& m) {
    nb::class_<mim::World>(m, "World", nb::never_destruct())
        .def("write", nb::overload_cast<>(&mim::World::write))
        .def("top_nat", &mim::World::top_nat, nb::rv_policy::reference_internal)
        .def("type_bool", &mim::World::type_bool, nb::rv_policy::reference_internal)
        .def("type_i8", &mim::World::type_i8, nb::rv_policy::reference_internal)
        .def("lit_nat_0", &mim::World::lit_nat_0, nb::rv_policy::reference_internal)
        .def("lit_bool", &mim::World::lit_bool, nb::rv_policy::reference_internal)
        .def("lit_ff", &mim::World::lit_ff, nb::rv_policy::reference_internal)
        .def("lit_tt", &mim::World::lit_tt, nb::rv_policy::reference_internal)
        .def("lit", &mim::World::lit, nb::rv_policy::reference_internal)
        .def("type_idx", static_cast<const mim::Def* (World::*)(const mim::Def*)>(&mim::World::type_idx),
             nb::rv_policy::reference_internal)
        .def("type_idx", static_cast<const mim::Def* (World::*)(nat_t)>(&mim::World::type_idx),
             nb::rv_policy::reference_internal)
        .def(
            "cn", [](mim::World& w, std::vector<Def*> dom) { return w.cn(Defs(dom)); },
            nb::rv_policy::reference_internal)
        .def("lit_i8", static_cast<const mim::Lit* (World::*)(u8)>(&mim::World::lit_i8),
             nb::rv_policy::reference_internal)
        .def(
            "implicit_app",
            [](mim::World& w, const mim::Def* callee, std::vector<Def*> args) {
                return w.implicit_app(callee, Defs(args));
            },
            nb::rv_policy::reference_internal)
        .def("type_i32", &mim::World::type_i32, nb::rv_policy::reference_internal)
        .def("sym", static_cast<fe::Sym (World::*)(std::string_view)>(&mim::World::sym))
        .def(
            "mut_fun2",
            [](mim::World& w, std::vector<mim::Def*> dom, std::vector<mim::Def*> codom) {
                return w.mut_fun(mim::Defs(dom), mim::Defs(codom));
            },
            nb::rv_policy::reference_internal)
        .def(
            "mut_fun",
            [](mim::World& w, const mim::Def* dom, std::vector<mim::Def*> codom) { return w.mut_fun(dom, codom); },
            nb::rv_policy::reference_internal)
        .def(
            "arr", [](mim::World& w, Def* arity, Def* body) { return w.arr(arity, body); },
            nb::rv_policy::reference_internal)
        .def("optimize", [](mim::World& w) { mim::optimize(w); })
        .def("dot", static_cast<void (World::*)(const char*, bool, bool) const>(&mim::World::dot))
        .def(
            "mut_con", [](mim::World& w, std::vector<Def*> domains) { return w.mut_con(Defs(domains)); },
            nb::rv_policy::reference_internal)
        .def("annex", [](mim::World& w, uint64_t id) { return w.annex(id); }, nb::rv_policy::reference_internal);
}

} // namespace mim
