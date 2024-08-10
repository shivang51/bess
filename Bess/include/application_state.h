#pragma once
#include "components_manager/components_manager.h"
#include "project_file.h"
#include <glm.hpp>
#include <uuid_v4.h>
#include <vector>
#include <memory>

namespace Bess {

enum class DrawMode { none = 0, connection };

struct DragData {
    glm::vec2 dragOffset;
    bool isDragging;
};

class ApplicationState {
  public:
    static void init();
    static void setSelectedId(const UUIDv4::UUID &uid);
    static const UUIDv4::UUID &getSelectedId();
    static const UUIDv4::UUID &getPrevSelectedId();

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

  private:
    static UUIDv4::UUID m_selectedId;
    static UUIDv4::UUID m_prevSelectedId;
};

} // namespace Bess
