#include "bess_core/scene/scene_ui/controls/segmented_button_comp.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/scene/scene_event.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include <algorithm>

namespace Bess::Canvas::UI {
    namespace {
        constexpr uint32_t kSegmentInfoBase = 100u;
        constexpr float kMinSegmentHeight = 18.f;

        [[nodiscard]] Color disabledTextColor(const Color &color) {
            return color.withAlpha(0.42f);
        }
    } // namespace

    std::shared_ptr<SegmentedButtonComp>
    SegmentedButtonComp::create(const CompConfig &config) {
        return create({}, 0, nullptr, config);
    }

    std::shared_ptr<SegmentedButtonComp>
    SegmentedButtonComp::create(
        const std::vector<UISegmentedButtonOption> &options,
        size_t selectedIndex,
        const UISegmentedButtonCallback &callback,
        const CompConfig &config) {
        auto button = std::make_shared<SegmentedButtonComp>();
        button->setOptions(options);
        button->setSelectedIndex(selectedIndex);
        button->setCallback(callback);
        applyCompConfig(button, config);
        return button;
    }

    void SegmentedButtonComp::setOptions(
        const std::vector<UISegmentedButtonOption> &options) {
        m_options = options;
        m_selectedIndex = validSelectedIndex(m_selectedIndex);
        m_hoveredInfo = 0u;
        makeUIDirty();
    }

    const std::vector<UISegmentedButtonOption> &
    SegmentedButtonComp::getOptions() const {
        return m_options;
    }

    size_t SegmentedButtonComp::getSelectedIndex() const {
        return m_selectedIndex;
    }

    void SegmentedButtonComp::setSelectedIndex(size_t index) {
        m_selectedIndex = validSelectedIndex(index);
    }

    void SegmentedButtonComp::onDraw(SceneDrawContext &state) {
        if (m_node == nullptr || state.renderer == nullptr ||
            m_options.empty()) {
            return;
        }

        drawGroupFrame(state);
        const float border = borderThickness();

        for (size_t index = 0; index < m_options.size(); ++index) {
            if (index >= m_segmentNodes.size() ||
                m_segmentNodes[index] == nullptr) {
                break;
            }

            const auto info = kSegmentInfoBase + static_cast<uint32_t>(index);
            const PickingId id{
                .runtimeId = resolveRuntimeId(),
                .info = info,
            };
            const bool enabled = m_options[index].enabled;
            const bool selected = index == m_selectedIndex && enabled;
            const bool hovered = m_hoveredInfo == info && enabled;
            const auto *segmentNode = m_segmentNodes[index];
            const bool first = index == 0;
            const bool last = index + 1 == m_options.size();
            const float leftInset = first ? border : 0.f;
            const float rightInset = last ? border : 0.f;

            Core::Renderer::QuadProps props;
            props.position = segmentNode->getDrawPos();
            props.position.x += (leftInset - rightInset) * 0.5f;
            props.size = {
                std::max(1.f,
                         segmentNode->getDrawSize().x - leftInset -
                             rightInset),
                std::max(1.f, segmentNode->getDrawSize().y - (border * 2.f)),
            };
            props.zIndex = segmentNode->getDrawPos().z + 0.0001f;
            props.color = selected ? m_style.activeColor
                          : hovered ? m_style.hoverColor
                                    : m_style.backgroundColor;
            props.thickness = glm::vec4(0.f);
            props.radius = segmentRadius(index);
            props.id = id;
            props.transformMode = state.transformMode;
            state.renderer->drawQuad(props);
        }

        drawSeparators(state);

        for (size_t index = 0; index < m_options.size(); ++index) {
            const auto info = kSegmentInfoBase + static_cast<uint32_t>(index);
            const PickingId id{
                .runtimeId = resolveRuntimeId(),
                .info = info,
            };
            const bool enabled = m_options[index].enabled;
            const bool selected = index == m_selectedIndex && enabled;
            const auto textColor =
                !enabled ? disabledTextColor(m_style.textStyle.textColor)
                : selected
                    ? m_selectedTextColor
                    : m_style.textStyle.textColor;
            drawSegmentText(state, index, id, textColor);
        }
    }

