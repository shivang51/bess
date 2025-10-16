from bessplug import Plugin, Component

class BessPlugin(Plugin):
    def __init__(self):
        super().__init__("BessPlugin", "0.0")

    def on_components_reg_load(self) -> list[Component]:
        return [Component("Gobo")]


plugin_hwd = BessPlugin()
