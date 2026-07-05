#pragma once

#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/scene/scene_draw_context.h"
#include "bess_core/scene/scene_state/components/scene_component.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include "bess_core/scene/scene_ui/layout.h"
#include "bess_core/style/bess_theme.h"
#include "bess_core/style/color_scheme.h"
#include "common/bess_uuid.h"
#include "common/logger.h"
#include "common/types.h"
#include <optional>

namespace Bess::Canvas::UI {

    using Color = Core::Style::Color;

    struct UIElementStyle {
        std::optional<Core::Style::Color> backgroundColor;
        std::optional<Core::Style::Color> hoverColor;
        std::optional<Core::Style::Color> borderColor;
        std::optional<Core::Style::Color> activeColor;

        std::optional<Core::Style::Padding> padding;
        std::optional<Core::Style::Margin> margin;
    };

    class UISceneComponent : public SceneComponent {
      public:
        DEFAULT_CONTRS(UISceneComponent)

        MAKE_GETTER_SETTER_PTR(UINode, UINode, m_node);
        MAKE_GETTER_SETTER(UIElementStyle, Style, m_customStyle);

        std::vector<UUID> cleanup(SceneState &state,
                                  UUID caller = UUID::null) override {
            auto reg = state.getUINodeRegistry();

            std::vector<UUID> removedComponents;

            for (const auto &childId : m_childComponents) {
                if (auto childComp = state.getComponentByUuid(childId)) {
                    auto ids = childComp->cleanup(state, m_uuid);
                    removedComponents.insert(
                        removedComponents.end(), ids.begin(), ids.end());
                }
            }

            if (m_node != nullptr) {
                reg->removeNode(m_node->getId());
                m_node = nullptr;
                removedComponents.push_back(m_uuid);
            }

            return removedComponents;
        }

        void prepareUI(SceneUIPrepareCtx &state) override {
            initNode(state.sceneState->getUINodeRegistry());

            prepStyle(state.theme);

            const auto size = state.renderer->measureText(
                getName(),
                {
                    .fontSize = m_style.textStyle.fontSize,
                });

            m_node->setSize(size);
            m_node->setSizeUnit(Unit::pixel);
            m_node->setSizeConstraint(SizeContraint::fixed);

            m_node->setPadding(m_style.metrics.padding);
            m_node->setMargin(m_style.metrics.margin);

            if (state.parentNode != nullptr) {
                state.parentNode->addChild(m_node);
            }
        }

        bool onMouseEnter(const Events::MouseEnterEvent &e) override {
            m_hovered = true;
            return true;
        }

        bool onMouseLeave(const Events::MouseLeaveEvent &e) override {
            m_hovered = false;
            return true;
        }

      protected:
        void onNameChanged() override {
            makeUIDirty();
        }

        void initNode(const std::shared_ptr<UINodeRegistry> &reg) {
            if (m_node == nullptr) {
                m_node = reg->addNode(m_uuid);
                setUINode(m_node);
            }
            m_node->clearChildren();
        }

        void makeUIDirty() {
            setUIDirty(true);
            if (m_node != nullptr) {
                m_node->setSizeDirty(true);
                m_node->setPosDirty(true);
            }
        }

        void prepStyle(const std::shared_ptr<Core::Style::BessTheme> &theme) {
            BESS_ASSERT(theme != nullptr, "Theme must be set in context.");
            m_style = theme->generalElementStyle();

            m_style.metrics.margin =
                resolveOptional(m_customStyle.margin, m_style.metrics.margin);

            m_style.metrics.padding =
                resolveOptional(m_customStyle.padding, m_style.metrics.padding);

            m_style.backgroundColor = resolveOptional(
                m_customStyle.backgroundColor, m_style.backgroundColor);

            m_style.hoverColor =
                resolveOptional(m_customStyle.hoverColor, m_style.hoverColor);

            m_style.borderColor =
                resolveOptional(m_customStyle.borderColor, m_style.borderColor);

            m_style.activeColor =
                resolveOptional(m_customStyle.activeColor, m_style.activeColor);
        }

        void drawBgQuad(SceneDrawContext &state) {
            PickingId pickingId{
                .runtimeId = m_runtimeId,
                .info = 0,
            };

            Core::Renderer::QuadProps quadProps;
            quadProps.position = m_node->getDrawPos();
            quadProps.size = m_node->getDrawSize();
            quadProps.zIndex = m_node->getZVal();
            quadProps.color =
                m_hovered ? m_style.hoverColor : m_style.backgroundColor;
            quadProps.borderColor = m_style.borderColor;
            quadProps.thickness = m_style.metrics.borderSize.toVec4();
            quadProps.radius = m_style.metrics.borderRadius;
            quadProps.id = pickingId;

            state.renderer->drawQuad(quadProps);
        }

