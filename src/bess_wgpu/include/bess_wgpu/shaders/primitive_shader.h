#pragma once

#include "bess_core/renderer/shader.h"
#include <vector>

namespace Bess::Wgpu::Shaders {

    [[nodiscard]] std::vector<Core::Renderer::ShaderModuleDesc>
    getPrimitiveShaderModules();

} // namespace Bess::Wgpu::Shaders
