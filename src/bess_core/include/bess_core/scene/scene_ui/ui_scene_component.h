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

        std::optional<glm::vec4> padding;
        std::optional<glm::vec4> margin;
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
                getVec4(m_customStyle.margin, m_style.metrics.margin);

            m_style.metrics.padding =
                getVec4(m_customStyle.padding, m_style.metrics.padding);

            m_style.backgroundColor = getColor(m_customStyle.backgroundColor,
                                               m_style.backgroundColor);

            m_style.hoverColor =
                getColor(m_customStyle.hoverColor, m_style.hoverColor);

            m_style.borderColor =
                getColor(m_customStyle.borderColor, m_style.borderColor);

            m_style.activeColor =
                getColor(m_customStyle.activeColor, m_style.activeColor);
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
            quadProps.thickness = m_style.metrics.borderSize;
            quadProps.radius = m_style.metrics.borderRadius;
            quadProps.id = pickingId;

            state.renderer->drawQuad(quadProps);
        }

        void drawText(SceneDrawContext &state,
                      const std::string &text,
                      const glm::vec3 &pos) {
            PickingId pickingId{
                .runtimeId = m_runtimeId,
                .info = 0,
            };
            const auto offsetY = state.renderer->textCenterOffsetY(
                text,
                {
                    .fontSize = m_style.textStyle.fontSize,
                });

            const auto pos_ = glm::vec2(pos.x - (m_node->getSize().x * 0.5f),
                                        pos.y + offsetY);
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
        glm::vec4 getVec4(const std::optional<glm::vec4> &custom,
                          const glm::vec4 &defaultVal) {
            return custom.has_value() ? custom.value() : defaultVal;
        }

        Core::Style::Color getColor(const std::optional<Color> &custom,
                                    const Color &defaultVal) {
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
            drawText(state, m_name, m_node->getDrawPos());
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
            drawText(state, m_name, m_node->getDrawPos());
        }

      private:
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
} // namespace Bess::Canvas::UI
