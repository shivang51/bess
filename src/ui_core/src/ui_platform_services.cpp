#include "ui_platform_services.h"

#include <memory>

namespace Bess::UI {

    UIPlatformServices::~UIPlatformServices() = default;

    std::optional<std::string> UIPlatformServices::readClipboardText() const {
        return std::nullopt;
    }

    bool UIPlatformServices::writeClipboardText(std::string_view) {
        return false;
    }

    void UIPlatformServices::beginTextInput() {
    }

    void UIPlatformServices::updateTextInputArea(WidgetBounds) {
    }

    void UIPlatformServices::endTextInput() {
    }

    std::shared_ptr<UIPlatformServices> nullUIPlatformServices() {
        static auto services = std::make_shared<UIPlatformServices>();
        return services;
    }

} // namespace Bess::UI
