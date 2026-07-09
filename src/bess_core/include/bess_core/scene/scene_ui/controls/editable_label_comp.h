#pragma once

#include "bess_core/scene/scene_ui/ui_scene_component.h"
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace Bess::Canvas::UI {

    using UIEditableLabelCallback = std::function<void(const std::string &)>;

    class EditableLabelComp : public UISceneComponent {
      public:
        DEFAULT_CONTRS(EditableLabelComp)

        MAKE_GETTER_SETTER_WC(std::string,
                              Placeholder,
                              m_placeholder,
                              makeUIDirty)
        MAKE_GETTER_SETTER_WC(glm::vec2,
                              TextBoxSize,
                              m_textBoxSize,
                              makeUIDirty)
        MAKE_GETTER_SETTER_WC(size_t, MaxLength, m_maxLength, makeUIDirty)
        MAKE_GETTER_SETTER(bool, SelectTextOnEdit, m_selectTextOnEdit)
        MAKE_GETTER_SETTER(UIEditableLabelCallback,
                           ChangedCallback,
                           m_changedCallback)
        MAKE_GETTER_SETTER(UIEditableLabelCallback,
                           SubmittedCallback,
                           m_submittedCallback)
        MAKE_GETTER_SETTER(UIEditableLabelCallback,
                           CanceledCallback,
                           m_canceledCallback)
        MAKE_GETTER(bool, Editing, m_editing)

        static std::shared_ptr<EditableLabelComp>
        create(const std::string &value = "",
               const UIEditableLabelCallback &changedCallback = nullptr);

        void draw(SceneDrawContext &state) override;
        void prepareUI(SceneUIPrepareCtx &state) override;
        bool onMouseButton(const Events::MouseButtonEvent &e) override;

        void beginEdit();
        void commitEdit();
        void cancelEdit();

      private:
        [[nodiscard]] std::string displayText() const;
        [[nodiscard]] glm::vec2
        resolveTextBoxSize(SceneUIPrepareCtx &state) const;
        [[nodiscard]] glm::vec2 stylePadding() const;
        [[nodiscard]] PickingId textBoxPickingId() const;

        void beginEditAt(std::optional<glm::vec2> focusPos);
        void finishEdit(bool commit);

        std::string m_placeholder = "Label";
        glm::vec2 m_textBoxSize{0.f, 0.f};
        size_t m_maxLength = 256;
        bool m_selectTextOnEdit = false;

        UIEditableLabelCallback m_changedCallback;
        UIEditableLabelCallback m_submittedCallback;
        UIEditableLabelCallback m_canceledCallback;

        bool m_editing = false;
        bool m_pendingTextBoxFocus = false;
        std::optional<glm::vec2> m_pendingTextBoxFocusPos = std::nullopt;
        bool m_wasTextBoxFocused = false;
        std::string m_editValue;
        std::string m_originalValue;
    };
} // namespace Bess::Canvas::UI
