#pragma once
#include "components_manager/components_manager.h"
#include "pages/page.h"
#include "pages/page_identifier.h"
#include "project_file.h"
#include "uuid.h"
#include "window.h"
#include <glm.hpp>
#include <memory>
#include <vector>

namespace Bess {

    enum class DrawMode { none = 0,
                          connection };

    struct DragData {
        glm::vec2 dragOffset;
        bool isDragging;
    };

    class ApplicationState {
      public:
        static void init(Window *mainWin);
        static void setSelectedId(const uuids::uuid &uid, bool updatePrevSel = true, bool dispatchFocusEvts = true);
        static const uuids::uuid &getSelectedId();
        static const uuids::uuid &getPrevSelectedId();

        static void createNewProject();
        static void saveCurrentProject();
        static void loadProject(const std::string &path);
        static void updateCurrentProject(std::shared_ptr<ProjectFile> project);

        static bool isKeyPressed(int key);
        static void setKeyPressed(int key, bool pressed);

        static void setMousePos(const glm::vec2 &pos);
        static const glm::vec2 &getMousePos();

        static void setCurrentPage(std::shared_ptr<Pages::Page> page);
        static std::shared_ptr<Pages::Page> getCurrentPage();

      public:
        // to be used according to the need of different ops
        static std::vector<glm::vec3> points;

        static DrawMode drawMode;

        static int hoveredId;
        static int prevHoveredId;

        // id of slot from which connection was started
        static uuids::uuid connStartId;

        static DragData dragData;

        static float normalizingFactor;

        static std::shared_ptr<ProjectFile> currentProject;

        static bool simulationPaused;

      private:
        static uuids::uuid m_selectedId;
        static uuids::uuid m_prevSelectedId;
        static Window *m_mainWindow;

        static std::unordered_map<int, bool> m_pressedKeys;

        static glm::vec2 m_mousePos;

        static std::shared_ptr<Pages::Page> m_currentPage;
    };

} // namespace Bess
