#pragma once

/// File contains code for laying out UI nodes,
/// something like a CSS box model, with padding, margin, and alignment.
#include "bess_core/style/bess_theme.h"
#include "common/bess_api.h"
#include "common/bess_uuid.h"
#include "common/class_helpers.h"
#include "common/types.h"
#include "ext/vector_float2.hpp"
#include "yoga/Yoga.h"
#include "yoga/config/Config.h"
#include <cstdint>

namespace Bess::Canvas::UI {
    enum class Unit : uint8_t {
        pixel,
        relative, // ranging from 0 to 1, relative to parent size, 0.5 means 50%
                  // of parent size
    };

    enum class LayoutDirection : uint8_t {
        horizontal,
        vertical,
        horizontalReverse,
        verticalReverse
    };

    enum class LayoutAlignment : uint8_t { start, center, end };

    enum class LayoutSelfAlignment : uint8_t {
        auto_,
        start,
        center,
        end,
        stretch
    };

    enum class LayoutSizeMode : uint8_t {
        auto_,
        point,
        percent,
        fitContent,
        maxContent,
        stretch
    };

    enum class DrawPivot : uint8_t {
        topLeft,
        topCenter,
        center,
        bottomLeft,
        bottomCenter
    };

    enum class PosMode : uint8_t { absolute, relative };

    class UINode;

    class BESS_API UINodeRegistry {
      public:
        UINodeRegistry();
        UINodeRegistry(const UINodeRegistry &) = delete;
        UINodeRegistry(UINodeRegistry &&) = delete;
        ~UINodeRegistry();
        UINodeRegistry &operator=(const UINodeRegistry &) = delete;
        UINodeRegistry &operator=(UINodeRegistry &&) = delete;

        UINode *addNode(const UINode &node);
        UINode *addNode(const UUID &nodeId);

        void removeNode(const UUID &id);

        UINode *getNode(const UUID &id);
        const UINode *getNode(const UUID &id) const;

        void clear();

        using NodesMap = NodeHashMap<UUID, UINode>;
        MAKE_GETTER(NodesMap, AllNodes, m_nodes);
        MAKE_GETTER(YGConfigRef, YogaConfig, m_ygConfig);

      private:
        NodesMap m_nodes;
        YGConfigRef m_ygConfig = nullptr;
    };

    // UINode represents a node in the UI layout tree.
    //
    // The layout model follows a CSS-like box contract:
    // - DrawSize describes the rendered box.
    // - padding is inside the rendered box.
    // - margin is outside the rendered box and contributes to CachedSize.
    // - CachedPos is the rendered box center used by renderer draw calls.
    //
    // Id is auto generated and unique for each node.
    class BESS_API UINode {
        friend class UINodeRegistry;

      public:
        UINode();
        UINode(const UUID &id);
        UINode(const UINode &other);
        UINode(UINode &&other) noexcept;
        ~UINode();
        UINode &operator=(const UINode &other);
        UINode &operator=(UINode &&other) noexcept;

        const DrawPivot &getDrawPivot() const;
        void setDrawPivot(const DrawPivot &drawPivot);
        DrawPivot &getDrawPivot();

        void setPosDirty(bool dirty = true);

        void setSizeDirty(bool dirty = true);

        MAKE_GETTER_SETTER(UUID, Id, m_id);
        const glm::vec2 &getPos() const;
        void setPos(const glm::vec2 &pos);
        glm::vec2 &getPos();

        const Unit &getPosUnit() const;
        void setPosUnit(const Unit &posUnit);
        Unit &getPosUnit();

        void setWidth(float width);
        void setHeight(float height);
        void setWidthPercent(float width);
        void setHeightPercent(float height);
        void setWidthAuto();
        void setHeightAuto();
        void setWidthFitContent();
        void setHeightFitContent();
        void setWidthMaxContent();
        void setHeightMaxContent();
        void setWidthStretch();
        void setHeightStretch();

        bool getPosDirty() const;
        bool getSizeDirty() const;

        const Core::Style::Padding &getPadding() const;

        // top, right, bottom, left
        void setPadding(const Core::Style::Padding &padding);

        // top, right, bottom, left
        Core::Style::Padding &getPadding();

        const Core::Style::Margin &getMargin() const;

        // top, right, bottom, left
        void setMargin(const Core::Style::Margin &margin);

        // top, right, bottom, left
        Core::Style::Margin &getMargin();

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

        const LayoutSelfAlignment &getAlignSelf() const;

