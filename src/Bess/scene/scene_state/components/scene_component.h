#pragma once

#include "bess_uuid.h"
#include "scene/renderer/material_renderer.h"
#include "scene/renderer/vulkan/path_renderer.h"
#include "scene/scene_state/components/behaviours/mouse_behaviour.h"

namespace Bess::Canvas {
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

    enum class SceneComponentType : uint8_t {
        simulation,
        slot,
        nonSimulation,
        connection,
    };

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

    using PathRenderer = Renderer2D::Vulkan::PathRenderer;

    class SceneState;

    struct PickingId {
        uint32_t runtimeId;
        uint32_t info;

        static constexpr uint32_t invalidRuntimeId = std::numeric_limits<uint32_t>::max();

        static constexpr PickingId invalid() noexcept {
            return {invalidRuntimeId, 0};
        }

        constexpr bool operator==(const PickingId &other) const noexcept {
            return runtimeId == other.runtimeId && info == other.info;
        }

        constexpr bool isValid() const noexcept {
            return runtimeId != invalidRuntimeId;
        }

        constexpr uint64_t toUint64() const noexcept {
            return (static_cast<uint64_t>(runtimeId) << 32) | static_cast<uint64_t>(info);
        }

        constexpr operator uint64_t() const noexcept { return toUint64(); }

        static constexpr PickingId fromUint64(uint64_t value) noexcept {
            return {
                static_cast<uint32_t>(value >> 32),
                static_cast<uint32_t>(value & 0xFFFFFFFF)};
        }

        void set(uint64_t value) {
            runtimeId = static_cast<uint32_t>(value >> 32);
            info = static_cast<uint32_t>(value & 0xFFFFFFFF);
        }
    };

    class SceneComponent : public std::enable_shared_from_this<SceneComponent>,
                           public MouseBehaviour<SceneComponent> {
      public:
        SceneComponent();
        SceneComponent(const SceneComponent &other) = default;
        SceneComponent(UUID uuid);
        SceneComponent(UUID uuid, const Transform &transform);
        virtual ~SceneComponent() = default;

        virtual void draw(SceneState &,
                          std::shared_ptr<Renderer::MaterialRenderer> /*unused*/,
                          std::shared_ptr<PathRenderer> /*unused*/);

        MAKE_GETTER_SETTER(UUID, Uuid, m_uuid)
        MAKE_GETTER_SETTER_WC(Transform, Transform, m_transform, onTransformChanged)
        MAKE_GETTER_SETTER_WC(Style, Style, m_style, onStyleChanged)
        MAKE_GETTER_SETTER_WC(std::string, Name, m_name, onNameChanged)
        MAKE_GETTER_SETTER(UUID, ParentComponent, m_parentComponent)
        MAKE_GETTER_SETTER(std::vector<UUID>, ChildComponents, m_childComponents)
        MAKE_GETTER_SETTER(uint32_t, RuntimeId, m_runtimeId)

        bool isDraggable() const;

        void setPosition(const glm::vec3 &pos);
        void setScale(const glm::vec2 &scale);

        SceneComponentType getType() const;

        template <typename T>
        std::shared_ptr<T> cast() {
            return std::static_pointer_cast<T>(shared_from_this());
        }

        void addChildComponent(const UUID &uuid);

        void setIsDraggable(bool draggable);

        bool isSelected() const;

        void setIsSelected(bool selected);

      protected:
        virtual void onTransformChanged() {}
        virtual void onStyleChanged() {}
        virtual void onNameChanged() {}

        virtual glm::vec2 calculateScale(
            std::shared_ptr<Renderer::MaterialRenderer> materialRenderer);

        virtual void onFirstDraw(SceneState &,
                                 std::shared_ptr<Renderer::MaterialRenderer> /*unused*/,
                                 std::shared_ptr<PathRenderer> /*unused*/);

        UUID m_uuid = UUID::null;
        uint32_t m_runtimeId = PickingId::invalidRuntimeId; // assigned during rendering for picking
        Transform m_transform;
        Style m_style;
        SceneComponentType m_type = SceneComponentType::nonSimulation;
        std::string m_name;

        bool m_isDraggable = false;
        bool m_isSelected = false;
        bool m_isFirstDraw = true;

        UUID m_parentComponent = UUID::null;
        std::vector<UUID> m_childComponents;
    };
} // namespace Bess::Canvas
