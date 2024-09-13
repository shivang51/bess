#include "renderer/gl/fb_attachment.h"

namespace Bess::Gl {

    FBAttachment::FBAttachment(FBAttachmentType type, float width, float height, GLint internalFormat, GLenum format, GLenum index) {
        m_format = format;
        m_internalFormat = internalFormat;
        m_texture = std::make_shared<Texture>(internalFormat, format, width, height);
        m_index = index;
        m_type = type;
    }

    void FBAttachment::resize(float width, float height) {
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

    GLuint FBAttachment::getIntenalFormat() const {
        return m_internalFormat;
    }

    GLenum FBAttachment::getFormat() const {
        return m_format;
    }

} // namespace Bess::Gl
