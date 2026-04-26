import copy
from typing import override
from bessplug.api.common import theme, vec3
from bessplug.api.scene import PickingId, SimulationSceneComponent
from bessplug.api.sim_engine.driver import CompDef
from components.digital_gates import schematic_diagrams


class DigitalGateComp(SimulationSceneComponent):
    @staticmethod
    def from_component_def(comp_def: CompDef):
        comp = DigitalGateComp()
        comp.schematic_diagram = schematic_diagrams.get(comp_def.name, None)
        return comp

    def __init__(self):
        super().__init__()
        self.label_size = 8
        self.schematic_diagram = None

    @override
    def copy(self):
        cloned = copy.deepcopy(self)
        cloned.schematic_diagram = self.schematic_diagram
        cloned.label_size = self.label_size
        return cloned

    @override
    def get_type_name(self):
        return "DigitalGateComp"

    @override
    def to_json(self):
        data = super().to_json()
        if self.schematic_diagram:
            data["schm_hash"] = self.comp_def.name
        return data

    @staticmethod
    @SimulationSceneComponent.deser
    def from_json(data):
        comp = DigitalGateComp()
        if data.has_key("schm_hash"):
            schm_hash = data["schm_hash"].as_int()
            comp.schematic_diagram = schematic_diagrams.get(schm_hash, None)
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
        scale = self.schematic_diagram.draw(transform, id, context.path_renderer)

        if scale != self.schematic_transform.scale:
            self.schematic_scale = scale

        size = context.material_renderer.get_text_render_size(
            self.name, self.label_size
        )

        context.material_renderer.draw_text(
            self.name,
            transform.position + vec3(-size.x / 2, scale.y / 2 + self.label_size, 0),
            self.label_size,
            theme.schematic.text,
            id.asUint64(),
        )

        self.draw_slots(context)
