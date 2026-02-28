#include "command.h"

namespace Bess::Cmd {
    std::string Command::getName() const {
        return m_name;
    }

    bool Command::mergeWith(const Command *other) {
        return false;
    }

    bool Command::canMergeWith(const Command *other) const {
        return false;
    }
} // namespace Bess::Cmd
