from typing import override
from bessplug import Plugin
from bessplug.api.common import vec2, vec3, vec4
from bessplug.api.scene import PickingId, SimulationSceneComponent
from bessplug.api.sim_engine import ComponentDefinition
from components.latches import latches
from components.digital_gates import digital_gates, draw_hooks
from components.flip_flops import flip_flops
from components.combinational_circuits import combinational_circuits
from components.tristate_buffer import tristate_buffer_def
from components import seven_segment_display, seven_segment_display_driver
from components.alu_74LS181 import dm74ls181


class PyOutput(SimulationSceneComponent):
    def __init__(self, comp_def):
        super().__init__()
        self.name = "Py Output"
        self.setup(comp_def)

    def update(self, time_step, scene_state):
        super().update(time_step, scene_state)

    @override
    def draw(self, scene_state, material_renderer, path_renderer):
        id = PickingId()
        id.runtime_id = self.runtime_id
        self.draw_background(scene_state)
        self.draw_slots(scene_state)


class BessPlugin(Plugin):
    def __init__(self):
        super().__init__()
        self.name = "BESS Plugin"
        self.version = "1.0.0.dev"
        # self.is_win_open = True

    @override
    def on_components_reg_load(self) -> list[ComponentDefinition]:
        return [
            *latches,
            *digital_gates,
            *flip_flops,
            *combinational_circuits,
            tristate_buffer_def,
            seven_segment_display.seven_seg_disp_def,
            seven_segment_display_driver.seven_seg_disp_driver_def,
            dm74ls181,
            # clock.clock_def, WILL ADD BACK LATER (POST UI HOOK ARCHITECUTRE)
        ]

    @override
    def on_scene_comp_load(self) -> dict[int, object]:
        return {**draw_hooks, **seven_segment_display.seven_seg_disp_draw_hook}

    @override
    def has_sim_comp(self, base_hash) -> bool:
        return base_hash == 15124334025293992558

    @override
    def get_sim_comp(self, component_def):
        if not self.has_sim_comp(component_def.get_hash()):
            return None
        return PyOutput(component_def)

    @override
    def draw_ui(self):
        pass
        # if not self.is_win_open:
        #     return
        # bess_ui.begin_panel("Bess Plugin Window", self.is_win_open, vec2(250, 250))
        # bess_ui.text("This is a plugin panel.")
        # bess_ui.text("You can add your own UI elements here.")
        # if bess_ui.button("Click me!"):
        #     print("Button clicked!")
        # bess_ui.end_panel()


plugin_hwd = BessPlugin()

if __name__ == "__main__":
    print("This is a plugin module and cannot be run directly.")
