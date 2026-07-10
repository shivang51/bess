#pragma once

#include "bess_core/scene/scene_ui/ui_scene_component.h"
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Bess::Canvas::UI {

    struct UISegmentedButtonOption {
        std::string label;
        bool enabled = true;
    };

    using UISegmentedButtonCallback =
        std::function<void(size_t, const UISegmentedButtonOption &)>;

    class SegmentedButtonComp : public UISceneComponent {
      public:
        DEFAULT_CONTRS(SegmentedButtonComp)

        static std::shared_ptr<SegmentedButtonComp>
        create(const std::vector<UISegmentedButtonOption> &options = {},
               size_t selectedIndex = 0,
               const UISegmentedButtonCallback &callback = nullptr);

        void setOptions(const std::vector<UISegmentedButtonOption> &options);
        [[nodiscard]] const std::vector<UISegmentedButtonOption> &
        getOptions() const;
        [[nodiscard]] size_t getSelectedIndex() const;
        void setSelectedIndex(size_t index);

        MAKE_GETTER_SETTER_WC(glm::vec2,
                              SegmentSize,
                              m_segmentSize,
                              makeUIDirty)
        MAKE_GETTER_SETTER_WC(float,
                              MinSegmentWidth,
                              m_minSegmentWidth,
                              makeUIDirty)
        MAKE_GETTER_SETTER_WC(bool,
                              EqualSegmentWidths,
                              m_equalSegmentWidths,
                              makeUIDirty)
        MAKE_GETTER_SETTER(UISegmentedButtonCallback, Callback, m_callback)

        void onDraw(SceneDrawContext &state) override;
        void prepareUI(SceneUIPrepareCtx &state) override;
        bool onMouseEnter(const Events::MouseEnterEvent &e) override;
        bool onMouseLeave(const Events::MouseLeaveEvent &e) override;
        bool onMouseButton(const Events::MouseButtonEvent &e) override;
        bool isFocusable() const override;
        bool wantsKeyboardInput() const override;
        bool onKeyEvent(const SceneEvent &evt) override;

      private:
        [[nodiscard]] bool isSegmentInfo(uint32_t info) const;
        [[nodiscard]] size_t segmentIndexFromInfo(uint32_t info) const;
        [[nodiscard]] size_t validSelectedIndex(size_t index) const;
        [[nodiscard]] size_t nextEnabledIndex(size_t from) const;
        [[nodiscard]] size_t previousEnabledIndex(size_t from) const;
        [[nodiscard]] glm::vec4 segmentRadius(size_t index) const;
        [[nodiscard]] float borderThickness() const;

        void selectFromUser(size_t index);
        void ensureNodes(const std::shared_ptr<UINodeRegistry> &reg);
        void drawGroupFrame(SceneDrawContext &state) const;
        void drawSeparators(SceneDrawContext &state) const;
        void drawSegmentText(SceneDrawContext &state,
                             size_t index,
                             const PickingId &id,
                             const Color &color) const;

        std::vector<UISegmentedButtonOption> m_options;
        size_t m_selectedIndex = 0;
        glm::vec2 m_segmentSize{0.f, 22.f};
        float m_minSegmentWidth = 36.f;
        bool m_equalSegmentWidths = true;
        std::vector<UINode *> m_segmentNodes;
        std::vector<UINode *> m_labelNodes;
        std::vector<glm::vec2> m_cachedSegmentSizes;
        UISegmentedButtonCallback m_callback = nullptr;
        Color m_selectedTextColor{1.f, 1.f, 1.f, 1.f};
        uint32_t m_hoveredInfo = 0u;
    };

} // namespace Bess::Canvas::UI
