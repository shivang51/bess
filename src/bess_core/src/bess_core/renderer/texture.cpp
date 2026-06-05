#include "bess_core/renderer/texture.h"
#include <stdexcept>

namespace Bess::Core::Renderer {
    ITexture::ITexture(const std::string &path) : m_path(path) {}

    ITexture::ITexture(const TextureCreateInfo &createInfo)
        : m_path(createInfo.path),
          m_format(createInfo.format) {}

    void ITexture::saveToFile(const std::string &path) const {
        static_cast<void>(path);
        throw std::runtime_error("Texture saveToFile is not implemented");
    }

    TextureHandle ITexture::getNextTextureHandle() {
        static TextureHandle nextHandle = 1;
        return nextHandle++;
    }

} // namespace Bess::Core::Renderer
