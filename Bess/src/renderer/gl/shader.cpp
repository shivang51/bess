#include "renderer/gl/shader.h"
#include "gtc/type_ptr.hpp"
#include <fstream>
#include <iostream>
#include <string>

namespace Bess::Gl {
    Shader::Shader(const std::string &vertexPath, const std::string &fragmentPath) {
        m_id = createProgram(vertexPath, fragmentPath);
    }

    Shader::Shader(const std::string &vertexPath, const std::string &fragmentPath,
                   const std::string &tessalationPath,
                   const std::string &evaluationPath) {
        m_id = createProgram(vertexPath, fragmentPath, tessalationPath,
                             evaluationPath);
    }

    Shader::~Shader() { glDeleteProgram(m_id); }

    void Shader::bind() const { GL_CHECK(glUseProgram(m_id)); }

    void Shader::unbind() const { glUseProgram(0); }

    GLuint Shader::createProgram(const std::string &vertexPath,
                                 const std::string &fragmentPath,
                                 const std::string &tessalationPath,
                                 const std::string &evaluationPath) {

        auto vertexShader = readFile(vertexPath);
        auto fragmentShader = readFile(fragmentPath);
        auto tessalationShader = readFile(tessalationPath);
        auto evaluationShader = readFile(evaluationPath);

        auto vertexShaderId = compileShader(vertexShader.c_str(), GL_VERTEX_SHADER);
        auto fragmentShaderId =
            compileShader(fragmentShader.c_str(), GL_FRAGMENT_SHADER);
        auto tessalationShaderId =
            compileShader(tessalationShader.c_str(), GL_TESS_CONTROL_SHADER);
        auto evaluationShaderId =
            compileShader(evaluationShader.c_str(), GL_TESS_EVALUATION_SHADER);

        auto shaderProgram = glCreateProgram();

        glAttachShader(shaderProgram, vertexShaderId);
        glAttachShader(shaderProgram, tessalationShaderId);
        glAttachShader(shaderProgram, evaluationShaderId);
        glAttachShader(shaderProgram, fragmentShaderId);

        glLinkProgram(shaderProgram);

        int success;
        char infoLog[512];
        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
            std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n"
                      << infoLog << std::endl;
        }

        glDeleteShader(vertexShaderId);
        glDeleteShader(fragmentShaderId);
        glDeleteShader(tessalationShaderId);
        glDeleteShader(evaluationShaderId);

        return shaderProgram;
    }
    GLuint Shader::createProgram(const std::string &vertexPath,
                                 const std::string &fragmentPath) {

        auto vertexShader = readFile(vertexPath);
        auto fragmentShader = readFile(fragmentPath);

        auto vertexShaderId = compileShader(vertexShader.c_str(), GL_VERTEX_SHADER);
        auto fragmentShaderId = compileShader(fragmentShader.c_str(), GL_FRAGMENT_SHADER);

        auto shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShaderId);
        glAttachShader(shaderProgram, fragmentShaderId);
        glLinkProgram(shaderProgram);

        int success;
        char infoLog[512];
        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
            std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED -> "
                      << fragmentPath << "\n"
                      << infoLog << std::endl;
        }

        glDeleteShader(vertexShaderId);
        glDeleteShader(fragmentShaderId);

        return shaderProgram;
    }

    std::string Shader::readFile(const std::string &path) {
        std::string content;
        std::ifstream fileStream(path, std::ios::ate);
        if (fileStream.is_open()) {
            size_t fileSize = fileStream.tellg();
            content.resize(fileSize);
            fileStream.seekg(0);
            fileStream.read((char *)content.data(), fileSize);
            fileStream.close();
        } else {
            std::cerr << "Could not read file " << path << std::endl;
        }
        return content;
    }

    GLuint Shader::compileShader(const std::string &shaderSrc, GLenum shaderType) {
        auto shaderId = glCreateShader(shaderType);
        const char *src = shaderSrc.c_str();
        glShaderSource(shaderId, 1, &src, nullptr);
        glCompileShader(shaderId);

        int success;
        char infoLog[512];
        glGetShaderiv(shaderId, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shaderId, 512, nullptr, infoLog);
            std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n"
                      << infoLog << std::endl;
        }

        return shaderId;
    }

    void Shader::setUniformVec4(const std::string &name, const glm::vec4 &value) {
        if (m_uniformLocationCache.find(name) == m_uniformLocationCache.end()) {
            m_uniformLocationCache[name] = glGetUniformLocation(m_id, name.c_str());
        }
        GL_CHECK(glUniform4fv(m_uniformLocationCache[name], 1, glm::value_ptr(value)));
    }

    void Shader::setUniformMat4(const std::string &name, const glm::mat4 &value) {
        if (m_uniformLocationCache.find(name) == m_uniformLocationCache.end()) {
            m_uniformLocationCache[name] = glGetUniformLocation(m_id, name.c_str());
        }
        GL_CHECK(glUniformMatrix4fv(m_uniformLocationCache[name], 1, GL_FALSE, glm::value_ptr(value)));
    }

    void Shader::setUniform1i(const std::string &name, int value) {
        if (m_uniformLocationCache.find(name) == m_uniformLocationCache.end()) {
            m_uniformLocationCache[name] = glGetUniformLocation(m_id, name.c_str());
        }
        GL_CHECK(glUniform1i(m_uniformLocationCache[name], value));
    }

    void Shader::setUniform1f(const std::string &name, float value) {
        if (m_uniformLocationCache.find(name) == m_uniformLocationCache.end()) {
            m_uniformLocationCache[name] = glGetUniformLocation(m_id, name.c_str());
        }
        GL_CHECK(glUniform1f(m_uniformLocationCache[name], value));
    }

    void Shader::setUniformIV(const std::string &name, const std::vector<int> &value) {
        if (m_uniformLocationCache.find(name) == m_uniformLocationCache.end()) {
            m_uniformLocationCache[name] = glGetUniformLocation(m_id, name.c_str());
        }
        GL_CHECK(glUniform1iv(m_uniformLocationCache[name], (GLsizei)value.size(), value.data()));
    }

    void Shader::setUniform3f(const std::string &name, const glm::vec3 &value) {
        if (m_uniformLocationCache.find(name) == m_uniformLocationCache.end()) {
            m_uniformLocationCache[name] = glGetUniformLocation(m_id, name.c_str());
        }
        GL_CHECK(glUniform3fv(m_uniformLocationCache[name], 1, glm::value_ptr(value)));
    }

    void Shader::setUniformVec2(const std::string &name, const glm::vec2 &value) {
        if (m_uniformLocationCache.find(name) == m_uniformLocationCache.end()) {
            m_uniformLocationCache[name] = glGetUniformLocation(m_id, name.c_str());
        }
        GL_CHECK(glUniform2fv(m_uniformLocationCache[name], 1, glm::value_ptr(value)));
    }

} // namespace Bess::Gl
