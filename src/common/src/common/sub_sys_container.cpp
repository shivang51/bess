#include "common/sub_sys_container.h"

namespace Bess {

    void ISubSysContainer::init() {
        if (m_initialized) {
            return;
        }

        for (auto &subsystem : m_subSystemsInOrder) {
            subsystem->onPreInit();
        }

        for (auto &subsystem : m_subSystemsInOrder) {
            subsystem->onInit();
        }

        for (auto &subsystem : m_subSystemsInOrder) {
            subsystem->onPostInit();
        }

        m_initialized = true;
    }

    void ISubSysContainer::beginFrame() {
        for (auto &subsystem : m_subSystemsInOrder) {
            subsystem->onBeginFrame();
        }
    }

    void ISubSysContainer::endFrame() {
        for (auto &subsystem : m_subSystemsInOrder) {
            subsystem->onEndFrame();
        }
    }

    void ISubSysContainer::draw() {
        for (auto &subsystem : m_subSystemsInOrder) {
            subsystem->onDraw();
        }
    }

    void ISubSysContainer::preDraw() {
        for (auto &subsystem : m_subSystemsInOrder) {
            subsystem->onPreDraw();
        }
    }

    void ISubSysContainer::postDraw() {
        for (auto &subsystem : std::ranges::reverse_view(m_subSystemsInOrder)) {
            subsystem->onPostDraw();
        }
    }

    void ISubSysContainer::preUpdate() {
        for (auto &subsystem : m_subSystemsInOrder) {
            subsystem->onPreUpdate();
        }
    }

    void ISubSysContainer::update(Bess::TimeMs dt) {
        for (auto &subsystem : m_subSystemsInOrder) {
            subsystem->onUpdate(dt);
        }
    }

    void ISubSysContainer::destroy() {

        if (!m_initialized || m_destroyed) {
            return;
        }

        for (auto &subsystem : std::ranges::reverse_view(m_subSystemsInOrder)) {
            subsystem->onShutdown();
        }

        for (auto &subsystem : std::ranges::reverse_view(m_subSystemsInOrder)) {
            subsystem->onDestroy();
        }

        m_subSystems.clear();
        m_destroyed = true;
    }

} // namespace Bess
