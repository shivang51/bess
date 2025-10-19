from typing import override
from bessplug import Plugin 
from bessplug.api.sim_engine import ComponentDefinition, ComponentState, PinState

class TestComponent(ComponentDefinition):
    def __init__(self):
        super().__init__()
        self.name = "TestComponent XYZOP"
        self.category = "Plugin Components"
        self.input_count = 2
        self.output_count = 2
        self.delay_ns = 2
        self.set_simulation_function(self.simulate)
        self.init()

    @staticmethod
    def simulate(inputs: list[PinState], simTime: float, oldState: ComponentState) -> ComponentState:
        newState = oldState
        print("Simulating TestComponent XYZOP")
        return newState

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
