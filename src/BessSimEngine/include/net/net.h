#pragma once

#include "bess_api.h"
#include "bess_uuid.h"
#include "types.h"

namespace Bess::SimEngine {
    class BESS_API Net {
      public:
        Net() = default;
        ~Net() = default;
        Net(const Net &other) = default;

        const UUID &getUUID() const;

        void setUUID(const UUID &uuid);

        void addComponent(const UUID &component_uuid);

        const std::vector<UUID> &getComponents() const;

        void join(Net &other);

        void removeComponent(const UUID &componentUuid);

        void removeComponents(const std::vector<UUID> &componentUuids);

        void addComponents(const std::vector<UUID> &componentUuids);

        void setComponents(const std::vector<UUID> &componentUuids);

        void clear();

        size_t size() const;

      private:
        UUID m_uuid;
        std::vector<UUID> m_components;
    };
} // namespace Bess::SimEngine
