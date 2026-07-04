#pragma once

/// File contains code for laying out UI nodes,
/// something like a CSS box model, with padding, margin, and alignment.

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

    class BESS_API UINodeRegistry {
      public:
        DEFAULT_CONTRS(UINodeRegistry)

        UINode *addNode(const UINode &node);
        UINode *addNode(const UUID &nodeId);

        void removeNode(const UUID &id);

        UINode *getNode(const UUID &id);
        const UINode *getNode(const UUID &id) const;

        void clear();

        using NodesMap = NodeHashMap<UUID, UINode>;
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
        friend class UINodeRegistry;

      public:
        DEFAULT_CONTRS(UINode)
        UINode(const UUID &id);

        void setPosDirty(bool dirty = true);

        void setSizeDirty(bool dirty = true);

        MAKE_GETTER_SETTER(UUID, Id, m_id);
        const glm::vec2 &getPos() const;
        void setPos(const glm::vec2 &pos);
        glm::vec2 &getPos();

        const Unit &getPosUnit() const;
        void setPosUnit(const Unit &posUnit);
        Unit &getPosUnit();

        const glm::vec2 &getSize() const;
        void setSize(const glm::vec2 &size);
        glm::vec2 &getSize();

        const Unit &getSizeUnit() const;
        void setSizeUnit(const Unit &sizeUnit);
        Unit &getSizeUnit();

        const SizeContraint &getSizeConstraint() const;
        void setSizeConstraint(const SizeContraint &sizeConstraint);
        SizeContraint &getSizeConstraint();

        bool getPosDirty() const;
        bool getSizeDirty() const;

        const glm::vec4 &getPadding() const;

        // top, right, bottom, left
        void setPadding(const glm::vec4 &padding);

        // top, right, bottom, left
        glm::vec4 &getPadding();

        const glm::vec4 &getMargin() const;

        // top, right, bottom, left
        void setMargin(const glm::vec4 &margin);

        // top, right, bottom, left
        glm::vec4 &getMargin();

        const glm::vec2 &getMinSize() const;
        void setMinSize(const glm::vec2 &minSize);
        glm::vec2 &getMinSize();

        const glm::vec2 &getMaxSize() const;
        void setMaxSize(const glm::vec2 &maxSize);
        glm::vec2 &getMaxSize();

        const glm::vec2 &getCachedPos() const;
        const glm::vec2 &getCachedSize() const;
        const glm::vec2 &getDrawSize() const;
        const float &getCachedZVal() const;
        const UUID &getParentId() const;

        const LayoutDirection &getDirection() const;

        void setDirection(const LayoutDirection &direction);

        LayoutDirection &getDirection();

        // Main axis follows Direction: x for horizontal, y for vertical.
        // This is equivalent to CSS flexbox justify-content for the supported
        // start/center/end values.
        const LayoutAlignment &getMainAxisAlignment() const;

        void setMainAxisAlignment(const LayoutAlignment &alignment);

        LayoutAlignment &getMainAxisAlignment();

        // Cross axis is perpendicular to Direction. This is equivalent to CSS
        // flexbox align-items for the supported start/center/end values.
        const LayoutAlignment &getCrossAxisAlignment() const;

        void setCrossAxisAlignment(const LayoutAlignment &alignment);

        LayoutAlignment &getCrossAxisAlignment();

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

        glm::vec3 getDrawPos() const;

      private:
        glm::vec2 measure(UINodeRegistry &registry,
                          const UINode *parentNode,
                          HashSet<UUID> &activeNodes);
        void layout(UINodeRegistry &registry,
                    const UINode *parentNode,
                    float parentZVal,
                    HashSet<UUID> &activeNodes);

        void attachRegistry(UINodeRegistry *registry);
        void propagateSizeDirtyToAncestors();
        void propagatePosDirtyToAncestors();

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
        LayoutAlignment m_mainAxisAlignment = LayoutAlignment::start;
        LayoutAlignment m_crossAxisAlignment = LayoutAlignment::start;
        PosMode m_posMode = PosMode::relative;
        SizeContraint m_sizeConstraint = SizeContraint::wrap_content;
        OrderedSet<UUID> m_children;
        bool m_posDirty = true;
        bool m_sizeDirty = true;

        glm::vec2 m_cachedPos{0};
        glm::vec2 m_cachedSize{-1};
        glm::vec2 m_drawSize{0};

        UUID m_parentId = UUID::null;
        UINodeRegistry *m_registry = nullptr;
    };
} // namespace Bess::Canvas::UI
