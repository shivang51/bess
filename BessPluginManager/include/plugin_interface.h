#pragma once

#include <string>
#include <vector>

namespace Bess {
    class PluginInterface {
      public:
        virtual ~PluginInterface() = default;

        virtual std::string getName() const = 0;
        virtual std::string getVersion() const = 0;
        virtual std::string getDescription() const = 0;

        virtual bool initialize() = 0;
        virtual void shutdown() = 0;

        virtual std::vector<std::string> getAvailableFunctions() const = 0;
        virtual bool executeFunction(const std::string &functionName) = 0;

        virtual bool isInitialized() const = 0;
    };

} // namespace Bess
