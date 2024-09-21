#pragma once

extern "C" {
#include "glad/glad.h"
}

void GLClearError();

void GLCheckError(const char *stmt, const char *filename, int line);

#ifndef NDEBUG
    #define GL_CHECK(stmt)                           \
        Bess::Gl::Api::getStatsRef().glCheckCalls++; \
        GLClearError();                              \
        stmt;                                        \
        GLCheckError(#stmt, __FILE__, __LINE__)
#else
    #define GL_CHECK(stmt) stmt
#endif

namespace Bess::Gl {

    class Api {
      public:
        struct GlStats {
            int drawCalls = 0;
            int vertices = 0;
            int glCheckCalls = 0;
        };

        static void drawElements(GLenum mode, GLsizei count);

        static const GlStats &getStats();
        static GlStats &getStatsRef();

        static void clearStats();

      private:
        static GlStats m_stats;
    };
} // namespace Bess::Gl
