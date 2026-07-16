#pragma once

#include "common/bess_api.h"

#include "bess_core/scene/scene_ui/ui_scene_component.h"
#include <memory>

namespace Bess::Canvas::UI {

    class BESS_API SpacerComp : public UISceneComponent {
      public:
        DEFAULT_CONTRS(SpacerComp)

        static std::shared_ptr<SpacerComp>
        create(const CompConfig &config = CompConfig{});
        static std::shared_ptr<SpacerComp>
        create(float grow, const CompConfig &config = CompConfig{});
        static std::shared_ptr<SpacerComp>
        createFixed(float size, const CompConfig &config = CompConfig{});

        float getFlexGrow() const;
        void setFlexGrow(float grow);

        float getFlexShrink() const;
        void setFlexShrink(float shrink);

        float getFlexBasis() const;
        Unit getFlexBasisUnit() const;
        void setFlexBasis(float basis, Unit unit = Unit::pixel);

        void setFlex(float grow,
                     float shrink = 1.f,
                     float basis = 0.f,
                     Unit basisUnit = Unit::pixel);
        void setFixedSize(float size);

        void prepareUI(SceneUIPrepareCtx &state) override;
        void onDraw(SceneDrawContext &state) override;
        Core::Viewport::SceneCursor getCursor() const override;

      private:
        void prepStyle(
            const std::shared_ptr<Core::Style::BessTheme> &theme) override;

        float m_flexGrow = 1.f;
        float m_flexShrink = 1.f;
        float m_flexBasis = 0.f;
        Unit m_flexBasisUnit = Unit::pixel;
    };
} // namespace Bess::Canvas::UI
