#pragma once
#include "common/bess_uuid.h"
#include "command.h"
#include "fwd.hpp"
#include <type_traits>
#include <typeindex>

namespace Bess::Cmd {
    template <typename ValType>
        requires std::is_same_v<ValType, glm::vec2> ||
                 std::is_same_v<ValType, glm::vec3> ||
                 std::is_same_v<ValType, glm::vec4> ||
                 std::is_same_v<ValType, UUID>
    class UpdateValCommand : public Bess::Cmd::Command {
      public:
        // Callback signature: void(bool isUndo, const ValType &newValue)
        typedef std::function<void(bool, const ValType &)> OnUndoRedoCB;

        UpdateValCommand(ValType *originalLoc, const ValType &newValue)
            : m_orignalLoc(originalLoc), m_newValue(newValue) {}

        UpdateValCommand(ValType *originalLoc,
                         const ValType &newValue,
                         const OnUndoRedoCB &onUndoRedo)
            : m_orignalLoc(originalLoc), m_newValue(newValue),
              m_onUndoRedo(onUndoRedo) {}

        UpdateValCommand(ValType *originalLoc,
                         const ValType &newValue,
                         const ValType &oldValue)
            : m_orignalLoc(originalLoc), m_newValue(newValue),
              m_oldValue(oldValue) {}

        UpdateValCommand(ValType *originalLoc,
                         const ValType &newValue,
                         const ValType &oldValue,
                         const OnUndoRedoCB &onUndoRedo)
            : m_orignalLoc(originalLoc), m_newValue(newValue),
              m_oldValue(oldValue), m_onUndoRedo(onUndoRedo) {
            m_name = "Update " + std::string(typeid(ValType).name());
        }

        bool execute(Canvas::Scene *scene,
                     SimEngine::SimulationEngine *simEngine) override {
            m_oldValue = *m_orignalLoc;
            *m_orignalLoc = m_newValue;
            return true;
        }

        void undo(Canvas::Scene *scene,
                  SimEngine::SimulationEngine *simEngine) override {
            *m_orignalLoc = m_oldValue;
            if (m_onUndoRedo) {
                m_onUndoRedo(true, m_oldValue);
            }
        }

        void redo(Canvas::Scene *scene,
                  SimEngine::SimulationEngine *simEngine) override {
            *m_orignalLoc = m_newValue;
            if (m_onUndoRedo) {
                m_onUndoRedo(false, m_newValue);
            }
        }

        bool mergeWith(const Command *other) override {
            if (!canMergeWith(other)) {
                return false;
            }

            if (auto otherCmd = dynamic_cast<const UpdateValCommand<ValType> *>(other)) {
                m_newValue = otherCmd->m_newValue;
                return true;
            }

            return false;
        }

        bool canMergeWith(const Command *other) const override {
            if (auto otherCmd = dynamic_cast<const UpdateValCommand<ValType> *>(other)) {
                return otherCmd->m_typeIndex == m_typeIndex &&
                       m_orignalLoc == otherCmd->m_orignalLoc;
            }
            return false;
        }

        std::type_index getTypeIndex() const {
            return m_typeIndex;
        }

      private:
        ValType *m_orignalLoc;
        ValType m_newValue;
        ValType m_oldValue;
        OnUndoRedoCB m_onUndoRedo;
        std::type_index m_typeIndex{typeid(ValType)};
    };
} // namespace Bess::Cmd
