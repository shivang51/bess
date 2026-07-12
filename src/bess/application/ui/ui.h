#pragma once

#include "bess_wgpu/wgpu_texture.h"
#include "imgui.h"

namespace Bess::Pages {
    class Page;
}

namespace Bess {
    class Window;

    class UIHandle {
      public:
        void init(const std::shared_ptr<Window> &window);
        void begin();
        void end();

        void draw();
        void drawStats(int fps);

        void update(TimeMs dt);

        void shutdown();
        void loadFontAndSetScale(float fontSize, float scale);
        void setCursorPointer();
        void setCursorMove();
        void setCursorText();
        void setCursorResizeHorizontal();
        void setCursorResizeVertical();
        void setCursorResizeDiagonalNWSE();
        void setCursorResizeDiagonalNESW();
        void setCursorNormal();

        enum class CursorType : uint8_t {
            pointer,
            move,
            text,
            resizeHorizontal,
            resizeVertical,
            resizeDiagonalNWSE,
            resizeDiagonalNESW,
            normal
        };

        class Fonts {
          public:
            static ImFont *largeFont;
            static ImFont *mediumFont;
        };

      private:
        int m_currentFps = 0;
        CursorType m_currentCursorType = CursorType::normal;
        std::shared_ptr<Wgpu::WgpuTexture> m_previewTex = nullptr;
        std::shared_ptr<Pages::Page> m_currentPage = nullptr;
    };
} // namespace Bess
