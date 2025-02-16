#include "scene_new/scene.h"
#include "GLFW/glfw3.h"
#include "entt/entity/fwd.hpp"
#include "events/application_event.h"
#include "ext/matrix_transform.hpp"
#include "ext/vector_float2.hpp"
#include "ext/vector_float3.hpp"
#include "ext/vector_float4.hpp"
#include "gtc/type_ptr.hpp"
#include "pages/main_page/main_page_state.h"
#include "scene/renderer/renderer.h"
#include "scene_new/components/components.h"
#include "settings/viewport_theme.h"
#include "ui/ui_main/ui_main.h"
#include <cstdint>

using Renderer = Bess::Renderer2D::Renderer;

namespace Bess::Canvas {
    Scene::Scene() {
        Renderer2D::Renderer::init();
        reset();
        auto fn = [&](Components::TransformComponent &tc, Components::SpriteComponent &sc, Components::TagComponent &tagC) {
            auto [pos, _, scale] = tc.decompose();
            auto entt = (entt::entity)tagC.id;
            bool isHovered = this->m_hoveredEntiy == entt;
            bool isSelected = this->m_registry.all_of<Components::SelectedComponent>(entt);
            auto borderColor = sc.borderColor;
            if (isHovered)
                borderColor = glm::vec4(1.f, 1.f, 0.f, 1.f);
            if (isSelected)
                borderColor = glm::vec4(1.f, 1.f, 0.5f, 1.f);
            Renderer2D::Renderer::quad(pos, scale,
                                       sc.color, tagC.id,
                                       sc.borderRadius, sc.borderSize, borderColor);
        };

        {
            auto entity = m_registry.create();
            auto &transformComponent = m_registry.emplace<Components::TransformComponent>(entity);
            auto &renderComponent = m_registry.emplace<Components::RenderComponent>(entity);
            auto &spriteComponent = m_registry.emplace<Components::SpriteComponent>(entity);
            auto &tagComponent = m_registry.emplace<Components::TagComponent>(entity);

            tagComponent.name = "Something something";
            tagComponent.id = (uint64_t)entity;

            spriteComponent.color = glm::vec4(1.f, 1.f, 1.f, 0.1f);
            spriteComponent.borderColor = glm::vec4(1.f, 0.f, 0.f, 1.f);
            spriteComponent.borderSize = glm::vec4(2.f);
            spriteComponent.borderRadius = glm::vec4(50.f);

            glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.f));
            transform = glm::rotate(transform, 0.f, {0.f, 0.f, 1.f});
            transform = glm::scale(transform, {100.f, 100.f, 1.f});

            transformComponent = transform;

