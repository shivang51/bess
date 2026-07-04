#pragma once
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/scene/scene_draw_context.h"
#include "bess_core/scene/scene_state/components/scene_component.h"
#include "bess_core/scene/scene_ui/layout.h"
#include "bess_core/style/bess_theme.h"
#include "common/types.h"

namespace Bess::Canvas::UI {
    class UISceneComponent : public SceneComponent {
      public:
        DEFAULT_CONTRS(UISceneComponent)

        MAKE_GETTER_SETTER_PTR(UINode, UINode, m_node);

        void prepareUI(SceneUIPrepareCtx &state) override {
            prepStyle(state);

            const auto labelSize = state.renderer->measureText(
                getName(),
                {
                    .fontSize = m_style.textStyle.fontSize,
                });

            initNode({
                labelSize.x,
                labelSize.y,
            });

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
        void initNode(const glm::vec2 &size,
                      const Unit &sizeUnit = Unit::pixel) {
            BESS_ASSERT(m_node != nullptr,
                        "UINode must be initialized before setting size.");
            m_node->setSize(size);
            m_node->setSizeUnit(sizeUnit);
            m_node->setSizeConstraint(SizeContraint::fixed);
        }

        void prepStyle(SceneUIPrepareCtx &ctx) {
            BESS_ASSERT(ctx.theme != nullptr, "Theme must be set in context.");
            m_style = ctx.theme->generalElementStyle();
            m_style.metrics.margin = glm::vec4(0);
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

            m_callback();
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
} // namespace Bess::Canvas::UI
