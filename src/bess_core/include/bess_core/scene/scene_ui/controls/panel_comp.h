#pragma once

#include "bess_core/scene/scene_ui/ui_scene_component.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace Bess::Canvas::UI {

    using UIPanelResizeCallback = std::function<void(const glm::vec2 &)>;

    enum class UIPanelResizeEdge : uint8_t {
        none = 0u,
        left = 1u << 0u,
        right = 1u << 1u,
        top = 1u << 2u,
        bottom = 1u << 3u,
        horizontal = 0x03u,
        vertical = 0x0cu,
        all = 0x0fu,
    };

    [[nodiscard]] constexpr uint8_t resizeEdgeMask(UIPanelResizeEdge edge) {
        return static_cast<uint8_t>(edge);
    }

    [[nodiscard]] constexpr UIPanelResizeEdge operator|(UIPanelResizeEdge lhs,
                                                        UIPanelResizeEdge rhs) {
        return static_cast<UIPanelResizeEdge>(resizeEdgeMask(lhs) |
                                              resizeEdgeMask(rhs));
    }

    [[nodiscard]] constexpr UIPanelResizeEdge operator&(UIPanelResizeEdge lhs,
                                                        UIPanelResizeEdge rhs) {
        return static_cast<UIPanelResizeEdge>(resizeEdgeMask(lhs) &
                                              resizeEdgeMask(rhs));
    }

    [[nodiscard]] constexpr bool hasResizeEdge(UIPanelResizeEdge edges,
                                               UIPanelResizeEdge edge) {
        return (resizeEdgeMask(edges) & resizeEdgeMask(edge)) != 0u;
    }

    class PanelComp : public UISceneComponent {
      public:
        DEFAULT_CONTRS(PanelComp)

        MAKE_GETTER_SETTER_WC(glm::vec2, PanelSize, m_panelSize, makeUIDirty)
        MAKE_GETTER_SETTER_WC(glm::vec2,
                              MinPanelSize,
                              m_minPanelSize,
                              makeUIDirty)
        MAKE_GETTER_SETTER_WC(glm::vec2,
                              MaxPanelSize,
                              m_maxPanelSize,
                              makeUIDirty)
        MAKE_GETTER_SETTER_WC(float, HeaderHeight, m_headerHeight, makeUIDirty)
        MAKE_GETTER_SETTER_WC(float,
                              ResizeGripSize,
                              m_resizeGripSize,
                              makeUIDirty)
        MAKE_GETTER_SETTER_WC(bool, Resizable, m_resizable, makeUIDirty)
        MAKE_GETTER_SETTER_WC(bool,
                              DrawResizeGrip,
                              m_drawResizeGrip,
                              makeUIDirty)
        MAKE_GETTER_SETTER_WC(float,
                              ResizeBorderHitSize,
                              m_resizeBorderHitSize,
                              makeUIDirty)
        MAKE_GETTER_SETTER_WC(bool,
                              DrawBackground,
                              m_drawBackground,
                              makeUIDirty)
        MAKE_GETTER_SETTER_WC(LayoutAlignment,
                              ContentAlignment,
                              m_contentAlignment,
                              makeUIDirty)
        MAKE_GETTER_SETTER(UIPanelResizeCallback,
                           ResizeCallback,
                           m_resizeCallback)

        [[nodiscard]] UIPanelResizeEdge getResizeEdges() const;
        void setResizeEdges(UIPanelResizeEdge edges);
        [[nodiscard]] bool isResizeEdgeEnabled(UIPanelResizeEdge edge) const;
        void setResizeEdgeEnabled(UIPanelResizeEdge edge, bool enabled = true);

        static std::shared_ptr<PanelComp> create(const CompConfig &config);
        static std::shared_ptr<PanelComp>
        create(const std::string &title,
               const CompConfig &config = CompConfig{});
        static std::shared_ptr<PanelComp>
        create(const std::string &title,
               const glm::vec2 &panelSize,
               const CompConfig &config = CompConfig{});

        std::vector<UUID> cleanup(SceneState &state,
                                  UUID caller = UUID::null) override;
        void onDraw(SceneDrawContext &state) override;
        void prepareUI(SceneUIPrepareCtx &state) override;
        bool onMouseEnter(const Events::MouseEnterEvent &e) override;
        bool onMouseLeave(const Events::MouseLeaveEvent &e) override;
        bool onMouseButton(const Events::MouseButtonEvent &e) override;
        bool onPointerMove(const Events::MouseMoveEvent &e) override;
        bool hasPointerCapture() const override;
        bool isFocusable() const override;
        Core::Viewport::SceneCursor getCursor() const override;

        struct Rect {
            float left = 0.f;
            float top = 0.f;
            float right = 0.f;
            float bottom = 0.f;
        };

      private:
        void prepStyle(
            const std::shared_ptr<Core::Style::BessTheme> &theme) override;
        void ensureNodes(const std::shared_ptr<UINodeRegistry> &registry);
        void configurePanelNode();
        void configureHeaderNode(const glm::vec2 &titleSize,
                                 float headerHeight);
        void configureContentNode(float headerHeight);
        void configureResizeGripNode();
        void prepareChildren(SceneUIPrepareCtx &state);
        void drawPanelBackground(SceneDrawContext &state);
        void drawHeader(SceneDrawContext &state);
        void drawChildren(SceneDrawContext &state);
        void drawResizeGrip(SceneDrawContext &state);
        void drawResizeHitRegions(SceneDrawContext &state);
        void updateSizeFromPointer(const glm::vec2 &pointerPos);
        void onChildrenChanged() override;

        [[nodiscard]] bool canResizeFromEdges(UIPanelResizeEdge edges) const;
        [[nodiscard]] bool isResizeInfo(uint32_t info) const;
        [[nodiscard]] uint32_t resizeInfo(UIPanelResizeEdge edges) const;
        [[nodiscard]] UIPanelResizeEdge
        resizeEdgesFromInfo(uint32_t info) const;
        [[nodiscard]] UIPanelResizeEdge hoveredResizeEdges() const;
        [[nodiscard]] Core::Viewport::SceneCursor
        cursorForResizeEdges(UIPanelResizeEdge edges) const;
        [[nodiscard]] Rect resizeHitRect(UIPanelResizeEdge edges) const;
        void drawResizeHitRegion(SceneDrawContext &state,
                                 UIPanelResizeEdge edges,
                                 float zIndex);
        [[nodiscard]] float
        resolveHeaderHeight(const glm::vec2 &titleSize) const;
        [[nodiscard]] Core::Style::Padding resolvedBorderSize() const;
        [[nodiscard]] Core::Style::Padding headerPadding() const;
        [[nodiscard]] glm::vec2 resolvedMinPanelSize() const;
        [[nodiscard]] glm::vec2
        resolvedMaxPanelSize(const glm::vec2 &minSize) const;
        [[nodiscard]] glm::vec2 clampPanelSize(const glm::vec2 &size) const;
        [[nodiscard]] Rect innerPanelRect() const;
        [[nodiscard]] Rect nodeRect(const UINode *node) const;
        [[nodiscard]] Rect insetRect(Rect rect,
                                     const Core::Style::Padding &insets) const;
        [[nodiscard]] Rect intersectRect(const Rect &lhs,
                                         const Rect &rhs) const;
        [[nodiscard]] bool rectEmpty(const Rect &rect) const;
        [[nodiscard]] bool pushClip(SceneDrawContext &state,
                                    const Rect &rect) const;

        glm::vec2 m_panelSize{260.f, 180.f};
        glm::vec2 m_minPanelSize{96.f, 64.f};
        glm::vec2 m_maxPanelSize{-1.f, -1.f};
        float m_headerHeight = 18.f;
        float m_resizeGripSize = 12.f;
        float m_resizeBorderHitSize = 6.f;
        bool m_resizable = true;
        bool m_drawResizeGrip = true;
        bool m_drawBackground = true;
        UIPanelResizeEdge m_resizeEdges = UIPanelResizeEdge::all;
        UIPanelResizeEdge m_activeResizeEdges = UIPanelResizeEdge::none;
        LayoutAlignment m_contentAlignment = LayoutAlignment::start;
        bool m_resizing = false;
        glm::vec2 m_resizeStartPointer{0.f};
        glm::vec2 m_resizeStartSize{260.f, 180.f};
        glm::vec3 m_resizeStartPosition{0.f};
        float m_cachedHeaderHeight = 18.f;
        uint32_t m_hoveredInfo = 0u;
        UINode *m_headerNode = nullptr;
        UINode *m_titleNode = nullptr;
        UINode *m_contentNode = nullptr;
        UINode *m_resizeGripNode = nullptr;
        UIPanelResizeCallback m_resizeCallback;
        Color m_headerColor{0.f, 0.f, 0.f, 0.f};
        Color m_headerHoverColor{0.f, 0.f, 0.f, 0.f};
        Color m_separatorColor{0.f, 0.f, 0.f, 0.f};
        Color m_resizeGripColor{0.f, 0.f, 0.f, 0.f};
    };
} // namespace Bess::Canvas::UI
