#pragma once

#include "scene/scene_state/components/behaviours/drag_behaviour.h"
#include "scene/scene_state/components/scene_component.h"
#include "scene/scene_state/scene_state.h"
#include "settings/viewport_theme.h"
#include "ui/ui_hook.h"
#include <typeindex>

namespace Bess::Canvas {
    class NonSimSceneComponent : public SceneComponent,
                                 public DragBehaviour<NonSimSceneComponent> {
      public:
        NonSimSceneComponent();

        static std::unordered_map<std::type_index, std::string> registry;

        template <typename T>
        static void registerComponent(const std::string &name) {
            auto tIdx = std::type_index(typeid(T));
            registry[tIdx] = name;
            m_contrRegistry[tIdx] = []() {
                return std::make_shared<T>();
            };
        }

        static std::shared_ptr<NonSimSceneComponent> getInstance(std::type_index tIdx);

        REG_SCENE_COMP(SceneComponentType::nonSimulation)

        MAKE_GETTER_SETTER(UI::Hook::UIHook, UIHook, m_uiHook)

        virtual std::type_index getTypeIndex() {
            return {typeid(void)};
        }

      protected:
        UI::Hook::UIHook m_uiHook;

      private:
        // this stores functions to invoke constructors of components
        static std::unordered_map<std::type_index,
                                  std::function<std::shared_ptr<NonSimSceneComponent>()>>
            m_contrRegistry;
    };

    class TextComponent : public NonSimSceneComponent {
      public:
        TextComponent() {
            m_subType = "TextComponent";
            m_name = "New Text";
            m_uiHook.addPropertyDescriptor(UI::Hook::PropertyDesc{
                .name = "Text",
                .type = UI::Hook::PropertyDescType::string_t,
                .defaultValue = m_data,
                .binding = {
                    .getter = [&]() -> std::string {
                        return m_data;
                    },
                    .setter = [&](const UI::Hook::PropertyValue &value) {
                        m_data = std::get<std::string>(value);
                        m_isScaleDirty = true; // to move bracket;
                    },
                },
            });

            m_uiHook.addPropertyDescriptor(UI::Hook::PropertyDesc{
                .name = "Font Size",
                .type = UI::Hook::PropertyDescType::uint_t,
                .defaultValue = static_cast<int64_t>(m_size),
                .binding = {
                    .getter = [&]() -> uint64_t {
                        return static_cast<uint64_t>(m_size);
                    },
                    .setter = [&](const UI::Hook::PropertyValue &value) {
                        m_size = static_cast<size_t>(std::get<uint64_t>(value));
                        m_isScaleDirty = true; // to move bracket;
                    },
                },
            });

            m_uiHook.addPropertyDescriptor(UI::Hook::PropertyDesc{
                .name = "Color",
                .type = UI::Hook::PropertyDescType::color_t,
                .defaultValue = m_foregroundColor,
                .binding = {
                    .getter = [&]() -> const glm::vec4 & {
                        return m_foregroundColor;
                    },
                    .setter = [&](const UI::Hook::PropertyValue &value) {
                        m_foregroundColor = std::get<glm::vec4>(value); // to move bracket;
                    },
                },
            });

            m_style.color = ViewportTheme::colors.componentBG;
            m_style.borderRadius = glm::vec4(6.f);
            m_style.borderSize = glm::vec4(1.f);
            m_style.color = ViewportTheme::colors.componentBG;
        } // namespace Bess::Canvas

        void draw(SceneState &state,
                  std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                  std::shared_ptr<PathRenderer> pathRenderer) override;

        std::type_index getTypeIndex() override {
            return typeid(TextComponent);
        }

        MAKE_GETTER_SETTER(std::string, Data, m_data)
        MAKE_GETTER_SETTER(glm::vec4, ForegroundColor, m_foregroundColor)
        MAKE_GETTER_SETTER(size_t, Size, m_size)

      private:
        glm::vec2 calculateScale(std::shared_ptr<Renderer::MaterialRenderer> materialRenderer) override;

      private:
        std::string m_data = "New Text";
        glm::vec4 m_foregroundColor = glm::vec4(1.f);
        size_t m_size = 12.f;
        bool m_isScaleDirty = true;
    };

} // namespace Bess::Canvas

REFLECT_DERIVED_EMPTY(Bess::Canvas::NonSimSceneComponent, Bess::Canvas::SceneComponent)

REFLECT_DERIVED_PROPS(Bess::Canvas::TextComponent, Bess::Canvas::NonSimSceneComponent,
                      ("data", getData, setData),
                      ("foregroundColor", getForegroundColor, setForegroundColor),
                      ("size", getSize, setSize));
