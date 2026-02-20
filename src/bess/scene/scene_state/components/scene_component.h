#pragma once

#include "common/bess_uuid.h"
#include "scene/renderer/material_renderer.h"
#include "scene/renderer/vulkan/path_renderer.h"
#include "scene/scene_state/components/behaviours/mouse_behaviour.h"
#include "scene/scene_state/components/scene_component_types.h"
#include <unordered_set>

namespace Bess::Canvas {
#define REG_SCENE_COMP(type) \
    SceneComponentType getType() const override { return type; }

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

#define MAKE_GETTER(type, name, varName)              \
    const type &get##name() const { return varName; } \
    type &get##name() { return varName; }

    using PathRenderer = Renderer2D::Vulkan::PathRenderer;

    class SceneState;

    class SceneComponent : public std::enable_shared_from_this<SceneComponent>,
                           public MouseBehaviour<SceneComponent> {
      public:
        SceneComponent();
        SceneComponent(const SceneComponent &other) = default;
        virtual ~SceneComponent() = default;

        virtual void draw(SceneState &,
                          std::shared_ptr<Renderer::MaterialRenderer> /*unused*/,
                          std::shared_ptr<PathRenderer> /*unused*/);

        virtual void drawSchematic(SceneState &,
                                   std::shared_ptr<Renderer::MaterialRenderer> /*unused*/,
                                   std::shared_ptr<PathRenderer> /*unused*/);

        MAKE_GETTER_SETTER(UUID, Uuid, m_uuid)
        MAKE_GETTER_SETTER_WC(Transform, Transform, m_transform, onTransformChanged)
        MAKE_GETTER_SETTER_WC(Style, Style, m_style, onStyleChanged)
        MAKE_GETTER_SETTER_WC(std::string, Name, m_name, onNameChanged)
        MAKE_GETTER_SETTER(UUID, ParentComponent, m_parentComponent)
        MAKE_GETTER_SETTER(std::unordered_set<UUID>, ChildComponents, m_childComponents)
        MAKE_GETTER_SETTER_WC(uint32_t, RuntimeId, m_runtimeId, onRuntimeIdChanged)
        MAKE_GETTER_SETTER(std::string, SubType, m_subType)
        MAKE_GETTER_SETTER_WC(bool, IsSelected, m_isSelected, onSelect)

        virtual void removeChildComponent(const UUID &uuid);

        bool isDraggable() const;

        void setPosition(const glm::vec3 &pos);
        void setScale(const glm::vec2 &scale);

        virtual SceneComponentType getType() const {
            return (SceneComponentType)-1;
        }

        template <typename T>
        std::shared_ptr<T> cast() {
            return std::static_pointer_cast<T>(shared_from_this());
        }

        template <typename T>
        const T &cast() const {
            return static_cast<const T &>(*this);
        }

        void addChildComponent(const UUID &uuid);

        void setIsDraggable(bool draggable);

        virtual glm::vec3 getAbsolutePosition(const SceneState &state) const;

        // Cleanup function
        // Default implementation removes all child components recursively
        // This must be called in the overrides as well
        virtual std::vector<UUID> cleanup(SceneState &state, UUID caller = UUID::null);

        // Called when component is added/attached to the scene
        virtual void onAttach(SceneState &state);

      protected:
        virtual void onNameChanged() {}
        virtual void onTransformChanged() {}
        virtual void onSelect() {}
        virtual void onStyleChanged() {}
        virtual void onRuntimeIdChanged() {}

        virtual glm::vec2 calculateScale(SceneState &,
                                         std::shared_ptr<Renderer::MaterialRenderer> materialRenderer);

        virtual void onFirstDraw(SceneState &,
                                 std::shared_ptr<Renderer::MaterialRenderer> /*unused*/,
                                 std::shared_ptr<PathRenderer> /*unused*/);

        virtual void onFirstSchematicDraw(SceneState &,
                                          std::shared_ptr<Renderer::MaterialRenderer> /*unused*/,
                                          std::shared_ptr<PathRenderer> /*unused*/);

        UUID m_uuid = UUID::null;
        uint32_t m_runtimeId = PickingId::invalidRuntimeId; // assigned during rendering for picking
        Transform m_transform;
        Style m_style;
        std::string m_name;

        bool m_isDraggable = false;
        bool m_isSelected = false;
        bool m_isFirstDraw = true;
        bool m_isFirstSchematicDraw = true;

        UUID m_parentComponent = UUID::null;
        std::unordered_set<UUID> m_childComponents;

        std::string m_subType;
    };
} // namespace Bess::Canvas

REFLECT_PROPS(Bess::Canvas::SceneComponent,
              ("uuid", getUuid, setUuid),
              ("transform", getTransform, setTransform),
              ("style", getStyle, setStyle),
              ("name", getName, setName),
              ("parentComponent", getParentComponent, setParentComponent),
              ("childComponents", getChildComponents, setChildComponents),
              ("subType", getSubType, setSubType));
