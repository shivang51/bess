#include "g_app_context.h"

namespace Bess {
    void GAppContext::init() {
        if (m_initialized) {
            return;
        }

        for (auto &[_, subsystem] : m_subSystems) {
            subsystem->onPreInit();
        }

        for (auto &[_, subsystem] : m_subSystems) {
            subsystem->onInit();
        }

        for (auto &[_, subsystem] : m_subSystems) {
            subsystem->onPostInit();
        }

        m_initialized = true;
    }

    void GAppContext::update(TimeMs dt) {
        for (auto &[_, subsystem] : m_subSystems) {
            subsystem->onUpdate(dt);
        }
    }

    void GAppContext::destroy() {
        if (!m_initialized || m_destroyed) {
            return;
        }

        for (auto &[_, subsystem] : m_subSystems) {
            subsystem->onShutdown();
        }

        for (auto &[_, subsystem] : m_subSystems) {
            subsystem->onDestroy();
        }

        m_subSystems.clear();
        m_destroyed = true;
    }
} // namespace Bess
