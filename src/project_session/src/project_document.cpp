#include "project_session/project_document.h"

#include "bess_core/scene/scene.h"
#include "bess_core/scene/scene_serializer.h"
#include "bess_core/scene_driver.h"
#include "common/bess_uuid.h"
#include "simulation_engine.h"

#include "json/reader.h"
#include "json/writer.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <system_error>

namespace Bess {
    namespace {
        constexpr std::uint32_t Schema = 1;

        std::filesystem::path normPath(const std::filesystem::path &path) {
            std::error_code ec;
            auto abs = std::filesystem::absolute(path, ec);
            return (ec ? path : abs).lexically_normal();
        }

        std::filesystem::path tmpPath(const std::filesystem::path &path) {
            static std::atomic<std::uint64_t> seq{0};
            const auto tick =
                std::chrono::steady_clock::now().time_since_epoch().count();
            return path.parent_path() /
                   (path.filename().string() + ".tmp." + std::to_string(tick) +
                    "." + std::to_string(seq.fetch_add(1)));
        }

        Status fsFail(std::string action,
                      const std::filesystem::path &path,
                      const std::error_code &ec) {
            return Status::fail(Err::io,
                                std::move(action) + " '" + path.string() +
                                    "': " + ec.message());
        }
    } // namespace

    ProjectDoc::ProjectDoc(std::shared_ptr<SceneDriver> scenes,
                           std::shared_ptr<SimEngine::SimulationEngine> sim,
                           DocOpts opts)
        : m_scenes(std::move(scenes)),
          m_sim(std::move(sim)),
          m_opts(opts) {
    }

    ProjectDoc::~ProjectDoc() = default;

    const std::string &ProjectDoc::name() const noexcept {
        return m_info.name;
    }

    const std::filesystem::path &ProjectDoc::path() const noexcept {
        return m_info.path;
    }

    bool ProjectDoc::hasPath() const noexcept {
        return m_info.hasPath();
    }

    DocInfo ProjectDoc::info() const {
        return m_info;
    }

    void ProjectDoc::setName(std::string name) {
        m_info.name = name.empty() ? "Unnamed" : std::move(name);
    }

    void ProjectDoc::setPath(std::filesystem::path path) {
        m_info.path = normPath(path);
    }

    void ProjectDoc::clearPath() {
        m_info.path.clear();
        m_info.bytes = 0;
    }

    Json::Value ProjectDoc::json() const {
        Json::Value root{Json::objectValue};
        root["name"] = m_info.name;
        root["version"] = "<dev>";
        root["schema"] = Schema;

        auto &sceneData = root["scene_data"];
        sceneData = Json::objectValue;
        JsonConvert::toJsonValue(m_scenes->getRootSceneId(),
                                 sceneData["root_scene_id"]);
        sceneData["scenes"] = Json::arrayValue;

        SceneSerializer ser;
        for (const auto &scene : m_scenes->getScenes()) {
            if (!scene) {
                continue;
            }
            Json::Value value{Json::objectValue};
            ser.serialize(value, scene);
            sceneData["scenes"].append(std::move(value));
        }

        root["sim_engine_data"] = m_sim->toJson();
        return root;
    }

    Status ProjectDoc::read(const std::filesystem::path &raw,
                            Json::Value &json) const {
        if (raw.empty()) {
            return Status::fail(Err::noPath, "project path is empty");
        }

        const auto path = normPath(raw);
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            return ec ? fsFail("could not inspect", path, ec)
                      : Status::fail(Err::notFound,
                                     "project file does not exist: " +
                                         path.string());
        }
        if (!std::filesystem::is_regular_file(path, ec)) {
            return ec ? fsFail("could not inspect", path, ec)
                      : Status::fail(Err::notFile,
                                     "project path is not a file: " +
                                         path.string());
        }

        const auto bytes = std::filesystem::file_size(path, ec);
        if (ec) {
            return fsFail("could not get file size for", path, ec);
        }
        if (bytes > m_opts.maxBytes) {
            return Status::fail(
                Err::tooLarge,
                "project file exceeds the configured size limit");
        }

