#pragma once

#include "common/bess_api.h"
#include "bess_core/commands/scene_component_command_hooks.h"
#include <memory>

namespace Bess::Pages {
    BESS_API std::shared_ptr<const Cmd::SceneComponentCommandHooks>
    createMainPageCommandHooks();
}
