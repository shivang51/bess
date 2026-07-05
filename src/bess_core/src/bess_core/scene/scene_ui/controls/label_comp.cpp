#include "bess_core/scene/scene_ui/controls/label_comp.h"

namespace Bess::Canvas::UI {
    std::shared_ptr<LabelComp> LabelComp::create(const std::string &label) {
        auto labelComp = std::make_shared<LabelComp>();
        labelComp->setName(label);
        return labelComp;
    }

    void LabelComp::draw(SceneDrawContext &state) {
        drawText(state, m_name, m_node);
    }
} // namespace Bess::Canvas::UI
