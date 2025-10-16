from bessplug import Plugin, Component
import bessplug

class BessPlugin(Plugin):
    def __init__(self):
        super().__init__("BessPlugin", "0.0")

    def on_components_load(self) -> list[Component]:
        return []


print(bessplug.__version__)
print(BessPlugin().name)
