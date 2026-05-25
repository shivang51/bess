#include "common/sub_sys_container.h"

namespace Bess {

    void ISubSysContainer::init() {
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

    void ISubSysContainer::beginFrame() {
        for (auto &[_, subsystem] : m_subSystems) {
            subsystem->onBeginFrame();
        }
    }

    void ISubSysContainer::endFrame() {
        for (auto &[_, subsystem] : m_subSystems) {
            subsystem->onEndFrame();
        }
    }

    void ISubSysContainer::draw() {
        for (auto &[_, subsystem] : m_subSystems) {
            subsystem->onDraw();
        }
    }

    void ISubSysContainer::preDraw() {
        for (auto &[_, subsystem] : m_subSystems) {
            subsystem->onPreDraw();
        }
    }

    void ISubSysContainer::postDraw() {
        for (auto &[_, subsystem] : m_subSystems) {
            subsystem->onPostDraw();
        }
    }

    void ISubSysContainer::preUpdate() {
        for (auto &[_, subsystem] : m_subSystems) {
            subsystem->onPreUpdate();
        }
    }

    void ISubSysContainer::update(Bess::TimeMs dt) {
        for (auto &[_, subsystem] : m_subSystems) {
            subsystem->onUpdate(dt);
        }
    }

    void ISubSysContainer::destroy() {

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