            renderComponent.render = fn;
        }
        {
            auto entity = m_registry.create();
            auto &transformComponent = m_registry.emplace<Components::TransformComponent>(entity);
            auto &renderComponent = m_registry.emplace<Components::RenderComponent>(entity);
            auto &spriteComponent = m_registry.emplace<Components::SpriteComponent>(entity);
            auto &tagComponent = m_registry.emplace<Components::TagComponent>(entity);

            tagComponent.name = "Something something";
            tagComponent.id = (uint64_t)entity;

            spriteComponent.color = glm::vec4(1.f, 1.f, 1.f, 0.1f);
            spriteComponent.borderColor = glm::vec4(1.f, 0.f, 1.f, 1.f);
            spriteComponent.borderSize = glm::vec4(2.f);
            spriteComponent.borderRadius = glm::vec4(50.f);

            glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(100.f, 100.f, 0.f));
            transform = glm::rotate(transform, 0.f, {0.f, 0.f, 1.f});
            transform = glm::scale(transform, {100.f, 100.f, 1.f});

            transformComponent = transform;

            renderComponent.render = fn;
        }
    }

    Scene::~Scene() {}

    void Scene::reset() {
        m_size = glm::vec2(800.f / 600.f, 1.f);
        m_camera = std::make_shared<Camera>(m_size.x, m_size.y);
        std::vector<Gl::FBAttachmentType> attachments = {Gl::FBAttachmentType::RGBA_RGBA, Gl::FBAttachmentType::R32I_REDI, Gl::DEPTH32F_STENCIL8};
        m_msaaFramebuffer = std::make_unique<Gl::FrameBuffer>(m_size.x, m_size.y, attachments, true);

        attachments = {Gl::FBAttachmentType::RGB_RGB, Gl::FBAttachmentType::R32I_REDI};
        m_normalFramebuffer = std::make_unique<Gl::FrameBuffer>(m_size.x, m_size.y, attachments);
        m_registry.clear();
    }

    unsigned int Scene::getTextureId() {
        return m_normalFramebuffer->getColorBufferTexId(0);
    }

    void Scene::render() {
        beginScene();
        Renderer::grid({0.f, 0.f, -2.f}, m_camera->getSpan(), -1, ViewportTheme::gridColor);

        auto view = m_registry.view<Components::RenderComponent, Components::TransformComponent, Components::SpriteComponent, Components::TagComponent>();
        for (auto entity : view) {
            auto [rc, tc, sc, tagC] = view.get<Components::RenderComponent, Components::TransformComponent, Components::SpriteComponent, Components::TagComponent>(entity);
            rc.render(tc, sc, tagC);
        }

        endScene();
    }

    const glm::vec2 &Scene::getSize() {
        return m_size;
    }

    void Scene::resize(const glm::vec2 &size) {
        m_size = UI::UIMain::state.viewportSize;
        m_msaaFramebuffer->resize(UI::UIMain::state.viewportSize.x, UI::UIMain::state.viewportSize.y);
        m_normalFramebuffer->resize(UI::UIMain::state.viewportSize.x, UI::UIMain::state.viewportSize.y);
        m_camera->resize(UI::UIMain::state.viewportSize.x, UI::UIMain::state.viewportSize.y);
    }

    glm::vec2 Scene::getViewportMousePos(const glm::vec2 &mousePos) {
        const auto &viewportPos = UI::UIMain::state.viewportPos;
        auto x = mousePos.x - viewportPos.x;
        auto y = mousePos.y - viewportPos.y;
        return {x, y};
    }

    glm::vec2 Scene::getNVPMousePos(const glm::vec2 &mousePos) {
        glm::vec2 pos = mousePos;

        const auto cameraPos = m_camera->getPos();
        glm::mat4 tansform = glm::translate(glm::mat4(1.f), glm::vec3(cameraPos.x, cameraPos.y, 0.f));
        tansform = glm::scale(tansform, glm::vec3(1.f / UI::UIMain::state.cameraZoom, 1.f / UI::UIMain::state.cameraZoom, 1.f));

        pos = glm::vec2(tansform * glm::vec4(pos.x, pos.y, 0.f, 1.f));
        auto span = m_camera->getSpan() / 2.f;
        pos -= glm::vec2({span.x, span.y});
        return pos;
    }

    void Scene::onMouseMove(const glm::vec2 &pos) {
        auto dPos = getNVPMousePos(pos) - getNVPMousePos(m_mousePos);
        m_mousePos = pos;

        // reading the hoverid
        {
            auto m_mousePos_ = pos;
            m_mousePos_.y = UI::UIMain::state.viewportSize.y - m_mousePos_.y;
            int x = static_cast<int>(m_mousePos_.x);
            int y = static_cast<int>(m_mousePos_.y);
            int32_t hoverId = m_normalFramebuffer->readIntFromColAttachment(1, x, y);
            m_hoveredEntiy = (entt::entity)hoverId;
        }

        if (m_isLeftMousePressed) {
            auto view = m_registry.view<Components::SelectedComponent, Components::TransformComponent>();

            for (auto &ent : view) {
                auto &transformComp = view.get<Components::TransformComponent>(ent);
                auto dPos_ = glm::vec3(dPos, 0.f);
                transformComp.translate(transformComp.getPosition() + dPos_);
            }
        }
    }

    bool Scene::isCursorInViewport(const glm::vec2 &pos) {
        const auto &viewportPos = UI::UIMain::state.viewportPos;
        const auto &viewportSize = UI::UIMain::state.viewportSize;
        const auto mousePos = getViewportMousePos(pos);
        return mousePos.x >= 5.f &&
               mousePos.x < viewportSize.x - 5.f &&
               mousePos.y >= 5.f &&
               mousePos.y < viewportSize.y - 5.f;
    }

    void Scene::onLeftMouse(bool isPressed) {
        m_isLeftMousePressed = isPressed;
        if (!isPressed)
            return;

        // toggeling selection of hovered entity on click
        if (m_registry.valid(m_hoveredEntiy)) {
            bool isSelected = m_registry.all_of<Canvas::Components::SelectedComponent>(m_hoveredEntiy);
            if (!Pages::MainPageState::getInstance()->isKeyPressed(GLFW_KEY_LEFT_CONTROL)) {
                m_registry.clear<Components::SelectedComponent>();
                isSelected = false;
            }

            if (isSelected)
                m_registry.erase<Canvas::Components::SelectedComponent>(m_hoveredEntiy);
            else
                m_registry.emplace<Canvas::Components::SelectedComponent>(m_hoveredEntiy);
        } else { // deselecting all when clicking outside
            m_registry.clear<Components::SelectedComponent>();
        }
    }

    void Scene::update(const std::vector<ApplicationEvent> &events) {
        for (auto &event : events) {
            switch (event.getType()) {
            case ApplicationEventType::MouseMove: {
                const auto data = event.getData<ApplicationEvent::MouseMoveData>();
                glm::vec2 pos = {data.x, data.y};
                if (!isCursorInViewport(pos)) {
                    m_mousePos = pos;
                    break;
                }
                auto pos_ = getViewportMousePos(glm::vec2(data.x, data.y));
                onMouseMove(pos_);
            } break;
            case ApplicationEventType::MouseButton: {
                if (!isCursorInViewport(m_mousePos)) {
                    break;
                }
                const auto data = event.getData<ApplicationEvent::MouseButtonData>();
                if (data.button == MouseButton::left) {
                    onLeftMouse(data.pressed);
                } else if (data.button == MouseButton::right) {
                    // onRightMouse(data.pressed);
                } else if (data.button == MouseButton::middle) {
                    // onMiddleMouse(data.pressed);
                }
            } break;
            default:
                break;
            }
        }
    }

    void Scene::beginScene() {
        static int value = -1;
        m_msaaFramebuffer->bind();
        m_msaaFramebuffer->clearColorAttachment<GL_FLOAT>(0, glm::value_ptr(ViewportTheme::backgroundColor));
        m_msaaFramebuffer->clearColorAttachment<GL_INT>(1, &value);
        Gl::FrameBuffer::clearDepthStencilBuf();
        Renderer::begin(m_camera);
    }

    void Scene::endScene() {
        Renderer2D::Renderer::end();
        Gl::FrameBuffer::unbindAll();
        for (int i = 0; i < 2; i++) {
            m_msaaFramebuffer->bindColorAttachmentForRead(i);
            m_normalFramebuffer->bindColorAttachmentForDraw(i);
            Gl::FrameBuffer::blitColorBuffer(m_size.x, m_size.y);
        }
        Gl::FrameBuffer::unbindAll();
    }

    glm::vec2 Scene::getCameraPos() {
        return m_camera->getPos();
    }

    float Scene::getCameraZoom() {
        return m_camera->getZoom();
    }

    void Scene::setZoom(float value) {
        m_camera->setZoom(value);
    }

    entt::registry &Scene::getEnttRegistry() {
        return m_registry;
    }

} // namespace Bess::Canvas
