#include "bess_wgpu/wgpu_shader.h"
#include <stdexcept>

namespace Bess::Wgpu {
    namespace {
        bool isSupportedLanguage(Core::Renderer::ShaderLanguage language) {
            return language == Core::Renderer::ShaderLanguage::WGSL;
        }
    } // namespace

    WgpuShader::WgpuShader(
        std::string name,
        std::vector<Core::Renderer::ShaderModuleDesc> modules,
        const wgpu::Device &device)
        : m_name(std::move(name)),
          m_modules(std::move(modules)) {
        if (m_modules.empty()) {
            throw std::runtime_error("WgpuShader requires at least one module");
        }

        m_language = m_modules.front().language;
        for (const auto &module : m_modules) {
            if (module.language != m_language) {
                throw std::runtime_error(
                    "WgpuShader modules must use one shader language");
            }
            if (!isSupportedLanguage(module.language)) {
                throw std::runtime_error(
                    "WgpuShader currently supports WGSL modules only");
            }
            if (module.entryPoint.empty()) {
                throw std::runtime_error(
                    "WgpuShader module is missing an entry point");
            }

            m_compiledModules.push_back(
                {module.stage, module.entryPoint, compileModule(device, module)});
        }
    }

    const std::string &WgpuShader::getName() const noexcept { return m_name; }

    Core::Renderer::ShaderLanguage
    WgpuShader::getLanguage() const noexcept {
        return m_language;
    }

    const std::vector<Core::Renderer::ShaderModuleDesc> &
    WgpuShader::getModules() const noexcept {
        return m_modules;
    }

    const wgpu::ShaderModule &
    WgpuShader::getModule(Core::Renderer::ShaderStage stage) const {
        for (const auto &module : m_compiledModules) {
            if (module.stage == stage) {
                return module.module;
            }
        }
        throw std::runtime_error("Requested WGPU shader stage is missing");
    }

    const std::string &
    WgpuShader::getEntryPoint(Core::Renderer::ShaderStage stage) const {
        for (const auto &module : m_compiledModules) {
            if (module.stage == stage) {
                return module.entryPoint;
            }
        }
        throw std::runtime_error("Requested WGPU shader stage is missing");
    }

    wgpu::ShaderModule WgpuShader::compileModule(
        const wgpu::Device &device,
        const Core::Renderer::ShaderModuleDesc &desc) {
        wgpu::ShaderSourceWGSL wgslSource{};
        wgslSource.code = desc.source.c_str();

        wgpu::ShaderModuleDescriptor shaderDescriptor{};
        shaderDescriptor.nextInChain = &wgslSource;

        wgpu::ShaderModule shaderModule =
            device.CreateShaderModule(&shaderDescriptor);
        if (shaderModule == nullptr) {
            throw std::runtime_error("Failed to create WGPU shader module");
        }
        return shaderModule;
    }

} // namespace Bess::Wgpu
