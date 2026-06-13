#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Bess::Core::Renderer {

    enum class ShaderLanguage : uint8_t { WGSL, GLSL, SPIRV, BackendNative };

    enum class ShaderStage : uint8_t { Vertex, Fragment, Compute };

    struct ShaderModuleDesc {
        ShaderLanguage language = ShaderLanguage::BackendNative;
        ShaderStage stage = ShaderStage::Vertex;
        std::string entryPoint;
        std::string source;
        std::vector<uint32_t> spirv;
    };

    class IShader {
      public:
        virtual ~IShader();

        [[nodiscard]] virtual const std::string &getName() const noexcept = 0;
        [[nodiscard]] virtual ShaderLanguage getLanguage() const noexcept = 0;
        [[nodiscard]] virtual const std::vector<ShaderModuleDesc> &
        getModules() const noexcept = 0;
    };

} // namespace Bess::Core::Renderer
