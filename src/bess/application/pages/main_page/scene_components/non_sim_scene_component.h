#pragma once

#include "scene/scene_state/components/behaviours/drag_behaviour.h"
#include "scene/scene_state/components/scene_component.h"
#include "scene/scene_state/scene_state.h"
#include "scene_comp_types.h"
#include "scene_draw_context.h"
#include <typeindex>

#define TEXT_SER_PROPS                                                         \
    ("data", getData, setData),                                                \
        ("foregroundColor", getForegroundColor, setForegroundColor),           \
        ("size", getSize, setSize)

#define WIDGETS_TEST_SER_PROPS                                                 \
    ("toggleValue", getToggleValue, setToggleValue),                           \
        ("textValue", getTextValue, setTextValue),                             \
        ("buttonClicks", getButtonClicks, setButtonClicks),                    \
        ("sliderValue", getSliderValue, setSliderValue),                       \
        ("intSliderValue", getIntSliderValue, setIntSliderValue),              \
        ("dropdownIndex", getDropdownIndex, setDropdownIndex)

namespace Bess::Canvas {
    class NonSimSceneComponent : public SceneComponent,
                                 public DragBehaviour<NonSimSceneComponent> {
      public:
        NonSimSceneComponent() = default;

        static std::unordered_map<std::type_index, std::string> &getRegistry();

        template <typename T>
        static void registerComponent(const std::string &name) {
            auto &registry = getRegistry();
            auto &m_contrRegistry = getContrRegistry();
            auto tIdx = std::type_index(typeid(T));
            registry[tIdx] = name;
            m_contrRegistry[tIdx] = []() { return std::make_shared<T>(); };
        }

        static std::shared_ptr<NonSimSceneComponent>
        getInstance(std::type_index tIdx);

        REG_SCENE_COMP_TYPE("NonSimComponent",
                            SceneComponentType::nonSimulation)
        SCENE_COMP_SER_NP(Bess::Canvas::NonSimSceneComponent,
                          Bess::Canvas::SceneComponent)

        std::vector<std::shared_ptr<SceneComponent>>
        clone(const SceneState &sceneState) const override;

        virtual std::type_index getTypeIndex();

        static void clearRegistry();
        typedef std::function<std::shared_ptr<NonSimSceneComponent>()>
            ContrFunc;

      private:
        // this stores functions to invoke constructors of components
        static std::unordered_map<std::type_index, ContrFunc> &
        getContrRegistry();
    };

    class TextComponent : public NonSimSceneComponent {
      public:
        TextComponent();

        REG_SCENE_COMP_TYPE("TextComponent", SceneComponentType::nonSimulation)
        SCENE_COMP_SER(Bess::Canvas::TextComponent,
                       Bess::Canvas::NonSimSceneComponent,
                       TEXT_SER_PROPS)

        std::vector<std::shared_ptr<SceneComponent>>
        clone(const SceneState &sceneState) const override;

        void draw(SceneDrawContext &context) override;
        void drawSchematic(SceneDrawContext &context) override;

        std::type_index getTypeIndex() override {
            return typeid(TextComponent);
        }

        MAKE_GETTER_SETTER(std::string, Data, m_data)
        MAKE_GETTER_SETTER(glm::vec4, ForegroundColor, m_foregroundColor)
        MAKE_GETTER_SETTER(size_t, Size, m_size)

        void drawPropertiesUI(SceneState &sceneState) override;

      private:
        glm::vec2 calculateScale(const SceneState &state) override;

      private:
        std::string m_data = "New Text";
        glm::vec4 m_foregroundColor = glm::vec4(1.f);
        size_t m_size = 12.f;
        bool m_isScaleDirty = true;
    };

    class WidgetsTestComponent : public NonSimSceneComponent {
      public:
        WidgetsTestComponent();

        REG_SCENE_COMP_TYPE("WidgetsTestComponent",
                            SceneComponentType::nonSimulation)
        SCENE_COMP_SER(Bess::Canvas::WidgetsTestComponent,
                       Bess::Canvas::NonSimSceneComponent,
                       WIDGETS_TEST_SER_PROPS)

        std::vector<std::shared_ptr<SceneComponent>>
        clone(const SceneState &sceneState) const override;

        void draw(SceneDrawContext &context) override;

        std::type_index getTypeIndex() override {
            return typeid(WidgetsTestComponent);
        }

        MAKE_GETTER_SETTER(bool, ToggleValue, m_toggleValue)
        MAKE_GETTER_SETTER(std::string, TextValue, m_textValue)
        MAKE_GETTER_SETTER(int, ButtonClicks, m_buttonClicks)
        MAKE_GETTER_SETTER(float, SliderValue, m_sliderValue)
        MAKE_GETTER_SETTER(int, IntSliderValue, m_intSliderValue)
        MAKE_GETTER_SETTER(int, DropdownIndex, m_dropdownIndex)

      private:
        glm::vec2 calculateScale(const SceneState &state) override;

      private:
        bool m_toggleValue = false;
        std::string m_textValue = "edit me";
        int m_buttonClicks = 0;
        float m_sliderValue = 0.35f;
        int m_intSliderValue = 6;
        int m_dropdownIndex = 1;
    };

} // namespace Bess::Canvas

REG_SCENE_COMP_NP(Bess::Canvas::NonSimSceneComponent,
                  Bess::Canvas::SceneComponent)

REFLECT_DERIVED_PROPS(Bess::Canvas::TextComponent,
                      Bess::Canvas::NonSimSceneComponent,
                      TEXT_SER_PROPS);

REFLECT_DERIVED_PROPS(Bess::Canvas::WidgetsTestComponent,
                      Bess::Canvas::NonSimSceneComponent,
                      WIDGETS_TEST_SER_PROPS);
