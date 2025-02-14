#include "scene_new/scene.h"
#include "entt/entity/fwd.hpp"
#include "scene_new/components/components.h"
#include "settings/viewport_theme.h"

namespace Bess::Canvas {
    Scene::Scene() {
        reset();
        auto entity = m_Registry.create();
        m_Registry.emplace<Components::TransformComponent>(entity);
        auto &renderComponent = m_Registry.emplace<Components::RenderComponent>(entity);
        renderComponent.render = []() {};
    }

    void Scene::reset() {
        m_size = glm::vec2(800.f, 600.f);
        std::vector<Gl::FBAttachmentType> attachments = {Gl::FBAttachmentType::RGBA_RGBA, Gl::FBAttachmentType::R32I_REDI, Gl::DEPTH32F_STENCIL8};
        m_msaaFramebuffer = std::make_unique<Gl::FrameBuffer>(800, 600, attachments, true);

        attachments = {Gl::FBAttachmentType::RGB_RGB, Gl::FBAttachmentType::R32I_REDI};
        m_framebuffer = std::make_unique<Gl::FrameBuffer>(800, 600, attachments);
        m_Registry.clear();
    }

    unsigned int Scene::getTextureId() {
        return m_framebuffer->getColorBufferTexId(0);
    }

    void Scene::beginScene() {
        static int value = -1;
        m_msaaFramebuffer->bind();

        const auto bgColor = ViewportTheme::backgroundColor;
        const float clearColor[] = {bgColor.x, bgColor.y, bgColor.z, bgColor.a};
        m_msaaFramebuffer->clearColorAttachment<GL_FLOAT>(0, clearColor);
        m_msaaFramebuffer->clearColorAttachment<GL_INT>(1, &value);
        Gl::FrameBuffer::clearDepthStencilBuf();
    }

    void Scene::endScene() {
        Gl::FrameBuffer::unbindAll();
        for (int i = 0; i < 2; i++) {
            m_msaaFramebuffer->bindColorAttachmentForRead(i);
            m_framebuffer->bindColorAttachmentForDraw(i);
            Gl::FrameBuffer::blitColorBuffer(m_size.x, m_size.y);
        }
        Gl::FrameBuffer::unbindAll();
    }

    void Scene::render() {
        beginScene();
        auto view = m_Registry.view<Components::RenderComponent>();
        for (auto entity : view) {
            auto renderComponent = view.get<Components::RenderComponent>(entity);
            renderComponent.render();
        }
        endScene();
    }

    void Scene::update() {
    }

    Scene::~Scene() {}
} // namespace Bess::Canvas
