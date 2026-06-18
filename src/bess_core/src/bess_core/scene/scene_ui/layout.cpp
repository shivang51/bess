#include "bess_core/scene/scene_ui/layout.h"

namespace Bess::Canvas::UI {
    void UINodeRegistry::addNode(const UINode &node) {
        m_nodes[node.getId()] = node;
    }

    void UINodeRegistry::removeNode(const UUID &id) {
        m_nodes.erase(id);
    }

    UINode *UINodeRegistry::getNode(const UUID &id) {
        auto it = m_nodes.find(id);
        if (it != m_nodes.end()) {
            return &it->second;
        }
        return nullptr;
    }
} // namespace Bess::Canvas::UI
