#pragma once

#include "bess_uuid.h"
#include "common/log.h"
#include "scene/renderer/material_renderer.h"
#include <cstdint>
namespace Bess::Canvas {
    class Transform {
      public:
        Transform() = default;
        Transform(const Transform &other) = default;

        glm::mat4 getTransform() const;

        operator glm::mat4() const { return getTransform(); }

        glm::vec3 position = {0.f, 0.f, 0.f};
        glm::vec2 scale = {1.f, 1.f};
        float angle = 0.f;
    };

    class Style {
      public:
        Style() = default;
        Style(const Style &other) = default;

        glm::vec4 color = glm::vec4(1.f);
        glm::vec4 borderColor = glm::vec4(1.f);
        glm::vec4 borderSize = glm::vec4(0.f);
        glm::vec4 borderRadius = glm::vec4(0.f);
        glm::vec4 headerColor = glm::vec4(0.2f, 0.2f, 0.2f, 1.f);
    };

    enum class SceneComponentType : uint8_t {
        simulation,
        slot,
        nonSimulation,
    };

#define REG_SCENE_COMP(type) \
    const SceneComponentType getStaticType() const { return type; }

#define MAKE_GETTER_SETTER_WC(type, name, varName, onChange) \
    const type &get##name() const { return varName; }        \
    void set##name(const type &value) {                      \
        varName = value;                                     \
        onChange();                                          \
    }                                                        \
    type &get##name() { return varName; }

