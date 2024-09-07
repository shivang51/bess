#pragma once

#include "common/types.h"
#include "components_manager/component_bank.h"
#include "project_file.h"
#include "uuid.h"

namespace Bess::Pages {
    class MainPageState {
      public:
        static std::shared_ptr<MainPageState> getInstance();

        MainPageState();

        void init();

        int getHoveredId();
        int getPrevHoveredId();
        void setHoveredId(int id);
        bool isHoveredIdChanged();

        void setSelectedId(const uuids::uuid &uid, bool updatePrevSel = true, bool dispatchFocusEvts = true);
        const uuids::uuid &getSelectedId();
        const uuids::uuid &getPrevSelectedId();

        void setConnStartId(const uuids::uuid &uid);
        const uuids::uuid &getConnStartId();

        void setSimulationPaused(bool paused);
        bool isSimulationPaused();

        void setDrawMode(UI::Types::DrawMode mode);
        UI::Types::DrawMode getDrawMode();

        void setDragData(const UI::Types::DragData &data);
        const UI::Types::DragData &getDragData();
        UI::Types::DragData &getDragDataRef();

        void setPoints(const std::vector<glm::vec3> &points);
        const std::vector<glm::vec3> &getPoints();
        void addPoint(const glm::vec3 &point);
        void clearPoints();
        std::vector<glm::vec3> &getPointsRef();

        void setKeyPressed(int key, bool pressed);
        bool isKeyPressed(int key);

        void resetProjectState();
        void createNewProject();
        void saveCurrentProject();
        void loadProject(const std::string &path);
        void updateCurrentProject(std::shared_ptr<ProjectFile> project);
        std::shared_ptr<ProjectFile> getCurrentProjectFile();

        void setMousePos(const glm::vec2 &pos);
        const glm::vec2 &getMousePos();

        void setPrevGenBankElement(const Simulator::ComponentBankElement &element);
        Simulator::ComponentBankElement *getPrevGenBankElement();

      private:
        // ids of entity hovered by mouse
        int m_hoveredId = -1;
        int m_prevHoveredId = -1;

        // id of slot from which connection was started
        uuids::uuid m_connStartId;

        // handle to currently open project file
        std::shared_ptr<ProjectFile> m_currentProjectFile;

        Simulator::ComponentBankElement *m_prevGenBankElement = nullptr;

        bool m_simulationPaused = false;

        // id of selected entity
        uuids::uuid m_selectedId;
        uuids::uuid m_prevSelectedId;

        // contains the state of keyboard keys pressed
        std::unordered_map<int, bool> m_pressedKeys = {};

        glm::vec2 m_mousePos = {};

        // to be used according to the need of different ops
        std::vector<glm::vec3> m_points = {};

        UI::Types::DrawMode m_drawMode = UI::Types::DrawMode::none;

        UI::Types::DragData m_dragData = {};
    };
} // namespace Bess::Pages
