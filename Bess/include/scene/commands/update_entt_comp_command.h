#pragma once

#include "bess_uuid.h"
#include "commands/command.h"
#include "scene/scene.h"

#include <entt/entt.hpp>
#include <memory>
#include <vector>

namespace Bess::Canvas::Commands {
    struct IUpdateOp {
        virtual ~IUpdateOp() = default;
        virtual void apply(entt::registry &reg) = 0;
        virtual void undo(entt::registry &reg) = 0;
        virtual void redo(entt::registry &reg) = 0;
    };

    template <typename T>
    struct UpdateOp final : IUpdateOp {
        UUID m_uuid;
        T m_newData;
        T m_oldData;
        bool m_skipApply{false};

        UpdateOp(UUID uuid, const T &data, bool skip)
            : m_uuid(uuid), m_newData(data), m_skipApply(skip) {}

        void apply(entt::registry &reg) override {
            auto ent = Scene::instance()->getEntityWithUuid(m_uuid);
            if (m_skipApply) {
                m_oldData = m_newData;
                m_newData = reg.get<T>(ent);
                return;
            }

            assert(reg.any_of<T>(ent));
            m_oldData = reg.get<T>(ent);

            reg.emplace_or_replace<T>(ent, m_newData);
        }

        void undo(entt::registry &reg) override {
            auto ent = Scene::instance()->getEntityWithUuid(m_uuid);
            reg.emplace_or_replace<T>(ent, m_oldData);
        }

        void redo(entt::registry &reg) override {
            auto ent = Scene::instance()->getEntityWithUuid(m_uuid);
            reg.emplace_or_replace<T>(ent, m_newData);
        }
    };

    class UpdateEnttComponentsCommand final : public SimEngine::Commands::Command {
      public:
        explicit UpdateEnttComponentsCommand(entt::registry &reg)
            : m_registry(reg) {}

        template <typename T>
        void addUpdate(UUID uuid, const T &updatedComponent, bool skipExecute = false) {
            m_updates.emplace_back(std::make_unique<UpdateOp<T>>(uuid, updatedComponent, skipExecute));
        }

        bool execute() override {
            if (m_updates.empty())
                return false;

            if (m_redo) {
                for (auto &u : m_updates) {
                    u->redo(m_registry);
                }
            } else {
                for (auto &u : m_updates) {
                    u->apply(m_registry);
                }
            }

            return true;
        }

        std::any undo() override {
            for (auto &u : m_updates) {
                u->undo(m_registry);
            }
            m_redo = true;
            return {};
        }

        std::any getResult() override {
            return std::string("updated all components");
        }

        using Command::getResult;

      private:
        entt::registry &m_registry;
        std::vector<std::unique_ptr<IUpdateOp>> m_updates;
        bool m_redo = false;
    };

} // namespace Bess::Canvas::Commands
