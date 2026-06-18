#pragma once

#include "common/bess_api.h"
#include "common/bess_assert.h"
#include "common/bess_uuid.h"
#include "common/class_helpers.h"
#include "common/logger.h"
#include "common/types.h"
#include "ext/vector_float2.hpp"
#include <cstdint>

namespace Bess::Canvas::UI {
    enum class Unit : uint8_t {
        pixel,
        relative, // ranging from 0 to 1, relative to parent size, 0.5 means 50%
                  // of parent size
    };

    enum class LayoutDirection : uint8_t { horizontal, vertical };

    enum class LayoutAlignment : uint8_t { start, center, end };

    enum class PosMode : uint8_t { absolute, relative };

    enum class SizeContraint : uint8_t { fixed, wrap_content };
    class UINode;

    struct UINodeIdHash {
        std::size_t operator()(const UUID &id) const {
            return (uint64_t)id;
        }
    };

    class BESS_API UINodeRegistry {
      public:
        UINodeRegistry() = default;

        void addNode(const UINode &node);

        void removeNode(const UUID &id);

        UINode *getNode(const UUID &id);

      private:
        HashMap<UUID, UINode> m_nodes;
    };

    // UINode represents a node in the UI layout tree.
    // Id is auto generated and unique for each node.
    class BESS_API UINode {
      public:
        UINode() = default;

        void setPosDirty(bool dirty = true) {
            m_posDirty = dirty;
        }

        void setSizeDirty(bool dirty = true) {
            m_sizeDirty = dirty;
        }

        MAKE_GETTER_SETTER(UUID, Id, m_id);
        MAKE_GETTER_SETTER_WC(glm::vec2, Pos, m_pos, setPosDirty);
        MAKE_GETTER_SETTER_WC(Unit, PosUnit, m_posUnit, setPosDirty);
        MAKE_GETTER_SETTER_WC(glm::vec2, Size, m_size, setSizeDirty);
        MAKE_GETTER_SETTER_WC(Unit, SizeUnit, m_sizeUnit, setSizeDirty);
        MAKE_GETTER_SETTER(LayoutDirection, Direction, m_direction);
        MAKE_GETTER_SETTER(LayoutAlignment, Alignment, m_alignment);
        MAKE_GETTER_SETTER(PosMode, PosMode, m_posMode);
        MAKE_GETTER_SETTER_WC(SizeContraint,
                              SizeConstraint,
                              m_sizeConstraint,
                              setSizeDirty);
        MAKE_GETTER_SETTER(OrderedSet<UUID>, Children, m_children);
        MAKE_GETTER_SETTER(bool, PosDirty, m_posDirty);
        MAKE_GETTER_SETTER(bool, SizeDirty, m_sizeDirty);
        MAKE_GETTER_SETTER(glm::vec2, CachedPos, m_cachedPos);
        MAKE_GETTER_SETTER(glm::vec2, CachedSize, m_cachedSize);
        MAKE_GETTER_SETTER(float, ZVal, zVal);
        MAKE_GETTER_SETTER_WC(glm::vec4, Padding, m_padding, setSizeDirty);
        MAKE_GETTER_SETTER_WC(glm::vec4, Margin, m_margin, setSizeDirty);
        MAKE_GETTER_SETTER_WC(glm::vec2, MinSize, m_minSize, setSizeDirty);
        MAKE_GETTER_SETTER_WC(glm::vec2, MaxSize, m_maxSize, setSizeDirty);

        void addChild(const UUID &childId) {
            m_children.insert(childId);
            m_sizeDirty = true;
        }

        void removeChild(const UUID &childId) {
            m_children.erase(childId);
            m_sizeDirty = true;
        }

        void clearChildren() {
            m_children.clear();
            m_sizeDirty = true;
        }

