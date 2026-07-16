#pragma once

#include "common/bess_api.h"

#include "bess_core/renderer/shader.h"
#include <string>
#include <vector>
#include <webgpu/webgpu_cpp.h>

namespace Bess::Wgpu {

    class BESS_API WgpuShader final : public Core::Renderer::IShader {
      public:
        WgpuShader(std::string name,
                   std::vector<Core::Renderer::ShaderModuleDesc> modules,
                   const wgpu::Device &device);

        [[nodiscard]] const std::string &getName() const noexcept override;
        [[nodiscard]] Core::Renderer::ShaderLanguage
        getLanguage() const noexcept override;
        [[nodiscard]] const std::vector<Core::Renderer::ShaderModuleDesc> &
        getModules() const noexcept override;

        [[nodiscard]] const wgpu::ShaderModule &
        getModule(Core::Renderer::ShaderStage stage) const;
        [[nodiscard]] const std::string &
        getEntryPoint(Core::Renderer::ShaderStage stage) const;

      private:
        struct CompiledModule {
            Core::Renderer::ShaderStage stage;
            std::string entryPoint;
            wgpu::ShaderModule module;
        };

        static wgpu::ShaderModule
        compileModule(const wgpu::Device &device,
                      const Core::Renderer::ShaderModuleDesc &desc);

        std::string m_name;
        Core::Renderer::ShaderLanguage m_language =
            Core::Renderer::ShaderLanguage::BackendNative;
        std::vector<Core::Renderer::ShaderModuleDesc> m_modules;
        std::vector<CompiledModule> m_compiledModules;
    };

} // namespace Bess::Wgpu