        std::ifstream in(path, std::ios::binary);
        if (!in) {
            return Status::fail(
                Err::io, "could not open project file: " + path.string());
        }

        Json::CharReaderBuilder builder;
        builder["collectComments"] = false;
        std::string errs;
        if (!Json::parseFromStream(builder, in, &json, &errs)) {
            return Status::fail(Err::parse,
                                "could not parse project file '" +
                                    path.string() + "': " + errs);
        }
        if (in.bad()) {
            return Status::fail(
                Err::io, "could not read project file: " + path.string());
        }
        return check(json);
    }

    Status ProjectDoc::write(const std::filesystem::path &raw,
                             const Json::Value &json) const {
        if (raw.empty()) {
            return Status::fail(Err::noPath, "project path is empty");
        }

        const auto path = normPath(raw);
        const auto parent = path.parent_path();
        std::error_code ec;
        if (!parent.empty()) {
            const auto exists = std::filesystem::exists(parent, ec);
            if (ec) {
                return fsFail("could not inspect directory", parent, ec);
            }
            if (!exists) {
                if (!m_opts.mkDirs) {
                    return Status::fail(Err::notFound,
                                        "project directory does not exist: " +
                                            parent.string());
                }
                std::filesystem::create_directories(parent, ec);
                if (ec) {
                    return fsFail("could not create directory", parent, ec);
                }
            } else if (!std::filesystem::is_directory(parent, ec)) {
                return ec ? fsFail("could not inspect directory", parent, ec)
                          : Status::fail(
                                Err::notFile,
                                "project directory path is not a directory: " +
                                    parent.string());
            }
        }

        const auto targetExists = std::filesystem::exists(path, ec);
        if (ec) {
            return fsFail("could not inspect", path, ec);
        }
        if (targetExists && !std::filesystem::is_regular_file(path, ec)) {
            return ec ? fsFail("could not inspect", path, ec)
                      : Status::fail(Err::notFile,
                                     "project path is not a regular file: " +
                                         path.string());
        }

        const auto tmp = tmpPath(path);
        {
            std::ofstream out(
                tmp, std::ios::binary | std::ios::out | std::ios::trunc);
            if (!out) {
                return Status::fail(
                    Err::io,
                    "could not create temporary project file: " + tmp.string());
            }

            Json::StreamWriterBuilder builder;
            builder["commentStyle"] = "None";
            builder["indentation"] = "    ";
            auto writer =
                std::unique_ptr<Json::StreamWriter>(builder.newStreamWriter());
            if (writer->write(json, &out) != 0) {
                out.close();
                std::filesystem::remove(tmp, ec);
                return Status::fail(Err::io, "could not encode project file");
            }
            out.flush();
            if (!out) {
                out.close();
                std::filesystem::remove(tmp, ec);
                return Status::fail(Err::io,
                                    "could not flush temporary project file: " +
                                        tmp.string());
            }
        }

        const auto bytes = std::filesystem::file_size(tmp, ec);
        if (ec) {
            const auto err =
                fsFail("could not get temporary file size for", tmp, ec);
            std::error_code cleanEc;
            std::filesystem::remove(tmp, cleanEc);
            return err;
        }
        if (bytes > m_opts.maxBytes) {
            std::filesystem::remove(tmp, ec);
            return Status::fail(
                Err::tooLarge,
                "encoded project exceeds the configured size limit");
        }

        std::filesystem::rename(tmp, path, ec);
        if (!ec) {
            return Status::ok();
        }

        // std::filesystem::rename does not replace files on every platform.
        // Keep the old file recoverable while doing a standards-only swap.
        ec.clear();
        if (!std::filesystem::exists(path, ec)) {
            const auto err =
                ec ? fsFail("could not inspect", path, ec)
                   : Status::fail(Err::io,
                                  "could not install project file: " +
                                      path.string());
            std::error_code cleanEc;
            std::filesystem::remove(tmp, cleanEc);
            return err;
        }

        const auto bak = tmpPath(path).concat(".bak");
        ec.clear();
        std::filesystem::rename(path, bak, ec);
        if (ec) {
            const auto err =
                fsFail("could not stage old project file", path, ec);
            std::filesystem::remove(tmp, ec);
            return err;
        }

        std::filesystem::rename(tmp, path, ec);
        if (ec) {
            const auto installErr = ec;
            std::error_code restoreErr;
            std::filesystem::rename(bak, path, restoreErr);
            std::error_code cleanErr;
            std::filesystem::remove(tmp, cleanErr);
            if (restoreErr) {
                return Status::fail(
                    Err::rollback,
                    "could not install the new project file or restore the "
                    "old file; recovery copy is at '" +
                        bak.string() + "'");
            }
            return fsFail("could not install project file", path, installErr);
        }

        std::filesystem::remove(bak, ec);
        return Status::ok();
    }

    Status ProjectDoc::check(const Json::Value &json) const {
        if (!json.isObject()) {
            return Status::fail(Err::schema,
                                "project root must be a JSON object");
        }
        if (json.isMember("schema") &&
            (!json["schema"].isUInt() || json["schema"].asUInt() > Schema)) {
            return Status::fail(Err::schema, "project schema is not supported");
        }
        if (json.isMember("name") && !json["name"].isString()) {
            return Status::fail(Err::schema, "project name must be a string");
        }
        if (!json.isMember("scene_data") || !json["scene_data"].isObject()) {
            return Status::fail(Err::schema, "project is missing scene_data");
        }

        const auto &sceneData = json["scene_data"];
        if (!sceneData.isMember("scenes") || !sceneData["scenes"].isArray()) {
            return Status::fail(Err::schema,
                                "scene_data.scenes must be an array");
        }
        for (const auto &scene : sceneData["scenes"]) {
            if (!scene.isObject() || !scene.isMember("scene_state") ||
                !scene["scene_state"].isObject()) {
                return Status::fail(Err::schema,
                                    "project contains an invalid scene");
            }
        }
        if (json.isMember("sim_engine_data") &&
            !json["sim_engine_data"].isObject()) {
            return Status::fail(Err::schema,
                                "sim_engine_data must be an object");
        }
        return Status::ok();
    }

    Status ProjectDoc::apply(const Json::Value &json) {
        const auto valid = check(json);
        if (!valid) {
            return valid;
        }

        try {
            m_sim->setSimulationState(SimEngine::SimulationState::paused);
            m_sim->loadJson(
                json.get("sim_engine_data", Json::Value{Json::objectValue}));

            m_scenes->removeScenes();
            SceneSerializer ser;
            const auto &sceneData = json["scene_data"];
            for (const auto &src : sceneData["scenes"]) {
                auto data = src;
                auto scene = std::make_shared<Canvas::Scene>();
                ser.deserialize(
                    data, scene, Canvas::SceneLoadCtx{.sim = m_sim.get()});

                float maxZ = 0.f;
                for (const auto &[id, comp] :
                     scene->getState().getAllComponents()) {
                    (void)id;
                    if (comp) {
                        maxZ = std::max(maxZ, comp->getTransform().position.z);
                    }
                }
                scene->setZCoord(maxZ);
                m_scenes->addScene(scene);
            }

            if (m_scenes->getSceneCount() == 0) {
                m_scenes->reset();
            } else {
                UUID root = UUID::null;
                if (sceneData.isMember("root_scene_id")) {
                    JsonConvert::fromJsonValue(sceneData["root_scene_id"],
                                               root);
                }
                if (root == UUID::null || !m_scenes->getSceneWithId(root)) {
                    root = m_scenes->getSceneAtIdx(0)->getSceneId();
                }
                m_scenes->setRootSceneId(root);
                m_scenes->makeRootSceneActive();
            }

            m_info.name = json.get("name", "Unnamed").asString();
            m_info.schema = json.get("schema", Schema).asUInt();
            m_sim->setSimulationState(SimEngine::SimulationState::running);
            return Status::ok();
        } catch (const std::exception &ex) {
            return Status::fail(Err::apply,
                                std::string("could not apply project data: ") +
                                    ex.what());
        } catch (...) {
            return Status::fail(Err::apply, "could not apply project data");
        }
    }
} // namespace Bess
