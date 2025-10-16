#pragma once

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace Bess {

    void bindPluginManager(pybind11::module &m);

    void bindPluginInterface(pybind11::module &m);

} // namespace Bess
