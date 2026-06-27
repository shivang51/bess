#pragma once

#include "bess_core/commands/command.h"
#include "common/bess_uuid.h"
#include "fwd.hpp"
#include <functional>
#include <type_traits>
#include <typeindex>
#include <utility>

namespace Bess::Cmd {
    template <typename ValType>
        requires std::is_same_v<ValType, glm::vec2> ||
                 std::is_same_v<ValType, glm::vec3> ||
                 std::is_same_v<ValType, glm::vec4> ||
                 std::is_same_v<ValType, UUID>
    class UpdateValCommand : public Command {
      public:
        using OnUndoRedoCB = std::function<void(bool, const ValType &)>;

        UpdateValCommand(ValType *originalLoc, const ValType &newValue)
            : m_originalLoc(originalLoc),
              m_newValue(newValue) {
            m_name = "UpdateValueCommand";
        }

        UpdateValCommand(ValType *originalLoc,
                         const ValType &newValue,
                         OnUndoRedoCB onUndoRedo)
            : m_originalLoc(originalLoc),
              m_newValue(newValue),
              m_onUndoRedo(std::move(onUndoRedo)) {
            m_name = "UpdateValueCommand";
        }

        UpdateValCommand(ValType *originalLoc,
                         const ValType &newValue,
                         const ValType &oldValue)
            : m_originalLoc(originalLoc),
              m_newValue(newValue),
              m_oldValue(oldValue) {
            m_name = "UpdateValueCommand";
        }

        UpdateValCommand(ValType *originalLoc,
                         const ValType &newValue,
                         const ValType &oldValue,
                         OnUndoRedoCB onUndoRedo)
            : m_originalLoc(originalLoc),
              m_newValue(newValue),
              m_oldValue(oldValue),
              m_onUndoRedo(std::move(onUndoRedo)) {
            m_name = "UpdateValueCommand";
        }

        bool execute(const CommandContext &context) override {
            (void)context;
            if (!m_originalLoc) {
                return false;
            }

            m_oldValue = *m_originalLoc;
            if (m_oldValue == m_newValue) {
                return false;
            }

            *m_originalLoc = m_newValue;
            return true;
        }

        void undo(const CommandContext &context) override {
            (void)context;
            if (!m_originalLoc) {
                return;
            }

            *m_originalLoc = m_oldValue;
            if (m_onUndoRedo) {
                m_onUndoRedo(true, m_oldValue);
            }
        }

        void redo(const CommandContext &context) override {
            (void)context;
            if (!m_originalLoc) {
                return;
            }

            *m_originalLoc = m_newValue;
            if (m_onUndoRedo) {
                m_onUndoRedo(false, m_newValue);
            }
        }

        bool mergeWith(const Command *other) override {
            if (!canMergeWith(other)) {
                return false;
            }

            const auto *otherCmd =
                dynamic_cast<const UpdateValCommand<ValType> *>(other);
            if (!otherCmd) {
                return false;
            }

            m_newValue = otherCmd->m_newValue;
            return true;
        }

        bool canMergeWith(const Command *other) const override {
            const auto *otherCmd =
                dynamic_cast<const UpdateValCommand<ValType> *>(other);
            return otherCmd && otherCmd->m_typeIndex == m_typeIndex &&
                   m_originalLoc == otherCmd->m_originalLoc;
        }

        std::type_index getTypeIndex() const {
            return m_typeIndex;
        }

      private:
        ValType *m_originalLoc = nullptr;
        ValType m_newValue{};
        ValType m_oldValue{};
        OnUndoRedoCB m_onUndoRedo;
        std::type_index m_typeIndex{typeid(ValType)};
    };
} // namespace Bess::Cmd
