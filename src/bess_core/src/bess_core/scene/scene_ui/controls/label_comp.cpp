#include "bess_core/scene/scene_ui/controls/label_comp.h"

namespace Bess::Canvas::UI {
    std::shared_ptr<LabelComp> LabelComp::create(const CompConfig &config) {
        return create("", config);
    }

    std::shared_ptr<LabelComp>
    LabelComp::create(const std::string &label, const CompConfig &config) {
        auto labelComp = std::make_shared<LabelComp>();
        labelComp->setName(label);
        applyCompConfig(labelComp, config);
        return labelComp;
    }

    void LabelComp::onDraw(SceneDrawContext &state) {
        drawText(state, m_name, m_node);
    }
} // namespace Bess::Canvas::UI
