#pragma once
#include "command.h"
#include "fwd.hpp"
#include <type_traits>
#include <typeindex>

namespace Bess::Cmd {
    template <typename VecType>
        requires std::is_same_v<VecType, glm::vec2> ||
                 std::is_same_v<VecType, glm::vec3> ||
                 std::is_same_v<VecType, glm::vec4>
    class UpdateVecCommand : public Command {
      public:
        UpdateVecCommand(VecType *originalLoc, const VecType &newValue)
            : m_orignalLoc(originalLoc), m_newValue(newValue) {}

        UpdateVecCommand(VecType *originalLoc, const VecType &newValue, const VecType &oldValue)
            : m_orignalLoc(originalLoc), m_newValue(newValue), m_oldValue(oldValue) {}

        bool execute(Canvas::Scene *scene,
                     SimEngine::SimulationEngine *simEngine) override {
            m_oldValue = *m_orignalLoc;
            *m_orignalLoc = m_newValue;
            return true;
        }

        void undo(Canvas::Scene *scene,
                  SimEngine::SimulationEngine *simEngine) override {
            *m_orignalLoc = m_oldValue;
        }

        void redo(Canvas::Scene *scene,
                  SimEngine::SimulationEngine *simEngine) override {
            *m_orignalLoc = m_newValue;
        }

        bool mergeWith(const Command *other) override {
            if (!canMergeWith(other)) {
                return false;
            }

            if (auto otherCmd = dynamic_cast<const UpdateVecCommand<VecType> *>(other)) {
                m_newValue = otherCmd->m_newValue;
                return true;
            }

            return false;
        }

        bool canMergeWith(const Command *other) const override {
            if (auto otherCmd = dynamic_cast<const UpdateVecCommand<VecType> *>(other)) {
                return otherCmd->m_typeIndex == m_typeIndex &&
                       m_orignalLoc == otherCmd->m_orignalLoc;
            }
            return false;
        }

        std::type_index getTypeIndex() const {
            return m_typeIndex;
        }

      private:
        VecType *m_orignalLoc;
        VecType m_newValue;
        VecType m_oldValue;
        std::type_index m_typeIndex{typeid(VecType)};
    };
} // namespace Bess::Cmd
