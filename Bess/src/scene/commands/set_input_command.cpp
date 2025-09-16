#include "scene/commands/set_input_command.h"
#include "scene/scene.h"

namespace Bess::Canvas::Commands {
    SetInputCommand::SetInputCommand(const UUID &compId, bool state)
        : SimEngine::Commands::SetInputCommand(compId, state) {}

    bool SetInputCommand::execute() {
        // TODO: implement
        return true;
    }

    std::any SetInputCommand::undo() {
        // TODO: implement
        return {};
    }

    std::any SetInputCommand::getResult() {
        // TODO: implement
        return {};
    }
} // namespace Bess::Canvas::Commands
