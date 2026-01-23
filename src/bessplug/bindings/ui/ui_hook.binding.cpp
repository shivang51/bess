#include "ui/ui_hook.h"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

class PyUIHook : public Bess::UI::Hook::UIHook,
                 public py::trampoline_self_life_support {
  public:
    using Bess::UI::Hook::UIHook::UIHook;

    void setPropertyDescriptors(const std::vector<Bess::UI::Hook::PropertyDesc> &descs) override {
        PYBIND11_OVERLOAD_NAME(
            void,
            Bess::UI::Hook::UIHook,
            "set_properties",
            setPropertyDescriptors,
            descs);
    }

    const std::vector<Bess::UI::Hook::PropertyDesc> &getPropertyDescriptors() const override {
        PYBIND11_OVERLOAD_NAME(
            const std::vector<Bess::UI::Hook::PropertyDesc> &,
            Bess::UI::Hook::UIHook,
            "get_properties",
            getPropertyDescriptors, );
    }

    void addPropertyDescriptor(const Bess::UI::Hook::PropertyDesc &desc) override {
        PYBIND11_OVERLOAD_NAME(
            void,
            Bess::UI::Hook::UIHook,
            "add_property",
            addPropertyDescriptor,
            desc);
    }
};

void bind_ui_hook(py::module &m) {
    py::enum_<Bess::UI::Hook::PropertyDescType>(m, "PropertyDescType")
        .value("bool_t", Bess::UI::Hook::PropertyDescType::bool_t)
        .value("int_t", Bess::UI::Hook::PropertyDescType::int_t)
        .value("float_t", Bess::UI::Hook::PropertyDescType::float_t)
        .value("string_t", Bess::UI::Hook::PropertyDescType::string_t)
        .value("enum_t", Bess::UI::Hook::PropertyDescType::enum_t)
        .export_values();

    py::class_<Bess::UI::Hook::NumericConstraints>(m, "NumericConstraints")
        .def(py::init<>())
        .def_readwrite("min", &Bess::UI::Hook::NumericConstraints::min)
        .def_readwrite("max", &Bess::UI::Hook::NumericConstraints::max)
        .def_readwrite("step", &Bess::UI::Hook::NumericConstraints::step);

    py::class_<Bess::UI::Hook::EnumConstraints>(m, "EnumConstraints")
        .def(py::init<>())
        .def_readwrite("labels", &Bess::UI::Hook::EnumConstraints::labels)
        .def_readwrite("values", &Bess::UI::Hook::EnumConstraints::values);

    py::class_<Bess::UI::Hook::PropertyDesc>(m, "PropertyDesc")
        .def(py::init<>())
        .def_readwrite("name", &Bess::UI::Hook::PropertyDesc::name)
        .def_readwrite("type", &Bess::UI::Hook::PropertyDesc::type)
        .def_readwrite("default_value", &Bess::UI::Hook::PropertyDesc::defaultValue)
        .def_readwrite("constraints", &Bess::UI::Hook::PropertyDesc::constraints);

    py::class_<Bess::UI::Hook::PropertyBinding>(m, "PropertyBinding")
        .def(py::init<>())
        .def_readwrite("getter", &Bess::UI::Hook::PropertyBinding::getter)
        .def_readwrite("setter", &Bess::UI::Hook::PropertyBinding::setter);

    py::class_<Bess::UI::Hook::UIHook, PyUIHook, py::smart_holder>(m, "UIHook")
        .def(py::init<>())
        .def("set_properties", &Bess::UI::Hook::UIHook::setPropertyDescriptors)
        .def("get_properties", &Bess::UI::Hook::UIHook::getPropertyDescriptors,
             py::return_value_policy::reference)
        .def("add_property", &Bess::UI::Hook::UIHook::addPropertyDescriptor);
}
