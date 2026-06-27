#pragma once

#include "bess_core/commands/scene_component_command_hooks.h"
#include <memory>

namespace Bess::Pages {
    std::shared_ptr<const Cmd::SceneComponentCommandHooks>
    createMainPageCommandHooks();
}