    void SegmentedButtonComp::prepareUI(SceneUIPrepareCtx &state) {
        prepStyle(state.theme);
        initNode(state.sceneState->getUINodeRegistry());
        ensureNodes(state.sceneState->getUINodeRegistry());

        m_cachedSegmentSizes.assign(m_options.size(), glm::vec2{0.f});

        float maxWidth = std::max(1.f, m_segmentSize.x);
        float maxHeight = std::max(kMinSegmentHeight, m_segmentSize.y);

        for (size_t index = 0; index < m_options.size(); ++index) {
            const auto labelSize = state.renderer->measureText(
                m_options[index].label,
                {
                    .fontSize = m_style.textStyle.fontSize,
                });
            const glm::vec2 contentSize{
                labelSize.x + m_style.metrics.padding.horizontal(),
                labelSize.y + m_style.metrics.padding.vertical(),
            };
            auto segmentSize = m_segmentSize;
            if (segmentSize.x <= 0.f) {
                segmentSize.x = contentSize.x;
            }
            if (segmentSize.y <= 0.f) {
                segmentSize.y = contentSize.y;
            }
            segmentSize.x =
                std::max({m_minSegmentWidth, segmentSize.x, contentSize.x});
            segmentSize.y =
                std::max({kMinSegmentHeight, segmentSize.y, contentSize.y});
            m_cachedSegmentSizes[index] = segmentSize;
            maxWidth = std::max(maxWidth, segmentSize.x);
            maxHeight = std::max(maxHeight, segmentSize.y);
        }

        float totalWidth = 0.f;
        for (auto &size : m_cachedSegmentSizes) {
            if (m_equalSegmentWidths) {
                size.x = maxWidth;
            }
            size.y = maxHeight;
            totalWidth += size.x;
        }

        m_node->setDirection(LayoutDirection::horizontal);
        m_node->setWidth(std::max(1.f, totalWidth));
        m_node->setHeight(std::max(kMinSegmentHeight, maxHeight));
        m_node->setCrossAxisAlignment(LayoutAlignment::center);
        m_node->setPadding(0.f);
        m_node->setMargin(m_style.metrics.margin);

        for (size_t index = 0; index < m_options.size(); ++index) {
            const auto *option = &m_options[index];
            auto *segmentNode = m_segmentNodes[index];
            auto *labelNode = m_labelNodes[index];

            segmentNode->setDirection(LayoutDirection::horizontal);
            segmentNode->setMainAxisAlignment(LayoutAlignment::center);
            segmentNode->setCrossAxisAlignment(LayoutAlignment::center);
            segmentNode->setWidth(m_cachedSegmentSizes[index].x);
            segmentNode->setHeight(m_cachedSegmentSizes[index].y);
            segmentNode->setPosMode(PosMode::relative);
            segmentNode->setPadding(0.f);
            segmentNode->setMargin(0.f);
            segmentNode->clearChildren();

            const auto labelSize = state.renderer->measureText(
                option->label,
                {
                    .fontSize = m_style.textStyle.fontSize,
                });
            labelNode->setWidth(labelSize.x);
            labelNode->setHeight(labelSize.y);
            labelNode->setPosMode(PosMode::relative);
            labelNode->setPadding(0.f);
            labelNode->setMargin(0.f);
            segmentNode->addChild(labelNode);
            m_node->addChild(segmentNode);
        }

        applyCustomLayoutStyle();

        if (state.parentNode != nullptr) {
            state.parentNode->addChild(m_node);
        }

        m_isUIDirty = false;
    }

    void SegmentedButtonComp::prepStyle(
        const std::shared_ptr<Core::Style::BessTheme> &theme) {
        UISceneComponent::prepStyle(theme);
        m_selectedTextColor = theme->getColorScheme().getColors().onPrimary;
    }

    bool SegmentedButtonComp::onMouseEnter(const Events::MouseEnterEvent &e) {
        UISceneComponent::onMouseEnter(e);
        m_hoveredInfo = e.details;
        return true;
    }

