#include "driver_registry.h"

namespace Bess::SimEngine {

    void DriverRegistry::registerDriver(const std::string &driverName,
                                        const TCreateFn &createFn) {
        getRegistry()[driverName] = createFn;
    }

    void DriverRegistry::unregisterDriver(const std::string &driverName) {
        getRegistry().erase(driverName);
    }

    DriverRegistry::TReg &DriverRegistry::getRegistry() {
        static TReg registry;
        return registry;
    }
} // namespace Bess::SimEngine
