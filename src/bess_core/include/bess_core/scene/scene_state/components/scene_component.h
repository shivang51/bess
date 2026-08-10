#pragma once

#include "common/bess_api.h"

#include "bess_core/scene/scene_draw_context.h"
#include "bess_core/scene/scene_event.h"
#include "bess_core/scene/scene_ser_reg.h"
#include "bess_core/scene/scene_state/components/behaviours/mouse_behaviour.h"
#include "bess_core/scene/scene_state/components/scene_component_types.h"
#include "bess_core/viewport.h"
#include "common/bess_assert.h"
#include "common/bess_uuid.h"
#include "common/class_helpers.h"
#include "common/types.h"
#include "json/value.h"

#include <cstddef>

namespace Bess::Canvas {
    struct SceneLoadCtx {
        SimEngine::SimulationEngine *sim = nullptr;
    };

#define REG_SCENE_COMP_TYPE(TypeName, type)                                    \
    SceneComponentType getType() const override {                              \
        return type;                                                           \
    }                                                                          \
    std::string getTypeName() const override {                                 \
        return TypeName;                                                       \
    }                                                                          \
    static std::string getStaticTypeName() {                                   \
        static std::string typeName = TypeName;                                \
        return typeName;                                                       \
    } // namespace Bess::Canvas

#define SCENE_COMP_SER(TClass, TBase, ...)                                     \
    Json::Value toJson() const override {                                      \
        onBeforeToJson();                                                      \
        auto json = TBase::toJson();                                           \
        const auto newJson = SERIALIZE_PROPS(__VA_ARGS__);                     \
        for (const auto &member : newJson.getMemberNames()) {                  \
            json[member] = newJson[member];                                    \
        }                                                                      \
        json["typeName"] = getTypeName();                                      \
        return json;                                                           \
    }                                                                          \
    static void fromJson(const Json::Value &j,                                 \
                         const std::shared_ptr<TClass> &ptr) {                 \
        auto castedComp = std::dynamic_pointer_cast<TBase>(ptr);               \
        TBase::fromJson(j, castedComp);                                        \
        DESERIALIZE_PROPS(ptr, __VA_ARGS__);                                   \
    }                                                                          \
    static std::shared_ptr<TClass> fromJson(const Json::Value &j) {            \
        const auto &typeName = j["typeName"].asString();                       \
        BESS_ASSERT(typeName == TClass::getStaticTypeName(),                   \
                    "[fromJson] Static type name mismatch");                   \
        auto comp = std::make_shared<TClass>();                                \
        TBase::fromJson(j, comp);                                              \
        TClass::fromJson(j, comp);                                             \
        return comp;                                                           \
    }                                                                          \
    void applyJson(const Json::Value &j) override {                            \
        auto self = std::dynamic_pointer_cast<TClass>(shared_from_this());     \
        TClass::fromJson(j, self);                                             \
        onJsonApplied();                                                       \
    }

#define SCENE_COMP_SER_NP(TClass, TBase)                                       \
    Json::Value toJson() const override {                                      \
        onBeforeToJson();                                                      \
        auto json = TBase::toJson();                                           \
        json["typeName"] = TClass::getStaticTypeName();                        \
        return json;                                                           \
    }                                                                          \
    static void fromJson(const Json::Value &j,                                 \
                         const std::shared_ptr<TClass> &ptr) {                 \
        auto castedComp = std::dynamic_pointer_cast<TBase>(ptr);               \
        TBase::fromJson(j, castedComp);                                        \
    }                                                                          \
    static std::shared_ptr<TClass> fromJson(const Json::Value &j) {            \
        auto comp = std::make_shared<TClass>();                                \
        auto castedComp = std::dynamic_pointer_cast<TBase>(comp);              \
        TBase::fromJson(j, castedComp);                                        \
        return comp;                                                           \
    }                                                                          \
    void applyJson(const Json::Value &j) override {                            \
        auto self = std::dynamic_pointer_cast<TClass>(shared_from_this());     \
        TClass::fromJson(j, self);                                             \
        onJsonApplied();                                                       \
    }

#define REG_TO_SER_REGISTRY(TClass)                                            \
    Bess::Canvas::SceneSerReg::registerComponent(                              \
        TClass::getStaticTypeName(),                                           \
        [&](const Json::Value &j)                                              \
            -> std::shared_ptr<Bess::Canvas::SceneComponent> {                 \
            return TClass::fromJson(j);                                        \
        });

#define REG_SCENE_COMP(TComp, TBase, ...)                                      \
    REFLECT_DERIVED_PROPS(TComp, TBase, __VA_ARGS__)

#define REG_SCENE_COMP_NP(TComp, TBase) REFLECT_DERIVED_EMPTY(TComp, TBase)

