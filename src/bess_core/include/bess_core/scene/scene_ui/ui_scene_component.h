#pragma once

#include "bess_core/renderer/renderer_types.h"
#include "bess_core/scene/scene_state/components/scene_component.h"
#include "bess_core/scene/scene_ui/layout.h"
#include "bess_core/style/bess_theme.h"
#include "bess_core/viewport.h"
#include "pages/main_page/scene_components/scene_comp_types.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Bess::Canvas::UI {

    using Color = Core::Style::Color;

    struct UIElementStyle {
        std::optional<Core::Style::Color> backgroundColor;
        std::optional<Core::Style::Color> hoverColor;
        std::optional<Core::Style::Color> borderColor;
        std::optional<Core::Style::Color> activeColor;

        std::optional<Core::Style::Padding> padding;
        std::optional<Core::Style::Margin> margin;

        std::optional<float> fontSize;
    };

    class UISceneComponent;
    typedef std::function<void(SceneDrawContext &ctx, UISceneComponent *comp)>
        UIDrawCallback;

    class UISceneComponent : public SceneComponent {
      public:
        UISceneComponent();

        MAKE_GETTER_SETTER_PTR(UINode, UINode, m_node);
        MAKE_GETTER_SETTER(UIElementStyle, Style, m_customStyle);
        MAKE_GETTER_SETTER(UIDrawCallback, DrawCallback, m_drawCallback);
        MAKE_GETTER_SETTER(std::optional<uint32_t>,
                           DrawRuntimeId,
                           m_drawRuntimeId);
        MAKE_GETTER(bool, Focused, m_focused)

        REG_SCENE_COMP_TYPE("UISceneComponent", SceneComponentType::ui)
        std::vector<UUID> cleanup(SceneState &state,
                                  UUID caller = UUID::null) override;

        void prepareUI(SceneUIPrepareCtx &state) override;

        bool onMouseEnter(const Events::MouseEnterEvent &e) override;
        bool onMouseLeave(const Events::MouseLeaveEvent &e) override;
        void onFocusGained(const Events::FocusEvent &e) override;
        void onFocusLost(const Events::FocusEvent &e) override;

        Core::Viewport::SceneCursor getCursor() const override;

        void setIsScreenSpace(bool val = true);
        void draw(SceneDrawContext &state) override;

        virtual void onDraw(SceneDrawContext &state) = 0;

      protected:
        uint32_t resolveRuntimeId() const;

        void onNameChanged() override;

        void initNode(const std::shared_ptr<UINodeRegistry> &reg);
        void makeUIDirty();
        virtual void
        prepStyle(const std::shared_ptr<Core::Style::BessTheme> &theme);
        void drawBgQuad(SceneDrawContext &state);
        void drawText(SceneDrawContext &state,
                      const std::string &text,
                      UINode *node);

        void onBeforeDraw(SceneDrawContext &state);
        void onAfterDraw(SceneDrawContext &state);

        UINode *m_node = nullptr;
        bool m_hovered = false;
        bool m_focused = false;
        Core::Style::ElementStyle m_style;
        UIElementStyle m_customStyle;
        UIDrawCallback m_drawCallback = nullptr;
        std::optional<uint32_t> m_drawRuntimeId = std::nullopt;
        Core::Renderer::RenderTransformMode m_transformMode =
            Core::Renderer::RenderTransformMode::Camera;
        Core::Renderer::RenderTransformMode m_lastTransformMode =
            Core::Renderer::RenderTransformMode::Camera;
    };
} // namespace Bess::Canvas::UI
