#pragma once

#include "sim_driver/sim_driver.h"
#include <memory>

namespace Bess::SimEngine {
    class DriverRegistry {
      public:
        typedef std::function<std::shared_ptr<Drivers::SimDriver>()> TCreateFn;

        static void registerDriver(const std::string &driverName,
                                   const TCreateFn &createFn);
        static void unregisterDriver(const std::string &driverName);

        typedef std::unordered_map<std::string, TCreateFn> TReg;
        static TReg &getRegistry();
    };
} // namespace Bess::SimEngine
