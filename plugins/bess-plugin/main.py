from typing import override
from bessplug import Plugin 
from bessplug.api.renderer.path import Path
from bessplug.api.sim_engine import ComponentDefinition
from components.latches import latches


class BessPlugin(Plugin):
    def __init__(self):
        super().__init__("BessPlugin", "0.0")

    @override
    def on_components_reg_load(self) -> list[ComponentDefinition]:
        return [*latches]

    @override
    def on_schematic_symbols_load(self) -> dict[int, Path]:
        latch: ComponentDefinition = latches[0]
        hash = latch.get_hash()
        path = Path()
        path.move_to((0, 0    ))
        path.line_to((1, 0  ))
        path.line_to((1, 1))
        path.line_to((0, 0))
        props = path.get_path_properties()
        props.render_fill = True
        return {hash: path}


plugin_hwd = BessPlugin()

if __name__ == "__main__":
    print("This is a plugin module and cannot be run directly.")
