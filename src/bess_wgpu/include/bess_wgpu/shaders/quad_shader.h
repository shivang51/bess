#pragma once

#include "bess_core/renderer/shader.h"
#include <vector>

namespace Bess::Wgpu::Shaders {

    [[nodiscard]] std::vector<Core::Renderer::ShaderModuleDesc>
    getQuadShaderModules();

} // namespace Bess::Wgpu::Shaders
