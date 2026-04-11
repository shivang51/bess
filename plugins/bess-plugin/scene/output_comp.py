import copy
from typing import override
from bessplug.api.common import theme, vec3
from bessplug.api.scene import PickingId, SimulationSceneComponent
from bessplug.api.sim_engine import ComponentDefinition, LogicState


class OutputComp(SimulationSceneComponent):
    def __init__(self, comp_def: ComponentDefinition):
        super().__init__()
        self.name = comp_def.name
        self.setup(comp_def)
        self.decimal_value = 0
        self.hex_value = "0x0"

    @override
    def get_type_name(self):
        return "OutputComp"

    @override
    def clone(self, scene_state):
        cloned = copy.deepcopy(self)
        cloned.decimal_value = self.decimal_value
        cloned.hex_value = self.hex_value
        return self.clone_sim_comp(scene_state, cloned)

    @override
    def update(self, time_step, scene_state):
        super().update(time_step, scene_state)
        self.update_decoded_vals(scene_state)

    @override
    def draw(self, context):
        self.draw_background(context)
        self.draw_slots(context)

        id = PickingId()
        id.runtime_id = self.runtime_id
        id.info = 0

        scale_hf = self.scale * 0.5
        posOffset = vec3(6, -scale_hf.y + 31.5, 0.0001)

        context.material_renderer.draw_text(
            f"Dec = {self.decimal_value}",
            self.position + posOffset,
            id=id.asUint64(),
            size=9,
            color=theme.schematic.text,
        )

        posOffset.y += 12

        context.material_renderer.draw_text(
            f"Hex = {self.hex_value}",
            self.position + posOffset,
            id=id.asUint64(),
            size=9,
            color=theme.schematic.text,
        )

    def update_decoded_vals(self, scene_state):
        states = self.get_input_states(scene_state)
        self.decimal_value = 0
        for i, state in enumerate(states):
            if state == LogicState.HIGH:
                self.decimal_value += 1 << i
        self.hex_value = hex(self.decimal_value)
