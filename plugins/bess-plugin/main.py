from typing import override
from bessplug import Plugin
from bessplug.api.renderer.path import Path
from bessplug.api.scene.scene import SceneComp
from bessplug.api.sim_engine import ComponentDefinition
from bessplug.plugin import SchematicDiagram
from components.latches import latches
from components.digital_gates import digital_gates, schematic_symbols


class DummySceneComp(SceneComp):
    def __init__(self):
        super().__init__()
        print("DummyScenComp created")
        self.set_comp_def_hash(12345)
        print(self.get_comp_def_hash())

    def onNameChanged(self):
        print("Name changed: og name")


class BessPlugin(Plugin):
    def __init__(self):
        super().__init__("BessPlugin", "0.0")

    @override
    def on_components_reg_load(self) -> list[ComponentDefinition]:
        return [*latches, *digital_gates]

    @override
    def on_schematic_symbols_load(self) -> dict[int, SchematicDiagram]:
        # return {**schematic_symbols}
        return {}

    @override
    def on_scene_comp_load(self) -> dict[int, type]:
        d = {12345: DummySceneComp}
        return d


plugin_hwd = BessPlugin()

if __name__ == "__main__":
    print("This is a plugin module and cannot be run directly.")
