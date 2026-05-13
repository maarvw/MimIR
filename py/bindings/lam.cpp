#include <nanobind/nanobind.h>
#include <nanobind/stl/vector.h>

#include <mim/def.h>
#include <mim/lam.h>
namespace nb = nanobind;

namespace mim {

void init_pi(nb::module_& m) {
    nb::class_<mim::Pi, mim::Def>(m, "Pi", nb::never_destruct());
    // .def(nb::init<>());
}

void init_lam(nb::module_& m) {
    nb::class_<mim::Lam, mim::Def>(m, "Lam", nb::never_destruct())
        .def("var", static_cast<const mim::Def* (mim::Lam::*)()>(&mim::Def::var), nb::rv_policy::reference_internal)
        .def(
            "app",
            [](mim::Lam& l, bool filter, mim::Def* callee, std::vector<mim::Def*> args) {
                return l.app(filter, callee, mim::Defs(args));
            },
            nb::rv_policy::reference_internal)
        .def("externalize", &mim::Lam::externalize);
}

} // namespace mim
