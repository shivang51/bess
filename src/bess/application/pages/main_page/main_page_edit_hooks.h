#pragma once

#include "common/bess_api.h"
#include "project_session/edit_hooks.h"
#include <memory>

namespace Bess::Svc {
    class SvcConnection;
}

namespace Bess::Pages {
    BESS_API std::shared_ptr<const Edit::Hooks>
    makeEditHooks(std::shared_ptr<Svc::SvcConnection> conn);
}
