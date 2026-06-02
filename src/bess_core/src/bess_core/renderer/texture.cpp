#include "bess_core/renderer/texture.h"

namespace Bess::Core::Renderer {
    ITexture::ITexture(const std::string &path) : m_path(path) {}

    ITexture::ITexture(const TextureCreateInfo &createInfo)
        : m_path(createInfo.path) {}
    TextureHandle ITexture::getNextTextureHandle() {
        static TextureHandle nextHandle = 1;
        return nextHandle++;
    }

} // namespace Bess::Core::Renderer
