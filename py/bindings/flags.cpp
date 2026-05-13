#include <nanobind/nanobind.h>

#include <mim/flags.h>

namespace nb = nanobind;

namespace mim {

void init_flags(nb::module_& m) {
    nb::class_<mim::Flags>(m, "Flags")
        .def(nb::init<>())
        .def_rw("scalarize_threshold", &mim::Flags::scalarize_threshold);
}
} // namespace mim
