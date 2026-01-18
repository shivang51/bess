from typing import override
from bessplug import Plugin
from bessplug.api.sim_engine import ComponentDefinition
from components.latches import latches
from components.digital_gates import digital_gates, draw_hooks
from components.flip_flops import flip_flops
from components.combinational_circuits import combinational_circuits
from components.tristate_buffer import tristate_buffer_def
from components import seven_segment_display, seven_segment_display_driver


class BessPlugin(Plugin):
    def __init__(self):
        super().__init__("BessPlugin", "0.0")

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
        ]

    @override
    def on_scene_comp_load(self) -> dict[int, object]:
        return {**draw_hooks, **seven_segment_display.seven_seg_disp_draw_hook}


plugin_hwd = BessPlugin()

if __name__ == "__main__":
    print("This is a plugin module and cannot be run directly.")
