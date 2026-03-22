#pragma once

#include "ext/quaternion_geometric.hpp"
#include "pages/main_page/scene_components/connection_scene_component.h"
#include "pages/main_page/scene_components/module_scene_component.h"
#include "pages/main_page/scene_components/non_sim_scene_component.h"
#include "pages/main_page/scene_components/scene_comp_types.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "scene.h"
#include "scene_state/components/scene_component.h"
#include "scene_state/scene_state.h"
#include <typeindex>
#include <vector>

namespace Bess::Svc::CopyPaste {
    struct ETSimComp {
        std::shared_ptr<Bess::Canvas::SimulationSceneComponent> comp = nullptr;
    };

    struct ETNonSimComp {
        std::type_index typeIdx = typeid(void);
        std::shared_ptr<Bess::Canvas::NonSimSceneComponent> comp = nullptr;
    };

    struct ETConnection {
        std::shared_ptr<Bess::Canvas::ConnectionSceneComponent> conn = nullptr;
    };

    struct CopiedEntity {
        Canvas::SceneComponentType type = Canvas::SceneComponentType::_base;
        glm::vec2 pos = {0.f, 0.f};
        std::variant<ETSimComp, ETNonSimComp, ETConnection> data;
    };

    class Context {
      public:
        static Context &instance();

        // I will allow a new instance creation as well,
        // so module can leverage it to clone it self
        Context() = default;
        ~Context() = default;

        void init();
        void destroy();

        void copy(const std::shared_ptr<Canvas::Scene> &scene);

        // Retruns og id to clone id map
        std::unordered_map<UUID, UUID> paste(const std::shared_ptr<Canvas::Scene> &scene,
                                             bool recordHistory = true);

      private:
        void addEntity(const CopiedEntity &entity);

        void clear();

        void calcCenter();

        std::vector<CopiedEntity> m_entities;
        glm::vec2 m_center;

        std::shared_ptr<Canvas::Scene> m_copiedScene = nullptr;
    };
} // namespace Bess::Svc::CopyPaste
