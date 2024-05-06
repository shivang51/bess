#include "gl/gl_wrapper.h"

#include <iostream>

void GLCheckError(const char *stmt, const char *fname, int line) {
    while (GLenum err = glGetError()) {
        if (err != GL_NO_ERROR) {
            std::cerr << "OpenGL error " << err << " in " << fname << ": "
                      << line << " at " << stmt << std::endl;
        }
    }
}

void GLClearError() {
    while (glGetError() != GL_NO_ERROR)
        ;
}
