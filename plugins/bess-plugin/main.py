from typing import override

from bessplug import Plugin
from bessplug.api.sim_engine.driver import CompDef

# from components import seven_segment_display, seven_segment_display_driver

# from components.alu_74LS181 import dm74ls181
# from components.combinational_circuits import combinational_circuits
from components.digital_gates import (
    digital_gates,
    schematic_diagrams as digital_gates_schematics,
)
from components.latches import latches

from components.flip_flops import flip_flops

# from components.tristate_buffer import tristate_buffer_def
from scene.digital_gate_comp import DigitalGateComp
from scene.output_comp import OutputComp

# from scene.seven_seg_disp_comp import SevenSegDispComp
from ui.scripting_panel import ScriptingPanel


class BessPlugin(Plugin):
    def __init__(self):
        super().__init__()
        self.name = "BESS Plugin"
        self.version = "1.0.0.dev"
        self.scripting_panel = ScriptingPanel()

    @override
    def on_comp_catalog_load(self) -> list[CompDef]:
        return [
            *digital_gates,
            *flip_flops,
            *latches,
        ]
        # return [
        #     *latches,
        #     *digital_gates,
        #     *flip_flops,
        #     *combinational_circuits,
        #     tristate_buffer_def,
        #     seven_segment_display.seven_seg_disp_def,
        #     seven_segment_display_driver.seven_seg_disp_driver_def,
        #     dm74ls181,
        #     # clock.clock_def, WILL ADD BACK LATER (POST UI HOOK ARCHITECUTRE)
        # ]

    @override
    def has_sim_scene_comp(self, def_name) -> bool:
        return (
            def_name == "Output"
            or digital_gates_schematics.get(def_name, None) is not None
            # or seven_segment_display.seven_seg_disp_def.get_hash() == base_hash
        )

    @override
    def get_sim_scene_comp(self, comp_def):
        name = comp_def.name
        if not self.has_sim_scene_comp(name):
            return None

        if name == "Output":
            return OutputComp()
        # elif seven_segment_display.seven_seg_disp_def.get_hash() == base_hash:
        #     return SevenSegDispComp()
        else:
            return DigitalGateComp.from_component_def(comp_def)

    @override
    def draw_ui(self):
        self.scripting_panel.draw()


plugin_hwd = BessPlugin()

if __name__ == "__main__":
    print("This is a plugin module and cannot be run directly.")
