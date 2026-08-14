#pragma once

#include "project_session/status.h"

#include "json/value.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>

namespace Bess {
    class ProjectSession;
    class ProjectSessionStep;
    class SceneDriver;

    namespace SimEngine {
        class SimulationEngine;
    }

    struct DocOpts {
        std::size_t maxBytes = std::size_t{256} * 1024U * 1024U;
        bool mkDirs = false;
    };

    struct DocInfo {
        std::filesystem::path path;
        std::string name = "Unnamed";
        std::uint32_t schema = 1;
        std::uintmax_t bytes = 0;

        [[nodiscard]] bool hasPath() const noexcept {
            return !path.empty();
        }
    };

    class ProjectDoc {
      public:
        ProjectDoc(std::shared_ptr<SceneDriver> scenes,
                   std::shared_ptr<SimEngine::SimulationEngine> sim,
                   DocOpts opts = {});
        ~ProjectDoc();

        ProjectDoc(const ProjectDoc &) = delete;
        ProjectDoc &operator=(const ProjectDoc &) = delete;
        ProjectDoc(ProjectDoc &&) = delete;
        ProjectDoc &operator=(ProjectDoc &&) = delete;

        [[nodiscard]] const std::string &name() const noexcept;
        [[nodiscard]] std::string &nameRef();
        [[nodiscard]] const std::filesystem::path &path() const noexcept;
        [[nodiscard]] bool hasPath() const noexcept;
        [[nodiscard]] DocInfo info() const;
        [[nodiscard]] Json::Value json() const;

      private:
        friend class ProjectSession;
        friend class ProjectSessionStep;

        void setName(std::string name);
        void setPath(std::filesystem::path path);
        void clearPath();

        [[nodiscard]] Status read(const std::filesystem::path &path,
                                  Json::Value &json) const;
        [[nodiscard]] Status write(const std::filesystem::path &path,
                                   const Json::Value &json) const;
        [[nodiscard]] Status check(const Json::Value &json) const;

        // Checks if all involved involved drivers are present
        [[nodiscard]] Status validateDrivers(const Json::Value &json) const;

        // Checks if all involved involved plugins are present
        [[nodiscard]] Status validatePlugins(const Json::Value &json) const;

        [[nodiscard]] Status apply(const Json::Value &json);

        std::shared_ptr<SceneDriver> m_scenes;
        std::shared_ptr<SimEngine::SimulationEngine> m_sim;
        DocOpts m_opts;
        DocInfo m_info;
    };
} // namespace Bess
