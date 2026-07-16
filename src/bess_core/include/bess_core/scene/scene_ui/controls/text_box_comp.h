#pragma once

#include "common/bess_api.h"

#include "bess_core/scene/scene_state/scene_state.h"
#include "bess_core/scene/scene_ui/text_box_context.h"
#include "bess_core/scene/scene_ui/ui_scene_component.h"
#include <cstddef>
#include <functional>
#include <memory>
#include <string>

namespace Bess::Canvas::UI {

    typedef std::function<void(const std::string &)> UITextBoxCallback;

    class BESS_API TextBoxComp : public UISceneComponent {
      public:
        DEFAULT_CONTRS(TextBoxComp)

        MAKE_GETTER_SETTER_WC(std::string, Value, m_value, makeUIDirty)
        MAKE_GETTER_SETTER_WC(std::string,
                              Placeholder,
                              m_placeholder,
                              makeUIDirty)
        MAKE_GETTER_SETTER_WC(glm::vec2,
                              TextBoxSize,
                              m_textBoxSize,
                              makeUIDirty)
        MAKE_GETTER_SETTER_WC(size_t, MaxLength, m_maxLength, makeUIDirty)
        MAKE_GETTER_SETTER(UITextBoxCallback,
                           ChangedCallback,
                           m_changedCallback)
        MAKE_GETTER_SETTER(UITextBoxCallback,
                           SubmittedCallback,
                           m_submittedCallback)
        MAKE_GETTER_SETTER(UITextBoxCallback,
                           CanceledCallback,
                           m_canceledCallback)

        static std::shared_ptr<TextBoxComp>
        create(const CompConfig &config);
        static std::shared_ptr<TextBoxComp>
        create(const std::string &value = "",
               const UITextBoxCallback &changedCallback = nullptr,
               const CompConfig &config = CompConfig{});

        void update(TimeMs ts, SceneState &state) override;
        void onDraw(SceneDrawContext &state) override;
        void prepareUI(SceneUIPrepareCtx &state) override;
        bool isFocusable() const override;
        bool wantsKeyboardInput() const override;
        void onFocusGained(const Events::FocusEvent &e) override;
        void onFocusLost(const Events::FocusEvent &e) override;
        bool onMouseButton(const Events::MouseButtonEvent &e) override;
        bool onPointerMove(const Events::MouseMoveEvent &e) override;
        bool onKeyEvent(const SceneEvent &evt) override;
        bool hasPointerCapture() const override;
        Core::Viewport::SceneCursor getCursor() const override;

      private:
        glm::vec2 resolveBoxSize(SceneUIPrepareCtx &state) const;
        glm::vec2 stylePadding() const;
        void applyTextInputResult(const TextBoxContextResult &result);

        std::string m_value;
        std::string m_placeholder;
        glm::vec2 m_textBoxSize{64.f, 0.f};
        size_t m_maxLength = 256;
        UITextBoxCallback m_changedCallback;
        UITextBoxCallback m_submittedCallback;
        UITextBoxCallback m_canceledCallback;
        TextBoxContext m_textInput;
        bool m_clearFocus = false;
    };
} // namespace Bess::Canvas::UI
