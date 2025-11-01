#pragma once
#include <pybind11/pybind11.h>
namespace Bess::Py {
    struct OwnedPyObject {
        pybind11::object object;
    };
} // namespace Bess::Py
