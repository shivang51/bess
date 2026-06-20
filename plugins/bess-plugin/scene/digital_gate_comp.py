import copy
from typing import override
from bessplug.api.common import theme, vec3
from bessplug.api.scene import PickingId, SchematicDiagram, SimulationSceneComponent
from bessplug.api.sim_engine.driver import CompDef
from components.digital_gates import schematic_diagrams, icons


class DigitalGateComp(SimulationSceneComponent):
    @staticmethod
    def from_component_def(comp_def: CompDef):
        comp = DigitalGateComp()
        diagram = schematic_diagrams.get(comp_def.name, None)
        comp.schematic_diagram = diagram.copy() if diagram else None
        if icons.get(comp_def.name, ""):
            comp.icon = icons[comp_def.name]
        return comp

    def __init__(self):
        super().__init__()
        self.label_size = 8
        self.schematic_diagram: SchematicDiagram | None = None

    @override
    def copy(self):
        cloned = copy.deepcopy(self)
        cloned.schematic_diagram = (
            self.schematic_diagram.copy() if self.schematic_diagram else None
        )
        cloned.icon = self.icon
        cloned.label_size = self.label_size
        return cloned

    @override
    def get_type_name(self):
        return "DigitalGateComp"

    @override
    def to_json(self):
        data = super().to_json()
        if self.schematic_diagram:
            data["schm_name"] = self.comp_def.name
        data["icon"] = self.icon
        return data

    @staticmethod
    @SimulationSceneComponent.deser
    def from_json(data):
        comp = DigitalGateComp()
        if data.has_key("schm_name") or data.has_key("schm_hash"):
            if data.has_key("schm_name"):
                schematic_name = data["schm_name"]
            else:
                schematic_name = data["schm_hash"]
            diagram = schematic_diagrams.get(schematic_name, None)
            comp.schematic_diagram = diagram.copy() if diagram else None

        if data.has_key("icon"):
            comp.icon = data["icon"].as_str()
        return comp

    @override
    def draw_schematic(self, context):
        if self.schematic_diagram is None:
            super().draw_schematic(context)
            return

        id = PickingId()
        id.runtime_id = self.runtime_id
        id.info = 0

        transform = self.schematic_transform
        scale = self.schematic_diagram.draw(transform, id, context)

        if scale != self.schematic_transform.scale:
            self.schematic_scale = scale

        if self.schematic_diagram.show_name:
            size = context.renderer.get_text_render_size(self.name, self.label_size)
            context.renderer.draw_text(
                self.name,
                transform.position
                + vec3(-size.x / 2, scale.y / 2 + self.label_size, 0),
                self.label_size,
                theme.schematic.text,
                id.asUint64(),
            )

        self.draw_slots(context)