#define MAKE_GETTER_SETTER(type, name, varName)       \
    const type &get##name() const { return varName; } \
    void set##name(const type &value) {               \
        varName = value;                              \
    }                                                 \
    type &get##name() { return varName; }

    class SceneComponent : public std::enable_shared_from_this<SceneComponent> {
      public:
        SceneComponent() : m_uuid{UUID()} {};
        SceneComponent(const SceneComponent &other) = default;
        SceneComponent(UUID uuid) : m_uuid(uuid) {}
        SceneComponent(UUID uuid, const Transform &transform)
            : m_uuid(uuid), m_transform(transform) {}
        virtual ~SceneComponent() = default;

        virtual void onTransformChanged() {}
        virtual void onStyleChanged() {}
        virtual void onNameChanged() {}

        virtual void draw(std::shared_ptr<Renderer::MaterialRenderer> renderer) {}

        MAKE_GETTER_SETTER(int, SceneId, sceneId)
        MAKE_GETTER_SETTER(UUID, Uuid, m_uuid)
        MAKE_GETTER_SETTER_WC(Transform, Transform, m_transform, onTransformChanged)
        MAKE_GETTER_SETTER_WC(Style, Style, m_style, onStyleChanged)
        MAKE_GETTER_SETTER_WC(std::string, Name, m_name, onNameChanged)

        void setPosition(const glm::vec3 &pos) { m_transform.position = pos; }
        void setScale(const glm::vec2 &scale) { m_transform.scale = scale; }

        SceneComponentType getType() const { return m_type; }

        template <typename T>
        std::shared_ptr<SceneComponent> cast() {
            return std::static_pointer_cast<T>(shared_from_this());
        }

      protected:
        UUID m_uuid = UUID::null;
        int sceneId = -1;
        Transform m_transform;
        Style m_style;
        SceneComponentType m_type = SceneComponentType::nonSimulation;
        std::string m_name;
    };

    enum class SlotType : uint8_t {
        none,
        digitalInput,
        digitalOutput,
    };

    class SlotSceneComponent : public SceneComponent {
      public:
        SlotSceneComponent() = default;
        SlotSceneComponent(const SlotSceneComponent &other) = default;
        SlotSceneComponent(UUID uuid, UUID parentId)
            : SceneComponent(uuid), m_parentComponentId(parentId) {
            m_type = SceneComponentType::slot;
        }

        SlotSceneComponent(UUID uuid, const Transform &transform)
            : SceneComponent(uuid, transform) {
            m_type = SceneComponentType::slot;
        }
        ~SlotSceneComponent() override = default;

        REG_SCENE_COMP(SceneComponentType::slot)

        MAKE_GETTER_SETTER(SlotType, SlotType, m_slotType)
        MAKE_GETTER_SETTER(UUID, SimEngineId, m_simEngineId)
        MAKE_GETTER_SETTER(UUID, ParentComponentId, m_parentComponentId)

      private:
        SlotType m_slotType = SlotType::none;
        UUID m_simEngineId = UUID::null;
        UUID m_parentComponentId = UUID::null;
    };

    class SimulationSceneComponent : public SceneComponent {
      public:
        SimulationSceneComponent() = default;
        SimulationSceneComponent(const SimulationSceneComponent &other) = default;
        SimulationSceneComponent(UUID uuid) : SceneComponent(uuid) {
            m_type = SceneComponentType::simulation;
        }
        SimulationSceneComponent(UUID uuid, const Transform &transform)
            : SceneComponent(uuid, transform) {
            m_type = SceneComponentType::simulation;
        }
        ~SimulationSceneComponent() override = default;

        void createIOSlots(size_t inputCount, size_t outputCount) {
            auto inpSlots = std::vector<SlotSceneComponent>(inputCount);
            auto outSlots = std::vector<SlotSceneComponent>(outputCount);

            for (auto &slot : inpSlots) {
                slot.setParentComponentId(m_uuid);
                m_inputSlots[slot.getUuid()] = slot;
            }

            for (auto &slot : inpSlots) {
                slot.setParentComponentId(m_uuid);
                m_outputSlots[slot.getUuid()] = slot;
            }
        }

        void onTransformChanged() override {
            BESS_TRACE("TODO update transform of slots");
        }

        void draw(std::shared_ptr<Renderer::MaterialRenderer> renderer) override {

            renderer->drawText(m_name, m_transform.position, 14.f, glm::vec4(1.f), 0);

            renderer->drawQuad(
                m_transform.position,
                glm::vec2(100.f, 100.f),
                glm::vec4(1.f, 0.f, 0.f, 1.f),
                (uint64_t)m_uuid,
                {
                    .borderColor = m_style.borderColor,
                    .borderRadius = m_style.borderRadius,
                    .borderSize = m_style.borderSize,
                });

            // Draw input slots
            for (auto &[uuid, slot] : m_inputSlots) {
                slot.draw(renderer);
            }

            // Draw output slots
            for (auto &[uuid, slot] : m_outputSlots) {
                slot.draw(renderer);
            }
        }

        REG_SCENE_COMP(SceneComponentType::simulation)

        MAKE_GETTER_SETTER(UUID, SimEngineId, m_simEngineId)

      private:
        // Associated simulation engine ID
        UUID m_simEngineId = UUID::null;
        std::unordered_map<UUID, SlotSceneComponent> m_inputSlots;
        std::unordered_map<UUID, SlotSceneComponent> m_outputSlots;
    };

    class SceneState {
      public:
        SceneState() = default;
        ~SceneState() = default;

        void clear() {
            m_componentsMap.clear();
            m_sceneIdToUuidMap.clear();
            m_uuidToSceneIdMap.clear();
            m_typeToUuidsMap.clear();
        }

        // T: SceneComponentType = type of scene component
        template <typename T>
        void addComponent(const std::shared_ptr<SceneComponent> &component) {
            const auto casted = component->cast<T>();
            m_componentsMap[casted->getUuid()] = casted;
            m_sceneIdToUuidMap[casted->getSceneId()] = casted->getUuid();
            m_uuidToSceneIdMap[casted->getUuid()] = casted->getSceneId();
            m_typeToUuidsMap[casted->getType()].emplace_back(casted->getUuid());
        }

        std::shared_ptr<SceneComponent> getComponentByUuid(const UUID &uuid) const {
            if (m_componentsMap.contains(uuid)) {
                return m_componentsMap.at(uuid);
            }
            return nullptr;
        }

        std::shared_ptr<SceneComponent> getComponentBySceneId(int sceneId) const {
            if (m_sceneIdToUuidMap.contains(sceneId)) {
                const auto &uuid = m_sceneIdToUuidMap.at(sceneId);
                return getComponentByUuid(uuid);
            }
            return nullptr;
        }

        const std::vector<UUID> &getComponentsByType(SceneComponentType type) const {
            static const std::vector<UUID> empty;
            auto it = m_typeToUuidsMap.find(type);
            return it != m_typeToUuidsMap.end() ? it->second : empty;
        }

        const std::unordered_map<UUID, std::shared_ptr<SceneComponent>> &getAllComponents() const {
            return m_componentsMap;
        }

      private:
        std::unordered_map<UUID, std::shared_ptr<SceneComponent>> m_componentsMap;
        std::unordered_map<int, UUID> m_sceneIdToUuidMap;
        std::unordered_map<UUID, int> m_uuidToSceneIdMap;
        std::unordered_map<SceneComponentType, std::vector<UUID>> m_typeToUuidsMap;
    };
} // namespace Bess::Canvas
