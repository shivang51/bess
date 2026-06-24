#pragma once

/// File contains code for laying out UI nodes,
/// somethig like a CSS box model, with padding, margin, and alignment.

#include "common/bess_api.h"
#include "common/bess_uuid.h"
#include "common/class_helpers.h"
#include "common/types.h"
#include "ext/vector_float2.hpp"
#include "ext/vector_float4.hpp"
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

    ;

    class BESS_API UINodeRegistry {
      public:
        UINodeRegistry() = default;

        UINode *addNode(const UINode &node);

        void removeNode(const UUID &id);

        UINode *getNode(const UUID &id);
        const UINode *getNode(const UUID &id) const;

        using NodesMap = HashMap<UUID, UINode>;
        MAKE_GETTER(NodesMap, AllNodes, m_nodes);

      private:
        NodesMap m_nodes;
    };

    // UINode represents a node in the UI layout tree.
    //
    // The layout model follows a CSS-like box contract:
    // - m_size / DrawSize describe the rendered box.
    // - padding is inside the rendered box.
    // - margin is outside the rendered box and contributes to CachedSize.
    // - CachedPos is the rendered box center used by renderer draw calls.
    //
    // Id is auto generated and unique for each node.
    class BESS_API UINode {
      public:
        UINode() = default;
        UINode(const UUID &id);

        void setPosDirty(bool dirty = true);

        void setSizeDirty(bool dirty = true);

        MAKE_GETTER_SETTER(UUID, Id, m_id);
        MAKE_GETTER_SETTER_WC(glm::vec2, Pos, m_pos, setPosDirty);
        MAKE_GETTER_SETTER_WC(Unit, PosUnit, m_posUnit, setPosDirty);
        MAKE_GETTER_SETTER_WC(glm::vec2, Size, m_size, setSizeDirty);
        MAKE_GETTER_SETTER_WC(Unit, SizeUnit, m_sizeUnit, setSizeDirty);
        MAKE_GETTER_SETTER_WC(SizeContraint,
                              SizeConstraint,
                              m_sizeConstraint,
                              setSizeDirty);
        MAKE_GETTER_SETTER(bool, PosDirty, m_posDirty);
        MAKE_GETTER_SETTER(bool, SizeDirty, m_sizeDirty);
        MAKE_GETTER_SETTER_WC(glm::vec4, Padding, m_padding, setSizeDirty);
        MAKE_GETTER_SETTER_WC(glm::vec4, Margin, m_margin, setSizeDirty);
        MAKE_GETTER_SETTER_WC(glm::vec2, MinSize, m_minSize, setSizeDirty);
        MAKE_GETTER_SETTER_WC(glm::vec2, MaxSize, m_maxSize, setSizeDirty);
        MAKE_GETTER(glm::vec2, CachedPos, m_cachedPos);
        MAKE_GETTER(glm::vec2, CachedSize, m_cachedSize);
        MAKE_GETTER(glm::vec2, DrawSize, m_drawSize);
        MAKE_GETTER(float, CachedZVal, m_cachedZVal);
        MAKE_GETTER(UUID, ParentId, m_parentId);

        const LayoutDirection &getDirection() const;

        void setDirection(const LayoutDirection &direction);

        LayoutDirection &getDirection();

        const LayoutAlignment &getAlignment() const;

        void setAlignment(const LayoutAlignment &alignment);

        LayoutAlignment &getAlignment();

        const PosMode &getPosMode() const;

        void setPosMode(const PosMode &posMode);

        PosMode &getPosMode();

        const OrderedSet<UUID> &getChildren() const;

        void setChildren(const OrderedSet<UUID> &children);

        OrderedSet<UUID> &getChildren();

        const float &getZVal() const;

        void setZVal(const float &zVal);

        float &getZVal();

        void addChild(UINode *node);

        void removeChild(UINode *node);

        void clearChildren();

        glm::vec2 measure(UINodeRegistry &registry, const UUID &parentId);

        void layout(UINodeRegistry &registry, const UUID &parentId);

      private:
        glm::vec2 measure(UINodeRegistry &registry,
                          const UINode *parentNode,
                          HashSet<UUID> &activeNodes);
        void layout(UINodeRegistry &registry,
                    const UINode *parentNode,
                    float parentZVal,
                    HashSet<UUID> &activeNodes);

        glm::vec2 resolveSize(const UINode *parentNode) const;
        glm::vec2 resolvePos(const UINode *parentNode) const;
        glm::vec2 contentSize() const;
        glm::vec2 marginSize() const;
        glm::vec2 paddingSize() const;

        UUID m_id;

        glm::vec2 m_pos{0};
        float m_zVal = 0.0f;
        float m_cachedZVal = 0.0f;
        Unit m_posUnit = Unit::pixel;

        glm::vec2 m_size{0};
        Unit m_sizeUnit = Unit::pixel;
        glm::vec2 m_minSize{-1};
        glm::vec2 m_maxSize{-1};

        glm::vec4 m_padding = glm::vec4(0.0f); // top, right, bottom, left
        glm::vec4 m_margin = glm::vec4(0.0f);  // top, right, bottom, left

        LayoutDirection m_direction = LayoutDirection::horizontal;
        LayoutAlignment m_alignment = LayoutAlignment::start;
        PosMode m_posMode = PosMode::relative;
        SizeContraint m_sizeConstraint = SizeContraint::fixed;
        OrderedSet<UUID> m_children;
        bool m_posDirty = true;
        bool m_sizeDirty = true;

        glm::vec2 m_cachedPos{0};
        glm::vec2 m_cachedSize{-1};
        glm::vec2 m_drawSize{0};

        UUID m_parentId = UUID::null;
    };
} // namespace Bess::Canvas::UI
