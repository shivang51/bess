#include "scene/scene_context.h"
#include "scene/events/event_type.h"
#include "settings/viewport_theme.h"

namespace Bess::Scene {

    SceneContext::~SceneContext() {
    }

    void SceneContext::init() {
        std::vector<Gl::FBAttachmentType> attachments = {Gl::FBAttachmentType::RGBA_RGBA, Gl::FBAttachmentType::R32I_REDI, Gl::DEPTH32F_STENCIL8};
        m_msaaFramebuffer = std::make_unique<Gl::FrameBuffer>(800, 600, attachments, true);

        attachments = {Gl::FBAttachmentType::RGB_RGB, Gl::FBAttachmentType::R32I_REDI};
        m_framebuffer = std::make_unique<Gl::FrameBuffer>(800, 600, attachments);

        m_events = {};
    }

    unsigned int SceneContext::getTextureId() {
        return m_framebuffer->getColorBufferTexId(0);
    }

    void SceneContext::beginScene() {
        static int value = -1;
        m_msaaFramebuffer->bind();

        const auto bgColor = ViewportTheme::backgroundColor;
        const float clearColor[] = {bgColor.x, bgColor.y, bgColor.z, bgColor.a};
        m_msaaFramebuffer->clearColorAttachment<GL_FLOAT>(0, clearColor);
        m_msaaFramebuffer->clearColorAttachment<GL_INT>(1, &value);
        Gl::FrameBuffer::clearDepthStencilBuf();
    }

    void SceneContext::endScene() {
        Gl::FrameBuffer::unbindAll();
        for (int i = 0; i < 2; i++) {
            m_msaaFramebuffer->bindColorAttachmentForRead(i);
            m_framebuffer->bindColorAttachmentForDraw(i);
            Gl::FrameBuffer::blitColorBuffer(m_size.x, m_size.y);
        }
        Gl::FrameBuffer::unbindAll();
    }

    void SceneContext::render() {
        beginScene();
        for (auto &entity : m_entities) {
            entity->render();
        }
        endScene();
    }

    void SceneContext::update() {
        while (!m_events.empty()) {
            const auto &evt = m_events.front();

            switch (evt.getType()) {
            case Events::EventType::mouseButton: {
            } break;
            case Events::EventType::mouseMove: {
            } break;
            case Events::EventType::mouseWheel: {
            } break;
            case Events::EventType::keyPress: {
            } break;
            case Events::EventType::keyRelease: {
            } break;
            case Events::EventType::mouseEnter: {
            } break;
            case Events::EventType::mouseLeave: {
            } break;
            case Events::EventType::resize: {
                const auto &data = evt.getData<Events::ResizeEventData>();
                m_size = data.size;
                m_msaaFramebuffer->resize(m_size.x, m_size.y);
                m_framebuffer->resize(m_size.x, m_size.y);
            } break;
            }

            m_events.pop();
        }

        for (auto &entity : m_entities) {
            entity->update();
        }
    }

    void SceneContext::addEntity(std::shared_ptr<Entity> entity) {
        m_entities.push_back(entity);
    }

    const std::vector<std::shared_ptr<Entity>> &SceneContext::getEntities() const {
        return m_entities;
    }

    void SceneContext::onEvent(const Events::Event &evt) {
        m_events.push(evt);
    }

} // namespace Bess::Scene
