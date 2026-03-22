#include "copy_paste_service.h"

namespace Bess::Svc::CopyPaste {
    Context &Context::instance() {
        static Context context;
        return context;
    }

    void Context::addEntity(const CopiedEntity &entity) {
        m_entities.push_back(entity);
    }

    void Context::clear() {
        m_entities.clear();
    }

    void Context::calcCenter() {
        if (m_entities.empty())
            return;
        if (m_entities.size() == 1)
            m_center = m_entities.front().pos;

        glm::vec2 sumPos{0.f, 0.f};

        for (const auto &ent : m_entities) {
            sumPos += ent.pos;
        }

        m_center = sumPos / (float)m_entities.size();
    }

    void Context::init() {
        clear();
    }

    void Context::destroy() {
        clear();
    }
} // namespace Bess::Svc::CopyPaste
