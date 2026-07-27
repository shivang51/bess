#pragma once

#include "component_catalog.h"
#include "ui_composer.h"

namespace Bess::UI {

    class CompCatalogView {
      public:
        void init() {
        }

        void compose(UIComposer &ui) {
            auto col = ui.column([this](UIComposer &col) {
                auto &catalog = SimEngine::ComponentCatalog::instance();
                const auto &tree = catalog.getComponentsTree();

                for (auto &itr : *tree) {
                    const auto &category = itr.first;
                    const auto &components = itr.second;

                    col.treeNode(category,
                                 [this, &components](UIComposer &node) {
                                     for (const auto &compDef : components) {
                                         node.textButton(
                                             compDef->getName(), []() {}, {});
                                     }
                                 });
                }
            });

            col.setLayout({
                .width = LayoutLength::percent(100.f),
                .height = LayoutLength::fitContent(),
                .crossAxisAlignment = LayoutAlignment::start,
            });
        }

      private:
        WidgetRef<Label> m_title;
    };

} // namespace Bess::UI
