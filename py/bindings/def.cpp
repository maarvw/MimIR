#include <pybind11/detail/common.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <mim/def.h>
#include <mim/driver.h>
#include <mim/world.h>

namespace py = pybind11;

namespace mim {

void init_def(py::module_& m) {
    // clang-format off
    py::class_<mim::Def, std::unique_ptr<mim::Def, py::nodelete>>(m, "Def")
        .def("world",  [](mim::Def& d) -> py::object { return py::cast(&d.world(),          py::return_value_policy::reference_internal, py::cast(&d)); })
        .def("driver", [](mim::Def& d) -> py::object { return py::cast(&d.world().driver(), py::return_value_policy::reference_internal, py::cast(&d)); })
        .def("var",  py::overload_cast<            >(&mim::Def::var             ), py::return_value_policy::reference_internal)
        .def("proj", py::overload_cast<nat_t, nat_t>(&mim::Def::proj, py::const_), py::return_value_policy::reference_internal)
        .def("proj", py::overload_cast<       nat_t>(&mim::Def::proj, py::const_), py::return_value_policy::reference_internal)
        .def("__getitem__", [](const mim::Def& d, py::object index) {
            if (py::isinstance<py::int_>(index)) return d.proj(index.cast<nat_t>());
            if (py::isinstance<py::tuple>(index)) {
                auto tuple = index.cast<py::tuple>();
                if (tuple.size() == 2) return d.proj(tuple[0].cast<nat_t>(), tuple[1].cast<nat_t>());
                throw py::index_error("tuple index must have exactly 2 elements (arity, index)");
            }
            throw py::index_error("index must be int or (arity, index)");
        }, py::return_value_policy::reference_internal)
        .def("dump", py::overload_cast<            >(&mim::Def::dump, py::const_), py::return_value_policy::reference_internal)
        .def("externalize", &mim::Def::externalize)
        .def("set", static_cast<mim::Def* (mim::Def::*)(std::string)>(&mim::Def::set), py::return_value_policy::reference_internal)
        .def("num_projs", &mim::Def::num_projs, py::return_value_policy::reference)
        // clang-format on
        .def("projs", [](mim::Def& d, nat_t a) {
            std::vector<const mim::Def*> ret_vec;
            ret_vec.reserve(a);

            for (auto* proj : d.projs()) {
                if (ret_vec.size() == a) break;
                ret_vec.push_back(proj);
            }

            return ret_vec;
        });
}

void init_lit(py::module_& m) { py::class_<mim::Lit, mim::Def>(m, "Lit"); }

} // namespace mim
