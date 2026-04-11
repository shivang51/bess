import copy
from typing import override
from bessplug.api.common import theme, vec3
from bessplug.api.scene import PickingId, SimulationSceneComponent
from bessplug.api.sim_engine import ComponentDefinition
from components.digital_gates import schematic_diagrams


class DigitalGateComp(SimulationSceneComponent):
    def __init__(self, comp_def: ComponentDefinition):
        super().__init__()
        self.name = comp_def.name
        self.setup(comp_def)
        self.schematic_diagram = schematic_diagrams.get(comp_def.get_hash(), None)
        self.label_size = 8

    @override
    def clone(self, scene_state):
        cloned = copy.deepcopy(self)
        cloned.schematic_diagram = self.schematic_diagram
        cloned.label_size = self.label_size
        return self.clone_sim_comp(scene_state, cloned)

    @override
    def get_type_name(self):
        return "DigitalGateComp"

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
