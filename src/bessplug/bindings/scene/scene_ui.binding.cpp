#include "bess_core/scene/scene_state/scene_state.h"
#include "bess_core/scene/scene_ui/controls/container_comp.h"
#include "bess_core/scene/scene_ui/layout.h"
#include "bess_core/scene/scene_ui/ui_scene_component.h"
#include "bess_core/style/bess_theme.h"
#include "common/bess_uuid.h"
#include "common/types.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <vector>

namespace py = pybind11;

namespace {
    using Bess::UUID;
    namespace UI = Bess::Canvas::UI;

    std::vector<UUID> childrenAsVector(const UI::UINode &node) {
        const auto &children = node.getChildren();
        return {children.begin(), children.end()};
    }

    void setChildrenFromVector(UI::UINode &node,
                               const std::vector<UUID> &children) {
        Bess::OrderedSet<UUID> orderedChildren;
        orderedChildren.insert(children.begin(), children.end());
        node.setChildren(orderedChildren);
    }

    std::vector<UUID> nodeIds(const UI::UINodeRegistry &registry) {
        std::vector<UUID> ids;
        ids.reserve(registry.getAllNodes().size());
        for (const auto &[id, node] : registry.getAllNodes()) {
            (void)node;
            ids.push_back(id);
        }
        return ids;
    }
} // namespace

