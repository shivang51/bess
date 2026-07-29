#include "project_session/project_session.h"

#include "session_priv.h"

#include "bess_core/scene/scene.h"
#include "bess_core/scene/scene_state/components/scene_component.h"
#include "bess_core/scene_driver.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "simulation_engine.h"

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <utility>

namespace Bess {
    namespace {
        enum class Call : std::uint8_t {
            apply,
            undo,
            redo,
        };

        Status
        callStep(ProjectSessionStep &step, ProjectSession &session, Call call) {
            try {
                switch (call) {
                case Call::apply:
                    return step.apply(session);
                case Call::undo:
                    return step.undo(session);
                case Call::redo:
                    return step.redo(session);
                }
            } catch (const std::exception &ex) {
                return Status::fail(Err::except,
                                    std::string("edit callback threw: ") +
                                        ex.what());
            } catch (...) {
                return Status::fail(Err::except, "edit callback threw");
            }
            return Status::fail(Err::except, "invalid edit phase");
        }

        class NameStep final : public ProjectSessionStep {
          public:
            NameStep(std::string from, std::string to)
                : m_from(std::move(from)),
                  m_to(std::move(to)) {
            }

            Status apply(ProjectSession &session) override {
                putName(session, m_to);
                return Status::ok();
            }

            Status undo(ProjectSession &session) override {
                putName(session, m_from);
                return Status::ok();
            }

            Status redo(ProjectSession &session) override {
                putName(session, m_to);
                return Status::ok();
            }

            bool merge(const ProjectSessionStep &next) override {
                const auto *name = dynamic_cast<const NameStep *>(&next);
                if (!name) {
                    return false;
                }
                m_to = name->m_to;
                return true;
            }

            std::size_t bytes() const noexcept override {
                return sizeof(*this) + m_from.size() + m_to.size();
            }

          private:
            std::string m_from;
            std::string m_to;
        };

        std::size_t entryBytes(const HistEntry &entry) {
            std::size_t bytes = sizeof(entry) + entry.name.size();
            for (const auto &op : entry.ops) {
                bytes += op ? op->bytes() : 0;
            }
            return bytes;
        }
    } // namespace

    class ProjectSession::Impl {
      public:
        explicit Impl(SessOpts opts) : opts(std::move(opts)) {
        }

        void prune() {
            while (!hist.empty() && (hist.size() > opts.maxHist ||
                                     histBytes > opts.maxHistBytes)) {
                histBytes -= std::min(histBytes, hist.front().bytes);
                hist.erase(hist.begin());
                if (pos > 0) {
                    --pos;
                }
            }
        }

        SessOpts opts;
        mutable std::mutex mu;
        std::vector<HistEntry> hist;
        std::size_t pos = 0;
        std::size_t histBytes = 0;
        StateId state = 1;
        StateId next = 1;
        std::optional<StateId> saved;
        bool busy = false;
        bool faulted = false;
        std::shared_ptr<const Edit::Hooks> hooks = Edit::baseHooks();
    };

    void ProjectSessionStep::putName(ProjectSession &session,
                                     std::string name) {
        session.m_doc->setName(std::move(name));
    }

    ProjectSession::ProjectSession(SessOpts opts)
        : m(std::make_unique<Impl>(std::move(opts))) {
        m_scenes = addSubSystem<SceneDriver>();
        m_sim = addSubSystem<SimEngine::SimulationEngine>();
    }

    ProjectSession::~ProjectSession() = default;

    void ProjectSession::onBeginFrame() {
        ISubSysContainer::beginFrame();
    }

    void ProjectSession::onEndFrame() {
        ISubSysContainer::endFrame();
    }

    void ProjectSession::onPreUpdate() {
        ISubSysContainer::preUpdate();
    }

    void ProjectSession::onUpdate(TimeMs dt) {
        ISubSysContainer::update(dt);
    }

    void ProjectSession::onPreDraw() {
        ISubSysContainer::preDraw();
    }

    void ProjectSession::onDraw() {
        ISubSysContainer::draw();
    }

    void ProjectSession::onPostDraw() {
        ISubSysContainer::postDraw();
    }

