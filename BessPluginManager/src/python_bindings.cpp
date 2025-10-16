#include "python_bindings.h"
#include "plugin_manager.h"
#include "plugin_interface.h"
#include <pybind11/pybind11.h>

namespace Bess {

void bindPluginManager(pybind11::module& m) {
    pybind11::class_<PluginManager>(m, "PluginManager")
        .def(pybind11::init<>())
        .def("load_plugin", &PluginManager::loadPlugin)
        .def("unload_plugin", &PluginManager::unloadPlugin)
        .def("unload_all_plugins", &PluginManager::unloadAllPlugins)
        .def("get_loaded_plugins", &PluginManager::getLoadedPlugins)
        .def("is_plugin_loaded", &PluginManager::isPluginLoaded)
        .def("get_plugin", &PluginManager::getPlugin)
        .def("execute_plugin_function", &PluginManager::executePluginFunction);
}

void bindPluginInterface(pybind11::module& m) {
    pybind11::class_<PluginInterface, std::shared_ptr<PluginInterface>>(m, "PluginInterface")
        .def("get_name", &PluginInterface::getName)
        .def("get_version", &PluginInterface::getVersion)
        .def("get_description", &PluginInterface::getDescription)
        .def("initialize", &PluginInterface::initialize)
        .def("shutdown", &PluginInterface::shutdown)
        .def("get_available_functions", &PluginInterface::getAvailableFunctions)
        .def("execute_function", &PluginInterface::executeFunction)
        .def("is_initialized", &PluginInterface::isInitialized);
}

} // namespace Bess