        glm::vec2 measure(UINodeRegistry &registry, const UUID &parentId) {
            if (!m_sizeDirty) {
                return m_cachedSize;
            }

            auto parentNode = registry.getNode(parentId);

            glm::vec2 size = {};
            if (m_sizeConstraint == SizeContraint::fixed) {
                BESS_ASSERT(m_sizeUnit != Unit::relative ||
                                parentNode != nullptr,
                            "Relative size requires a parent node.");

                if (!parentNode || m_sizeUnit == Unit::pixel) {
                    size = m_size;
                } else {
                    size = m_size * registry.getNode(parentId)->getCachedSize();
                }
            }

            glm::vec2 childrenSpan{0.f};
            for (const auto &childId : m_children) {
                UINode *childNode = registry.getNode(childId);
                if (childNode) {
                    const auto childSize = childNode->measure(registry, m_id);
                    if (m_sizeConstraint != SizeContraint::fixed) {
                        if (m_direction == LayoutDirection::horizontal) {
                            childrenSpan.x += childSize.x;
                            childrenSpan.y =
                                std::max(childrenSpan.y, childSize.y);
                        } else {
                            childrenSpan.y += childSize.y;
                            childrenSpan.x =
                                std::max(childrenSpan.x, childSize.x);
                        }
                    }
                }
            }

            if (m_sizeConstraint != SizeContraint::fixed) {
                size =
                    childrenSpan +
                    glm::vec2(m_padding.x + m_padding.z,
                              m_padding.y + m_padding.w) +
                    glm::vec2(m_margin.x + m_margin.z, m_margin.y + m_margin.w);
            }

            if (m_maxSize.x >= 0.f)
                size.x = std::min(size.x, m_maxSize.x);
            if (m_maxSize.y >= 0.f)
                size.y = std::min(size.y, m_maxSize.y);

            // Minimum size overrides the max-size if needed
            if (m_minSize.x >= 0.f)
                size.x = std::max(size.x, m_minSize.x);
            if (m_minSize.y >= 0.f)
                size.y = std::max(size.y, m_minSize.y);

            m_cachedSize = size;
            m_sizeDirty = false;
            m_posDirty = true;

            return m_cachedSize;
        }

        void layout(UINodeRegistry &registry, const UUID &parentId) {
            if (!m_posDirty) {
                BESS_TRACE("Node {} position is not dirty, skipping layout.",
                           (uint64_t)m_id);
                return;
            }

            if (parentId == UUID::null) {
                m_cachedPos = m_pos;
            }

            glm::vec2 cursor = m_cachedPos - (m_cachedSize / 2.f);
            cursor += glm::vec2(m_padding.x, m_padding.y);

            for (const auto &childId : m_children) {
                UINode *childNode = registry.getNode(childId);

                BESS_ASSERT(childNode,
                            "Child node {} not found in registry.",
                            (uint64_t)childId);

                if (childNode == nullptr)
                    continue;

                const glm::vec2 &childMargin = childNode->getMargin();
                glm::vec2 childPos = cursor + glm::vec2(childMargin);
                childNode->setCachedPos(childPos +
                                        (childNode->getCachedSize() / 2.f));

                const auto childZ = childNode->getZVal();
                childNode->setZVal(zVal + childZ);
                childNode->layout(registry, m_id);

                if (m_direction == LayoutDirection::horizontal) {
                    cursor.x += childNode->getCachedSize().x;
                } else {
                    cursor.y += childNode->getCachedSize().y;
                }
            }

            m_posDirty = false;
        }

      private:
        UUID m_id;

        glm::vec2 m_pos;
        float zVal = 0.0f;
        Unit m_posUnit = Unit::pixel;

        glm::vec2 m_size;
        Unit m_sizeUnit = Unit::pixel;
        glm::vec2 m_minSize{-1};
        glm::vec2 m_maxSize{-1};

        glm::vec4 m_padding = glm::vec4(0.0f); // top, right, bottom, left
        glm::vec4 m_margin = glm::vec4(0.0f);  // top, right, bottom, left
                                               //
        LayoutDirection m_direction = LayoutDirection::horizontal;
        LayoutAlignment m_alignment = LayoutAlignment::start;
        PosMode m_posMode = PosMode::absolute;
        SizeContraint m_sizeConstraint = SizeContraint::fixed;
        OrderedSet<UUID> m_children;
        bool m_posDirty = true;
        bool m_sizeDirty = true;

        glm::vec2 m_cachedPos{0};
        glm::vec2 m_cachedSize{-1};
    };
} // namespace Bess::Canvas::UI
