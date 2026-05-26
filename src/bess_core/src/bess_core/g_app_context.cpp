#include "bess_core/g_app_context.h"

namespace Bess {
    GAppContext &GAppContext::getInstance() {
        static GAppContext instance;
        return instance;
    }
} // namespace Bess