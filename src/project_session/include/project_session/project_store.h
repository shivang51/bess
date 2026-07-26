#pragma once

#include "common/bess_api.h"
#include "project_session/project_document.h"
#include "project_session/result.h"

#include <filesystem>
#include <memory>

namespace Bess::Session {
    class BESS_API IProjectStore {
      public:
        virtual ~IProjectStore() = default;

        virtual Result<std::unique_ptr<ProjectDocument>>
        load(const std::filesystem::path &path) = 0;
        virtual Status save(const std::filesystem::path &path,
                            const ProjectDocument &document) = 0;
    };
} // namespace Bess::Session
