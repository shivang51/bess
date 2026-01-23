#pragma once

#include "scene/scene_state/components/behaviours/drag_behaviour.h"
#include "scene/scene_state/components/scene_component.h"
#include "scene/scene_state/components/styles/comp_style.h"
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
                    .setter = [&](const UI::Hook::PropertyValue &value) {
                        m_data = std::get<std::string>(value); // to move bracket;
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
                .defaultValue = m_style.color,
                .binding = {
                    .getter = [&]() -> const glm::vec4 & {
                        return m_style.color;
                    },
                    .setter = [&](const UI::Hook::PropertyValue &value) {
                        m_style.color = std::get<glm::vec4>(value); // to move bracket;
                    },
                },
            });
        } // namespace Bess::Canvas

        void draw(SceneState &state,
                  std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                  std::shared_ptr<PathRenderer> pathRenderer) override {
            if (m_isFirstDraw) {
                onFirstDraw(state, materialRenderer, pathRenderer);
                m_isFirstDraw = false;
            }

            if (m_isScaleDirty) {
                m_transform.scale = calculateScale(materialRenderer);
                m_isScaleDirty = false;
            }

            materialRenderer->drawText(m_data, m_transform.position, m_size, m_style.color, PickingId{m_runtimeId, 0});
        }

      private:
        glm::vec2 calculateScale(std::shared_ptr<Renderer::MaterialRenderer> materialRenderer) override {
            auto textSize = materialRenderer->getTextRenderSize(m_data, (float)m_size);
            textSize.y += Styles::componentStyles.paddingY * 2.f;
            textSize.x += Styles::componentStyles.paddingX * 2.f;
            return textSize;
        }

      private:
        std::string m_data = "New Text";
        size_t m_size = 12.f;
        bool m_isScaleDirty = true;
    };

} // namespace Bess::Canvas
