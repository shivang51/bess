#pragma once

#include "common/bess_api.h"

#include "bess_core/renderer/renderer_2d.h"
#include "common/sub_system.h"
namespace Bess {

    class BESS_API RendererContext : public ISubSystem {
      public:
        void onInit() override;
        void onPreInit() override;
        void onPostInit() override;
        void onDestroy() override;

        [[nodiscard]] std::shared_ptr<Core::Renderer::IRenderer2D>
        getRenderer() const {
            return m_renderer;
        }

        template <typename T>
            requires std::derived_from<T, Core::Renderer::IRenderer2D>
        [[nodiscard]] std::shared_ptr<T> getRenderer() const {
            static_assert(std::derived_from<T, Core::Renderer::IRenderer2D>,
                          "T must be derived from IRenderer2D");
            return std::dynamic_pointer_cast<T>(m_renderer);
        }

      private:
        std::shared_ptr<Core::Renderer::IRenderer2D> m_renderer = nullptr;
    };

} // namespace Bess
