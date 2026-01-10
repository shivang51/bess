from typing import override
from bessplug import Plugin
from bessplug.api.renderer.path import Path
from bessplug.api.scene import SimCompDrawHook
from bessplug.api.sim_engine import ComponentDefinition
from bessplug.plugin import SchematicDiagram
from components.latches import latches
from components.digital_gates import digital_gates, schematic_symbols
from components.flip_flops import flip_flops
from components.combinational_circuits import combinational_circuits
from components.tristate_buffer import tristate_buffer_def

# FIXME: will use it soon
# class DummyHook(SimCompDrawHook):
#     def __init__(self):
#         super().__init__()
#
#     def onDraw(self, state, material_renderer, path_renderer):
#         print("DummyHook draw called")
#


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
        # return {**schematic_symbols}
        return {}

    @override
    def on_scene_comp_load(self) -> dict[int, type]:
        # FIXME: temporarily disabled will use it later
        # flip_flops[0].compute_hash()
        # hash = flip_flops[0].get_hash()
        # self.logger.log(f"FlipFlop {flip_flops[0].name} hash: {hash}")
        # return {hash: DummyHook}
        return {}


plugin_hwd = BessPlugin()

if __name__ == "__main__":
    print("This is a plugin module and cannot be run directly.")
