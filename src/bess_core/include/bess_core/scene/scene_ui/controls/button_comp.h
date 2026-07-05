#pragma once

#include "bess_core/scene/scene_ui/ui_scene_component.h"
#include <functional>
#include <memory>
#include <string>

namespace Bess::Canvas::UI {

    typedef std::function<void()> UIButtonCallback;

    class ButtonComp : public UISceneComponent {
      public:
        DEFAULT_CONTRS(ButtonComp)

        static std::shared_ptr<ButtonComp>
        create(const std::string &label, const UIButtonCallback &callback);

        bool onMouseButton(const Events::MouseButtonEvent &e) override;

        MAKE_GETTER_SETTER(UIButtonCallback, Callback, m_callback)

        void update(TimeMs ts, SceneState &state) override;
        void draw(SceneDrawContext &state) override;
        void prepareUI(SceneUIPrepareCtx &state) override;

      private:
        UINode *m_labelNode = nullptr;
        UIButtonCallback m_callback;
    };
} // namespace Bess::Canvas::UI
