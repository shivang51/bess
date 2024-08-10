#pragma once

#include <string>

namespace Bess::UI {
    class ComponentExplorer {
    public:
        static void draw();
    private:
        static std::string m_searchQuery;
        static void firstTime();
        static bool isfirstTimeDraw;
    };
}