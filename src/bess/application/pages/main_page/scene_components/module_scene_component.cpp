#include "module_scene_component.h"
#include "common/bess_uuid.h"
#include "dock_ids.h"
#include "pages/main_page/main_page.h"
#include "scene/scene_state/scene_state.h"

namespace Bess::Canvas {
    void ModuleSceneComponent::onAttach(SceneState &state) {
    }

    std::vector<UUID> ModuleSceneComponent::cleanup(SceneState &state, UUID caller) {
        return {};
    }

    void ModuleSceneComponent::onSelect() {
    }

    std::shared_ptr<ModuleSceneComponent> ModuleSceneComponent::fromNet(const UUID &netId,
                                                                        const std::string &name) {

        auto &mainPageState = Pages::MainPage::getInstance()->getState();
        auto &sceneDriver = mainPageState.getSceneDriver();
        auto &sceneState = sceneDriver->getState();
        auto &netCompMap = mainPageState.getNetIdToCompMap(sceneDriver->getSceneId());

        if (!netCompMap.contains(netId) || netCompMap[netId].empty()) {
            BESS_WARN("[ModuleSceneComponent] No components found for netId {}, cannot create module component.",
                      (uint64_t)netId);
            return nullptr;
        }

        const auto &compIds = netCompMap.at(netId);

        auto newScene = sceneDriver.createNewScene();

        auto &newSceneState = newScene->getState();

        auto comp = std::make_shared<ModuleSceneComponent>();
        comp->setSceneId(newScene->getSceneId());
        comp->setName(name);

        std::vector<std::shared_ptr<SceneComponent>> compsToMove;

        // move components and their dependants to new scene
        {
            std::unordered_set<UUID> visited;
            std::function<void(const UUID &)> collect = [&](const UUID &uuid) {
                if (visited.contains(uuid))
                    return;
                auto comp = sceneState.getComponentByUuid(uuid);
                if (!comp)
                    return;

                visited.insert(uuid);
                for (const auto &depUuid : comp->getDependants(sceneState)) {
                    collect(depUuid);
                }
                compsToMove.push_back(comp);
            };

            for (const auto &startUuid : compIds) {
                collect(startUuid);
            }
        }

        // moving i.e. removing from the active scene to the new scene
        for (const auto &comp : compsToMove) {
            sceneState.removeFromMap(comp->getUuid());
            newSceneState.addComponent(comp, false, false);
        }

        return comp;
    }

    void ModuleSceneComponent::draw(SceneDrawContext &context) {
    }

    void ModuleSceneComponent::update(TimeMs frameTime, SceneState &state) {}
} // namespace Bess::Canvas
