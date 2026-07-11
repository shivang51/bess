#pragma once

#include "bess_core/scene/scene_ui/ui_scene_component.h"
#include <functional>
#include <memory>
#include <string>

namespace Bess::Canvas::UI {

    using UICheckboxCallback = std::function<void(bool)>;

    class CheckboxComp : public UISceneComponent {
      public:
        DEFAULT_CONTRS(CheckboxComp)

        MAKE_GETTER_SETTER(bool, Checked, m_checked)
        MAKE_GETTER_SETTER_WC(bool, ShowLabel, m_showLabel, makeUIDirty)
        MAKE_GETTER_SETTER_WC(float,
                              LabelBoxSpacing,
                              m_labelBoxSpacing,
                              makeUIDirty)
        MAKE_GETTER_SETTER_WC(glm::vec2, BoxSize, m_boxSize, makeUIDirty)
        MAKE_GETTER_SETTER(UICheckboxCallback, Callback, m_callback)

        static std::shared_ptr<CheckboxComp>
        create(const CompConfig &config);
        static std::shared_ptr<CheckboxComp>
        create(const std::string &label,
               const UICheckboxCallback &callback = nullptr,
               bool checked = false,
               const CompConfig &config = CompConfig{});

        void prepareUI(SceneUIPrepareCtx &state) override;
        bool onMouseButton(const Events::MouseButtonEvent &e) override;
        bool isFocusable() const override;
        bool wantsKeyboardInput() const override;
        bool onKeyEvent(const SceneEvent &evt) override;
        void onDraw(SceneDrawContext &state) override;

      private:
        void prepStyle(
            const std::shared_ptr<Core::Style::BessTheme> &theme) override;
        void toggleFromUser();

        bool m_checked = false;
        bool m_showLabel = true;
        float m_labelBoxSpacing = 6.f;
        glm::vec2 m_boxSize{12.f, 12.f};
        UINode *m_boxNode = nullptr;
        UINode *m_labelNode = nullptr;
        UICheckboxCallback m_callback;
        Color m_checkedColor;
        Color m_checkColor;
        Color m_boxColor;
    };
} // namespace Bess::Canvas::UI
