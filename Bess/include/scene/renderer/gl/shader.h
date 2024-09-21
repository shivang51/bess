#pragma once
#include "fwd.hpp"
#include "gl_wrapper.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace Bess::Gl {
    class Shader {
      public:
        Shader() = default;
        Shader(const std::string &vertexPath, const std::string &fragmentPath);
        Shader(const std::string &vertexPath, const std::string &fragmentPath,
               const std::string &tessalationPath, const std::string &evaluationPath);
        ~Shader();

        void bind() const;
        void unbind() const;

        void setUniformVec4(const std::string &name, const glm::vec4 &value);
        void setUniformMat4(const std::string &name, const glm::mat4 &value);
        void setUniform1i(const std::string &name, int value);
        void setUniform1f(const std::string &name, float value);
        void setUniform3f(const std::string &name, const glm::vec3 &value);
        void setUniformVec2(const std::string &name, const glm::vec2 &value);

        void setUniformIV(const std::string &name, const std::vector<int> &value);

      private:
        GLuint m_id;

        GLuint compileShader(const std::string &source, GLenum type);
        GLuint createProgram(const std::string &vertexPath,
                             const std::string &fragmentPath);
        GLuint createProgram(const std::string &vertexPath,
                             const std::string &fragmentPath,
                             const std::string &tessalationPath,
                             const std::string &evaluationPath);
        std::string readFile(const std::string &path);

        std::unordered_map<std::string, int> m_uniformLocationCache;
    };
} // namespace Bess::Gl
