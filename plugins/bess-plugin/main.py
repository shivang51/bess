from typing import override

from bessplug import Plugin
from bessplug.api import bess_ui
from bessplug.api.common import vec2
from bessplug.api.sim_engine.driver import CompDef

from components import seven_segment_display, seven_segment_display_driver

# from components.alu_74LS181 import dm74ls181
from components.combinational_circuits import combinational_circuits
from components.digital_gates import (
    digital_gates,
    schematic_diagrams as digital_gates_schematics,
)
from components.latches import latches

from components.flip_flops import flip_flops

from components.tristate_buffer import tristate_buffer_def
from scene.digital_gate_comp import DigitalGateComp
from scene.output_comp import OutputComp

from scene.seven_seg_disp_comp import SevenSegDispComp
from ui.scripting_panel import ScriptingPanel

from components.clock import clock_def
from scene.clock_comp import ClockComp
from ui.truth_table_panel import TruthTablePanel


class BessPlugin(Plugin):
    def __init__(self):
        super().__init__()
        self.name = "BESS Plugin"
        self.version = "1.0.0.dev"
        self.scripting_panel = ScriptingPanel()
        self.truth_table_panel = TruthTablePanel()

    @override
    def on_comp_catalog_load(self) -> list[CompDef]:
        return [
            *combinational_circuits,
            *digital_gates,
            *flip_flops,
            *latches,
            clock_def,
            tristate_buffer_def,
            seven_segment_display.seven_seg_disp_def,
            seven_segment_display_driver.seven_seg_disp_driver_def,
        ]

    @override
    def has_sim_scene_comp(self, def_name) -> bool:
        return (
            def_name == "Output"
            or def_name == "Clock"
            or digital_gates_schematics.get(def_name, None) is not None
            or seven_segment_display.seven_seg_disp_def.name == def_name
        )

    @override
    def get_sim_scene_comp(self, comp_def):
        name = comp_def.name
        if not self.has_sim_scene_comp(name):
            return None

        if name == "Output":
            return OutputComp()
        elif seven_segment_display.seven_seg_disp_def.name == comp_def.name:
            return SevenSegDispComp()
        elif name == "Clock":
            return ClockComp()
        else:
            return DigitalGateComp.from_component_def(comp_def)

    @override
    def draw_ui(self):

        bess_ui.begin_menu_bar()
        if bess_ui.begin_menu("View"):
            bess_ui.set_next_window_size(vec2(300, 0))
            if bess_ui.begin_menu(self.name):
                [changed, val] = bess_ui.checkbox(
                    self.scripting_panel.name, self.scripting_panel.is_open
                )

                if changed:
                    self.scripting_panel.is_open = val

                [changed, val] = bess_ui.checkbox(
                    self.truth_table_panel.name, self.truth_table_panel.is_open
                )

                if changed:
                    self.truth_table_panel.is_open = val
                bess_ui.end_menu()
            bess_ui.end_menu()
        bess_ui.end_menu_bar()

        self.scripting_panel.draw()
        self.truth_table_panel.draw()


plugin_hwd = BessPlugin()

if __name__ == "__main__":
    print("This is a plugin module and cannot be run directly.")
