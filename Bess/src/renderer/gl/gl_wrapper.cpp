#include "renderer/gl/gl_wrapper.h"

#include <iostream>

void GLCheckError(const char *stmt, const char *filename, const int line) {
    while (const GLenum err = glGetError()) {
        std::string error;
        switch (err) {
        case GL_INVALID_ENUM:
            error = "GL_INVALID_ENUM: An unacceptable value is specified for an enumerated argument.";
            break;
        case GL_INVALID_VALUE:
            error = "GL_INVALID_VALUE: A numeric argument is out of range.";
            break;
        case GL_INVALID_OPERATION:
            error = "GL_INVALID_OPERATION: The specified operation is not allowed in the current state.";
            break;
        case GL_STACK_OVERFLOW:
            error = "GL_STACK_OVERFLOW: This command would cause a stack overflow.";
            break;
        case GL_STACK_UNDERFLOW:
            error = "GL_STACK_UNDERFLOW: This command would cause a stack underflow.";
            break;
        case GL_OUT_OF_MEMORY:
            error = "GL_OUT_OF_MEMORY: There is not enough memory left to execute the command.";
            break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            error = "GL_INVALID_FRAMEBUFFER_OPERATION: The framebuffer object is not complete.";
            break;
        default:
            error = "Unknown OpenGL error.";
        }

        std::cerr << "OpenGL error " << err << " (" << error << ") in " << filename << ": " << line << " at " << stmt << std::endl;
    }
}

void GLClearError() {
    while (glGetError() != GL_NO_ERROR)
        ;
}

namespace Bess::Gl {
    Api::GlStats Api::m_stats = {};

    void Api::drawElements(const GLenum mode, const GLsizei count) {
        if (count == 0) {
            return;
        }
        GL_CHECK(glDrawElements(mode, count, GL_UNSIGNED_INT, nullptr));
        m_stats.drawCalls++;
        m_stats.vertices += count;
    }

    const Api::GlStats &Api::getStats() {
        return m_stats;
    }

    Api::GlStats &Api::getStatsRef() {
        return m_stats;
    }

    void Api::clearStats() {
        m_stats = {};
    }
} // namespace Bess::Gl
