#include "scene/renderer/gl/fb_attachment.h"

namespace Bess::Gl {

    FBAttachment::FBAttachment(const FBAttachmentType type, float width, float height, GLint internalFormat, GLenum format, const GLenum index, const bool multisampled) {
        m_format = format;
        m_internalFormat = internalFormat;
        m_texture = std::make_shared<Texture>(internalFormat, format, width, height, nullptr, multisampled);
        m_index = index;
        m_type = type;
        m_multisampled = multisampled;
    }

    void FBAttachment::resize(const float width, const float height) const {
        m_texture->resize(width, height);
    }

    GLuint FBAttachment::getTextureId() const {
        return m_texture->getId();
    }

    GLenum FBAttachment::getIndex() const {
        return m_index;
    }

    FBAttachmentType FBAttachment::getType() const {
        return m_type;
    }

    GLuint FBAttachment::getInternalFormat() const {
        return m_internalFormat;
    }

    GLenum FBAttachment::getFormat() const {
        return m_format;
    }

    void FBAttachment::bindTexture() const {
        m_texture->bind();
    }
} // namespace Bess::Gl
