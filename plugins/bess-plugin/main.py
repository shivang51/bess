from typing import override
from bessplug import Plugin 
from bessplug.api.sim_engine import ComponentDefinition

class TestComponent(ComponentDefinition):
    def __init__(self):
        super().__init__()
        self.name = "TestComponent XYZOP"

class BessPlugin(Plugin):
    def __init__(self):
        super().__init__("BessPlugin", "0.0")

    @override
    def on_components_reg_load(self) -> list[ComponentDefinition]:
        return [TestComponent()]


plugin_hwd = BessPlugin()

if __name__ == "__main__":
    print(TestComponent())
    print("This is a plugin module and cannot be run directly.")