        void drawText(SceneDrawContext &state,
                      const std::string &text,
                      UINode *node) {

            PickingId pickingId{
                .runtimeId = m_runtimeId,
                .info = 0,
            };

            const auto offsetY = state.renderer->textCenterOffsetY(
                text,
                {
                    .fontSize = m_style.textStyle.fontSize,
                });

            auto pos = node->getDrawPos();
            const auto pos_ =
                glm::vec2(pos.x - (node->getSize().x * 0.5f), pos.y + offsetY);

            state.renderer->drawFont(text,
                                     {
                                         .position = pos_,
                                         .fontSize = m_style.textStyle.fontSize,
                                         .color = m_style.textStyle.textColor,
                                         .zIndex = pos.z + 0.0001f,
                                         .id = pickingId,
                                     });
        }

      protected:
        UINode *m_node = nullptr;
        bool m_hovered = false;
        Core::Style::ElementStyle m_style;
        UIElementStyle m_customStyle;

      private:
        template <typename T>
        T resolveOptional(const std::optional<T> &custom, const T &defaultVal) {
            return custom.has_value() ? custom.value() : defaultVal;
        }
    };

    class LabelComp : public UISceneComponent {
      public:
        DEFAULT_CONTRS(LabelComp)

        static std::shared_ptr<LabelComp> create(const std::string &label) {
            auto labelComp = std::make_shared<LabelComp>();
            labelComp->setName(label);
            return labelComp;
        }

        void draw(SceneDrawContext &state) override {
            drawText(state, m_name, m_node);
        }
    };

    typedef std::function<void()> UIButtonCallback;

    class ButtonComp : public UISceneComponent {
      public:
        DEFAULT_CONTRS(ButtonComp)

        static std::shared_ptr<ButtonComp>
        create(const std::string &label, const UIButtonCallback &callback) {
            auto button = std::make_shared<ButtonComp>();
            button->setName(label);
            button->setCallback(callback);
            return button;
        }

        bool onMouseButton(const Events::MouseButtonEvent &e) override {
            BESS_ASSERT(m_callback,
                        "UIButtonComponent must have a valid callback");

            if (e.action == Events::MouseClickAction::press &&
                e.button == Events::MouseButton::left) {
                m_callback();
            }
            return true;
        }

        MAKE_GETTER_SETTER(UIButtonCallback, Callback, m_callback)

        void update(TimeMs ts, SceneState &state) override {
            UISceneComponent::update(ts, state);
        }

        void draw(SceneDrawContext &state) override {
            drawBgQuad(state);
            drawText(state, m_name, m_labelNode);
        }

        void prepareUI(SceneUIPrepareCtx &state) override {
            initNode(state.sceneState->getUINodeRegistry());
            prepStyle(state.theme);

            if (m_labelNode == nullptr) {
                m_labelNode =
                    state.sceneState->getUINodeRegistry()->addNode(UUID());
            }

            const auto size = state.renderer->measureText(
                getName(),
                {
                    .fontSize = m_style.textStyle.fontSize,
                });

            m_labelNode->setSize(size);
            m_labelNode->setSizeUnit(Unit::pixel);
            m_labelNode->setSizeConstraint(SizeContraint::fixed);

            m_node->addChild(m_labelNode);

            m_node->setPadding(m_style.metrics.padding);
            m_node->setMargin(m_style.metrics.margin);

            if (state.parentNode != nullptr) {
                state.parentNode->addChild(m_node);
            }
        }

      private:
        UINode *m_labelNode = nullptr;
        UIButtonCallback m_callback;
    };

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
        create(const LayoutDirection &direction = LayoutDirection::horizontal) {
            auto container = std::make_shared<ContainerComp>();
            container->setDirection(direction);
            return container;
        }

        void draw(SceneDrawContext &state) override {
            if (m_drawBg) {
                drawBgQuad(state);
            }

            drawChildren(state);
        }

        void prepareUI(SceneUIPrepareCtx &state) override {
            initNode(state.sceneState->getUINodeRegistry());
            prepStyle(state.theme);

            m_node->setDirection(m_direction);
            m_node->setMainAxisAlignment(m_mainAxisAlignment);
            m_node->setCrossAxisAlignment(m_crossAxisAlignment);

            if (state.parentNode != nullptr) {
                state.parentNode->addChild(m_node);
            }

            prepChildren(state);
        }

      private:
        void prepChildren(SceneUIPrepareCtx &state) {
            auto prevParent = state.parentNode;
            state.parentNode = m_node;
            for (const auto &childId : m_childComponents) {
                auto childComp = state.sceneState->getComponentByUuid(childId);
                if (childComp == nullptr) {
                    BESS_WARN("Child component with UUID {} not found in "
                              "scene state.",
                              (uint64_t)childId);
                    continue;
                }
                childComp->prepareUI(state);
            }
            state.parentNode = prevParent;
        }

