#pragma once

#include "bess_core/scene/scene_ui/ui_scene_component.h"
#include <memory>

namespace Bess::Canvas::UI {

    class ContainerComp : public UISceneComponent {
      public:
        DEFAULT_CONTRS(ContainerComp)

        MAKE_GETTER_SETTER_WC(LayoutDirection,
                              Direction,
                              m_direction,
                              makeUIDirty)

        MAKE_GETTER_SETTER_WC(LayoutAlignment,
                              MainAxisAlignment,
                              m_mainAxisAlignment,
                              makeUIDirty)

        MAKE_GETTER_SETTER_WC(LayoutAlignment,
                              CrossAxisAlignment,
                              m_crossAxisAlignment,
                              makeUIDirty)

        MAKE_GETTER_SETTER(bool, DrawBackground, m_drawBg)

        static std::shared_ptr<ContainerComp>
        create(const CompConfig &config);
        static std::shared_ptr<ContainerComp>
        create(const LayoutDirection &direction = LayoutDirection::horizontal,
               const CompConfig &config = CompConfig{});

        void onDraw(SceneDrawContext &state) override;
        void prepareUI(SceneUIPrepareCtx &state) override;

      private:
        void drawBackground(SceneDrawContext &state);
        void prepChildren(SceneUIPrepareCtx &state);
        void drawChildren(SceneDrawContext &state);

        LayoutDirection m_direction = LayoutDirection::horizontal;
        LayoutAlignment m_mainAxisAlignment = LayoutAlignment::start;
        LayoutAlignment m_crossAxisAlignment = LayoutAlignment::center;

        bool m_drawBg = false;
    };
} // namespace Bess::Canvas::UI
