#include "component_definition.h"
#include "component_catalog.h"
#include "expression_evalutator/expr_evaluator.h"
#include "types.h"
#include <logger.h>
#include <memory>
#include <type_traits>

template <>
std::atomic_int Bess::TypeMap<std::shared_ptr<Bess::SimEngine::Trait>>::LastTypeId(0);

namespace Bess::SimEngine {
    // --- hashing helpers (FNV-1a 64-bit) ---
    namespace {
        constexpr uint64_t FNV_OFFSET_BASIS_64 = 1469598103934665603ull;
        constexpr uint64_t FNV_PRIME_64 = 1099511628211ull;

        inline uint64_t fnv1aBytes(uint64_t hash, const uint8_t *data, size_t len) noexcept {
            for (size_t i = 0; i < len; ++i) {
                hash ^= static_cast<uint64_t>(data[i]);
                hash *= FNV_PRIME_64;
            }
            return hash;
        }

        template <typename T>
        inline uint64_t fnv1aPod(uint64_t hash, const T &value) noexcept {
            static_assert(std::is_trivially_copyable_v<T>, "fnv1aPod requires trivially copyable type");
            return fnv1aBytes(hash, reinterpret_cast<const uint8_t *>(&value), sizeof(T));
        }

        inline uint64_t fnv1aString(uint64_t hash, const std::string &s) noexcept {
            return fnv1aBytes(hash, reinterpret_cast<const uint8_t *>(s.data()), s.size());
        }

        template <typename E>
        inline uint64_t fnv1aEnum(uint64_t hash, E e) noexcept {
            using U = std::underlying_type_t<E>;
            U v = static_cast<U>(e);
            return fnv1aPod(hash, v);
        }

        template <typename T>
        inline uint64_t hashVector(uint64_t hash, const std::vector<T> &vec, auto elemHasher) noexcept {
            hash = fnv1aPod(hash, vec.size());
            for (const auto &elem : vec) {
                hash = elemHasher(hash, elem);
                // separator byte to reduce accidental concatenation collisions
                const uint8_t sep = 0xff;
                hash = fnv1aBytes(hash, &sep, 1);
            }
            return hash;
        }
    } // namespace

    bool ComponentDefinition::onSlotsResizeReq(SlotsGroupType groupType, size_t newSize) {
        if (groupType == SlotsGroupType::input)
            return m_inputSlotsInfo.isResizeable;
        else
            return m_outputSlotsInfo.isResizeable;
    }

    uint64_t ComponentDefinition::getHash() const {
        return m_hash;
    }

    void ComponentDefinition::computeHash() {
        uint64_t hash = FNV_OFFSET_BASIS_64;

        hash = fnv1aString(hash, m_name);
        hash = fnv1aString(hash, m_groupName);
        if (!m_inputSlotsInfo.isResizeable) {
            hash = fnv1aPod(hash, m_inputSlotsInfo.count);
        }

        if (!m_outputSlotsInfo.isResizeable) {
            hash = fnv1aPod(hash, m_outputSlotsInfo.count);
        }

        if (m_opInfo.op != '0') {
            hash = fnv1aPod(hash, m_opInfo.op);
            hash = fnv1aPod(hash, m_opInfo.shouldNegateOutput);
        }

        hash = fnv1aPod(hash, m_simDelay.count());

        m_hash = hash;
        if (m_baseHash == 0) {
            m_baseHash = m_hash;
        }
    }

    SimDelayNanoSeconds ComponentDefinition::getSimDelay() const {
        return m_simDelay;
    }

    SimTime ComponentDefinition::getRescheduleDelay() {
        if (!m_shouldAutoReschedule) {
            BESS_SE_ERROR("[ComponentDefinition] For comp {}, getRescheduleDelay called on component that should not auto-reschedule", m_name);
        }
        return getSimDelay();
    }

    bool ComponentDefinition::computeExpressionsIfNeeded() {
        // operator '0' means no operation
        // if no operation is defined, no expressions to compute
        if (m_opInfo.op == '0') {
            return false;
        }

        if (m_inputSlotsInfo.count <= 0) {
            BESS_SE_WARN("[SimulationEngine][ComponentDefinition] Input count not provided for expression(s) generation");
            return false;
        }

        m_outputExpressions.clear();

        // handeling unary and binary operators
        // For binary operators, only single output is supported
        // and for uniary operator, each input generates one output
        if (!ExprEval::isUninaryOperator(m_opInfo.op) &&
            m_outputSlotsInfo.count == 1) {
            std::string expr = m_opInfo.shouldNegateOutput ? "!(0" : "0";
            for (size_t i = 1; i < m_inputSlotsInfo.count; i++) {
                expr += m_opInfo.op + std::to_string(i);
            }
            if (m_opInfo.shouldNegateOutput)
                expr += ")";
            m_outputExpressions = {expr};
        } else if (ExprEval::isUninaryOperator(m_opInfo.op)) {
            m_outputExpressions.reserve(m_inputSlotsInfo.count);
            for (size_t i = 0; i < m_inputSlotsInfo.count; i++) {
                m_outputExpressions.emplace_back(std::format("{}{}", m_opInfo.op, i));
            }
        } else {
            BESS_SE_ERROR("Invalid IO config for expression generation");
            assert(false);
        }

        onExpressionsChange();

        return true;
    }

    void ComponentDefinition::setAuxData(const std::any &data) {
        m_auxData = data;
    }

    std::shared_ptr<ComponentDefinition> ComponentDefinition::clone() const {
        assert(m_ownership == CompDefinitionOwnership::NativeCpp &&
               "Cloning of Python-owned ComponentDefinitions is not supported.");
        return std::make_shared<ComponentDefinition>(*this);
    }

