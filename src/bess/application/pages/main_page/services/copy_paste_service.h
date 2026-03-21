#pragma once

#include "ext/quaternion_geometric.hpp"
#include "pages/main_page/scene_components/connection_scene_component.h"
#include "pages/main_page/scene_components/non_sim_scene_component.h"
#include "pages/main_page/scene_components/scene_comp_types.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "scene_state/components/scene_component.h"
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
        static Context &instance() {
            static Context context;
            return context;
        }

        void addEntity(const CopiedEntity &entity) {
            m_entities.push_back(entity);
        }

        void clear() {
            m_entities.clear();
        }

        void calcCenter() {
            if (m_entities.empty())
                return;
            if (m_entities.size() == 1)
                m_center = m_entities.front().pos;

            glm::vec2 sumPos{0.f, 0.f};

            for (const auto &ent : m_entities) {
                sumPos += ent.pos;
            }

            m_center = sumPos / (float)m_entities.size();
        }

        MAKE_GETTER_SETTER(std::vector<CopiedEntity>, Entites, m_entities);
        MAKE_GETTER_SETTER(glm::vec2, Center, m_center);

      private:
        Context() = default;
        ~Context() = default;

        std::vector<CopiedEntity> m_entities;
        glm::vec2 m_center;
    };
} // namespace Bess::Svc::CopyPaste