        void drawChildren(SceneDrawContext &state) {
            for (const auto &childId : m_childComponents) {
                auto childComp = state.sceneState->getComponentByUuid(childId);
                if (childComp != nullptr) {
                    childComp->draw(state);
                }
            }
        }

      private:
        LayoutDirection m_direction = LayoutDirection::horizontal;
        LayoutAlignment m_mainAxisAlignment = LayoutAlignment::start;
        LayoutAlignment m_crossAxisAlignment = LayoutAlignment::center;

        bool m_drawBg = false;
    };

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
               bool toggled = false) {
            auto toggleBtn = std::make_shared<ToggleBtnComp>();
            toggleBtn->m_name = label;
            toggleBtn->m_callback = callback;
            toggleBtn->m_toggled = toggled;
            return toggleBtn;
        }

        void draw(SceneDrawContext &state) override {
            if (m_showLabel) {
                drawText(state, m_name, m_labelNode);
            }

            Core::Renderer::QuadProps trackProps;
            trackProps.position = m_trackNode->getDrawPos();
            trackProps.size = m_trackNode->getDrawSize();
            trackProps.zIndex = m_trackNode->getZVal();
            trackProps.color = m_trackColor;
            trackProps.borderColor = m_style.borderColor;
            trackProps.thickness = m_style.metrics.borderSize.toVec4();
            trackProps.radius = m_style.metrics.borderRadius;
            trackProps.id = PickingId{.runtimeId = m_runtimeId, .info = 1};

            state.renderer->drawQuad(trackProps);
            trackProps.position.x +=
                m_toggled ? (m_trackNode->getDrawSize().x / 2.f) -
                                (m_thumbSize.x / 2.f) -
                                (m_style.metrics.borderSize.left)
                          : -(m_trackNode->getDrawSize().x / 2.f) +
                                ((m_thumbSize.x / 2.f) +
                                 m_style.metrics.borderSize.right);
            trackProps.size = m_thumbSize;
            trackProps.zIndex += 0.0001f;
            trackProps.thickness = glm::vec4(0.f);
            trackProps.color = m_toggled ? m_style.activeColor : m_thumbColor;
            state.renderer->drawQuad(trackProps);
        }

        bool onMouseButton(const Events::MouseButtonEvent &e) override {
            if (e.action == Events::MouseClickAction::press &&
                e.button == Events::MouseButton::left && e.details == 1) {
                m_toggled = !m_toggled;
                if (m_callback) {
                    m_callback(m_toggled);
                }
                return true;
            }

            return false;
        }

        void prepareUI(SceneUIPrepareCtx &state) override {
            initNode(state.sceneState->getUINodeRegistry());

            if (m_labelNode == nullptr || m_trackNode == nullptr) {
                m_labelNode =
                    state.sceneState->getUINodeRegistry()->addNode(UUID());
                m_trackNode =
                    state.sceneState->getUINodeRegistry()->addNode(UUID());
            }

            prepStyle(state.theme);
            const auto &colors = state.theme->getColorScheme().getColors();
            m_style.metrics.borderSize = Core::Style::BorderSize(0.f);
            m_trackColor = colors.secondaryContainer;
            m_thumbColor = colors.tertiaryContainer;

            m_node->setDirection(LayoutDirection::horizontal);
            m_node->setSizeConstraint(SizeContraint::wrap_content);
            m_node->setCrossAxisAlignment(LayoutAlignment::center);
            m_node->setPadding(m_style.metrics.padding);
            m_node->setMargin(m_style.metrics.margin);

            if (m_showLabel) {
                auto labelSize = state.renderer->measureText(
                    m_name,
                    {
                        .fontSize = m_style.textStyle.fontSize,
                    });

                m_labelNode->setSize(labelSize);
                m_labelNode->setSizeUnit(Unit::pixel);
                m_labelNode->setSizeConstraint(SizeContraint::fixed);
                m_labelNode->setPosMode(PosMode::relative);
                m_labelNode->setPadding(0.f);
                m_labelNode->setMargin(
                    Core::Style::Margin::onlyRight(m_labelTrackSpacing));

                m_node->addChild(m_labelNode);
            }

            m_node->addChild(m_trackNode);

            m_trackNode->setSize(m_trackSize);
            m_trackNode->setSizeUnit(Unit::pixel);
            m_trackNode->setSizeConstraint(SizeContraint::fixed);
            m_trackNode->setPosMode(PosMode::relative);
            m_trackNode->setPos(glm::vec2(0.f));
            m_trackNode->setPadding(0.f);
            m_trackNode->setMargin(0.f);

            if (state.parentNode != nullptr) {
                state.parentNode->addChild(m_node);
            }
        }

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
