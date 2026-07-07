#pragma once

#include "bess_core/scene/scene_state/components/scene_component.h"
#include "bess_core/scene/scene_ui/layout.h"
#include "bess_core/style/bess_theme.h"
#include "common/types.h"
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
    };

    class UISceneComponent;
    typedef std::function<void(SceneDrawContext &ctx, UISceneComponent *comp)>
        UIDrawCallback;

    class UISceneComponent : public SceneComponent {
      public:
        DEFAULT_CONTRS(UISceneComponent)

        MAKE_GETTER_SETTER_PTR(UINode, UINode, m_node);
        MAKE_GETTER_SETTER(UIElementStyle, Style, m_customStyle);
        MAKE_GETTER_SETTER(UIDrawCallback, DrawCallback, m_drawCallback);
        MAKE_GETTER_SETTER(std::optional<uint32_t>,
                           DrawRuntimeId,
                           m_drawRuntimeId);

        std::vector<UUID> cleanup(SceneState &state,
                                  UUID caller = UUID::null) override;

        void prepareUI(SceneUIPrepareCtx &state) override;

        bool onMouseEnter(const Events::MouseEnterEvent &e) override;
        bool onMouseLeave(const Events::MouseLeaveEvent &e) override;

      protected:
        uint32_t resolveRuntimeId() const;

        void onNameChanged() override;

        void initNode(const std::shared_ptr<UINodeRegistry> &reg);
        void makeUIDirty();
        void prepStyle(const std::shared_ptr<Core::Style::BessTheme> &theme);
        void drawBgQuad(SceneDrawContext &state);
        void drawText(SceneDrawContext &state,
                      const std::string &text,
                      UINode *node);

        UINode *m_node = nullptr;
        bool m_hovered = false;
        Core::Style::ElementStyle m_style;
        UIElementStyle m_customStyle;
        UIDrawCallback m_drawCallback = nullptr;
        std::optional<uint32_t> m_drawRuntimeId = std::nullopt;
    };
} // namespace Bess::Canvas::UI
