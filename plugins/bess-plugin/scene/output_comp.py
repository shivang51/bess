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

    def update(self, time_step, scene_state):
        super().update(time_step, scene_state)
        self.update_decoded_vals(scene_state)

    @override
    def draw(self, scene_state, material_renderer, path_renderer):
        self.draw_background(scene_state, material_renderer, path_renderer)
        self.draw_slots(scene_state, material_renderer, path_renderer)

        id = PickingId()
        id.runtime_id = self.runtime_id
        id.info = 0

        scale_hf = self.scale * 0.5
        posOffset = vec3(6, -scale_hf.y + 31.5, 0.0001)

        material_renderer.draw_text(
            f"Dec = {self.decimal_value}",
            self.position + posOffset,
            id=id.asUint64(),
            size=9,
            color=theme.schematic.text,
        )

        posOffset.y += 12

        material_renderer.draw_text(
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