    class SceneState;

    class BESS_API SceneComponent
        : public std::enable_shared_from_this<SceneComponent>,
          public MouseBehaviour<SceneComponent> {
      public:
        SceneComponent();
        SceneComponent(const SceneComponent &other) = default;
        virtual ~SceneComponent() = default;

        static std::string getStaticTypeName() {
            return "SceneComponent";
        }

        virtual std::string getTypeName() const {
            return "SceneComponent";
        }

        virtual void prepareUI(SceneUIPrepareCtx &ctx);

        virtual void update(TimeMs /*frameTime*/, SceneState & /*state*/) {
        }

        virtual void draw(SceneDrawContext &);
        virtual void drawSchematic(SceneDrawContext &);

        virtual void drawPropertiesUI(SceneState &sceneState);

        virtual bool isFocusable() const {
            return false;
        }

        virtual bool wantsKeyboardInput() const {
            return false;
        }

        virtual Core::Viewport::SceneCursor getCursor() const {
            return Core::Viewport::SceneCursor::normal;
        }

        virtual void onFocusGained(const Events::FocusEvent &e) {
            (void)e;
        }

        virtual void onFocusLost(const Events::FocusEvent &e) {
            (void)e;
        }

        virtual bool onKeyEvent(const SceneEvent &evt) {
            (void)evt;
            return false;
        }

        virtual bool hasPointerCapture() const {
            return false;
        }

        virtual bool onPointerMove(const Events::MouseMoveEvent &e) {
            (void)e;
            return false;
        }

        virtual std::vector<std::shared_ptr<SceneComponent>>
        clone(const SceneState &sceneState) const;

        MAKE_GETTER_SETTER(UUID, Uuid, m_uuid)
        MAKE_GETTER_SETTER_WC(Transform,
                              Transform,
                              m_transform,
                              onTransformChanged)
        MAKE_GETTER_SETTER_WC(Style, Style, m_style, onStyleChanged)
        MAKE_GETTER_SETTER_WC(std::string, Name, m_name, onNameChanged)
        MAKE_GETTER_SETTER(UUID, ParentComponent, m_parentComponent)
        MAKE_GETTER_SETTER(OrderedSet<UUID>, ChildComponents, m_childComponents)
        MAKE_GETTER_SETTER_WC(uint32_t,
                              RuntimeId,
                              m_runtimeId,
                              onRuntimeIdChanged)
        MAKE_GETTER_SETTER_WC(bool, IsSelected, m_isSelected, onSelect)
        MAKE_GETTER_SETTER(std::string, Icon, m_icon);
        MAKE_GETTER_SETTER(bool, UIDirty, m_isUIDirty);

        virtual void removeChildComponent(const UUID &uuid);

        bool isDraggable() const;

        void setPosition(const glm::vec3 &pos);
        void setScale(const glm::vec2 &scale);

        virtual SceneComponentType getType() const {
            return (SceneComponentType)-1;
        }

        template <typename T> std::shared_ptr<T> cast() {
            return std::static_pointer_cast<T>(shared_from_this());
        }

        template <typename T> const T &cast() const {
            return static_cast<const T &>(*this);
        }

        void addChildComponent(const UUID &uuid);

        void setIsDraggable(bool draggable);

        virtual glm::vec3 getAbsolutePosition(const SceneState &state,
                                              bool isSchematicMode) const;
        virtual glm::vec3 getConnectionPos(const SceneState &state,
                                           bool isSchematicMode) const;

        // Cleanup function
        // Default implementation removes all child components recursively
        // This must be called in the overrides as well
        virtual std::vector<UUID> cleanup(SceneState &state,
                                          UUID caller = UUID::null);

        // Called when component is added/attached to the scene
        virtual void onAttach(SceneState &state);

        // Serialize the component to JSON for saving
        virtual Json::Value toJson() const;
        virtual void applyJson(const Json::Value &json);

        // Capture state used by interactive edit tracking. The default is the
        // complete serialized state. Components with large immutable payloads
        // may return a partial object as long as it contains identity and
        // hierarchy fields and can be passed to applyJson().
        [[nodiscard]] virtual Json::Value toEditJson() const;

        // Approximate the host memory retained by this component. This must
        // stay cheap: transaction history uses it while updating its budget.
        // Components that own sizeable dynamic buffers should override it.
        [[nodiscard]] virtual std::size_t
        estimatedMemoryUsage() const noexcept;

        virtual void beforeSerialize(const SceneState &state);
        virtual void onLoaded(const SceneLoadCtx &ctx);
        virtual void onRuntimeReady(SceneState &state);

        [[nodiscard]] virtual glm::vec3 editPos(bool schematic) const;
        virtual void setEditPos(const glm::vec3 &pos, bool schematic);
        [[nodiscard]] SceneState *sceneState() const noexcept;

        virtual std::vector<UUID> getDependants(const SceneState &state) const;

        virtual void onScaleChanged();
        virtual void onNameChanged() {
        }

      protected:
        // Deserialize the component from JSON
        static void fromJson(const Json::Value &j,
                             const std::shared_ptr<SceneComponent> &ptr);

        void prepareClone(SceneComponent &clonedComponent) const;
        virtual void resetCloneRuntimeState();
        virtual void onTransformChanged() {
        }
        virtual void onSelect() {
        }
        virtual void onStyleChanged() {
        }
        virtual void onRuntimeIdChanged() {
        }

        virtual void onBeforeToJson() const {
        }

        virtual void onJsonApplied();

        virtual glm::vec2 calculateScale(const SceneState &);

        virtual void onFirstDraw(SceneDrawContext &);

        virtual void onFirstSchematicDraw(SceneDrawContext &);

        // Called when children count changes (added / removed)
        virtual void onChildrenChanged();

      private:
        friend class SceneState;
        void bindState(SceneState *state) noexcept;

      protected:
        UUID m_uuid = UUID::null;
        uint32_t m_runtimeId =
            PickingId::invalidRuntimeId; // assigned during rendering for
                                         // picking
        Transform m_transform;
        Style m_style;
        std::string m_name;
        std::string m_icon;
        bool m_isUIDirty = true;

        bool m_isDraggable = false;
        bool m_isSelected = false;
        bool m_isFirstDraw = true;
        bool m_isFirstSchematicDraw = true;
        SceneState *m_sceneState = nullptr;

        UUID m_parentComponent = UUID::null;
        OrderedSet<UUID> m_childComponents;
    };
} // namespace Bess::Canvas

REFLECT_PROPS(Bess::Canvas::SceneComponent,
              ("uuid", getUuid, setUuid),
              ("transform", getTransform, setTransform),
              ("style", getStyle, setStyle),
              ("name", getName, setName),
              ("icon", getIcon, setIcon),
              ("parentComponent", getParentComponent, setParentComponent),
              ("childComponents", getChildComponents, setChildComponents))
