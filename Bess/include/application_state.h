#pragma once
#include <vector>
#include <glm.hpp>
#include <uuid_v4.h>
#include "components_manager/components_manager.h"

namespace Bess
{

  enum class DrawMode
  {
    none = 0,
    connection
  };

  struct DragData
  {
    glm::vec2 dragOffset;
    bool isDragging;
  };

  class ApplicationState
  {
  public:
    static void init();
    static void setSelectedId(const UUIDv4::UUID &uid);
    static const UUIDv4::UUID &getSelectedId();
    static const UUIDv4::UUID &getPrevSelectedId();

  public:
    // to be used according to the need of different ops
    static std::vector<glm::vec2> points;

    static DrawMode drawMode;

    static int hoveredId;
    static int prevHoveredId;

    // id of slot from which connection was started
    static UUIDv4::UUID connStartId;

    static DragData dragData;

  private:
    static UUIDv4::UUID m_selectedId;
    static UUIDv4::UUID m_prevSelectedId;
  };

}