void bind_scene_ui(py::module_ &m) {
    py::enum_<UI::Unit>(m, "UIUnit")
        .value("pixel", UI::Unit::pixel)
        .value("relative", UI::Unit::relative)
        .export_values();

    py::enum_<UI::LayoutDirection>(m, "UILayoutDirection")
        .value("horizontal", UI::LayoutDirection::horizontal)
        .value("vertical", UI::LayoutDirection::vertical)
        .value("horizontal_reverse", UI::LayoutDirection::horizontalReverse)
        .value("vertical_reverse", UI::LayoutDirection::verticalReverse)
        .export_values();

    py::enum_<UI::LayoutAlignment>(m, "UILayoutAlignment")
        .value("start", UI::LayoutAlignment::start)
        .value("center", UI::LayoutAlignment::center)
        .value("end", UI::LayoutAlignment::end)
        .export_values();

    py::enum_<UI::LayoutSelfAlignment>(m, "UILayoutSelfAlignment")
        .value("auto", UI::LayoutSelfAlignment::auto_)
        .value("start", UI::LayoutSelfAlignment::start)
        .value("center", UI::LayoutSelfAlignment::center)
        .value("end", UI::LayoutSelfAlignment::end)
        .value("stretch", UI::LayoutSelfAlignment::stretch)
        .export_values();

    py::enum_<UI::LayoutSizeMode>(m, "UILayoutSizeMode")
        .value("auto", UI::LayoutSizeMode::auto_)
        .value("point", UI::LayoutSizeMode::point)
        .value("percent", UI::LayoutSizeMode::percent)
        .value("fit_content", UI::LayoutSizeMode::fitContent)
        .value("max_content", UI::LayoutSizeMode::maxContent)
        .value("stretch", UI::LayoutSizeMode::stretch);

    py::enum_<UI::DrawPivot>(m, "UIDrawPivot")
        .value("top_left", UI::DrawPivot::topLeft)
        .value("top_center", UI::DrawPivot::topCenter)
        .value("center", UI::DrawPivot::center)
        .value("bottom_left", UI::DrawPivot::bottomLeft)
        .value("bottom_center", UI::DrawPivot::bottomCenter);

    py::enum_<UI::PosMode>(m, "UIPosMode")
        .value("absolute", UI::PosMode::absolute)
        .value("relative", UI::PosMode::relative)
        .export_values();

    py::class_<Bess::Core::Style::Padding>(m, "Padding")
        .def(py::init<>())
        .def(py::init<float>(), py::arg("value"))
        .def(py::init<float, float, float, float>(),
             py::arg("top"),
             py::arg("right"),
             py::arg("bottom"),
             py::arg("left"))
        .def_readwrite("top", &Bess::Core::Style::Padding::top)
        .def_readwrite("right", &Bess::Core::Style::Padding::right)
        .def_readwrite("bottom", &Bess::Core::Style::Padding::bottom)
        .def_readwrite("left", &Bess::Core::Style::Padding::left)
        .def_static("horizontal",
                    &Bess::Core::Style::Padding::fromHorizontal,
                    py::arg("horizontal"))
        .def_static("vertical",
                    &Bess::Core::Style::Padding::fromVertical,
                    py::arg("vertical"))
        .def_static("symmetric",
                    &Bess::Core::Style::Padding::fromSymmetric,
                    py::arg("horizontal"),
                    py::arg("vertical"))
        .def_static("zero", &Bess::Core::Style::Padding::zero)
        .def_static(
            "only_top", &Bess::Core::Style::Padding::onlyTop, py::arg("top"))
        .def_static("only_right",
                    &Bess::Core::Style::Padding::onlyRight,
                    py::arg("right"))
        .def_static("only_bottom",
                    &Bess::Core::Style::Padding::onlyBottom,
                    py::arg("bottom"))
        .def_static(
            "only_left", &Bess::Core::Style::Padding::onlyLeft, py::arg("left"))
        .def("to_vec4", &Bess::Core::Style::Padding::toVec4)
        .def_property_readonly("horizontal_size",
                               &Bess::Core::Style::Padding::horizontal)
        .def_property_readonly("vertical_size",
                               &Bess::Core::Style::Padding::vertical)
        .def("__repr__", [](const Bess::Core::Style::Padding &padding) {
            return "Padding(" + std::to_string(padding.top) + ", " +
                   std::to_string(padding.right) + ", " +
                   std::to_string(padding.bottom) + ", " +
                   std::to_string(padding.left) + ")";
        });

    py::class_<UI::UIFlex>(m, "UIFlex")
        .def(py::init<>())
        .def(py::init<float, float, float, UI::Unit>(),
             py::arg("grow"),
             py::arg("shrink"),
             py::arg("basis") = 0.f,
             py::arg_v("basis_unit", UI::Unit::pixel, "UIUnit.pixel"))
        .def_readwrite("grow", &UI::UIFlex::grow)
        .def_readwrite("shrink", &UI::UIFlex::shrink)
        .def_readwrite("basis", &UI::UIFlex::basis)
        .def_readwrite("basis_unit", &UI::UIFlex::basisUnit);

    auto uiElementStyleBinding =
        py::class_<UI::UIElementStyle>(m, "UIElementStyle");
    uiElementStyleBinding.def(py::init<>())
        .def_readwrite("background_color", &UI::UIElementStyle::backgroundColor)
        .def_readwrite("hover_color", &UI::UIElementStyle::hoverColor)
        .def_readwrite("border_color", &UI::UIElementStyle::borderColor)
        .def_readwrite("active_color", &UI::UIElementStyle::activeColor)
        .def_readwrite("padding", &UI::UIElementStyle::padding)
        .def_readwrite("margin", &UI::UIElementStyle::margin)
        .def_readwrite("font_size", &UI::UIElementStyle::fontSize)
        .def_readwrite("pos", &UI::UIElementStyle::pos)
        .def_readwrite("pos_unit", &UI::UIElementStyle::posUnit)
        .def_readwrite("width_mode", &UI::UIElementStyle::widthMode)
        .def_readwrite("width", &UI::UIElementStyle::width)
        .def_readwrite("height_mode", &UI::UIElementStyle::heightMode)
        .def_readwrite("height", &UI::UIElementStyle::height)
        .def_readwrite("min_size", &UI::UIElementStyle::minSize)
        .def_readwrite("max_size", &UI::UIElementStyle::maxSize)
        .def_readwrite("direction", &UI::UIElementStyle::direction)
        .def_readwrite("main_axis_alignment",
                       &UI::UIElementStyle::mainAxisAlignment)
        .def_readwrite("cross_axis_alignment",
                       &UI::UIElementStyle::crossAxisAlignment)
        .def_readwrite("align_self", &UI::UIElementStyle::alignSelf)
        .def_readwrite("flex", &UI::UIElementStyle::flex)
        .def_readwrite("flex_grow", &UI::UIElementStyle::flexGrow)
        .def_readwrite("flex_shrink", &UI::UIElementStyle::flexShrink)
        .def_readwrite("flex_basis_mode", &UI::UIElementStyle::flexBasisMode)
        .def_readwrite("flex_basis", &UI::UIElementStyle::flexBasis)
        .def_readwrite("flex_basis_unit", &UI::UIElementStyle::flexBasisUnit)
        .def_readwrite("pos_mode", &UI::UIElementStyle::posMode)
        .def_readwrite("z_val", &UI::UIElementStyle::zVal)
        .def_readwrite("draw_pivot", &UI::UIElementStyle::drawPivot);
    m.attr("UIStyle") = uiElementStyleBinding;

    auto uiNodeBinding = py::class_<UI::UINode>(m, "UINode");
    auto uiNodeRegistryBinding =
        py::class_<UI::UINodeRegistry, std::shared_ptr<UI::UINodeRegistry>>(
            m, "UINodeRegistry");

    uiNodeBinding.def(py::init<>())
        .def(py::init<const UUID &>(), py::arg("id"))
        .def_property(
            "id",
            [](const UI::UINode &self) { return self.getId(); },
            &UI::UINode::setId)
        .def_property(
            "pos",
            [](const UI::UINode &self) { return self.getPos(); },
            &UI::UINode::setPos)
        .def_property(
            "pos_unit",
            [](const UI::UINode &self) { return self.getPosUnit(); },
            &UI::UINode::setPosUnit)
        .def_property(
            "draw_pivot",
            [](const UI::UINode &self) { return self.getDrawPivot(); },
            &UI::UINode::setDrawPivot)
        .def("set_pos_dirty", &UI::UINode::setPosDirty, py::arg("dirty") = true)
        .def("set_size_dirty",
             &UI::UINode::setSizeDirty,
             py::arg("dirty") = true)
        .def_property_readonly("pos_dirty", &UI::UINode::getPosDirty)
        .def_property_readonly("size_dirty", &UI::UINode::getSizeDirty)
        .def("set_width", &UI::UINode::setWidth, py::arg("width"))
        .def("set_height", &UI::UINode::setHeight, py::arg("height"))
        .def(
            "set_width_percent", &UI::UINode::setWidthPercent, py::arg("width"))
        .def("set_height_percent",
             &UI::UINode::setHeightPercent,
             py::arg("height"))
        .def("set_width_auto", &UI::UINode::setWidthAuto)
        .def("set_height_auto", &UI::UINode::setHeightAuto)
        .def("set_width_fit_content", &UI::UINode::setWidthFitContent)
        .def("set_height_fit_content", &UI::UINode::setHeightFitContent)
        .def("set_width_max_content", &UI::UINode::setWidthMaxContent)
        .def("set_height_max_content", &UI::UINode::setHeightMaxContent)
        .def("set_width_stretch", &UI::UINode::setWidthStretch)
        .def("set_height_stretch", &UI::UINode::setHeightStretch)
        .def_property(
            "padding",
            [](const UI::UINode &self) { return self.getPadding(); },
            &UI::UINode::setPadding)
        .def_property(
            "margin",
            [](const UI::UINode &self) { return self.getMargin(); },
            &UI::UINode::setMargin)
        .def_property(
            "min_size",
            [](const UI::UINode &self) { return self.getMinSize(); },
            &UI::UINode::setMinSize)
        .def_property(
            "max_size",
            [](const UI::UINode &self) { return self.getMaxSize(); },
            &UI::UINode::setMaxSize)
        .def_property_readonly("cached_pos", &UI::UINode::getCachedPos)
        .def_property_readonly("cached_size", &UI::UINode::getCachedSize)
        .def_property_readonly("draw_size", &UI::UINode::getDrawSize)
        .def_property_readonly("cached_z_val", &UI::UINode::getCachedZVal)
        .def_property_readonly("parent_id", &UI::UINode::getParentId)
        .def_property(
            "direction",
            [](const UI::UINode &self) { return self.getDirection(); },
            &UI::UINode::setDirection)
        .def_property(
            "main_axis_alignment",
            [](const UI::UINode &self) { return self.getMainAxisAlignment(); },
            &UI::UINode::setMainAxisAlignment)
        .def_property(
            "cross_axis_alignment",
            [](const UI::UINode &self) { return self.getCrossAxisAlignment(); },
            &UI::UINode::setCrossAxisAlignment)
        .def_property(
            "align_self",
            [](const UI::UINode &self) { return self.getAlignSelf(); },
            &UI::UINode::setAlignSelf)
        .def_property(
            "flex_grow", &UI::UINode::getFlexGrow, &UI::UINode::setFlexGrow)
        .def_property("flex_shrink",
                      &UI::UINode::getFlexShrink,
                      &UI::UINode::setFlexShrink)
        .def("set_flex",
             &UI::UINode::setFlex,
             py::arg("grow"),
             py::arg("shrink"),
             py::arg("basis") = 0.f)
        .def("set_flex_basis",
             &UI::UINode::setFlexBasis,
             py::arg("basis"),
             py::arg_v("unit", UI::Unit::pixel, "UIUnit.pixel"))
        .def("set_flex_basis_auto", &UI::UINode::setFlexBasisAuto)
        .def("set_flex_basis_fit_content", &UI::UINode::setFlexBasisFitContent)
        .def("set_flex_basis_max_content", &UI::UINode::setFlexBasisMaxContent)
        .def("set_flex_basis_stretch", &UI::UINode::setFlexBasisStretch)
        .def_property(
            "pos_mode",
            [](const UI::UINode &self) { return self.getPosMode(); },
            &UI::UINode::setPosMode)
        .def_property("children", &childrenAsVector, &setChildrenFromVector)
        .def_property(
            "z_val",
            [](const UI::UINode &self) { return self.getZVal(); },
            &UI::UINode::setZVal)
        .def(
            "add_child",
            [](UI::UINode &self, UI::UINode &child) { self.addChild(&child); },
            py::arg("child"))
        .def(
            "remove_child",
            [](UI::UINode &self, UI::UINode &child) {
                self.removeChild(&child);
            },
            py::arg("child"))
        .def("clear_children", &UI::UINode::clearChildren)
        .def("measure",
             &UI::UINode::measure,
             py::arg("registry"),
             py::arg("parent_id"))
        .def_property_readonly("draw_pos", &UI::UINode::getDrawPos)
        .def("__repr__", [](const UI::UINode &self) {
            return "<UINode " + self.getId().toString() + ">";
        });

    uiNodeRegistryBinding.def(py::init<>())
        .def(
            "add_node",
            [](UI::UINodeRegistry &self, const UUID &id) {
                return self.addNode(id);
            },
            py::arg("id"),
            py::return_value_policy::reference_internal)
        .def(
            "add_node_copy",
            [](UI::UINodeRegistry &self, const UI::UINode &node) {
                return self.addNode(node);
            },
            py::arg("node"),
            py::return_value_policy::reference_internal)
        .def("remove_node", &UI::UINodeRegistry::removeNode, py::arg("id"))
        .def("get_node",
             py::overload_cast<const UUID &>(&UI::UINodeRegistry::getNode),
             py::arg("id"),
             py::return_value_policy::reference_internal)
        .def("clear", &UI::UINodeRegistry::clear)
        .def_property_readonly("node_ids", &nodeIds);

    py::class_<UI::UISceneComponent,
               Bess::Canvas::SceneComponent,
               py::smart_holder>(m, "UISceneComponent")
        .def_property_readonly(
            "uuid",
            [](const UI::UISceneComponent &self) { return self.getUuid(); })
        .def_property(
            "name",
            [](const UI::UISceneComponent &self) { return self.getName(); },
            &UI::UISceneComponent::setName)
        .def_property(
            "custom_style",
            [](UI::UISceneComponent &self) -> UI::UIElementStyle & {
                return self.getStyle();
            },
            &UI::UISceneComponent::setStyle,
            py::return_value_policy::reference_internal)
        .def_property(
            "draw_runtime_id",
            [](const UI::UISceneComponent &self) {
                return self.getDrawRuntimeId();
            },
            &UI::UISceneComponent::setDrawRuntimeId)
        .def_property_readonly(
            "ui_node",
            [](UI::UISceneComponent &self) { return self.getUINode(); },
            py::return_value_policy::reference_internal)
        .def(
            "get_ui_node",
            [](UI::UISceneComponent &self) { return self.getUINode(); },
            py::return_value_policy::reference_internal)
        .def(
            "set_ui_node",
            [](UI::UISceneComponent &self, UI::UINode *node) {
                self.setUINode(node);
            },
            py::arg("node"))
        .def("cleanup",
             &UI::UISceneComponent::cleanup,
             py::arg("state"),
             py::arg_v("caller", UUID::null, "UUID.null"))
        .def("on_draw", &UI::UISceneComponent::onDraw)
        .def("get_type_name", &UI::UISceneComponent::getTypeName);

    py::class_<UI::ContainerComp, UI::UISceneComponent, py::smart_holder>(
        m, "ContainerComp")
        .def(py::init<>())
        .def_static("create",
                    &UI::ContainerComp::create,
                    py::arg_v("direction",
                              UI::LayoutDirection::horizontal,
                              "UILayoutDirection.horizontal"))
        .def_property(
            "direction",
            [](const UI::ContainerComp &self) { return self.getDirection(); },
            &UI::ContainerComp::setDirection)
        .def_property(
            "main_axis_alignment",
            [](const UI::ContainerComp &self) {
                return self.getMainAxisAlignment();
            },
            &UI::ContainerComp::setMainAxisAlignment)
        .def_property(
            "cross_axis_alignment",
            [](const UI::ContainerComp &self) {
                return self.getCrossAxisAlignment();
            },
            &UI::ContainerComp::setCrossAxisAlignment)
        .def_property(
            "draw_background",
            [](const UI::ContainerComp &self) {
                return self.getDrawBackground();
            },
            &UI::ContainerComp::setDrawBackground);
}