    void ProjectSession::onInit() {
        if (m_doc || m_destroyed) {
            return;
        }
        ISubSysContainer::init();
        m_doc = std::make_unique<ProjectDoc>(m_scenes, m_sim, m->opts.doc);
    }

    void ProjectSession::onDestroy() {
        {
            std::lock_guard lock(m->mu);
            m->hist.clear();
            m->pos = 0;
            m->histBytes = 0;
            m->busy = false;
        }
        m_doc.reset();
        ISubSysContainer::destroy();
        m_sim.reset();
        m_scenes.reset();
    }

    ProjectTx ProjectSession::tx(std::string name, TxOpts opts) {
        std::lock_guard lock(m->mu);
        return ProjectTx(
            *this, name.empty() ? "Edit" : std::move(name), opts, m->state);
    }

    TxResult ProjectSession::commit(ProjectTx &edit) {
        if (!edit.m || edit.m->session != this || edit.m->done) {
            return {.status = Status::fail(
                        Err::invalid,
                        "transaction is invalid or already finished")};
        }

        const auto finish = [&] {
            edit.m->done = true;
            edit.m->session = nullptr;
        };

        StateId before = 0;
        {
            std::lock_guard lock(m->mu);
            before = m->state;
            if (m->faulted) {
                finish();
                return {.status = Status::fail(Err::faulted,
                                               "project session is faulted"),
                        .before = before,
                        .after = before};
            }
            if (m->busy) {
                finish();
                return {.status = Status::fail(
                            Err::busy, "another project edit is in progress"),
                        .before = before,
                        .after = before};
            }
            if (!edit.m->opts.rebase && edit.m->base != m->state) {
                finish();
                return {.status = Status::fail(
                            Err::conflict,
                            "transaction was based on an old project state"),
                        .before = before,
                        .after = before};
            }
            if (!edit.m->err) {
                const auto status = edit.m->err;
                finish();
                return {.status = status, .before = before, .after = before};
            }
            if (edit.m->ops.empty()) {
                finish();
                return edit.m->opts.empty
                           ? TxResult{.status = Status::ok(),
                                      .before = before,
                                      .after = before}
                           : TxResult{
                                 .status = Status::fail(
                                     Err::empty, "transaction has no edits"),
                                 .before = before,
                                 .after = before};
            }
            m->busy = true;
        }

        std::size_t done = 0;
        Status status = Status::ok();
        for (; done < edit.m->ops.size(); ++done) {
            status = callStep(*edit.m->ops[done], *this, Call::apply);
            if (!status) {
                break;
            }
        }

        if (!status) {
            Status roll = Status::ok();
            for (std::size_t i = done; i > 0; --i) {
                const auto one =
                    callStep(*edit.m->ops[i - 1], *this, Call::undo);
                if (!one && roll) {
                    roll = one;
                }
            }

            std::lock_guard lock(m->mu);
            m->busy = false;
            if (!roll || status.err() == Err::rollback) {
                m->faulted = true;
                if (!roll) {
                    status = Status::fail(
                        Err::rollback,
                        "edit failed and rollback also failed: " + roll.msg());
                }
            }
            finish();
            return {.status = std::move(status),
                    .before = before,
                    .after = before,
                    .op = done};
        }

        StateId after = 0;
        {
            std::lock_guard lock(m->mu);
            if (m->pos < m->hist.size()) {
                for (std::size_t i = m->pos; i < m->hist.size(); ++i) {
                    m->histBytes -= std::min(m->histBytes, m->hist[i].bytes);
                }
                m->hist.erase(m->hist.begin() +
                                  static_cast<std::ptrdiff_t>(m->pos),
                              m->hist.end());
            }

            after = ++m->next;
            m->state = after;

            if (edit.m->opts.hist && m->opts.maxHist > 0) {
                bool merged = false;
                const bool atSave = m->saved && *m->saved == before;
                if (!atSave && m->pos > 0 && edit.m->ops.size() == 1) {
                    auto &prev = m->hist[m->pos - 1];
                    if (prev.ops.size() == 1 &&
                        prev.ops.front()->merge(*edit.m->ops.front())) {
                        m->histBytes -= std::min(m->histBytes, prev.bytes);
                        prev.after = after;
                        prev.bytes = entryBytes(prev);
                        m->histBytes += prev.bytes;
                        merged = true;
                    }
                }

                if (!merged) {
                    HistEntry entry;
                    entry.name = std::move(edit.m->name);
                    entry.ops = std::move(edit.m->ops);
                    entry.before = before;
                    entry.after = after;
                    entry.bytes = entryBytes(entry);
                    m->histBytes += entry.bytes;
                    m->hist.push_back(std::move(entry));
                    m->pos = m->hist.size();
                }
                m->prune();
            } else {
                m->hist.clear();
                m->pos = 0;
                m->histBytes = 0;
            }

            m->busy = false;
        }
        finish();
        return {.status = Status::ok(), .before = before, .after = after};
    }