    bool SegmentedButtonComp::onMouseLeave(const Events::MouseLeaveEvent &e) {
        UISceneComponent::onMouseLeave(e);
        m_hoveredInfo = 0u;
        return true;
    }

    bool SegmentedButtonComp::onMouseButton(
        const Events::MouseButtonEvent &e) {
        if (e.button != Events::MouseButton::left) {
            return false;
        }

        if (e.action != Events::MouseClickAction::press) {
            return e.action == Events::MouseClickAction::release &&
                   isSegmentInfo(e.details);
        }

        if (isSegmentInfo(e.details)) {
            selectFromUser(segmentIndexFromInfo(e.details));
            return true;
        }

        return false;
    }

    bool SegmentedButtonComp::isFocusable() const {
        return true;
    }

    bool SegmentedButtonComp::wantsKeyboardInput() const {
        return m_focused;
    }

    bool SegmentedButtonComp::onKeyEvent(const SceneEvent &evt) {
        if (evt.type != SceneEvent::Type::key ||
            evt.data.keyPress.action != KeyAction::press ||
            m_options.empty()) {
            return false;
        }

        if (evt.data.keyPress.keycode == KeyCode::arrowRight) {
            selectFromUser(nextEnabledIndex(m_selectedIndex));
            return true;
        }

        if (evt.data.keyPress.keycode == KeyCode::arrowLeft) {
            selectFromUser(previousEnabledIndex(m_selectedIndex));
            return true;
        }

        if (evt.data.keyPress.keycode == KeyCode::space ||
            evt.data.keyPress.keycode == KeyCode::enter) {
            selectFromUser(m_selectedIndex);
            return true;
        }

        return false;
    }

    bool SegmentedButtonComp::isSegmentInfo(uint32_t info) const {
        if (info < kSegmentInfoBase) {
            return false;
        }
        return segmentIndexFromInfo(info) < m_options.size();
    }

    size_t SegmentedButtonComp::segmentIndexFromInfo(uint32_t info) const {
        return static_cast<size_t>(info - kSegmentInfoBase);
    }

    size_t SegmentedButtonComp::validSelectedIndex(size_t index) const {
        if (m_options.empty()) {
            return 0;
        }

        if (index < m_options.size() && m_options[index].enabled) {
            return index;
        }

        for (size_t i = 0; i < m_options.size(); ++i) {
            if (m_options[i].enabled) {
                return i;
            }
        }

        return 0;
    }

    size_t SegmentedButtonComp::nextEnabledIndex(size_t from) const {
        if (m_options.empty()) {
            return 0;
        }

        const auto start = std::min(from, m_options.size() - 1);
        for (size_t i = start + 1; i < m_options.size(); ++i) {
            if (m_options[i].enabled) {
                return i;
            }
        }
        return validSelectedIndex(start);
    }

    size_t SegmentedButtonComp::previousEnabledIndex(size_t from) const {
        if (m_options.empty()) {
            return 0;
        }

        const auto start = std::min(from, m_options.size() - 1);
        for (size_t i = start; i > 0; --i) {
            const auto candidate = i - 1;
            if (m_options[candidate].enabled) {
                return candidate;
            }
        }
        return validSelectedIndex(start);
    }

    glm::vec4 SegmentedButtonComp::segmentRadius(size_t index) const {
        const auto radius = m_style.metrics.borderRadius;
        const float border = borderThickness();
        const auto innerRadius =
            glm::vec4{std::max(0.f, radius.x - border),
                      std::max(0.f, radius.y - border),
                      std::max(0.f, radius.z - border),
                      std::max(0.f, radius.w - border)};
        if (m_options.size() <= 1) {
            return innerRadius;
        }

        if (index == 0) {
            return {innerRadius.x, 0.f, 0.f, innerRadius.w};
        }

        if (index + 1 == m_options.size()) {
            return {0.f, innerRadius.y, innerRadius.z, 0.f};
        }

        return {0.f, 0.f, 0.f, 0.f};
    }

    float SegmentedButtonComp::borderThickness() const {
        return std::max({m_style.metrics.borderSize.top,
                         m_style.metrics.borderSize.right,
                         m_style.metrics.borderSize.bottom,
                         m_style.metrics.borderSize.left,
                         0.f});
    }

