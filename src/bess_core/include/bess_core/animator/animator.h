#pragma once

#include "common/logger.h"
#include "common/sub_system.h"
#include "common/types.h"
#include <functional>

#include "common/bess_assert.h"
#include "common/class_helpers.h"

namespace Bess::Core {

    enum class AnimationState : uint8_t { stopped, playing, paused };

    template <typename T> struct AnimDesc {
        T *valPtr = nullptr;
        T start;
        T end;
        TimeMs duration;
        bool loop = false;
    };

    class AnimationBase {
      public:
        virtual ~AnimationBase() = default;
        virtual void update(TimeMs ts) = 0;
    };

    template <typename TVal> class Animation : public AnimationBase {
      public:
        DEFAULT_CONTRS(Animation);

        typedef std::function<TVal(float t, TVal curr, TVal end)> TMixerFn;

        MAKE_GETTER_SETTER_PTR(TVal, CurrValPtr, m_currVal);
        MAKE_GETTER_SETTER(TVal, StartVal, m_startVal);
        MAKE_GETTER_SETTER(TVal, EndVal, m_endVal);
        MAKE_GETTER_SETTER(TMixerFn, MixerFn, m_mixerFn);

        MAKE_GETTER_SETTER(TimeMs, Duration, m_duration);
        MAKE_GETTER_SETTER(TimeMs, CurrTime, m_currTime);

        MAKE_GETTER_SETTER(std::string, Label, m_label);

        static std::shared_ptr<Animation<TVal>>
        fromDesc(const AnimDesc<TVal> &desc) {
            BESS_ASSERT(desc.valPtr, "Needs pointer to the value");
            auto anim = std::make_shared<Animation<TVal>>();
            anim->m_currVal = desc.valPtr;
            anim->m_duration = desc.duration;
            anim->m_startVal = desc.start;
            anim->m_endVal = desc.end;
            anim->m_loop = desc.loop;
            return anim;
        }

        void update(TimeMs ts) override {
            if (!isRunning()) {
                return;
            }

            BESS_ASSERT(m_mixerFn,
                        "No mixer function for animation {} not found.",
                        m_label);

            m_currTime += ts;

            if (m_currTime >= m_duration) {
                setVal(m_endVal);

                if (!m_loop) {
                    stop();
                } else {
                    reset();
                }

                return;
            }

            const float t = m_currTime.count() / m_duration.count();

            const TVal nextVal = m_mixerFn(t, m_startVal, m_endVal);
            setVal(nextVal);
        }

        void play(bool restart = false) {
            m_state = AnimationState::playing;

            if (restart) {
                reset();
            }
        }

        void stop() {
            m_state = AnimationState::stopped;
            reset();
        }

        void pause() {
            m_state = AnimationState::paused;
        }

        bool isPaused() const {
            return AnimationState::paused == m_state;
        }

        bool isStopped() const {
            return AnimationState::stopped == m_state;
        }

        bool isRunning() const {
            return AnimationState::playing == m_state;
        }

      private:
        void reset() {
            m_currTime = TimeMs(0);
            *m_currVal = m_startVal;
        }

        void setVal(TVal val) {
            *m_currVal = val;
        }

      private:
        TVal *m_currVal = nullptr;
        TVal m_startVal{}, m_endVal; // start and end values of animation

        TimeMs m_currTime = TimeMs(0);
        TimeMs m_duration = TimeMs(0);
        AnimationState m_state = AnimationState::stopped;

        TMixerFn m_mixerFn = nullptr;
        std::string m_label = "Unlabeled";
        bool m_loop = false;
    };

    template <typename T>
    concept TScalar = requires {
        std::is_same_v<T, int> || std::is_same_v<T, float> ||
            std::is_same_v<T, double>;
    };

    template <typename T>
    concept TVec = requires {
        std::is_same_v<T, glm::vec2> || std::is_same_v<T, glm::vec3> ||
            std::is_same_v<T, glm::vec4>;
    };

    namespace Anim {

        template <TScalar T>
        std::shared_ptr<Animation<T>> createScalar(const AnimDesc<T> desc) {
            auto anim = Animation<T>::fromDesc(desc);

            const auto mixer = [](float t, T curr, T end) {
                return std::lerp(curr, end, t);
            };

            anim->setMixerFn(mixer);
            return anim;
        }

        template <TVec T>
        std::shared_ptr<Animation<T>> createVec(const AnimDesc<T> desc) {
            auto anim = Animation<T>::fromDesc(desc);

            const auto mixer = [](float t, T curr, T end) {
                return glm::mix(curr, end, t);
            };

            anim->setMixerFn(mixer);
            return anim;
        }
    } // namespace Anim

    class Animator : public ISubSystem {
      public:
        DEFAULT_CONTRS(Animator);

        void onInit() override {
        }

        void onDestroy() override {
            m_animations.clear();
        }

        void onUpdate(TimeMs dt) override {
            for (const auto &anim : m_animations) {
                anim->update(dt);
            }
        }

        void add(const std::shared_ptr<AnimationBase> &anim) {
            m_animations.push_back(anim);
        }

      private:
        std::vector<std::shared_ptr<AnimationBase>> m_animations;
    };
} // namespace Bess::Core
