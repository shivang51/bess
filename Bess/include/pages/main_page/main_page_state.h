#pragma once

#include "common/types.h"
#include "project_file.h"
#include "uuid.h"

namespace Bess::Pages {
    class MainPageState {
      public:
        static std::shared_ptr<MainPageState> getInstance();

        MainPageState();

        int getHoveredId();
        int getPrevHoveredId();
        void setHoveredId(int id);
        bool isHoveredIdChanged();

        void setConnStartId(const uuids::uuid &uid);
        const uuids::uuid &getConnStartId();

        void setSimulationPaused(bool paused);
        bool isSimulationPaused();

        void setDrawMode(UI::Types::DrawMode mode);
        UI::Types::DrawMode getDrawMode();

        void setDragData(const Types::DragData &data);
        const Types::DragData &getDragData();
        void clearDragData();

        void setPoints(const std::vector<glm::vec3> &points);
        const std::vector<glm::vec3> &getPoints();
        void addPoint(const glm::vec3 &point);
        void clearPoints();
        std::vector<glm::vec3> &getPointsRef();

        void setKeyPressed(int key, bool pressed);
        bool isKeyPressed(int key);

        void resetProjectState();
        void createNewProject(bool updateWindowName = true);
        void saveCurrentProject();
        void loadProject(const std::string &path);
        void updateCurrentProject(std::shared_ptr<ProjectFile> project);
        std::shared_ptr<ProjectFile> getCurrentProjectFile();

        void setMousePos(const glm::vec2 &pos);
        const glm::vec2 &getMousePos();

        void setReadBulkIds(bool readBulkIds);

        bool shouldReadBulkIds();

        void setBulkIds(const std::vector<uuids::uuid> &ids);
        void setBulkId(const uuids::uuid &ids);

        const std::vector<uuids::uuid> &getBulkIds();

        void clearBulkIds();

        void addBulkId(const uuids::uuid &id);

        bool isBulkIdPresent(const uuids::uuid &id);

        void removeBulkId(const uuids::uuid &id, bool dispatchEvent = true);

        bool isBulkIdEmpty();

        const uuids::uuid &getBulkIdAt(int index);

      private:
        void addFocusLostEvent(const uuids::uuid &id);
        void addFocusEvent(const uuids::uuid &id);

      private:
        // ids of entity hovered by mouse
        int m_hoveredId = -1;
        int m_prevHoveredId = -1;

        // id of slot from which connection was started
        uuids::uuid m_connStartId;

        // handle to currently open project file
        std::shared_ptr<ProjectFile> m_currentProjectFile;

        bool m_simulationPaused = false;

        // contains the state of keyboard keys pressed
        std::unordered_map<int, bool> m_pressedKeys = {};

        glm::vec2 m_mousePos = {};

        // to be used according to the need of different ops
        std::vector<glm::vec3> m_points = {};

        UI::Types::DrawMode m_drawMode = UI::Types::DrawMode::none;

        Types::DragData m_dragData = {};

        bool m_readBulkIds = false;

        // ids of selected entities for bulk operations
        std::vector<uuids::uuid> m_bulkIds = {};
        std::vector<uuids::uuid> m_prevBulkIds = {};
    };
} // namespace Bess::Pages
