#pragma once

#include "bess_core/scene/scene_ui/ui_scene_component.h"
#include <cstddef>
#include <functional>
#include <memory>
#include <string>

namespace Bess::Canvas::UI {

    typedef std::function<void(const std::string &)> UITextBoxCallback;

    class TextBoxComp : public UISceneComponent {
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
        MAKE_GETTER(bool, Focused, m_focused)

        static std::shared_ptr<TextBoxComp>
        create(const std::string &value = "",
               const UITextBoxCallback &changedCallback = nullptr);

        void draw(SceneDrawContext &state) override;
        void prepareUI(SceneUIPrepareCtx &state) override;

      private:
        glm::vec2 resolveBoxSize(SceneUIPrepareCtx &state) const;
        glm::vec2 stylePadding() const;

        std::string m_value;
        std::string m_placeholder;
        glm::vec2 m_textBoxSize{120.f, 0.f};
        size_t m_maxLength = 256;
        UITextBoxCallback m_changedCallback;
        UITextBoxCallback m_submittedCallback;
        UITextBoxCallback m_canceledCallback;
        bool m_focused = false;
    };
} // namespace Bess::Canvas::UI