    void ComponentDefinition::onExpressionsChange() {
        this->setAuxData(m_outputExpressions);
    }
} // namespace Bess::SimEngine

namespace Bess::JsonConvert {

    void toJsonValue(Json::Value &j, const Bess::SimEngine::SlotsGroupInfo &info) {
        j = Json::Value(Json::objectValue);
        j["type"] = static_cast<Json::UInt64>(static_cast<uint8_t>(info.type));
        j["is_resizeable"] = info.isResizeable;
        j["count"] = static_cast<Json::UInt64>(info.count);
        j["names"] = Json::Value(Json::arrayValue);
        for (const auto &name : info.names) {
            j["names"].append(name);
        }
        j["categories"] = Json::Value(Json::arrayValue);
        for (const auto &[index, category] : info.categories) {
            Json::Value catJ = Json::Value(Json::objectValue);
            catJ["index"] = static_cast<Json::UInt64>(index);
            catJ["category"] = static_cast<Json::UInt64>(static_cast<uint8_t>(category));
            j["categories"].append(catJ);
        }
    }

    void fromJsonValue(const Json::Value &j, Bess::SimEngine::SlotsGroupInfo &info) {
        if (!j.isObject()) {
            return;
        }
        info.type = static_cast<Bess::SimEngine::SlotsGroupType>(
            j.get("type", 0).asUInt64());
        info.isResizeable = j.get("is_resizeable", false).asBool();
        info.count = j.get("count", 0).asUInt64();
        info.names.clear();
        if (j.isMember("names")) {
            for (const auto &nameJ : j["names"]) {
                info.names.push_back(nameJ.asString());
            }
        }
        info.categories.clear();
        if (j.isMember("categories")) {
            for (const auto &catJ : j["categories"]) {
                size_t index = catJ.get("index", 0).asUInt64();
                Bess::SimEngine::SlotCatergory category =
                    static_cast<Bess::SimEngine::SlotCatergory>(
                        catJ.get("category", 0).asUInt64());
                info.categories.emplace_back(index, category);
            }
        }
    }

    void toJsonValue(const Bess::SimEngine::ComponentDefinition &def, Json::Value &j) {
        j = Json::Value(Json::objectValue);
        j["name"] = def.getName();
        j["group_name"] = def.getGroupName();
        j["should_auto_reschedule"] = def.getShouldAutoReschedule();
        toJsonValue(j["input_slots_info"], def.getInputSlotsInfo());
        toJsonValue(j["output_slots_info"], def.getOutputSlotsInfo());
        j["op_info"] = Json::Value(Json::objectValue);
        j["op_info"]["op"] = std::string(1, def.getOpInfo().op);
        j["op_info"]["should_negate_output"] = def.getOpInfo().shouldNegateOutput;
        j["sim_delay_ns"] = static_cast<Json::UInt64>(def.getSimDelay().count());
        j["behavior_type"] = static_cast<Json::UInt64>(static_cast<uint8_t>(def.getBehaviorType()));
        j["output_expressions"] = Json::Value(Json::arrayValue);
        for (const auto &expr : def.getOutputExpressions()) {
            j["output_expressions"].append(expr);
        }
        // Will be used to get simulation function from the correct
        // comp definition from catalog
        j["base_hash"] = static_cast<Json::UInt64>(def.getBaseHash());
    }

    void fromJsonValue(const Json::Value &j, std::shared_ptr<Bess::SimEngine::ComponentDefinition> &def) {
        if (!j.isObject()) {
            return;
        }
        const auto baseDefHash = j.get("base_hash", 0).asUInt64();
        auto &compCatalog = SimEngine::ComponentCatalog::instance();
        if (!compCatalog.isRegistered(baseDefHash)) {
            BESS_SE_ERROR("ComponentDefinition deserialization: could not find base definition with hash {} in catalog for {}",
                          baseDefHash, def->getName());
            assert(false && "ComponentDefinition base definition not found in catalog");
        }

        def = compCatalog.getComponentDefinition(baseDefHash)->clone();

        def->setName(j.get("name", "Unnamed Component").asString());
        def->setGroupName(j.get("group_name", "").asString());
        def->setShouldAutoReschedule(j.get("should_auto_reschedule", false).asBool());
        fromJsonValue(j["input_slots_info"], def->getInputSlotsInfo());
        fromJsonValue(j["output_slots_info"], def->getOutputSlotsInfo());
        if (j.isMember("op_info")) {
            const auto &opJ = j["op_info"];
            std::string opStr = opJ.get("op", "0").asString();
            char opChar = (opStr.size() > 0) ? opStr[0] : '0';
            Bess::SimEngine::OperatorInfo opInfo;
            opInfo.op = opChar;
            opInfo.shouldNegateOutput = opJ.get("should_negate_output", false).asBool();
            def->setOpInfo(opInfo);
        }
        def->setSimDelay(SimEngine::SimDelayNanoSeconds(j.get("sim_delay_ns", 0).asUInt64()));
        def->setBehaviorType(static_cast<Bess::SimEngine::ComponentBehaviorType>(
            j.get("behavior_type", 0).asUInt64()));
        def->getOutputExpressions().clear();
        if (j.isMember("output_expressions")) {
            for (const auto &exprJ : j["output_expressions"]) {
                def->getOutputExpressions().push_back(exprJ.asString());
            }
        }
        def->setBaseHash(baseDefHash);
        def->computeHash();
    }
} // namespace Bess::JsonConvert