    TxResult ProjectSession::undo() {
        HistEntry *entry = nullptr;
        StateId before = 0;
        {
            std::lock_guard lock(m->mu);
            before = m->state;
            if (m->faulted) {
                return {.status = Status::fail(Err::faulted,
                                               "project session is faulted"),
                        .before = before,
                        .after = before};
            }
            if (m->busy) {
                return {.status = Status::fail(
                            Err::busy, "another project edit is in progress"),
                        .before = before,
                        .after = before};
            }
            if (m->pos == 0) {
                return {.status = Status::fail(Err::empty,
                                               "there is nothing to undo"),
                        .before = before,
                        .after = before};
            }
            m->busy = true;
            entry = &m->hist[m->pos - 1];
        }

        std::size_t idx = entry->ops.size();
        Status status = Status::ok();
        while (idx > 0) {
            --idx;
            status = callStep(*entry->ops[idx], *this, Call::undo);
            if (!status) {
                break;
            }
        }

        if (!status) {
            Status roll = Status::ok();
            for (std::size_t i = idx + 1; i < entry->ops.size(); ++i) {
                const auto one = callStep(*entry->ops[i], *this, Call::redo);
                if (!one && roll) {
                    roll = one;
                }
            }
            std::lock_guard lock(m->mu);
            m->busy = false;
            if (!roll || status.err() == Err::rollback) {
                m->faulted = true;
                if (!roll) {
                    status = Status::fail(
                        Err::rollback,
                        "undo failed and rollback also failed: " + roll.msg());
                }
            }
            return {.status = std::move(status),
                    .before = before,
                    .after = before,
                    .op = idx};
        }

        std::lock_guard lock(m->mu);
        --m->pos;
        m->state = entry->before;
        m->busy = false;
        return {.status = Status::ok(), .before = before, .after = m->state};
    }

    TxResult ProjectSession::redo() {
        HistEntry *entry = nullptr;
        StateId before = 0;
        {
            std::lock_guard lock(m->mu);
            before = m->state;
            if (m->faulted) {
                return {.status = Status::fail(Err::faulted,
                                               "project session is faulted"),
                        .before = before,
                        .after = before};
            }
            if (m->busy) {
                return {.status = Status::fail(
                            Err::busy, "another project edit is in progress"),
                        .before = before,
                        .after = before};
            }
            if (m->pos >= m->hist.size()) {
                return {.status = Status::fail(Err::empty,
                                               "there is nothing to redo"),
                        .before = before,
                        .after = before};
            }
            m->busy = true;
            entry = &m->hist[m->pos];
        }

        std::size_t done = 0;
        Status status = Status::ok();
        for (; done < entry->ops.size(); ++done) {
            status = callStep(*entry->ops[done], *this, Call::redo);
            if (!status) {
                break;
            }
        }

        if (!status) {
            Status roll = Status::ok();
            for (std::size_t i = done; i > 0; --i) {
                const auto one =
                    callStep(*entry->ops[i - 1], *this, Call::undo);
                if (!one && roll) {
                    roll = one;
                }
            }
            std::lock_guard lock(m->mu);
            m->busy = false;
            if (!roll || status.err() == Err::rollback) {
                m->faulted = true;
                if (!roll) {
                    status = Status::fail(
                        Err::rollback,
                        "redo failed and rollback also failed: " + roll.msg());
                }
            }
            return {.status = std::move(status),
                    .before = before,
                    .after = before,
                    .op = done};
        }

        std::lock_guard lock(m->mu);
        ++m->pos;
        m->state = entry->after;
        m->busy = false;
        return {.status = Status::ok(), .before = before, .after = m->state};
    }

