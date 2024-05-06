#pragma once

extern "C" {
#include "glad/glad.h"
}

void GLClearError();

void GLCheckError(const char *stmt, const char *fname, int line);

#ifdef _DEBUG
#define GL_CHECK(stmt)                                                         \
    GLClearError();                                                            \
    stmt;                                                                      \
    GLCheckError(#stmt, __FILE__, __LINE__)
#else
#define GL_CHECK(stmt) stmt
#endif
