#include "common/theme.h"

namespace Bess {
    glm::vec3 Theme::backgroundColor = {0.1f, 0.1f, 0.1f};
const glm::vec3 Theme::componentBGColor = {.22f, .22f, 0.22f};
const glm::vec4 Theme::componentBorderColor = {0.42f, 0.42f, 0.42f, 1.f};
const glm::vec4 Theme::stateHighColor = {0.42f, 0.72f, 0.42f, 1.f};
const glm::vec4 Theme::wireColor = { 200.f / 255.f, 200.f / 255.f, 200.f / 255.f, 1.f };
const glm::vec3 Theme::selectedWireColor = {236.f / 255.f, 110.f / 255.f,
                                            34.f / 255.f};
const glm::vec3 Theme::compHeaderColor = {81.f / 255.f, 74.f / 255.f,
                                          74.f / 255.f};
const glm::vec3 Theme::selectedCompColor = {200.f / 255.f, 200.f / 255.f,
                                            200.f / 255.f};
} // namespace Bess
