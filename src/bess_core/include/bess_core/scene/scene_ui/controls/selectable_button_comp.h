#pragma once

#include "common/bess_api.h"

#include "bess_core/scene/scene_ui/ui_scene_component.h"
#include <functional>
#include <memory>
#include <string>

namespace Bess::Canvas::UI {

    using UISelectableButtonCallback = std::function<void(bool)>;

    class BESS_API SelectableButtonComp : public UISceneComponent {
      public:
        DEFAULT_CONTRS(SelectableButtonComp)

        static std::shared_ptr<SelectableButtonComp>
        create(const CompConfig &config);
        static std::shared_ptr<SelectableButtonComp>
        create(const std::string &label,
               const UISelectableButtonCallback &callback = nullptr,
               bool selected = false,
               const CompConfig &config = CompConfig{});

        [[nodiscard]] bool getSelected() const noexcept;
        void setSelected(bool selected) noexcept;
        void toggleSelected();

        MAKE_GETTER_SETTER_WC(glm::vec2, ButtonSize, m_buttonSize, makeUIDirty)
        MAKE_GETTER_SETTER(bool, ToggleOnClick, m_toggleOnClick)
        MAKE_GETTER_SETTER(UISelectableButtonCallback, Callback, m_callback)

        void onDraw(SceneDrawContext &state) override;
        void prepareUI(SceneUIPrepareCtx &state) override;
        bool onMouseButton(const Events::MouseButtonEvent &e) override;
        bool isFocusable() const override;
        bool wantsKeyboardInput() const override;
        bool onKeyEvent(const SceneEvent &evt) override;

      private:
        void prepStyle(
            const std::shared_ptr<Core::Style::BessTheme> &theme) override;
        void activateFromUser();
        void drawLabel(SceneDrawContext &state);
        [[nodiscard]] glm::vec2 resolveButtonSize(SceneUIPrepareCtx &state);

        bool m_selected = false;
        bool m_toggleOnClick = true;
        glm::vec2 m_buttonSize{0.f};
        UINode *m_labelNode = nullptr;
        UISelectableButtonCallback m_callback = nullptr;
        Color m_selectedTextColor{1.f, 1.f, 1.f, 1.f};
    };

} // namespace Bess::Canvas::UI
