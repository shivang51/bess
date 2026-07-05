#pragma once

#include "bess_core/scene/scene_ui/ui_scene_component.h"
#include <memory>
#include <string>

namespace Bess::Canvas::UI {

    class LabelComp : public UISceneComponent {
      public:
        DEFAULT_CONTRS(LabelComp)

        static std::shared_ptr<LabelComp> create(const std::string &label);

        void draw(SceneDrawContext &state) override;
    };
} // namespace Bess::Canvas::UI
