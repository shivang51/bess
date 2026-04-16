from typing import override
from bessplug import Plugin
from bessplug.api import bess_ui
from bessplug.api.common import vec2
from bessplug.api.sim_engine import ComponentDefinition
from components.latches import latches
from components.digital_gates import (
    digital_gates,
    schematic_diagrams as digital_gates_schematics,
)
from components.flip_flops import flip_flops
from components.combinational_circuits import combinational_circuits
from components.tristate_buffer import tristate_buffer_def
from components import seven_segment_display, seven_segment_display_driver
from components.alu_74LS181 import dm74ls181
from scene.output_comp import OutputComp
from scene.digital_gate_comp import DigitalGateComp
from scene.seven_seg_disp_comp import SevenSegDispComp


class BessPlugin(Plugin):
    def __init__(self):
        super().__init__()
        self.name = "BESS Plugin"
        self.version = "1.0.0.dev"
        self.is_win_open = True

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
    def has_sim_comp(self, base_hash) -> bool:
        return (
            base_hash == 15124334025293992558
            or digital_gates_schematics.get(int(base_hash), None) is not None
            or seven_segment_display.seven_seg_disp_def.get_hash() == base_hash
        )

    @override
    def get_sim_comp(self, component_def):
        base_hash = component_def.get_hash()
        if not self.has_sim_comp(base_hash):
            return None

        if base_hash == 15124334025293992558:
            return OutputComp()
        elif seven_segment_display.seven_seg_disp_def.get_hash() == base_hash:
            return SevenSegDispComp()
        else:
            return DigitalGateComp.from_component_def(component_def)

    @override
    def draw_ui(self):
        if not self.is_win_open:
            return
        self.is_win_open = bess_ui.begin_panel(
            "Bess Plugin Window", vec2(250, 250), self.is_win_open
        )
        bess_ui.text("This is a plugin panel.")
        bess_ui.text("You can add your own UI elements here.")
        if bess_ui.button("Click me!"):
            print("Button clicked!")
        bess_ui.end_panel()


plugin_hwd = BessPlugin()

if __name__ == "__main__":
    print("This is a plugin module and cannot be run directly.")
