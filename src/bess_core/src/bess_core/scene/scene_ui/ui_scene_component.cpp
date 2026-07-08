#include "bess_core/scene/scene_ui/ui_scene_component.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include "common/bess_assert.h"
#include "common/bess_uuid.h"

namespace Bess::Canvas::UI {
    namespace {
        template <typename T>
        T resolveOptional(const std::optional<T> &custom, const T &defaultVal) {
            return custom.has_value() ? custom.value() : defaultVal;
        }
    } // namespace

    std::vector<UUID> UISceneComponent::cleanup(SceneState &state,
                                                UUID caller) {
        (void)caller;

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

    void UISceneComponent::prepareUI(SceneUIPrepareCtx &state) {
        initNode(state.sceneState->getUINodeRegistry());

        prepStyle(state.theme);

        const auto size = state.renderer->measureText(
            getName(),
            {
                .fontSize = m_style.textStyle.fontSize,
            });

        m_node->setWidth(size.x);
        m_node->setHeight(size.y);

        m_node->setPadding(m_style.metrics.padding);
        m_node->setMargin(m_style.metrics.margin);

        if (state.parentNode != nullptr) {
            state.parentNode->addChild(m_node);
        }
    }

    bool UISceneComponent::onMouseEnter(const Events::MouseEnterEvent &e) {
        (void)e;
        m_hovered = true;
        return true;
    }

    bool UISceneComponent::onMouseLeave(const Events::MouseLeaveEvent &e) {
        (void)e;
        m_hovered = false;
        return true;
    }

    uint32_t UISceneComponent::resolveRuntimeId() const {
        return resolveOptional(m_drawRuntimeId, m_runtimeId);
    }

    void UISceneComponent::onNameChanged() {
        makeUIDirty();
    }

    void
    UISceneComponent::initNode(const std::shared_ptr<UINodeRegistry> &reg) {
        if (m_node == nullptr) {
            m_node = reg->addNode(m_uuid);
            setUINode(m_node);
        }
        m_node->clearChildren();
        m_node->setZVal(m_transform.position.z);
        m_node->setPos(m_transform.position);
        m_node->setPadding(m_style.metrics.padding);
        m_node->setMargin(m_style.metrics.margin);
    }

    void UISceneComponent::makeUIDirty() {
        setUIDirty(true);
        if (m_node != nullptr) {
            m_node->setSizeDirty(true);
            m_node->setPosDirty(true);
        }
    }

    void UISceneComponent::prepStyle(
        const std::shared_ptr<Core::Style::BessTheme> &theme) {
        BESS_ASSERT(theme != nullptr, "Theme must be set in context.");
        m_style = theme->generalElementStyle();

        m_style.metrics.margin =
            resolveOptional(m_customStyle.margin, m_style.metrics.margin);

        m_style.metrics.padding =
            resolveOptional(m_customStyle.padding, m_style.metrics.padding);

        m_style.backgroundColor = resolveOptional(m_customStyle.backgroundColor,
                                                  m_style.backgroundColor);

        m_style.hoverColor =
            resolveOptional(m_customStyle.hoverColor, m_style.hoverColor);

        m_style.borderColor =
            resolveOptional(m_customStyle.borderColor, m_style.borderColor);

        m_style.activeColor =
            resolveOptional(m_customStyle.activeColor, m_style.activeColor);

        m_style.textStyle.fontSize =
            resolveOptional(m_customStyle.fontSize, m_style.textStyle.fontSize);
    }

    void UISceneComponent::drawBgQuad(SceneDrawContext &state) {
        PickingId pickingId{
            .runtimeId = m_runtimeId,
            .info = 0,
        };

        Core::Renderer::QuadProps quadProps;
        quadProps.position = m_node->getDrawPos();
        quadProps.size = m_node->getDrawSize();
        quadProps.zIndex = m_node->getDrawPos().z;
        quadProps.color =
            m_hovered ? m_style.hoverColor : m_style.backgroundColor;
        quadProps.borderColor = m_style.borderColor;
        quadProps.thickness = m_style.metrics.borderSize.toVec4();
        quadProps.radius = m_style.metrics.borderRadius;
        quadProps.id = pickingId;

        state.renderer->drawQuad(quadProps);
    }

    void UISceneComponent::drawText(SceneDrawContext &state,
                                    const std::string &text,
                                    UINode *node) {

        SceneComponent *parent = nullptr;

        if (m_parentComponent != UUID::null) {
            parent = state.sceneState->getComponentByUuid(m_parentComponent);
        }

        PickingId pickingId{
            .runtimeId = resolveRuntimeId(),
            .info = 0,
        };

        const auto offsetY = state.renderer->textCenterOffsetY(
            text,
            {
                .fontSize = m_style.textStyle.fontSize,
            });

        auto pos = node->getDrawPos();
        const auto pos_ =
            glm::vec2(pos.x - (node->getDrawSize().x * 0.5f), pos.y + offsetY);

        state.renderer->drawFont(text,
                                 {
                                     .position = pos_,
                                     .fontSize = m_style.textStyle.fontSize,
                                     .color = m_style.textStyle.textColor,
                                     .zIndex = pos.z,
                                     .id = pickingId,
                                 });
    }
} // namespace Bess::Canvas::UI
