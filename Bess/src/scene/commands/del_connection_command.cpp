#include "scene/commands/del_connection_command.h"
#include "scene/scene.h"

namespace Bess::Canvas::Commands {
    DelConnectionCommand::DelConnectionCommand(const UUID &src, int srcPin, SimEngine::PinType srcType,
                                             const UUID &dst, int dstPin, SimEngine::PinType dstType)
        : SimEngine::Commands::DelConnectionCommand(src, srcPin, srcType, dst, dstPin, dstType) {}

    bool DelConnectionCommand::execute() {
        // TODO: implement
        return true;
    }

    std::any DelConnectionCommand::undo() {
        // TODO: implement
        return {};
    }

    std::any DelConnectionCommand::getResult() {
        // TODO: implement
        return {};
    }
} // namespace Bess::Canvas::Commands
