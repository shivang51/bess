#pragma once

#include "scene/scene_state/components/behaviours/drag_behaviour.h"
#include "scene/scene_state/components/scene_component.h"
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
            m_name = "New Text";
            m_uiHook.addPropertyDescriptor(UI::Hook::PropertyDesc{
                .name = "Text",
                .type = UI::Hook::PropertyDescType::string_t,
                .defaultValue = m_data,
                .binding = {
                    .getter = [&]() -> std::string {
                        return m_data;
                    },
                    .setter = [&](const UI::Hook::PropertyValue &value) { m_data = std::get<std::string>(value); },
                },
            });
        } // namespace Bess::Canvas

        void draw(SceneState &state,
                  std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                  std::shared_ptr<PathRenderer> pathRenderer) override {
            materialRenderer->drawText(m_data, m_transform.position, 14, m_style.color, PickingId{m_runtimeId, 0});
        }

      private:
        std::string m_data = "New Text";
    };

} // namespace Bess::Canvas