    TxResult ProjectSession::addComp(
        std::shared_ptr<Canvas::SceneComponent> comp,
        std::vector<std::shared_ptr<Canvas::SceneComponent>> kids,
        UUID scene) {
        auto edit = tx("Add component");
        const auto status =
            edit.addComp(std::move(comp), std::move(kids), scene);
        if (!status) {
            edit.cancel();
            return {.status = status};
        }
        return edit.commit();
    }

    TxResult ProjectSession::trackAdd(
        std::shared_ptr<Canvas::SceneComponent> comp,
        std::vector<std::shared_ptr<Canvas::SceneComponent>> kids,
        UUID scene) {
        auto edit = tx("Add component");
        const auto status =
            edit.trackAdd(std::move(comp), std::move(kids), scene);
        if (!status) {
            edit.cancel();
            return {.status = status};
        }
        return edit.commit();
    }

    ValResult<UUID>
    ProjectSession::addComp(std::shared_ptr<SimEngine::Drivers::CompDef> def,
                            glm::vec2 pos,
                            UUID scene) {
        if (!m_doc || !m_scenes || !m_sim) {
            return {.status = Status::fail(
                        Err::invalid, "project session is not initialized")};
        }
        if (!def) {
            return {.status = Status::fail(Err::badArg,
                                           "component definition is null")};
        }
        auto comps = Canvas::SimulationSceneComponent::createNew(def);
        if (comps.empty()) {
            return {.status = Status::fail(
                        Err::apply, "component factory returned no data")};
        }

        const auto main =
            std::dynamic_pointer_cast<Canvas::SimulationSceneComponent>(
                comps.front());
        if (!main) {
            return {.status = Status::fail(
                        Err::apply, "component factory returned wrong type")};
        }
        comps.erase(comps.begin());
        main->setCompDef(def->clone());
        main->getTransform().position.x = pos.x;
        main->getTransform().position.y = pos.y;
        const auto id = main->getUuid();
        const auto result = addComp(main, std::move(comps), scene);
        return {.status = result.status, .val = result ? id : UUID::null};
    }

    TxResult ProjectSession::rmComp(std::vector<UUID> ids, UUID scene) {
        auto edit = tx("Remove component");
        const auto status = edit.rmComp(std::move(ids), scene);
        if (!status) {
            edit.cancel();
            return {.status = status};
        }
        return edit.commit();
    }

    TxResult ProjectSession::rmComp(UUID id, UUID scene) {
        return rmComp(std::vector<UUID>{id}, scene);
    }

    TxResult
    ProjectSession::addSlot(std::shared_ptr<Canvas::SlotSceneComponent> slot,
                            UUID parent,
                            UUID scene) {
        auto edit = tx("Add slot");
        const auto status = edit.addSlot(std::move(slot), parent, scene);
        if (!status) {
            edit.cancel();
            return {.status = status};
        }
        return edit.commit();
    }

    TxResult ProjectSession::addConn(
        std::shared_ptr<Canvas::ConnectionSceneComponent> conn, UUID scene) {
        auto edit = tx("Add connection");
        const auto status = edit.addConn(std::move(conn), scene);
        if (!status) {
            edit.cancel();
            return {.status = status};
        }
        return edit.commit();
    }

    TxResult ProjectSession::trackConn(
        std::shared_ptr<Canvas::ConnectionSceneComponent> conn, UUID scene) {
        auto edit = tx("Add connection");
        const auto status = edit.trackConn(std::move(conn), scene);
        if (!status) {
            edit.cancel();
            return {.status = status};
        }
        return edit.commit();
    }

