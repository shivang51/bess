from typing import override
from bessplug import Plugin
from bessplug.api.sim_engine import ComponentDefinition
from bessplug.plugin import SchematicDiagram
from components.latches import latches
from components.digital_gates import digital_gates, draw_hooks
from components.flip_flops import flip_flops
from components.combinational_circuits import combinational_circuits
from components.tristate_buffer import tristate_buffer_def


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
        ]

    @override
    def on_schematic_symbols_load(self) -> dict[int, SchematicDiagram]:
        return {}

    @override
    def on_scene_comp_load(self) -> dict[int, object]:
        return {**draw_hooks}


plugin_hwd = BessPlugin()

if __name__ == "__main__":
    print("This is a plugin module and cannot be run directly.")
