#include <nanobind/nanobind.h>

#include <mim/util/log.h>

namespace nb = nanobind;

namespace mim {

void init_log(nb::module_& m) {
    nb::enum_<mim::Log::Level>(m, "Level")
        .value("Error", mim::Log::Level::Error)
        .value("Warn", mim::Log::Level::Warn)
        .value("Info", mim::Log::Level::Info)
        .value("Verbose", mim::Log::Level::Verbose)
        .value("Debug", mim::Log::Level::Debug)
        .export_values();

    nb::class_<mim::Log>(m, "Log")
        .def(nb::init<mim::Flags&>())
        .def("set", static_cast<Log& (Log::*)(Log::Level)>(&mim::Log::set), nb::rv_policy::reference)
        .def("set_stdout", [](mim::Log& self) -> mim::Log& { return self.set(&std::cout); }, nb::rv_policy::reference);
}

} // namespace mim