    TxResult ProjectSession::moveComp(UUID id,
                                      glm::vec3 pos,
                                      UUID scene,
                                      bool schematic) {
        auto edit = tx("Move component");
        const auto status = edit.moveComp(id, pos, scene, schematic);
        if (!status) {
            edit.cancel();
            return {.status = status};
        }
        return edit.commit();
    }

    TxResult ProjectSession::trackMove(
        UUID id, glm::vec3 from, glm::vec3 to, UUID scene, bool schematic) {
        auto edit = tx("Move component");
        const auto status = edit.trackMove(id, from, to, scene, schematic);
        if (!status) {
            edit.cancel();
            return {.status = status};
        }
        return edit.commit();
    }

    TxResult ProjectSession::parentComp(UUID id, UUID parent, UUID scene) {
        auto edit = tx("Reparent component");
        const auto status = edit.parentComp(id, parent, scene);
        if (!status) {
            edit.cancel();
            return {.status = status};
        }
        return edit.commit();
    }

    TxResult
    ProjectSession::trackParent(UUID id, UUID from, UUID to, UUID scene) {
        auto edit = tx("Reparent component");
        const auto status = edit.trackParent(id, from, to, scene);
        if (!status) {
            edit.cancel();
            return {.status = status};
        }
        return edit.commit();
    }

    TxResult ProjectSession::nameComp(UUID id, std::string name, UUID scene) {
        auto edit = tx("Rename component", {.empty = true});
        const auto status = edit.nameComp(id, std::move(name), scene);
        if (!status) {
            edit.cancel();
            return {.status = status};
        }
        return edit.commit();
    }

    TxResult ProjectSession::setName(std::string name) {
        if (!m_doc) {
            return {.status = Status::fail(
                        Err::invalid, "project session is not initialized")};
        }
        if (name.empty()) {
            name = "Unnamed";
        }
        if (name == m_doc->name()) {
            std::lock_guard lock(m->mu);
            return {
                .status = Status::ok(), .before = m->state, .after = m->state};
        }

        auto edit = tx("Rename project");
        edit.m->ops.push_back(
            std::make_unique<NameStep>(m_doc->name(), std::move(name)));
        return edit.commit();
    }

    bool ProjectSession::canUndo() const {
        std::lock_guard lock(m->mu);
        return !m->busy && !m->faulted && m->pos > 0;
    }

    bool ProjectSession::canRedo() const {
        std::lock_guard lock(m->mu);
        return !m->busy && !m->faulted && m->pos < m->hist.size();
    }

    bool ProjectSession::dirty() const {
        std::lock_guard lock(m->mu);
        return !m->saved || *m->saved != m->state;
    }

    bool ProjectSession::busy() const {
        std::lock_guard lock(m->mu);
        return m->busy;
    }

    bool ProjectSession::faulted() const {
        std::lock_guard lock(m->mu);
        return m->faulted;
    }

    SessView ProjectSession::view() const {
        std::lock_guard lock(m->mu);
        SessView view;
        view.state = m->state;
        view.undo = !m->busy && !m->faulted && m->pos > 0;
        view.redo = !m->busy && !m->faulted && m->pos < m->hist.size();
        view.dirty = !m->saved || *m->saved != m->state;
        view.busy = m->busy;
        view.faulted = m->faulted;
        view.undoCount = m->pos;
        view.redoCount = m->hist.size() - m->pos;
        if (m->pos > 0) {
            view.undoName = m->hist[m->pos - 1].name;
        }
        if (m->pos < m->hist.size()) {
            view.redoName = m->hist[m->pos].name;
        }
        return view;
    }

    void ProjectSession::clearHist() {
        std::lock_guard lock(m->mu);
        if (m->busy) {
            return;
        }
        m->hist.clear();
        m->pos = 0;
        m->histBytes = 0;
    }

    Status ProjectSession::recover() {
        std::lock_guard lock(m->mu);
        if (m->busy) {
            return Status::fail(Err::busy,
                                "another project edit is in progress");
        }
        m->hist.clear();
        m->pos = 0;
        m->histBytes = 0;
        m->faulted = false;
        m->saved.reset();
        m->state = ++m->next;
        return Status::ok();
    }

