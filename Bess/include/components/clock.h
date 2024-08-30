#include "component.h"
#include <uuid.h>
#include "json.hpp"


namespace Bess::Simulator::Components {
    class Clock : public Component {
      public:
        Clock() = default;
        Clock(const uuids::uuid &uid, int renderId, glm::vec3 position, const uuids::uuid &slotUid);
        void render() override;
        void generate(const glm::vec3 &pos) override;
        void deleteComponent() override;
        void update();

        void drawProperties() override;

        static void fromJson(const nlohmann::json &data);
        nlohmann::json toJson();

        void setFrequency(float frequency);
        float getFrequency();

      private:
        float m_frequency;
        uuids::uuid m_outputSlotId;

        float m_prevUpdateTime = 0.f;

        void onLeftClick(const glm::vec2& pos);
    };
} // namespace Bess::Simulator::Components