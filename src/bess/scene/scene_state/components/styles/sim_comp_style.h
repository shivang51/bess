#pragma once

#include "scene/scene_state/components/styles/comp_style.h"

namespace Bess::Canvas::Styles {
    constexpr class SimCompNodeStyles : public CompNodeStyles {
    } simCompStyles;

    constexpr struct CompSchematicStyles {
        float paddingX = 8.f;
        float paddingY = 4.f;

        float pinSize = 20.f;
        float pinRowGap = 12.f;
        float pinLabelSize = 8.f;

        float nameFontSize = 10.f;

        float strokeSize = 1.f;
        float connJointRadius = 2.f;

        float negCircleR = 4.f, negCircleOff = negCircleR * 2.f;
    } compSchematicStyles;

    constexpr float SIM_COMP_SLOT_DX = simCompStyles.paddingX +
                                       simCompStyles.slotRadius +
                                       simCompStyles.slotMargin;
    constexpr float SIM_COMP_SLOT_START_Y = simCompStyles.headerHeight;
    constexpr float SIM_COMP_SLOT_ROW_SIZE = (simCompStyles.rowMargin * 2.f) +
                                             (simCompStyles.slotRadius * 2.f) +
                                             simCompStyles.rowGap;
    constexpr float SIM_COMP_SLOT_COLUMN_SIZE = (simCompStyles.slotRadius +
                                                 simCompStyles.slotMargin +
                                                 simCompStyles.slotLabelSize) *
                                                2;

    constexpr float SCHEMATIC_VIEW_PIN_ROW_SIZE = compSchematicStyles.nameFontSize +
                                                  compSchematicStyles.strokeSize +
                                                  compSchematicStyles.pinRowGap;

} // namespace Bess::Canvas::Styles