    Status ProjectSession::newProj(std::string name) {
        if (!m_doc || !m_scenes || !m_sim) {
            return Status::fail(Err::invalid,
                                "project session is not initialized");
        }

        Json::Value old;
        DocInfo oldInfo;
        {
            std::lock_guard lock(m->mu);
            if (m->faulted) {
                return Status::fail(Err::faulted, "project session is faulted");
            }
            if (m->busy) {
                return Status::fail(Err::busy,
                                    "another project edit is in progress");
            }
            m->busy = true;
        }

        try {
            old = m_doc->json();
            oldInfo = m_doc->info();
        } catch (const std::exception &ex) {
            std::lock_guard lock(m->mu);
            m->busy = false;
            return Status::fail(
                Err::except,
                std::string("could not snapshot current project: ") +
                    ex.what());
        } catch (...) {
            std::lock_guard lock(m->mu);
            m->busy = false;
            return Status::fail(Err::except,
                                "could not snapshot current project");
        }

        Status status = Status::ok();
        try {
            m_sim->clear();
            m_scenes->reset();
            m_doc->setName(name.empty() ? "Unnamed" : std::move(name));
            m_doc->clearPath();
        } catch (const std::exception &ex) {
            status = Status::fail(Err::except,
                                  std::string("could not create project: ") +
                                      ex.what());
        } catch (...) {
            status = Status::fail(Err::except, "could not create project");
        }

        if (!status) {
            const auto roll = m_doc->apply(old);
            m_doc->m_info = oldInfo;
            if (!roll) {
                std::lock_guard lock(m->mu);
                m->faulted = true;
                m->busy = false;
                return Status::fail(
                    Err::rollback,
                    "new project failed and rollback also failed");
            }
        }

        std::lock_guard lock(m->mu);
        m->busy = false;
        if (!status) {
            return status;
        }
        m->hist.clear();
        m->pos = 0;
        m->histBytes = 0;
        m->state = ++m->next;
        m->saved.reset();
        return Status::ok();
    }

    Status ProjectSession::save() {
        if (!m_doc) {
            return Status::fail(Err::invalid,
                                "project session is not initialized");
        }
        if (!m_doc->hasPath()) {
            return Status::fail(Err::noPath, "project has no save path");
        }
        return saveAs(m_doc->path());
    }

    Status ProjectSession::saveAs(const std::filesystem::path &path) {
        if (!m_doc || !m_scenes || !m_sim) {
            return Status::fail(Err::invalid,
                                "project session is not initialized");
        }

        StateId state = 0;
        {
            std::lock_guard lock(m->mu);
            if (m->faulted) {
                return Status::fail(Err::faulted, "project session is faulted");
            }
            if (m->busy) {
                return Status::fail(Err::busy,
                                    "another project edit is in progress");
            }
            m->busy = true;
            state = m->state;
        }

        Status status = Status::ok();
        try {
            const auto data = m_doc->json();
            status = m_doc->write(path, data);
            if (status) {
                m_doc->setPath(path);
                std::error_code ec;
                m_doc->m_info.bytes =
                    std::filesystem::file_size(m_doc->path(), ec);
                if (ec) {
                    m_doc->m_info.bytes = 0;
                }
            }
        } catch (const std::exception &ex) {
            status = Status::fail(Err::except,
                                  std::string("could not save project: ") +
                                      ex.what());
        } catch (...) {
            status = Status::fail(Err::except, "could not save project");
        }

        std::lock_guard lock(m->mu);
        m->busy = false;
        if (!status) {
            return status;
        }
        m->saved = state;
        return Status::ok();
    }

