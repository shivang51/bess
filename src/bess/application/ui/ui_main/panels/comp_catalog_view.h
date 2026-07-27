#pragma once

#include "ui_composer.h"
namespace Bess::UI {

    class CompCatalogView {
      public:
        void init() {
        }

        void compose(UIComposer &ui) {
            auto card = ui.card([this](UIComposer &card) {
                auto col = card.column([this](UIComposer &col) {
                    col.label("Component Explorer",
                              LabelOptions{.fontSize = 14.f});
                    col.spacer();
                    col.label("Migrating from ImGui — placeholder");
                    col.spacer();
                });

                col.setLayout({
                    .crossAxisAlignment = LayoutAlignment::start,
                });
            });
        }

      private:
        WidgetRef<Label> m_title;
    };

} // namespace Bess::UI
