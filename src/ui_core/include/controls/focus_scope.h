#pragma once

#include "controls/basic_widgets.h"

namespace Bess::UI {

    struct FocusScopeOptions {
        FocusScopePolicy focus{
            .trapFocus = false,
            .autoFocus = true,
            .restoreFocus = true,
        };
        FlexContainerOptions container{
            .direction = LayoutDirection::vertical,
            .mainAxisAlignment = LayoutAlignment::start,
            .crossAxisAlignment = LayoutAlignment::center,
            .stretchWidth = true,
            .stretchHeight = true,
            .clipChildren = false,
            .hitTestVisible = false,
        };
    };

    // Transparent layout boundary with a retained focus scope. FocusScope is
    // intentionally independent of dialog or popup visuals so it can also be
    // used for panels, inspectors, composite controls, and keyboard islands.
    class BESS_API FocusScope final : public FlexContainer {
      public:
        explicit FocusScope(FocusScopeOptions options = {});

        [[nodiscard]] std::string_view typeName() const noexcept override;
        void onMount(WidgetMountContext &context) override;
        void onUnmount(WidgetTree &state, WidgetId id) override;

        bool setDefaultFocus(WidgetId widget);
        bool focusDefault();

      private:
        FocusScopeOptions m_options;
        WidgetTree *m_state = nullptr;
        WidgetId m_id;
    };

} // namespace Bess::UI
