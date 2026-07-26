#pragma once

#include "common/bess_api.h"
#include "project_session/project_store.h"

#include "json/value.h"

namespace Bess::Session {
    class BESS_API JsonProjectStore final : public IProjectStore {
      public:
        Result<std::unique_ptr<ProjectDocument>>
        load(const std::filesystem::path &path) override;
        Status save(const std::filesystem::path &path,
                    const ProjectDocument &document) override;

        Result<Json::Value> encode(const ProjectDocument &document) const;
        Result<std::unique_ptr<ProjectDocument>>
        decode(const Json::Value &json) const;
    };
} // namespace Bess::Session
