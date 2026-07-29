#include "project_session/project_transaction.h"

#include "session_priv.h"

#include "bess_core/scene/scene.h"
#include "bess_core/scene/scene_state/components/scene_component.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include "bess_core/scene_driver.h"
#include "pages/main_page/scene_components/connection_scene_component.h"
#include "pages/main_page/scene_components/slot_scene_component.h"

#include <algorithm>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace Bess {
    namespace {
        std::shared_ptr<Canvas::Scene> getScene(ProjectSession &session,
                                                UUID id) {
            try {
                return id == UUID::null ? session.scenes().getActiveScene()
                                        : session.scenes().getSceneWithId(id);
            } catch (const std::logic_error &) {
                return nullptr;
            }
        }

        Status noScene(UUID id) {
            return Status::fail(Err::invalid,
                                id == UUID::null
                                    ? "there is no active scene"
                                    : "scene " +
                                          std::to_string((std::uint64_t)id) +
                                          " was not found");
        }

        Status addGraph(
            ProjectSession &session,
            UUID sceneId,
            const std::shared_ptr<Canvas::SceneComponent> &comp,
            const std::vector<std::shared_ptr<Canvas::SceneComponent>> &kids,
            UUID parent,
            bool setZ) {
            const auto scene = getScene(session, sceneId);
            if (!scene) {
                return noScene(sceneId);
            }
            if (!comp) {
                return Status::fail(Err::badArg, "component is null");
            }

            const auto hooks = session.hooks();
            const bool deferAttach = !kids.empty() || parent != UUID::null;
            auto &state = scene->getState();
            std::unordered_set<UUID> ids;
            const auto valid = [&](const auto &item) {
                return item && ids.insert(item->getUuid()).second &&
                       !state.isComponentValid(item->getUuid());
            };
            if (!valid(comp)) {
                return Status::fail(
                    Err::invalid,
                    "component is null, duplicated, or already in the scene");
            }
            for (const auto &kid : kids) {
                if (!valid(kid)) {
                    return Status::fail(
                        Err::invalid,
                        "child is null, duplicated, or already in the scene");
                }
            }
            if (parent != UUID::null && !state.isComponentValid(parent)) {
                return Status::fail(Err::invalid, "slot parent was not found");
            }

            std::vector<std::shared_ptr<Canvas::SceneComponent>> added;
            const auto roll = [&] {
                for (auto it = added.rbegin(); it != added.rend(); ++it) {
                    if (*it && state.isComponentValid((*it)->getUuid())) {
                        (void)Edit::rm(scene, *it, UUID::master, hooks);
                    }
                }
                return std::ranges::none_of(added, [&](const auto &item) {
                    return item && state.isComponentValid(item->getUuid());
                });
            };
            const auto addFail = [&](std::string msg) {
                return roll() ? Status::fail(Err::apply, std::move(msg))
                              : Status::fail(
                                    Err::rollback,
                                    std::move(msg) +
                                        " and its partial changes could not be "
                                        "rolled back");
            };

            const auto rootOk =
                Edit::add(scene,
                          comp,
                          {.setZ = setZ, .attach = !deferAttach, .event = true},
                          hooks);
            if (state.isComponentValid(comp->getUuid())) {
                added.push_back(comp);
            }
            if (!rootOk) {
                return addFail("could not add component");
            }

            for (const auto &kid : kids) {
                const auto kidOk =
                    Edit::add(scene,
                              kid,
                              {.setZ = false, .attach = true, .event = true},
                              hooks);
                if (state.isComponentValid(kid->getUuid())) {
                    added.push_back(kid);
                }
                if (!kidOk) {
                    return addFail("could not add a child component");
                }
                state.attachChild(comp->getUuid(), kid->getUuid(), false);
            }

            if (parent != UUID::null) {
                state.attachChild(parent, comp->getUuid(), false);
            } else if (deferAttach && state.isComponentValid(comp->getUuid())) {
                comp->onAttach(state);
            }
            return Status::ok();
        }

        Status rmGraph(
            ProjectSession &session,
            UUID sceneId,
            const std::shared_ptr<Canvas::SceneComponent> &comp,
            const std::vector<std::shared_ptr<Canvas::SceneComponent>> &kids) {
            const auto scene = getScene(session, sceneId);
            if (!scene) {
                return noScene(sceneId);
            }
            if (!comp || !scene->getState().isComponentValid(comp->getUuid())) {
                return Status::fail(Err::invalid,
                                    "component is not in the scene");
            }
            (void)Edit::rm(scene, comp, UUID::master, session.hooks());
            auto &state = scene->getState();
            for (const auto &kid : kids) {
                if (kid && state.isComponentValid(kid->getUuid())) {
                    (void)Edit::rm(scene, kid, UUID::master, session.hooks());
                }
            }
            if (state.isComponentValid(comp->getUuid()) ||
                std::ranges::any_of(kids, [&](const auto &kid) {
                    return kid && state.isComponentValid(kid->getUuid());
                })) {
                return Status::fail(Err::undo,
                                    "could not remove component graph");
            }
            return Status::ok();
        }

        class FnStep final : public ProjectSessionStep {
          public:
            FnStep(std::string name,
                   ProjectTx::Act apply,
                   ProjectTx::Act undo,
                   ProjectTx::Act redo,
                   std::size_t bytes)
                : m_name(std::move(name)),
                  m_apply(std::move(apply)),
                  m_undo(std::move(undo)),
                  m_redo(std::move(redo)),
                  m_bytes(bytes) {
            }

            Status apply(ProjectSession &session) override {
                return tag(m_apply ? m_apply(session)
                                   : Status::fail(Err::badArg,
                                                  "apply callback is empty"));
            }

            Status undo(ProjectSession &session) override {
                return tag(m_undo ? m_undo(session)
                                  : Status::fail(Err::badArg,
                                                 "undo callback is empty"));
            }

            Status redo(ProjectSession &session) override {
                return m_redo ? m_redo(session) : apply(session);
            }

            std::size_t bytes() const noexcept override {
                return sizeof(*this) + m_name.size() + m_bytes;
            }

          private:
            Status tag(Status status) const {
                if (status || m_name.empty()) {
                    return status;
                }
                return Status::fail(status.err(), m_name + ": " + status.msg());
            }

            std::string m_name;
            ProjectTx::Act m_apply;
            ProjectTx::Act m_undo;
            ProjectTx::Act m_redo;
            std::size_t m_bytes = 0;
        };

        class AddStep final : public ProjectSessionStep {
          public:
            AddStep(UUID scene,
                    std::shared_ptr<Canvas::SceneComponent> comp,
                    std::vector<std::shared_ptr<Canvas::SceneComponent>> kids,
                    UUID parent,
                    bool tracked)
                : m_scene(scene),
                  m_comp(std::move(comp)),
                  m_kids(std::move(kids)),
                  m_parent(parent),
                  m_tracked(tracked) {
            }

            Status apply(ProjectSession &session) override {
                if (m_tracked && m_first) {
                    m_first = false;
                    const auto scene = getScene(session, m_scene);
                    if (!scene || !m_comp ||
                        !scene->getState().isComponentValid(
                            m_comp->getUuid())) {
                        return Status::fail(
                            Err::invalid,
                            "tracked component is not in the scene");
                    }
                    for (const auto &kid : m_kids) {
                        if (!kid || !scene->getState().isComponentValid(
                                        kid->getUuid())) {
                            return Status::fail(
                                Err::invalid,
                                "tracked child is not in the scene");
                        }
                    }
                    return Status::ok();
                }
                m_first = false;
                return addGraph(
                    session, m_scene, m_comp, m_kids, m_parent, !m_added);
            }

            Status undo(ProjectSession &session) override {
                m_added = true;
                return rmGraph(session, m_scene, m_comp, m_kids);
            }

            Status redo(ProjectSession &session) override {
                m_added = true;
                return addGraph(
                    session, m_scene, m_comp, m_kids, m_parent, false);
            }

            std::size_t bytes() const noexcept override {
                return sizeof(*this) +
                       m_kids.size() *
                           sizeof(std::shared_ptr<Canvas::SceneComponent>);
            }

          private:
            UUID m_scene;
            std::shared_ptr<Canvas::SceneComponent> m_comp;
            std::vector<std::shared_ptr<Canvas::SceneComponent>> m_kids;
            UUID m_parent;
            bool m_tracked = false;
            bool m_first = true;
            bool m_added = false;
        };

        struct RmData {
            std::set<UUID> roots;
            std::vector<std::shared_ptr<Canvas::SceneComponent>> comps;
            std::unordered_map<UUID, OrderedSet<UUID>> kids;
            bool captured = false;
        };

        class RmStep final : public ProjectSessionStep {
          public:
            RmStep(UUID scene, std::vector<UUID> ids)
                : m_scene(scene),
                  m_data(std::make_shared<RmData>()) {
                m_data->roots.insert(ids.begin(), ids.end());
            }

            Status apply(ProjectSession &session) override {
                const auto scene = getScene(session, m_scene);
                if (!scene) {
                    return noScene(m_scene);
                }
                if (!m_data->captured) {
                    const auto status = capture(session, scene);
                    if (!status) {
                        return status;
                    }
                }
                return remove(session, scene, Err::apply);
            }

            Status undo(ProjectSession &session) override {
                const auto scene = getScene(session, m_scene);
                if (!scene) {
                    return noScene(m_scene);
                }
                return restore(session, scene);
            }

            Status redo(ProjectSession &session) override {
                const auto scene = getScene(session, m_scene);
                if (!scene) {
                    return noScene(m_scene);
                }
                return remove(session, scene, Err::redo);
            }

            std::size_t bytes() const noexcept override {
                return sizeof(*this) +
                       m_data->comps.size() *
                           sizeof(std::shared_ptr<Canvas::SceneComponent>);
            }

          private:
            Status capture(ProjectSession &session,
                           const std::shared_ptr<Canvas::Scene> &scene) {
                std::vector<UUID> order;
                std::unordered_set<UUID> seen;
                const auto collect = [&](UUID id, const auto &self) -> void {
                    if (seen.contains(id)) {
                        return;
                    }
                    const auto comp =
                        scene->getState()
                            .getComponentByUuidSP<Canvas::SceneComponent>(id);
                    if (!comp) {
                        return;
                    }
                    seen.insert(id);
                    for (const auto dep :
                         Edit::deps(scene, comp, session.hooks())) {
                        self(dep, self);
                    }
                    order.push_back(id);
                };
                for (const auto id : m_data->roots) {
                    collect(id, collect);
                }
                Edit::sort(scene, order, session.hooks());
                if (order.empty()) {
                    return Status::fail(Err::invalid,
                                        "no requested component was found");
                }

                auto &state = scene->getState();
                const auto snapKids = [&](UUID id) {
                    if (id == UUID::null || m_data->kids.contains(id)) {
                        return;
                    }
                    const auto comp = state.getComponentByUuid(id);
                    if (comp) {
                        m_data->kids.emplace(id, comp->getChildComponents());
                    }
                };

                for (const auto id : order) {
                    const auto comp =
                        state.getComponentByUuidSP<Canvas::SceneComponent>(id);
                    if (!comp) {
                        continue;
                    }
                    snapKids(id);
                    snapKids(comp->getParentComponent());
                    m_data->comps.push_back(comp);
                }
                m_data->captured = true;
                return Status::ok();
            }

            Status remove(ProjectSession &session,
                          const std::shared_ptr<Canvas::Scene> &scene,
                          Err err) {
                bool any = false;
                auto &state = scene->getState();
                for (const auto &comp : m_data->comps) {
                    if (!comp || !state.isComponentValid(comp->getUuid())) {
                        continue;
                    }
                    (void)Edit::rm(scene, comp, UUID::master, session.hooks());
                    any = true;
                    if (state.isComponentValid(comp->getUuid())) {
                        const auto roll = restore(session, scene);
                        return roll ? Status::fail(err,
                                                   "could not remove component")
                                    : Status::fail(
                                          Err::rollback,
                                          "component removal failed and "
                                          "partial changes could not be "
                                          "rolled back");
                    }
                }
                return any ? Status::ok()
                           : Status::fail(err, "no component could be removed");
            }

            Status restore(ProjectSession &session,
                           const std::shared_ptr<Canvas::Scene> &scene) {
                auto &state = scene->getState();
                std::vector<std::shared_ptr<Canvas::SceneComponent>> pending;
                const auto addOne =
                    [&](const std::shared_ptr<Canvas::SceneComponent> &comp) {
                        if (!comp) {
                            return false;
                        }
                        if (state.isComponentValid(comp->getUuid())) {
                            return true;
                        }
                        if (!Edit::add(
                                scene,
                                comp,
                                {.setZ = false, .attach = false, .event = true},
                                session.hooks())) {
                            return false;
                        }
                        const auto parent = comp->getParentComponent();
                        if (parent != UUID::null &&
                            state.isComponentValid(parent)) {
                            state.attachChild(parent, comp->getUuid(), false);
                        }
                        comp->onAttach(state);
                        return true;
                    };

                for (auto it = m_data->comps.rbegin();
                     it != m_data->comps.rend();
                     ++it) {
                    if (!addOne(*it)) {
                        pending.push_back(*it);
                    }
                }
                while (!pending.empty()) {
                    std::vector<std::shared_ptr<Canvas::SceneComponent>> next;
                    bool progress = false;
                    for (const auto &comp : pending) {
                        if (addOne(comp)) {
                            progress = true;
                        } else {
                            next.push_back(comp);
                        }
                    }
                    if (!progress) {
                        return Status::fail(
                            Err::undo,
                            "could not restore all removed components");
                    }
                    pending = std::move(next);
                }

                for (const auto &[id, kids] : m_data->kids) {
                    const auto comp = state.getComponentByUuid(id);
                    if (comp) {
                        comp->setChildComponents(kids);
                    }
                }
                return Status::ok();
            }

            UUID m_scene;
            std::shared_ptr<RmData> m_data;
        };

        class MoveStep final : public ProjectSessionStep {
          public:
            MoveStep(
                UUID scene, UUID id, glm::vec3 from, glm::vec3 to, bool tracked)
                : m_scene(scene),
                  m_id(id),
                  m_from(from),
                  m_to(to),
                  m_tracked(tracked) {
            }

            Status apply(ProjectSession &session) override {
                if (m_tracked && m_first) {
                    m_first = false;
                    return check(session);
                }
                m_first = false;
                return put(session, m_to);
            }

            Status undo(ProjectSession &session) override {
                return put(session, m_from);
            }

            Status redo(ProjectSession &session) override {
                return put(session, m_to);
            }

            bool merge(const ProjectSessionStep &next) override {
                const auto *move = dynamic_cast<const MoveStep *>(&next);
                if (!move || move->m_scene != m_scene || move->m_id != m_id) {
                    return false;
                }
                m_to = move->m_to;
                return true;
            }

          private:
            Status check(ProjectSession &session) const {
                const auto scene = getScene(session, m_scene);
                return scene && scene->getState().isComponentValid(m_id)
                           ? Status::ok()
                           : Status::fail(Err::invalid,
                                          "moved component was not found");
            }

            Status put(ProjectSession &session, const glm::vec3 &pos) {
                const auto scene = getScene(session, m_scene);
                const auto comp =
                    scene ? scene->getState().getComponentByUuid(m_id)
                          : nullptr;
                if (!comp) {
                    return Status::fail(Err::invalid,
                                        "moved component was not found");
                }
                comp->getTransform().position = pos;
                return Status::ok();
            }

            UUID m_scene;
            UUID m_id;
            glm::vec3 m_from;
            glm::vec3 m_to;
            bool m_tracked = false;
            bool m_first = true;
        };

        class ParentStep final : public ProjectSessionStep {
          public:
            ParentStep(UUID scene, UUID id, UUID from, UUID to, bool tracked)
                : m_scene(scene),
                  m_id(id),
                  m_from(from),
                  m_to(to),
                  m_tracked(tracked) {
            }

            Status apply(ProjectSession &session) override {
                if (m_tracked && m_first) {
                    m_first = false;
                    return check(session);
                }
                m_first = false;
                return put(session, m_to);
            }

            Status undo(ProjectSession &session) override {
                return put(session, m_from);
            }

            Status redo(ProjectSession &session) override {
                return put(session, m_to);
            }

            bool merge(const ProjectSessionStep &next) override {
                const auto *parent = dynamic_cast<const ParentStep *>(&next);
                if (!parent || parent->m_scene != m_scene ||
                    parent->m_id != m_id) {
                    return false;
                }
                m_to = parent->m_to;
                return true;
            }

          private:
            Status check(ProjectSession &session) const {
                const auto scene = getScene(session, m_scene);
                return scene && scene->getState().isComponentValid(m_id)
                           ? Status::ok()
                           : Status::fail(Err::invalid,
                                          "reparented component was not found");
            }

            Status put(ProjectSession &session, UUID parent) {
                const auto scene = getScene(session, m_scene);
                if (!scene) {
                    return noScene(m_scene);
                }
                auto &state = scene->getState();
                if (!state.isComponentValid(m_id) ||
                    (parent != UUID::null && !state.isComponentValid(parent))) {
                    return Status::fail(Err::invalid,
                                        "component or parent was not found");
                }
                if (parent == UUID::null) {
                    state.detachChild(m_id, false);
                } else {
                    state.attachChild(parent, m_id, false);
                }
                return Status::ok();
            }

            UUID m_scene;
            UUID m_id;
            UUID m_from;
            UUID m_to;
            bool m_tracked = false;
            bool m_first = true;
        };
    } // namespace

    ProjectTx::ProjectTx(ProjectSession &session,
                         std::string name,
                         TxOpts opts,
                         StateId base)
        : m(std::make_unique<Impl>()) {
        m->session = &session;
        m->name = std::move(name);
        m->opts = opts;
        m->base = base;
    }

    ProjectTx::ProjectTx(ProjectTx &&) noexcept = default;
    ProjectTx &ProjectTx::operator=(ProjectTx &&) noexcept = default;
    ProjectTx::~ProjectTx() = default;

    Status ProjectTx::bad(Status status) {
        if (m && m->err) {
            m->err = status;
        }
        return status;
    }

    Status ProjectTx::addComp(
        std::shared_ptr<Canvas::SceneComponent> comp,
        std::vector<std::shared_ptr<Canvas::SceneComponent>> kids,
        UUID scene) {
        if (!m || m->done || !m->session) {
            return bad(
                Status::fail(Err::invalid, "transaction is already finished"));
        }
        if (!comp) {
            return bad(Status::fail(Err::badArg, "component is null"));
        }
        scene = m->session->sceneId(scene);
        if (!getScene(*m->session, scene)) {
            return bad(noScene(scene));
        }
        m->ops.push_back(std::make_unique<AddStep>(
            scene, std::move(comp), std::move(kids), UUID::null, false));
        return Status::ok();
    }

    Status ProjectTx::trackAdd(
        std::shared_ptr<Canvas::SceneComponent> comp,
        std::vector<std::shared_ptr<Canvas::SceneComponent>> kids,
        UUID scene) {
        if (!m || m->done || !m->session) {
            return bad(
                Status::fail(Err::invalid, "transaction is already finished"));
        }
        if (!comp) {
            return bad(Status::fail(Err::badArg, "component is null"));
        }
        scene = m->session->sceneId(scene);
        const auto target = getScene(*m->session, scene);
        if (!target) {
            return bad(noScene(scene));
        }
        if (!target->getState().isComponentValid(comp->getUuid())) {
            return bad(Status::fail(Err::invalid,
                                    "tracked component is not in the scene"));
        }
        for (const auto &kid : kids) {
            if (!kid || !target->getState().isComponentValid(kid->getUuid())) {
                return bad(Status::fail(Err::invalid,
                                        "tracked child is not in the scene"));
            }
        }
        m->ops.push_back(std::make_unique<AddStep>(
            scene, std::move(comp), std::move(kids), UUID::null, true));
        return Status::ok();
    }

    Status ProjectTx::rmComp(std::vector<UUID> ids, UUID scene) {
        if (!m || m->done || !m->session) {
            return bad(
                Status::fail(Err::invalid, "transaction is already finished"));
        }
        if (ids.empty()) {
            return bad(Status::fail(Err::badArg, "component id list is empty"));
        }
        scene = m->session->sceneId(scene);
        if (!getScene(*m->session, scene)) {
            return bad(noScene(scene));
        }
        m->ops.push_back(std::make_unique<RmStep>(scene, std::move(ids)));
        return Status::ok();
    }

    Status ProjectTx::rmComp(UUID id, UUID scene) {
        return rmComp(std::vector<UUID>{id}, scene);
    }

    Status ProjectTx::addSlot(std::shared_ptr<Canvas::SlotSceneComponent> slot,
                              UUID parent,
                              UUID scene) {
        if (!m || m->done || !m->session) {
            return bad(
                Status::fail(Err::invalid, "transaction is already finished"));
        }
        if (!slot || parent == UUID::null) {
            return bad(Status::fail(Err::badArg, "slot or parent is invalid"));
        }
        scene = m->session->sceneId(scene);
        const auto target = getScene(*m->session, scene);
        if (!target) {
            return bad(noScene(scene));
        }
        if (!target->getState().isComponentValid(parent)) {
            return bad(Status::fail(Err::invalid, "slot parent was not found"));
        }
        m->ops.push_back(std::make_unique<AddStep>(
            scene,
            std::move(slot),
            std::vector<std::shared_ptr<Canvas::SceneComponent>>{},
            parent,
            false));
        return Status::ok();
    }

    Status
    ProjectTx::addConn(std::shared_ptr<Canvas::ConnectionSceneComponent> conn,
                       UUID scene) {
        return addComp(std::move(conn), {}, scene);
    }

    Status
    ProjectTx::trackConn(std::shared_ptr<Canvas::ConnectionSceneComponent> conn,
                         UUID scene) {
        return trackAdd(std::move(conn), {}, scene);
    }

    Status ProjectTx::moveComp(UUID id, glm::vec3 pos, UUID scene) {
        if (!m || m->done || !m->session) {
            return bad(
                Status::fail(Err::invalid, "transaction is already finished"));
        }
        scene = m->session->sceneId(scene);
        const auto target = getScene(*m->session, scene);
        const auto comp =
            target ? target->getState().getComponentByUuid(id) : nullptr;
        if (!comp) {
            return bad(Status::fail(Err::invalid, "component was not found"));
        }
        m->ops.push_back(std::make_unique<MoveStep>(
            scene, id, comp->getTransform().position, pos, false));
        return Status::ok();
    }

    Status
    ProjectTx::trackMove(UUID id, glm::vec3 from, glm::vec3 to, UUID scene) {
        if (!m || m->done || !m->session) {
            return bad(
                Status::fail(Err::invalid, "transaction is already finished"));
        }
        scene = m->session->sceneId(scene);
        const auto target = getScene(*m->session, scene);
        if (!target || !target->getState().isComponentValid(id)) {
            return bad(
                Status::fail(Err::invalid, "moved component was not found"));
        }
        m->ops.push_back(std::make_unique<MoveStep>(scene, id, from, to, true));
        return Status::ok();
    }

    Status ProjectTx::parentComp(UUID id, UUID parent, UUID scene) {
        if (!m || m->done || !m->session) {
            return bad(
                Status::fail(Err::invalid, "transaction is already finished"));
        }
        scene = m->session->sceneId(scene);
        const auto target = getScene(*m->session, scene);
        const auto comp =
            target ? target->getState().getComponentByUuid(id) : nullptr;
        if (!comp) {
            return bad(Status::fail(Err::invalid, "component was not found"));
        }
        if (parent != UUID::null &&
            !target->getState().isComponentValid(parent)) {
            return bad(
                Status::fail(Err::invalid, "parent component was not found"));
        }
        m->ops.push_back(std::make_unique<ParentStep>(
            scene, id, comp->getParentComponent(), parent, false));
        return Status::ok();
    }

    Status ProjectTx::trackParent(UUID id, UUID from, UUID to, UUID scene) {
        if (!m || m->done || !m->session) {
            return bad(
                Status::fail(Err::invalid, "transaction is already finished"));
        }
        scene = m->session->sceneId(scene);
        const auto target = getScene(*m->session, scene);
        if (!target || !target->getState().isComponentValid(id) ||
            (to != UUID::null && !target->getState().isComponentValid(to))) {
            return bad(Status::fail(
                Err::invalid, "reparented component or parent was not found"));
        }
        m->ops.push_back(
            std::make_unique<ParentStep>(scene, id, from, to, true));
        return Status::ok();
    }

    Status ProjectTx::step(
        std::string name, Act apply, Act undo, Act redo, std::size_t bytes) {
        if (!m || m->done || !m->session) {
            return bad(
                Status::fail(Err::invalid, "transaction is already finished"));
        }
        if (!apply || !undo) {
            return bad(Status::fail(
                Err::badArg,
                "transaction step needs apply and undo callbacks"));
        }
        m->ops.push_back(std::make_unique<FnStep>(
            name.empty() ? "Edit step" : std::move(name),
            std::move(apply),
            std::move(undo),
            std::move(redo),
            bytes));
        return Status::ok();
    }

    TxResult ProjectTx::commit() {
        if (!m || !m->session) {
            return {.status = Status::fail(Err::invalid,
                                           "transaction has no session")};
        }
        return m->session->commit(*this);
    }

    void ProjectTx::cancel() noexcept {
        if (m) {
            m->ops.clear();
            m->done = true;
            m->session = nullptr;
        }
    }

    bool ProjectTx::isEmpty() const noexcept {
        return !m || m->ops.empty();
    }

    bool ProjectTx::done() const noexcept {
        return !m || m->done;
    }

    std::string_view ProjectTx::name() const noexcept {
        return m ? std::string_view{m->name} : std::string_view{};
    }

    StateId ProjectTx::base() const noexcept {
        return m ? m->base : 0;
    }
} // namespace Bess
