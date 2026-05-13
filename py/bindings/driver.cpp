#include <fe/sym.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/vector.h>

#include <mim/driver.h>

#include <mim/ast/ast.h>

namespace nb = nanobind;

namespace mim {

void init_driver(nb::module_& m) {
    nb::class_<mim::Driver, fe::SymPool>(m, "Driver")
        .def(nb::init<>())
        .def("world", &mim::Driver::world, nb::rv_policy::reference_internal)
        // clang-format off
        .def("add_import", [](mim::Driver& driver, std::string path, std::string str) { return driver.imports().add(fs::path(path), driver.sym(str), ast::Tok::Tag::K_import); }, nb::rv_policy::reference_internal)
        .def("add_plugin", [](mim::Driver& driver, std::string path, std::string str) { return driver.imports().add(fs::path(path), driver.sym(str), ast::Tok::Tag::K_plugin); }, nb::rv_policy::reference_internal)
        // clang-format on
        .def("add_search_path", &mim::Driver::add_search_path)
        .def("log", &mim::Driver::log, nb::rv_policy::reference_internal)
        .def("backend",
             [](mim::Driver& d, std::string backend, std::string output_file_name, mim::World& world) {
                 std::ofstream ofs(output_file_name);
                 d.backend(backend)(world, ofs);
                 ofs.close();
             })
        .def("load_plugins",
             [](mim::Driver& d, std::vector<std::string> plugins) { ast::load_plugins(d.world(), plugins); });
}

} // namespace mim