    void SegmentedButtonComp::selectFromUser(size_t index) {
        if (index >= m_options.size() || !m_options[index].enabled) {
            return;
        }

        const bool changed = index != m_selectedIndex;
        m_selectedIndex = index;
        if (changed && m_callback) {
            m_callback(m_selectedIndex, m_options[m_selectedIndex]);
        }
    }

    void SegmentedButtonComp::ensureNodes(
        const std::shared_ptr<UINodeRegistry> &reg) {
        while (m_segmentNodes.size() < m_options.size()) {
            m_segmentNodes.push_back(reg->addNode(UUID()));
        }
        while (m_labelNodes.size() < m_options.size()) {
            m_labelNodes.push_back(reg->addNode(UUID()));
        }
    }

    void SegmentedButtonComp::drawGroupFrame(SceneDrawContext &state) const {
        if (m_node == nullptr || state.renderer == nullptr) {
            return;
        }

        Core::Renderer::QuadProps props;
        props.position = m_node->getDrawPos();
        props.size = m_node->getDrawSize();
        props.zIndex = m_node->getDrawPos().z;
        props.color = m_style.backgroundColor;
        props.borderColor = m_focused ? m_style.activeColor
                                      : m_style.borderColor;
        props.thickness = m_style.metrics.borderSize.toVec4();
        props.radius = m_style.metrics.borderRadius;
        props.shadow = m_style.shadowProps;
        props.id = PickingId::invalid();
        props.transformMode = state.transformMode;
        state.renderer->drawQuad(props);
    }

    void SegmentedButtonComp::drawSeparators(SceneDrawContext &state) const {
        if (m_node == nullptr || state.renderer == nullptr ||
            m_options.size() < 2) {
            return;
        }

        const float border = borderThickness();
        if (border <= 0.f) {
            return;
        }

        const auto groupPos = m_node->getDrawPos();
        const auto groupSize = m_node->getDrawSize();
        const auto separatorHeight =
            std::max(1.f, groupSize.y - (border * 2.f));
        const auto separatorWidth = std::max(1.f, border);

        for (size_t index = 1; index < m_options.size(); ++index) {
            if (index >= m_segmentNodes.size() ||
                m_segmentNodes[index] == nullptr) {
                break;
            }

            const auto *segmentNode = m_segmentNodes[index];
            const float x = segmentNode->getDrawPos().x -
                            (segmentNode->getDrawSize().x * 0.5f);
            Core::Renderer::QuadProps props;
            props.position = {x, groupPos.y};
            props.size = {separatorWidth, separatorHeight};
            props.zIndex = groupPos.z + 0.0002f;
            props.color = m_style.borderColor;
            props.id = PickingId{
                .runtimeId = resolveRuntimeId(),
                .info = kSegmentInfoBase + static_cast<uint32_t>(index),
            };
            props.transformMode = state.transformMode;
            state.renderer->drawQuad(props);
        }
    }

    void SegmentedButtonComp::drawSegmentText(SceneDrawContext &state,
                                              size_t index,
                                              const PickingId &id,
                                              const Color &color) const {
        if (index >= m_labelNodes.size() || m_labelNodes[index] == nullptr ||
            state.renderer == nullptr) {
            return;
        }

        const auto &label = m_options[index].label;
        const auto *labelNode = m_labelNodes[index];
        const auto offsetY = state.renderer->textCenterOffsetY(
            label,
            {
                .fontSize = m_style.textStyle.fontSize,
            });
        const auto pos = labelNode->getDrawPos();
        const auto drawPos =
            glm::vec2{pos.x - (labelNode->getDrawSize().x * 0.5f),
                      pos.y + offsetY};

        state.renderer->drawFont(label,
                                 {
                                     .position = drawPos,
                                     .fontSize = m_style.textStyle.fontSize,
                                     .color = color,
                                     .zIndex = pos.z + 0.0003f,
                                     .id = id,
                                     .transformMode = state.transformMode,
                                 });
    }
} // namespace Bess::Canvas::UI
