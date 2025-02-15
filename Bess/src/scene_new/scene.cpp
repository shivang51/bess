#include "scene_new/scene.h"
#include "entt/entity/fwd.hpp"
#include "ext/vector_float3.hpp"
#include "ext/vector_float4.hpp"
#include "gtc/type_ptr.hpp"
#include "scene/renderer/renderer.h"
#include "scene_new/components/components.h"
#include "settings/viewport_theme.h"
#include "ui/ui_main/ui_main.h"

using Renderer = Bess::Renderer2D::Renderer;

namespace Bess::Canvas {
    Scene::Scene() {
        Renderer2D::Renderer::init();
        reset();
        auto entity = m_Registry.create();
        m_Registry.emplace<Components::TransformComponent>(entity);
        auto &renderComponent = m_Registry.emplace<Components::RenderComponent>(entity);
        renderComponent.render = []() {
            Renderer2D::Renderer::quad(glm::vec3(0.f, 0.f, 1.f), glm::vec2(100.f, 100.f),
                                       glm::vec4(1.f), 10,
                                       glm::vec4(50.f), glm::vec4(2.f), glm::vec4(1.f, 0.f, 0.f, 1.f));
        };
    }

    Scene::~Scene() {}

    void Scene::reset() {
        m_size = glm::vec2(800.f / 600.f, 1.f);
        m_camera = std::make_shared<Camera>(m_size.x, m_size.y);
        std::vector<Gl::FBAttachmentType> attachments = {Gl::FBAttachmentType::RGBA_RGBA, Gl::FBAttachmentType::R32I_REDI, Gl::DEPTH32F_STENCIL8};
        m_msaaFramebuffer = std::make_unique<Gl::FrameBuffer>(m_size.x, m_size.y, attachments, true);

        attachments = {Gl::FBAttachmentType::RGB_RGB, Gl::FBAttachmentType::R32I_REDI};
        m_normalFramebuffer = std::make_unique<Gl::FrameBuffer>(m_size.x, m_size.y, attachments);
        m_Registry.clear();
    }

    unsigned int Scene::getTextureId() {
        return m_normalFramebuffer->getColorBufferTexId(0);
    }

    void Scene::render() {
        beginScene();
        Renderer::grid({0.f, 0.f, -2.f}, m_camera->getSpan(), -1, ViewportTheme::gridColor);

        auto view = m_Registry.view<Components::RenderComponent>();
        for (auto entity : view) {
            auto renderComponent = view.get<Components::RenderComponent>(entity);
            renderComponent.render();
        }

        endScene();
    }

    void Scene::resize(const glm::vec2 &size) {
        m_size = UI::UIMain::state.viewportSize;
        m_msaaFramebuffer->resize(UI::UIMain::state.viewportSize.x, UI::UIMain::state.viewportSize.y);
        m_normalFramebuffer->resize(UI::UIMain::state.viewportSize.x, UI::UIMain::state.viewportSize.y);
        m_camera->resize(UI::UIMain::state.viewportSize.x, UI::UIMain::state.viewportSize.y);
    }

    void Scene::update() {
        if (m_size != UI::UIMain::state.viewportSize) {
            resize(UI::UIMain::state.viewportSize);
        }
        //
        if (UI::UIMain::state.cameraZoom != m_camera->getZoom()) {
            m_camera->setZoom(UI::UIMain::state.cameraZoom);
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

    const entt::registry &Scene::getEnttRegistry() const {
        return m_Registry;
    }

} // namespace Bess::Canvas
