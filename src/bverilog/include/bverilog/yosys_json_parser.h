#pragma once

#include "bverilog/types.h"
#include <json/value.h>
#include <optional>
#include <string>

namespace Bess::Verilog {
    BESS_API Design parseDesignFromYosysJson(const Json::Value &root,
                                             const std::optional<std::string> &explicitTopModule = std::nullopt);
} // namespace Bess::Verilog
