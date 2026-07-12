#include "bess_core/scene/scene_ui/controls/spacer_comp.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include <algorithm>
#include <cmath>

namespace Bess::Canvas::UI {
    namespace {
        float nonNegativeFinite(float value) {
            return std::max(0.f, std::isfinite(value) ? value : 0.f);
        }
    } // namespace

    std::shared_ptr<SpacerComp>
    SpacerComp::create(const CompConfig &config) {
        auto spacer = std::make_shared<SpacerComp>();
        applyCompConfig(spacer, config);
        return spacer;
    }

    std::shared_ptr<SpacerComp>
    SpacerComp::create(float grow, const CompConfig &config) {
        auto spacer = create(config);
        spacer->setFlexGrow(grow);
        return spacer;
    }

    std::shared_ptr<SpacerComp>
    SpacerComp::createFixed(float size, const CompConfig &config) {
        auto spacer = create(config);
        spacer->setFixedSize(size);
        return spacer;
    }

    float SpacerComp::getFlexGrow() const {
        return m_flexGrow;
    }

    void SpacerComp::setFlexGrow(float grow) {
        grow = nonNegativeFinite(grow);
        if (m_flexGrow == grow) {
            return;
        }
        m_flexGrow = grow;
        makeUIDirty();
    }

    float SpacerComp::getFlexShrink() const {
        return m_flexShrink;
    }

    void SpacerComp::setFlexShrink(float shrink) {
        shrink = nonNegativeFinite(shrink);
        if (m_flexShrink == shrink) {
            return;
        }
        m_flexShrink = shrink;
        makeUIDirty();
    }

    float SpacerComp::getFlexBasis() const {
        return m_flexBasis;
    }

    Unit SpacerComp::getFlexBasisUnit() const {
        return m_flexBasisUnit;
    }

    void SpacerComp::setFlexBasis(float basis, Unit unit) {
        basis = nonNegativeFinite(basis);
        if (m_flexBasis == basis && m_flexBasisUnit == unit) {
            return;
        }
        m_flexBasis = basis;
        m_flexBasisUnit = unit;
        makeUIDirty();
    }

    void SpacerComp::setFlex(float grow,
                             float shrink,
                             float basis,
                             Unit basisUnit) {
        const auto sanitizedGrow = nonNegativeFinite(grow);
        const auto sanitizedShrink = nonNegativeFinite(shrink);
        const auto sanitizedBasis = nonNegativeFinite(basis);

        if (m_flexGrow == sanitizedGrow &&
            m_flexShrink == sanitizedShrink &&
            m_flexBasis == sanitizedBasis && m_flexBasisUnit == basisUnit) {
            return;
        }

        m_flexGrow = sanitizedGrow;
        m_flexShrink = sanitizedShrink;
        m_flexBasis = sanitizedBasis;
        m_flexBasisUnit = basisUnit;
        makeUIDirty();
    }

    void SpacerComp::setFixedSize(float size) {
        setFlex(0.f, 0.f, size);
    }

    void SpacerComp::prepareUI(SceneUIPrepareCtx &state) {
        prepStyle(state.theme);
        initNode(state.sceneState->getUINodeRegistry());

        m_node->setWidth(0.f);
        m_node->setHeight(0.f);
        m_node->setFlexGrow(m_flexGrow);
        m_node->setFlexShrink(m_flexShrink);
        m_node->setFlexBasis(m_flexBasis, m_flexBasisUnit);
        applyCustomLayoutStyle();

        if (state.parentNode != nullptr) {
            state.parentNode->addChild(m_node);
        }

        m_isUIDirty = false;
    }

    void SpacerComp::onDraw(SceneDrawContext &state) {
        (void)state;
    }

    Core::Viewport::SceneCursor SpacerComp::getCursor() const {
        return Core::Viewport::SceneCursor::normal;
    }

    void SpacerComp::prepStyle(
        const std::shared_ptr<Core::Style::BessTheme> &theme) {
        (void)theme;
        m_style = {};
        m_style.metrics.padding =
            m_customStyle.padding.value_or(Core::Style::Padding::zero());
        m_style.metrics.margin =
            m_customStyle.margin.value_or(Core::Style::Margin::zero());
    }
} // namespace Bess::Canvas::UI