        void setAlignSelf(const LayoutSelfAlignment &alignment);

        LayoutSelfAlignment &getAlignSelf();

        float getFlexGrow() const;
        void setFlexGrow(float grow);

        float getFlexShrink() const;
        void setFlexShrink(float shrink);

        void setFlex(float grow, float shrink, float basis = 0.f);
        void setFlexBasis(float basis, Unit unit = Unit::pixel);
        void setFlexBasisAuto();
        void setFlexBasisFitContent();
        void setFlexBasisMaxContent();
        void setFlexBasisStretch();

        const PosMode &getPosMode() const;

        void setPosMode(const PosMode &posMode);

        PosMode &getPosMode();

        void setChildren(const OrderedSet<UUID> &children);
        OrderedSet<UUID> &getChildren();
        const OrderedSet<UUID> &getChildren() const;

        const float &getZVal() const;

        void setZVal(const float &zVal);

        float &getZVal();

        void addChild(UINode *node);

        void removeChild(UINode *node);

        void clearChildren();

        glm::vec2 measure(UINodeRegistry &registry, const UUID &parentId);

        glm::vec3 getDrawPos() const;

        YGNodeRef getYogaNode();
        YGNodeConstRef getYogaNode() const;

      private:
        enum class DimensionMode : uint8_t {
            auto_,
            point,
            percent,
            fitContent,
            maxContent,
            stretch
        };

        enum class FlexBasisMode : uint8_t {
            auto_,
            point,
            percent,
            fitContent,
            maxContent,
            stretch
        };

        struct Dimension {
            DimensionMode mode = DimensionMode::fitContent;
            float value = 0.f;
        };

        void attachRegistry(UINodeRegistry *registry);
        void propagateSizeDirtyToAncestors();
        void propagatePosDirtyToAncestors();
        void rebuildYogaChildren();
        void syncLayoutFromYoga(UINodeRegistry &registry,
                                const UINode *parentNode,
                                HashSet<UUID> &activeNodes);

        void copyFrom(const UINode &other);
        void moveFrom(UINode &other) noexcept;
        void releaseYogaNode();
        void createYogaNode(YGConfigRef config = nullptr);
        void applyYogaStyle();
        void applyWidthStyle();
        void applyHeightStyle();
        void applyPositionStyle();
        void applyMinSizeStyle();
        void applyMaxSizeStyle();
        void applyFlexBasisStyle();
        void setWidthDimension(const Dimension &dimension);
        void setHeightDimension(const Dimension &dimension);

        glm::vec2 resolvePos(const UINode *parentNode) const;
        glm::vec2 contentSize() const;
        glm::vec2 marginSize() const;
        glm::vec2 paddingSize() const;

        UUID m_id;

        glm::vec2 m_pos{0};
        float m_zVal = 0.0f;
        float m_cachedZVal = 0.0f;
        Unit m_posUnit = Unit::pixel;

        glm::vec2 m_minSize{-1};
        glm::vec2 m_maxSize{-1};

        Core::Style::Padding m_padding;
        Core::Style::Margin m_margin;

        LayoutDirection m_direction = LayoutDirection::horizontal;
        LayoutAlignment m_mainAxisAlignment = LayoutAlignment::start;
        LayoutAlignment m_crossAxisAlignment = LayoutAlignment::start;
        LayoutSelfAlignment m_alignSelf = LayoutSelfAlignment::auto_;
        float m_flexGrow = 0.f;
        float m_flexShrink = 0.f;
        FlexBasisMode m_flexBasisMode = FlexBasisMode::auto_;
        float m_flexBasis = 0.f;
        Unit m_flexBasisUnit = Unit::pixel;
        PosMode m_posMode = PosMode::relative;
        Dimension m_width = {DimensionMode::fitContent, 0.f};
        Dimension m_height = {DimensionMode::fitContent, 0.f};
        OrderedSet<UUID> m_children;
        bool m_posDirty = true;
        bool m_sizeDirty = true;

        glm::vec2 m_cachedPos{0};
        glm::vec2 m_cachedSize{-1};
        glm::vec2 m_drawSize{0};

        UUID m_parentId = UUID::null;
        UINodeRegistry *m_registry = nullptr;

        YGNodeRef m_ygNode = nullptr;
        DrawPivot m_drawPivot = DrawPivot::center;
        glm::vec2 m_topLeftPos{0};
    };
} // namespace Bess::Canvas::UI
