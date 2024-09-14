#pragma once
#include "fwd.hpp"
#include "gl_wrapper.h"
#include "renderer/gl/texture.h"
#include "fb_attachment.h"
#include <memory>
#include <vector>

namespace Bess::Gl {

    class FrameBuffer {
    public:
        FrameBuffer() = default;

        FrameBuffer(float width, float height, const std::vector<FBAttachmentType>& attachments, bool multisampled = false);
        ~FrameBuffer();

        GLuint getColorBufferTexId(int idx = 0) const;

        glm::vec2 getSize() const;



        void bind() const;
        static void unbindForDraw() ;
        void resize(float width, float height);

        template<GLenum GlType>
        void clearColorAttachment(const unsigned int idx, const void *data) const {
            bind();
            const auto texId = m_colorAttachments[idx].getTextureId();
            auto [_, type] = getFormatsForAttachmentType(m_colorAttachments[idx].getType());
            GL_CHECK(glClearTexImage(texId, 0, type, GlType, data));
        }

        static void clearDepthStencilBuf();

        int readIntFromColAttachment(int idx, int x, int y) const;

        template<class T, GLenum GlType>
        T readFromColorAttachment(const int idx, const int x, const int y) const {
            auto& attachment = m_colorAttachments[idx];
            GL_CHECK(glReadBuffer(GL_COLOR_ATTACHMENT0 + idx));
            T data{};
            GL_CHECK(glReadPixels(x, y, 1, 1, attachment.getFormat(), GlType, &data));
			return data;
        }


    private:
        GLuint m_fbo;

        std::vector<FBAttachment> m_colorAttachments;
        GLuint m_rbo = 0;

        float m_width;
        float m_height;
        bool m_multisampled;

        FBAttachmentType m_depthBufferStencilType;
    private:
        static std::pair<GLint, GLenum> getFormatsForAttachmentType(FBAttachmentType type);
        static GLint getDepthStencilInternalFormat(FBAttachmentType type);
        static std::string getFramebufferStatusReason(GLenum status);
    };
} // namespace Bess::Gl
