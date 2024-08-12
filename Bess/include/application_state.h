#pragma once
#include "components_manager/components_manager.h"
#include "project_file.h"
#include <glm.hpp>
#include <uuid_v4.h>
#include <vector>
#include <memory>
#include "window.h"

namespace Bess {

enum class DrawMode { none = 0, connection };

struct DragData {
    glm::vec2 dragOffset;
    bool isDragging;
};

class ApplicationState {
  public:
    static void init(Window* mainWin);
    static void setSelectedId(const UUIDv4::UUID &uid);
    static const UUIDv4::UUID &getSelectedId();
    static const UUIDv4::UUID &getPrevSelectedId();

    static void createNewProject();
    static void saveCurrentProject();
    static void loadProject(const std::string& path);
    static void updateCurrentProject(std::shared_ptr<ProjectFile> project);

  public:
    // to be used according to the need of different ops
    static std::vector<glm::vec3> points;

    static DrawMode drawMode;

    static int hoveredId;
    static int prevHoveredId;

    // id of slot from which connection was started
    static UUIDv4::UUID connStartId;

    static DragData dragData;

    static float normalizingFactor;

    static std::shared_ptr<ProjectFile> currentProject;

    static bool simulationPaused;

  private:
    static UUIDv4::UUID m_selectedId;
    static UUIDv4::UUID m_prevSelectedId;
    static Window* m_mainWindow;
};

} // namespace Bess
