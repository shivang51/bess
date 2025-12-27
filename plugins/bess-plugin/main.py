from typing import override
from bessplug import Plugin
from bessplug.api.renderer.path import Path
from bessplug.api.sim_engine import ComponentDefinition
from bessplug.plugin import SchematicDiagram
from components.latches import latches

# from components.digital_gates import digital_gates, schematic_symbols


class BessPlugin(Plugin):
    def __init__(self):
        super().__init__("BessPlugin", "0.0")

    @override
    def on_components_reg_load(self) -> list[ComponentDefinition]:
        # return [*latches, *digital_gates]
        return [*latches]

    @override
    def on_schematic_symbols_load(self) -> dict[int, SchematicDiagram]:
        # return {**schematic_symbols}
        return {}


plugin_hwd = BessPlugin()

if __name__ == "__main__":
    print("This is a plugin module and cannot be run directly.")
