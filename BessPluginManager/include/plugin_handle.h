#pragma once

#include <pybind11/pybind11.h>

namespace Bess::Plugins {
    struct Component {
        std::string name;
        int type;

        pybind11::object onUpdateFn;
    };

    class PluginHandle {
      public:
        PluginHandle() = default;
        PluginHandle(const pybind11::object &pluginObj);
        ~PluginHandle() = default;

        const pybind11::object &getPluginObject() const;

        std::vector<Component> onComponentsRegLoad() const;

      private:
        pybind11::object m_pluginObj;
    };
} // namespace Bess::Plugins
