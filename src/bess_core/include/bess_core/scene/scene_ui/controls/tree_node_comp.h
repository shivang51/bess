#pragma once

#include "common/bess_api.h"

#include "bess_core/scene/scene_ui/ui_scene_component.h"
#include <functional>
#include <memory>
#include <string>

namespace Bess::Canvas::UI {

    using UITreeNodeCallback = std::function<void(bool)>;

    class BESS_API TreeNodeComp : public UISceneComponent {
      public:
        DEFAULT_CONTRS(TreeNodeComp)

        MAKE_GETTER_SETTER_WC(bool, Expanded, m_expanded, makeUIDirty)
        MAKE_GETTER_SETTER_WC(float, ChevronSize, m_chevronSize, makeUIDirty)
        MAKE_GETTER_SETTER_WC(float,
                              ChevronLabelSpacing,
                              m_chevronLabelSpacing,
                              makeUIDirty)
        MAKE_GETTER_SETTER_WC(float,
                              ContentIndent,
                              m_contentIndent,
                              makeUIDirty)
        MAKE_GETTER_SETTER_WC(float,
                              HeaderContentIndent,
                              m_headerContentIndent,
                              makeUIDirty)
        MAKE_GETTER_SETTER(bool, DrawHeaderBackground, m_drawHeaderBackground)
        MAKE_GETTER_SETTER(bool, DrawHeaderBorder, m_drawHeaderBorder)
        MAKE_GETTER_SETTER(UITreeNodeCallback,
                           ToggledCallback,
                           m_toggledCallback)

        static std::shared_ptr<TreeNodeComp>
        create(const CompConfig &config);
        static std::shared_ptr<TreeNodeComp>
        create(const std::string &label,
               bool expanded = true,
               const UITreeNodeCallback &toggledCallback = nullptr,
               const CompConfig &config = CompConfig{});

        void onDraw(SceneDrawContext &state) override;
        void prepareUI(SceneUIPrepareCtx &state) override;
        bool onMouseButton(const Events::MouseButtonEvent &e) override;
        bool isFocusable() const override;
        bool wantsKeyboardInput() const override;
        bool onKeyEvent(const SceneEvent &evt) override;

      private:
        void toggleFromUser();
        void prepChildren(SceneUIPrepareCtx &state);
        void drawChildren(SceneDrawContext &state);
        void drawChevron(SceneDrawContext &state);

        bool m_expanded = true;
        float m_chevronSize = 8.f;
        float m_chevronLabelSpacing = 5.f;
        float m_contentIndent = 12.f;
        float m_headerContentIndent = 0.f;
        bool m_drawHeaderBackground = false;
        bool m_drawHeaderBorder = false;
        UINode *m_headerNode = nullptr;
        UINode *m_chevronNode = nullptr;
        UINode *m_labelNode = nullptr;
        UINode *m_contentNode = nullptr;
        UITreeNodeCallback m_toggledCallback;
    };
} // namespace Bess::Canvas::UI
