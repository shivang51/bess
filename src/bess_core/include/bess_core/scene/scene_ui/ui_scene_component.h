#pragma once

#include "common/bess_api.h"

#include "bess_core/renderer/renderer_types.h"
#include "bess_core/scene/scene_draw_context.h"
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

    struct BESS_API UIFlex {
        float grow = 0.f;
        float shrink = 0.f;
        float basis = 0.f;
        Unit basisUnit = Unit::pixel;
    };

    struct BESS_API UIElementStyle {
        std::optional<Core::Style::Color> backgroundColor;
        std::optional<Core::Style::Color> hoverColor;
        std::optional<Core::Style::Color> borderColor;
        std::optional<Core::Style::Color> activeColor;

        std::optional<Core::Style::Padding> padding;
        std::optional<Core::Style::Margin> margin;

        std::optional<float> fontSize;

        std::optional<glm::vec2> pos;
        std::optional<Unit> posUnit;
        std::optional<LayoutSizeMode> widthMode;
        std::optional<float> width;
        std::optional<LayoutSizeMode> heightMode;
        std::optional<float> height;
        std::optional<glm::vec2> minSize;
        std::optional<glm::vec2> maxSize;
        std::optional<LayoutDirection> direction;
        std::optional<LayoutAlignment> mainAxisAlignment;
        std::optional<LayoutAlignment> crossAxisAlignment;
        std::optional<LayoutSelfAlignment> alignSelf;
        std::optional<UIFlex> flex;
        std::optional<float> flexGrow;
        std::optional<float> flexShrink;
        std::optional<LayoutSizeMode> flexBasisMode;
        std::optional<float> flexBasis;
        std::optional<Unit> flexBasisUnit;
        std::optional<PosMode> posMode;
        std::optional<float> zVal;
        std::optional<DrawPivot> drawPivot;
        std::optional<Core::Renderer::ShadowProps> shadow;
        std::optional<glm::vec4> borderRadius;
        std::optional<Core::Style::BorderSize> borderSize;
        std::optional<bool> drawBg;
    };

    class UISceneComponent;
    typedef std::function<void(SceneDrawContext &ctx, UISceneComponent *comp)>
        UIDrawCallback;

    struct BESS_API CompConfig {
        SceneState *sceneState = nullptr;
        std::vector<std::shared_ptr<UISceneComponent>> children;
        std::optional<UIElementStyle> style = std::nullopt;
        bool triggerAttach = true;
        bool dispatchAddEvent = true;
        bool emitReparentEvent = false;
    };

    class BESS_API UISceneComponent : public SceneComponent {
      public:
        UISceneComponent();

        typedef std::function<bool(const SceneDrawContext &ctx)> IsHiddenCb;

        MAKE_GETTER_SETTER_PTR(UINode, UINode, m_node);
        MAKE_GETTER_SETTER(UIElementStyle, Style, m_customStyle);
        MAKE_GETTER_SETTER(UIDrawCallback, DrawCallback, m_drawCallback);
        MAKE_GETTER_SETTER(std::optional<uint32_t>,
                           DrawRuntimeId,
                           m_drawRuntimeId);
        MAKE_GETTER(bool, Focused, m_focused)
        MAKE_GETTER_SETTER(IsHiddenCb, IsHiddenCb, m_isHiddenCb)

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
        void hide();
        void show();

        virtual void onDraw(SceneDrawContext &state) = 0;

      protected:
        uint32_t resolveRuntimeId() const;

        void onNameChanged() override;

        void initNode(const std::shared_ptr<UINodeRegistry> &reg);
        void makeUIDirty();
        virtual void
        prepStyle(const std::shared_ptr<Core::Style::BessTheme> &theme);
        void applyCustomLayoutStyle();
        void drawBgQuad(SceneDrawContext &state);
        void drawText(SceneDrawContext &state,
                      const std::string &text,
                      UINode *node);
        void drawText(SceneDrawContext &state,
                      const std::string &text,
                      UINode *node,
                      const PickingId &pickingId);

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
        bool m_hidden = false;
        IsHiddenCb m_isHiddenCb = nullptr;
    };

    void applyCompConfig(const std::shared_ptr<UISceneComponent> &component,
                         const CompConfig &config);
} // namespace Bess::Canvas::UI
