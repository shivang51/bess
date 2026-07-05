#pragma once

#include "bess_core/scene/scene_ui/ui_scene_component.h"
#include <functional>
#include <memory>
#include <string>

namespace Bess::Canvas::UI {

    typedef std::function<void(bool)> ToggleBtnCallback;

    class ToggleBtnComp : public UISceneComponent {
      public:
        DEFAULT_CONTRS(ToggleBtnComp)

        MAKE_GETTER_SETTER(bool, Toggled, m_toggled)

        MAKE_GETTER_SETTER_WC(bool, ShowLabel, m_showLabel, makeUIDirty)
        MAKE_GETTER_SETTER_WC(float,
                              LabelTrackSpacing,
                              m_labelTrackSpacing,
                              makeUIDirty)
        MAKE_GETTER_SETTER_WC(glm::vec2, TrackSize, m_trackSize, makeUIDirty)
        MAKE_GETTER_SETTER_WC(glm::vec2, ThumbSize, m_thumbSize, makeUIDirty)
        MAKE_GETTER_SETTER(ToggleBtnCallback, Callback, m_callback)

        static std::shared_ptr<ToggleBtnComp>
        create(const std::string &label,
               const ToggleBtnCallback &callback = nullptr,
               bool toggled = false);

        void draw(SceneDrawContext &state) override;
        bool onMouseButton(const Events::MouseButtonEvent &e) override;
        void prepareUI(SceneUIPrepareCtx &state) override;

      private:
        bool m_toggled = false;
        bool m_showLabel = true;
        float m_labelTrackSpacing = 4.f;
        glm::vec2 m_trackSize{24.f, 12.f};
        glm::vec2 m_thumbSize{12.f, 12.f};
        UINode *m_trackNode = nullptr;
        UINode *m_labelNode = nullptr;
        ToggleBtnCallback m_callback;
        Color m_trackColor, m_thumbColor;
    };
} // namespace Bess::Canvas::UI
