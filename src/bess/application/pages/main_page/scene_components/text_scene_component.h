#pragma once

#include "common/bess_api.h"

#include "pages/main_page/scene_components/non_sim_scene_component.h"

#define TEXT_SER_PROPS                                                         \
    ("data", getData, setData),                                                \
        ("foregroundColor", getForegroundColor, setForegroundColor),           \
        ("size", getSize, setSize)

namespace Bess::Canvas {
    class BESS_API TextComponent : public NonSimSceneComponent {
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

        bool onMouseButton(const Events::MouseButtonEvent &e) override;

      private:
        glm::vec2 calculateScale(const SceneState &state) override;
        void onJsonApplied() override;

      private:
        std::string m_data = "New Text";
        std::string m_editBuffer;
        glm::vec4 m_foregroundColor = glm::vec4(1.f);
        size_t m_size = 12.f;
        bool m_isScaleDirty = true;
        bool m_editMode = false;
        bool m_justEnteredEdit = false;
    };
} // namespace Bess::Canvas

REFLECT_DERIVED_PROPS(Bess::Canvas::TextComponent,
                      Bess::Canvas::NonSimSceneComponent,
                      TEXT_SER_PROPS);