    Status ProjectSession::load(const std::filesystem::path &path) {
        if (!m_doc || !m_scenes || !m_sim) {
            return Status::fail(Err::invalid,
                                "project session is not initialized");
        }

        {
            std::lock_guard lock(m->mu);
            if (m->faulted) {
                return Status::fail(Err::faulted, "project session is faulted");
            }
            if (m->busy) {
                return Status::fail(Err::busy,
                                    "another project edit is in progress");
            }
            m->busy = true;
        }

        Json::Value next;
        Status status = Status::ok();
        try {
            status = m_doc->read(path, next);
        } catch (const std::exception &ex) {
            status = Status::fail(Err::except,
                                  std::string("could not read project: ") +
                                      ex.what());
        } catch (...) {
            status = Status::fail(Err::except, "could not read project");
        }
        if (!status) {
            std::lock_guard lock(m->mu);
            m->busy = false;
            return status;
        }

        Json::Value old;
        DocInfo oldInfo;
        try {
            old = m_doc->json();
            oldInfo = m_doc->info();
        } catch (const std::exception &ex) {
            std::lock_guard lock(m->mu);
            m->busy = false;
            return Status::fail(
                Err::except,
                std::string("could not snapshot current project: ") +
                    ex.what());
        } catch (...) {
            std::lock_guard lock(m->mu);
            m->busy = false;
            return Status::fail(Err::except,
                                "could not snapshot current project");
        }

        status = m_doc->apply(next);
        if (!status) {
            const auto roll = m_doc->apply(old);
            m_doc->m_info = oldInfo;
            std::lock_guard lock(m->mu);
            m->busy = false;
            if (!roll) {
                m->faulted = true;
                return Status::fail(
                    Err::rollback,
                    "load failed and the old project could not be restored");
            }
            return status;
        }

        try {
            m_doc->setPath(path);
            std::error_code ec;
            m_doc->m_info.bytes = std::filesystem::file_size(m_doc->path(), ec);
            if (ec) {
                m_doc->m_info.bytes = 0;
            }
        } catch (const std::exception &ex) {
            status = Status::fail(
                Err::except,
                std::string("could not finish loading project: ") + ex.what());
        } catch (...) {
            status =
                Status::fail(Err::except, "could not finish loading project");
        }

        if (!status) {
            const auto roll = m_doc->apply(old);
            m_doc->m_info = oldInfo;
            std::lock_guard lock(m->mu);
            m->busy = false;
            if (!roll) {
                m->faulted = true;
                return Status::fail(
                    Err::rollback,
                    "load failed and the old project could not be restored");
            }
            return status;
        }

        std::lock_guard lock(m->mu);
        m->hist.clear();
        m->pos = 0;
        m->histBytes = 0;
        m->state = ++m->next;
        m->saved = m->state;
        m->busy = false;
        return Status::ok();
    }

    ProjectDoc &ProjectSession::doc() {
        if (!m_doc) {
            throw std::logic_error("ProjectSession is not initialized");
        }
        return *m_doc;
    }

    const ProjectDoc &ProjectSession::doc() const {
        if (!m_doc) {
            throw std::logic_error("ProjectSession is not initialized");
        }
        return *m_doc;
    }

    SceneDriver &ProjectSession::scenes() {
        if (!m_scenes) {
            throw std::logic_error("ProjectSession is not initialized");
        }
        return *m_scenes;
    }

    const SceneDriver &ProjectSession::scenes() const {
        if (!m_scenes) {
            throw std::logic_error("ProjectSession is not initialized");
        }
        return *m_scenes;
    }

    SimEngine::SimulationEngine &ProjectSession::sim() {
        if (!m_sim) {
            throw std::logic_error("ProjectSession is not initialized");
        }
        return *m_sim;
    }

    const SimEngine::SimulationEngine &ProjectSession::sim() const {
        if (!m_sim) {
            throw std::logic_error("ProjectSession is not initialized");
        }
        return *m_sim;
    }

    void ProjectSession::setHooks(std::shared_ptr<const Edit::Hooks> hooks) {
        std::lock_guard lock(m->mu);
        if (!m->busy) {
            m->hooks = hooks ? std::move(hooks) : Edit::baseHooks();
        }
    }

    std::shared_ptr<const Edit::Hooks> ProjectSession::hooks() const {
        std::lock_guard lock(m->mu);
        return m->hooks;
    }

    UUID ProjectSession::sceneId(UUID id) const {
        if (id != UUID::null) {
            return id;
        }
        if (!m_scenes) {
            return UUID::null;
        }
        const auto scene = m_scenes->getActiveScene();
        return scene ? scene->getSceneId() : UUID::null;
    }
} // namespace Bess
