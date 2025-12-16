#pragma once

#include "bess_uuid.h"
#include "scene/renderer/material_renderer.h"
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

    class SceneComponent : public std::enable_shared_from_this<SceneComponent>,
                           public MouseBehaviour<SceneComponent> {
      public:
        SceneComponent();
        SceneComponent(const SceneComponent &other) = default;
        SceneComponent(UUID uuid);
        SceneComponent(UUID uuid, const Transform &transform);
        virtual ~SceneComponent() = default;

        virtual void draw(std::shared_ptr<Renderer::MaterialRenderer> /*unused*/);

        MAKE_GETTER_SETTER(UUID, Uuid, m_uuid)
        MAKE_GETTER_SETTER_WC(Transform, Transform, m_transform, onTransformChanged)
        MAKE_GETTER_SETTER_WC(Style, Style, m_style, onStyleChanged)
        MAKE_GETTER_SETTER_WC(std::string, Name, m_name, onNameChanged)

        bool isDraggable() const;

        void setPosition(const glm::vec3 &pos);
        void setScale(const glm::vec2 &scale);

        SceneComponentType getType() const;

        template <typename T>
        std::shared_ptr<T> cast() {
            return std::static_pointer_cast<T>(shared_from_this());
        }

        void setIsDraggable(bool draggable);

        bool isSelected() const;

        void setIsSelected(bool selected);

      protected:
        virtual void onTransformChanged() {}
        virtual void onStyleChanged() {}
        virtual void onNameChanged() {}

        virtual glm::vec2 calculateScale(std::shared_ptr<Renderer::MaterialRenderer> materialRenderer);
        virtual void onFirstDraw(const std::shared_ptr<Renderer::MaterialRenderer> &materialRenderer);

        UUID m_uuid = UUID::null;
        Transform m_transform;
        Style m_style;
        SceneComponentType m_type = SceneComponentType::nonSimulation;
        std::string m_name;
        bool m_isDraggable = false;

        bool m_isSelected = false;

        bool m_isFirstDraw = true;
    };
} // namespace Bess::Canvas
