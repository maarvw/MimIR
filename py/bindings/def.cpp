#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include <mim/def.h>
#include <mim/driver.h>
#include <mim/world.h>

namespace nb = nanobind;

namespace mim {

void init_def(nb::module_& m) {
    // clang-format off
    nb::class_<mim::Def>(m, "Def", nb::never_destruct())
        .def("world",  [](mim::Def& d) { return &d.world(); }, nb::rv_policy::reference_internal)
        .def("driver", [](mim::Def& d) { return &d.world().driver(); }, nb::rv_policy::reference_internal)
        .def("var",  nb::overload_cast<            >(&mim::Def::var             ), nb::rv_policy::reference_internal)
        .def("proj", nb::overload_cast<nat_t, nat_t>(&mim::Def::proj, nb::const_), nb::rv_policy::reference_internal)
        .def("proj", nb::overload_cast<       nat_t>(&mim::Def::proj, nb::const_), nb::rv_policy::reference_internal)
        .def("dump", nb::overload_cast<            >(&mim::Def::dump, nb::const_))
        .def("__getitem__", [](const mim::Def& d, nb::object index) -> const mim::Def* {
            if (nb::isinstance<nb::int_ >(index)) return d.proj(nb::cast<nat_t>(index));
            if (nb::isinstance<nb::tuple>(index)) {
                auto tuple = nb::borrow<nb::tuple>(index);
                if (tuple.size() == 2) return d.proj(nb::cast<nat_t>(tuple[0]), nb::cast<nat_t>(tuple[1]));
                throw nb::index_error("tuple index must have exactly 2 elements (arity, index)");
            }
            throw nb::index_error("index must be int or (arity, index)");
        }, nb::rv_policy::reference_internal)
        .def("externalize", &mim::Def::externalize)
        .def("set", static_cast<mim::Def* (mim::Def::*)(std::string)>(&mim::Def::set), nb::rv_policy::reference_internal)
        .def("num_projs", &mim::Def::num_projs)
        // clang-format on
        .def(
            "projs",
            [](mim::Def& d, nat_t a) {
                std::vector<const mim::Def*> ret_vec;
                ret_vec.reserve(a);

                for (auto* proj : d.projs()) {
                    if (ret_vec.size() == a) break;
                    ret_vec.push_back(proj);
                }

                return ret_vec;
            },
            nb::rv_policy::reference_internal);
}

void init_lit(nb::module_& m) { nb::class_<mim::Lit, mim::Def>(m, "Lit", nb::never_destruct()); }

} // namespace mim
