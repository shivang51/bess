#include "stamp_data_viewer.h"
#include "bess_core/scene/scene.h"
#include "bess_core/scene_driver.h"
#include "common/helpers.h"
#include "common/logger.h"
#include "imgui.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "simulation_engine.h"
#include "ui/icons/CodIcons_Remapped.h"
#include "ui/project_api.h"
#include "ui/ui_main/dialogs.h"
#include "ui/widgets/m_widgets.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <filesystem>
#include <format>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace Bess::UI {
    namespace {
        using ComponentStamp = SimEngine::Drivers::SimDriver::ComponentStamp;

        static constexpr auto windowName = Common::Helpers::concat(
            Icons::CodIcons::DATABASE, "  Stamped Data Viewer");

        static constexpr std::string_view timeUnitNames[] = {
            "ns", "us", "ms", "s"};

        struct ComponentEntry {
            UUID id = UUID::null;
            std::string name;
            std::string type;
            std::shared_ptr<SimEngine::Drivers::CompDef> definition;
        };

        struct PortColumn {
            std::string displayName;
            std::string csvName;
        };

        struct CsvExportData {
            std::string componentName;
            UUID componentId = UUID::null;
            std::vector<PortColumn> inputs;
            std::vector<PortColumn> outputs;
            std::vector<ComponentStamp> samples;
        };

        std::string lowerCase(std::string_view value) {
            std::string result(value);
            std::ranges::transform(result, result.begin(), [](char ch) {
                return static_cast<char>(
                    std::tolower(static_cast<unsigned char>(ch)));
            });
            return result;
        }

        std::string displayComponentName(
            const Canvas::SimulationSceneComponent &component) {
            if (!component.getName().empty()) {
                return component.getName();
            }
            if (const auto &definition = component.getCompDef();
                definition && !definition->getName().empty()) {
                return definition->getName();
            }
            return std::format(
                "Component {}",
                static_cast<uint64_t>(component.getSimEngineId()));
        }

        std::vector<ComponentEntry> collectComponents() {
            std::vector<ComponentEntry> result;
            const auto scene = Proj::scenes().getActiveScene();
            if (!scene) {
                return result;
            }

            for (const auto &[_, base] : scene->getState().getAllComponents()) {
                const auto component =
                    std::dynamic_pointer_cast<Canvas::SimulationSceneComponent>(
                        base);
                if (!component || component->getSimEngineId() == UUID::null) {
                    continue;
                }

                const auto &definition = component->getCompDef();
                result.push_back({.id = component->getSimEngineId(),
                                  .name = displayComponentName(*component),
                                  .type = definition
                                              ? definition->getTypeName()
                                              : std::string("Unknown type"),
                                  .definition = definition});
            }

            std::ranges::sort(result, [](const auto &lhs, const auto &rhs) {
                const auto lhsName = lowerCase(lhs.name);
                const auto rhsName = lowerCase(rhs.name);
                return lhsName == rhsName ? static_cast<uint64_t>(lhs.id) <
                                                static_cast<uint64_t>(rhs.id)
                                          : lhsName < rhsName;
            });
            return result;
        }

        std::string portValue(const SimEngine::PortState &state,
                              bool csv = false) {
            switch (state.signalKind) {
            case SimEngine::SignalKind::digital:
                switch (state.getLogicState()) {
                case SimEngine::LogicState::low:
                    return "0";
                case SimEngine::LogicState::high:
                    return "1";
                case SimEngine::LogicState::unknown:
                    return "X";
                case SimEngine::LogicState::high_z:
                    return "Z";
                }
                break;
            case SimEngine::SignalKind::scalar:
                return csv ? std::format("{:.17g}", state.scalarValue)
                           : std::format("{:.9g}", state.scalarValue);
            case SimEngine::SignalKind::vector: {
                std::string value = "[";
                for (std::size_t i = 0; i < state.vectorValue.size(); ++i) {
                    if (i != 0) {
                        value += csv ? "," : ", ";
                    }
                    value += csv ? std::format("{:.17g}", state.vectorValue[i])
                                 : std::format("{:.9g}", state.vectorValue[i]);
                }
                value += ']';
                return value;
            }
            case SimEngine::SignalKind::string:
                if (csv) {
                    return state.stringValue;
                } else {
                    std::string value;
                    value.reserve(state.stringValue.size());
                    for (const char ch : state.stringValue) {
                        switch (ch) {
                        case '\n':
                            value += "\\n";
                            break;
                        case '\r':
                            value += "\\r";
                            break;
                        case '\t':
                            value += "\\t";
                            break;
                        default:
                            value += ch;
                            break;
                        }
                    }
                    return value;
                }
            case SimEngine::SignalKind::none:
                return csv ? std::string{} : std::string("-");
            }
            return csv ? std::string{} : std::string("-");
        }

        void drawPortValue(const SimEngine::PortState &state) {
            const auto value = portValue(state);
            if (state.signalKind != SimEngine::SignalKind::digital) {
                ImGui::TextUnformatted(value.c_str());
                return;
            }

            ImVec4 color;
            const char *description = nullptr;
            switch (state.getLogicState()) {
            case SimEngine::LogicState::low:
                color = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
                description = "Low";
                break;
            case SimEngine::LogicState::high:
                color = ImVec4(0.35f, 0.85f, 0.45f, 1.0f);
                description = "High";
                break;
            case SimEngine::LogicState::unknown:
                color = ImVec4(1.0f, 0.72f, 0.25f, 1.0f);
                description = "Unknown";
                break;
            case SimEngine::LogicState::high_z:
                color = ImVec4(0.45f, 0.70f, 1.0f, 1.0f);
                description = "High impedance";
                break;
            }

            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(value.c_str());
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", description);
            }
        }

        std::string timeValue(TimeNs time, int unit) {
            switch (unit) {
            case 1:
                return std::format(
                    "{:.9g}",
                    std::chrono::duration<double, std::micro>(time).count());
            case 2:
                return std::format(
                    "{:.9g}",
                    std::chrono::duration<double, std::milli>(time).count());
            case 3:
                return std::format("{:.9g}",
                                   std::chrono::duration<double>(time).count());
            default:
                return std::format("{:.9g}", time.count());
            }
        }

        std::vector<PortColumn>
        makePortColumns(const SimEngine::PortDescriptor &descriptor,
                        std::size_t count,
                        bool input) {
            std::vector<PortColumn> result;
            result.reserve(count);
            for (std::size_t i = 0; i < count; ++i) {
                const auto spec = descriptor.portSpec(i);
                const auto name =
                    spec.name.empty()
                        ? std::format("{} {}", input ? "Input" : "Output", i)
                        : spec.name;
                const auto unit = spec.unit.empty()
                                      ? std::string{}
                                      : std::format(" ({})", spec.unit);
                result.push_back(
                    {.displayName = std::format(
                         "{}  {}{}", input ? "IN" : "OUT", name, unit),
                     .csvName = std::format(
                         "{}[{}].{}", input ? "input" : "output", i, name)});
            }
            return result;
        }

        std::pair<std::size_t, std::size_t> maxPortCounts(
            std::span<const ComponentStamp> samples,
            const std::shared_ptr<SimEngine::Drivers::CompDef> &definition) {
            std::size_t inputCount = 0;
            std::size_t outputCount = 0;
            if (definition) {
                inputCount = definition->getInputPortDescriptor().portCount();
                outputCount = definition->getOutputPortDescriptor().portCount();
            }
            for (const auto &sample : samples) {
                inputCount = std::max(inputCount, sample.inputStates.size());
                outputCount = std::max(outputCount, sample.outputStates.size());
            }
            return {inputCount, outputCount};
        }

        std::string
        groupedPortValues(const std::vector<PortColumn> &columns,
                          const std::vector<SimEngine::PortState> &states) {
            if (states.empty()) {
                return "-";
            }

            std::string result;
            for (std::size_t i = 0; i < states.size(); ++i) {
                if (!result.empty()) {
                    result += "  |  ";
                }
                const auto &name = i < columns.size()
                                       ? columns[i].displayName
                                       : std::format("Port {}", i);
                result += name;
                result += '=';
                result += portValue(states[i]);
            }
            return result;
        }

        void writeCsvField(std::ostream &stream, std::string_view value) {
            const bool quote =
                value.find_first_of(",\"\r\n") != std::string_view::npos;
            if (!quote) {
                stream << value;
                return;
            }

            stream << '"';
            for (const char ch : value) {
                if (ch == '"') {
                    stream << "\"\"";
                } else {
                    stream << ch;
                }
            }
            stream << '"';
        }

        bool writeCsv(const std::filesystem::path &path,
                      const CsvExportData &data,
                      std::string &error) {
            std::ofstream stream(path, std::ios::binary | std::ios::trunc);
            if (!stream) {
                error = std::format("Could not open '{}' for writing",
                                    path.string());
                return false;
            }

            stream << "sample,time_ns";
            for (const auto &column : data.inputs) {
                stream << ',';
                writeCsvField(stream, column.csvName);
            }
            for (const auto &column : data.outputs) {
                stream << ',';
                writeCsvField(stream, column.csvName);
            }
            stream << '\n';

            for (std::size_t row = 0; row < data.samples.size(); ++row) {
                const auto &sample = data.samples[row];
                stream << row + 1 << ','
                       << std::format("{:.17g}", sample.simTime.count());
                for (std::size_t i = 0; i < data.inputs.size(); ++i) {
                    stream << ',';
                    if (i < sample.inputStates.size()) {
                        writeCsvField(stream,
                                      portValue(sample.inputStates[i], true));
                    }
                }
                for (std::size_t i = 0; i < data.outputs.size(); ++i) {
                    stream << ',';
                    if (i < sample.outputStates.size()) {
                        writeCsvField(stream,
                                      portValue(sample.outputStates[i], true));
                    }
                }
                stream << '\n';
            }

            stream.flush();
            if (!stream) {
                error =
                    std::format("Could not finish writing '{}'", path.string());
                return false;
            }
            return true;
        }

        void drawComponentSelector(const std::vector<ComponentEntry> &entries,
                                   UUID &selectedId,
                                   std::string &filter) {
            const auto selected =
                std::ranges::find_if(entries, [selectedId](const auto &entry) {
                    return entry.id == selectedId;
                });
            const auto preview =
                selected == entries.end()
                    ? std::string("Select a component")
                    : std::format("{}  [{}]", selected->name, selected->type);

            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Component");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(
                std::max(220.0f, ImGui::GetContentRegionAvail().x));
            if (!ImGui::BeginCombo("##StampedComponent", preview.c_str())) {
                return;
            }

            if (ImGui::IsWindowAppearing()) {
                ImGui::SetKeyboardFocusHere();
            }
            ImGui::SetNextItemWidth(-FLT_MIN);
            Widgets::TextBox(
                "##StampComponentFilter", filter, "Search components...");
            ImGui::Separator();

            const auto normalizedFilter = lowerCase(filter);
            bool found = false;
            for (const auto &entry : entries) {
                const auto searchable = lowerCase(
                    std::format("{} {} {}", entry.name, entry.type, entry.id));
                if (!normalizedFilter.empty() &&
                    !searchable.contains(normalizedFilter)) {
                    continue;
                }

                found = true;
                const bool isSelected = entry.id == selectedId;
                const auto label = std::format(
                    "{}  [{}]##{}", entry.name, entry.type, entry.id);
                if (ImGui::Selectable(label.c_str(), isSelected)) {
                    selectedId = entry.id;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Component ID: %s",
                                      entry.id.toString().c_str());
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            if (!found) {
                ImGui::TextDisabled("No matching components");
            }
            ImGui::EndCombo();
        }

        void drawStampTable(std::span<const ComponentStamp> samples,
                            const std::vector<PortColumn> &inputs,
                            const std::vector<PortColumn> &outputs,
                            bool newestFirst,
                            int timeUnit) {
            // Very wide components are more usable as two grouped signal
            // columns and also stay below Dear ImGui's table-column limit.
            constexpr std::size_t maxExpandedPorts = 128;
            const bool groupPorts =
                inputs.size() + outputs.size() > maxExpandedPorts;
            const int columnCount =
                groupPorts
                    ? 4
                    : static_cast<int>(2 + inputs.size() + outputs.size());
            static constexpr ImGuiTableFlags tableFlags =
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
                ImGuiTableFlags_Hideable | ImGuiTableFlags_ScrollX |
                ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;

            if (!ImGui::BeginTable("##StampHistory",
                                   columnCount,
                                   tableFlags,
                                   ImVec2(0.0f, 0.0f))) {
                return;
            }

            ImGui::TableSetupScrollFreeze(2, 1);
            ImGui::TableSetupColumn("Sample",
                                    ImGuiTableColumnFlags_NoHide |
                                        ImGuiTableColumnFlags_WidthFixed,
                                    72.0f);
            const auto timeHeader =
                std::format("Time ({})", timeUnitNames[timeUnit]);
            ImGui::TableSetupColumn(timeHeader.c_str(),
                                    ImGuiTableColumnFlags_NoHide |
                                        ImGuiTableColumnFlags_WidthFixed,
                                    125.0f);
            if (groupPorts) {
                ImGui::TableSetupColumn(
                    "Inputs", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableSetupColumn(
                    "Outputs", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            } else {
                for (const auto &column : inputs) {
                    ImGui::TableSetupColumn(column.displayName.c_str(),
                                            ImGuiTableColumnFlags_WidthFixed,
                                            105.0f);
                }
                for (const auto &column : outputs) {
                    ImGui::TableSetupColumn(column.displayName.c_str(),
                                            ImGuiTableColumnFlags_WidthFixed,
                                            105.0f);
                }
            }
            ImGui::TableHeadersRow();

            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(samples.size()));
            while (clipper.Step()) {
                for (int displayRow = clipper.DisplayStart;
                     displayRow < clipper.DisplayEnd;
                     ++displayRow) {
                    const auto sampleIndex =
                        newestFirst ? samples.size() - 1U -
                                          static_cast<std::size_t>(displayRow)
                                    : static_cast<std::size_t>(displayRow);
                    const auto &sample = samples[sampleIndex];

                    ImGui::TableNextRow();
                    if (sampleIndex + 1U == samples.size()) {
                        ImGui::TableSetBgColor(
                            ImGuiTableBgTarget_RowBg0,
                            ImGui::GetColorU32(ImGuiCol_HeaderHovered));
                    }

                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%zu", sampleIndex + 1U);

                    ImGui::TableSetColumnIndex(1);
                    const auto displayedTime =
                        timeValue(sample.simTime, timeUnit);
                    ImGui::TextUnformatted(displayedTime.c_str());
                    if (timeUnit != 0 && ImGui::IsItemHovered()) {
                        const auto exactTime =
                            std::format("{:.17g} ns", sample.simTime.count());
                        ImGui::SetTooltip("%s", exactTime.c_str());
                    }

                    if (groupPorts) {
                        ImGui::TableSetColumnIndex(2);
                        const auto values =
                            groupedPortValues(inputs, sample.inputStates);
                        ImGui::TextUnformatted(values.c_str());
                        ImGui::TableSetColumnIndex(3);
                        const auto outputValues =
                            groupedPortValues(outputs, sample.outputStates);
                        ImGui::TextUnformatted(outputValues.c_str());
                        continue;
                    }

                    int columnIndex = 2;
                    for (std::size_t i = 0; i < inputs.size(); ++i) {
                        ImGui::TableSetColumnIndex(columnIndex++);
                        if (i < sample.inputStates.size()) {
                            drawPortValue(sample.inputStates[i]);
                        } else {
                            ImGui::TextDisabled("-");
                        }
                    }
                    for (std::size_t i = 0; i < outputs.size(); ++i) {
                        ImGui::TableSetColumnIndex(columnIndex++);
                        if (i < sample.outputStates.size()) {
                            drawPortValue(sample.outputStates[i]);
                        } else {
                            ImGui::TextDisabled("-");
                        }
                    }
                }
            }
            ImGui::EndTable();
        }
    } // namespace

    StampDataViewer::StampDataViewer() : Panel(std::string(windowName.data())) {
        m_defaultDock = Dock::bottom;
        m_visible = true;
    }

    void StampDataViewer::onDraw() {
        auto components = collectComponents();
        if (components.empty()) {
            m_selectedComponentId = UUID::null;
            ImGui::TextDisabled(
                "The active scene does not contain simulation components.");
            return;
        }

        const auto selectionExists =
            std::ranges::any_of(components, [this](const auto &entry) {
                return entry.id == m_selectedComponentId;
            });
        if (!selectionExists) {
            m_selectedComponentId = components.front().id;
        }

        drawComponentSelector(
            components, m_selectedComponentId, m_componentFilter);
        const auto selected =
            std::ranges::find_if(components, [this](const auto &entry) {
                return entry.id == m_selectedComponentId;
            });
        if (selected == components.end()) {
            return;
        }

        std::optional<CsvExportData> exportData;
        {
            const auto stampData = Proj::sim().getStampData();
            const auto history = stampData.find(selected->id);

            ImGui::Spacing();
            if (!history || history->samples.empty()) {
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::TextDisabled(
                    "No stamped samples are available for this component.");
                ImGui::TextDisabled(
                    "Run or step the simulation to capture signal history.");
                return;
            }

            const auto [inputCount, outputCount] =
                maxPortCounts(history->samples, selected->definition);
            const auto inputDescriptor =
                selected->definition
                    ? selected->definition->getInputPortDescriptor()
                    : SimEngine::PortDescriptor{};
            const auto outputDescriptor =
                selected->definition
                    ? selected->definition->getOutputPortDescriptor()
                    : SimEngine::PortDescriptor{};
            const auto inputColumns =
                makePortColumns(inputDescriptor, inputCount, true);
            const auto outputColumns =
                makePortColumns(outputDescriptor, outputCount, false);

            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("%zu samples  |  %zu inputs  |  %zu outputs",
                                history->samples.size(),
                                inputCount,
                                outputCount);
            ImGui::SameLine();
            ImGui::TextDisabled("  Time");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(68.0f);
            ImGui::Combo("##StampTimeUnit", &m_timeUnit, "ns\0us\0ms\0s\0");
            m_timeUnit = std::clamp(m_timeUnit, 0, 3);
            ImGui::SameLine();
            ImGui::Checkbox("Newest first", &m_newestFirst);
            ImGui::SameLine();
            const auto exportLabel =
                std::format("{}  Export CSV", Icons::CodIcons::EXPORT);
            if (ImGui::Button(exportLabel.c_str())) {
                exportData = CsvExportData{
                    .componentName = selected->name,
                    .componentId = selected->id,
                    .inputs = inputColumns,
                    .outputs = outputColumns,
                    .samples = std::vector<ComponentStamp>(
                        history->samples.begin(), history->samples.end())};
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Export all samples in chronological order");
            }

            ImGui::TextDisabled(
                "ID %s  |  Type %s  |  Revision %llu",
                selected->id.toString().c_str(),
                selected->type.c_str(),
                static_cast<unsigned long long>(history->revision));
            if (!m_statusMessage.empty()) {
                const auto color = m_statusIsError
                                       ? ImVec4(0.95f, 0.40f, 0.40f, 1.0f)
                                       : ImVec4(0.35f, 0.85f, 0.45f, 1.0f);
                ImGui::TextColored(color, "%s", m_statusMessage.c_str());
            }
            ImGui::Separator();

            drawStampTable(history->samples,
                           inputColumns,
                           outputColumns,
                           m_newestFirst,
                           m_timeUnit);
        }

        // The view above owns simulation locks. Open the native dialog and
        // write only after that view has gone out of scope.
        if (!exportData) {
            return;
        }

        auto selectedPath = Dialogs::showSaveFileDialog(
            std::format("Export stamped data for {}",
                        exportData->componentName),
            {"CSV file", "*.csv"});
        if (selectedPath.empty()) {
            return;
        }

        std::filesystem::path path(std::move(selectedPath));
        if (lowerCase(path.extension().string()) != ".csv") {
            path += ".csv";
        }

        std::string error;
        if (!writeCsv(path, *exportData, error)) {
            m_statusMessage = std::move(error);
            m_statusIsError = true;
            BESS_ERROR("[StampDataViewer] {}", m_statusMessage);
            return;
        }

        m_statusMessage = std::format("Exported {} samples to {}",
                                      exportData->samples.size(),
                                      path.string());
        m_statusIsError = false;
        BESS_INFO("[StampDataViewer] Exported {} samples for component {} to "
                  "{}",
                  exportData->samples.size(),
                  static_cast<uint64_t>(exportData->componentId),
                  path.string());
    }

} // namespace Bess::UI
