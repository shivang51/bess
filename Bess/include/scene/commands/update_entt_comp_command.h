#pragma once

#include "bess_uuid.h"
#include "commands/command.h"
#include "scene/scene.h"
namespace Bess::Canvas::Commands {

    using Command = SimEngine::Commands::Command;

    template <typename TComponent>
    class UpdateEnttCompCommand : public Command {
      public:
        /// If skipExcute is true, then we will skip the exectuion and will assume passed comp is the original
        /// So, m_updatedComponent will be populated from the registry and m_component will be stored with passed value
        UpdateEnttCompCommand(UUID uuid, const TComponent &comp, bool skipExcute = false) : m_uuid(uuid), m_updatedComponent(comp), m_skipExecute(true) {
        }

        bool execute() override {
            auto &scene = Scene::instance();
            if (!scene.isEntityValid(m_uuid))
                return false;

            const auto ent = scene.getEntityWithUuid(m_uuid);
            auto &reg = scene.getEnttRegistry();

            if (!m_redo && m_skipExecute) {
                m_component = m_updatedComponent;
                m_updatedComponent = reg.get<TComponent>(ent);
                return true;
            }

            m_component = reg.get<TComponent>(ent);
            reg.emplace_or_replace<TComponent>(ent, m_updatedComponent);

            return true;
        }

        std::any undo() override {
            auto &scene = Scene::instance();
            auto &reg = scene.getEnttRegistry();

            const auto ent = scene.getEntityWithUuid(m_uuid);
            reg.emplace_or_replace<TComponent>(ent, m_component);
            m_redo = true;
            return {};
        }

        std::any getResult() override {
            return std::string("updated successfully");
        }

        using Command::getResult;

      private:
        UUID m_uuid;
        TComponent m_updatedComponent;
        TComponent m_component;
        bool m_skipExecute, m_redo = false;
    };

} // namespace Bess::Canvas::Commands
