import copy
from typing import override
from bessplug.api import bess_ui
from bessplug.api.common import JVal
from bessplug.api.scene import SceneDrawContext, SceneState, SimulationSceneComponent
from bessplug.api.sim_engine.driver import DigCompDef
from components.clock import FrequencyUnit


class ClockComp(SimulationSceneComponent):
    FREQ_UNITS = [str(v.name) for v in FrequencyUnit]

    def __init__(self):
        super().__init__()

    @override
    def get_type_name(self):
        return "ClockComp"

    @override
    def copy(self):
        cloned = copy.deepcopy(self)
        return cloned

    @override
    def draw(self, context: SceneDrawContext):
        self.draw_background(context)
        self.draw_slots(context)

    @override
    def to_json(self):
        data = super().to_json()
        return data

    @staticmethod
    @SimulationSceneComponent.deser
    def from_json(data: JVal):
        comp = ClockComp()
        return comp

    @override
    def draw_properties_ui(self, scene_state: SceneState):
        super().draw_properties_ui(scene_state)
        if not bess_ui.collapsing_node(0, "Clock Component"):
            return

        freq = getattr(self.comp_def, "frequency", 1.0)
        changed, val = bess_ui.slider_float("Frequency", freq, 1, 1000.0)
        if changed and val > 0:
            setattr(self.comp_def, "frequency", val)

        unit = getattr(self.comp_def, "unit", FrequencyUnit.HZ)
        changed, val = bess_ui.combo_box("Unit", str(unit.name), self.FREQ_UNITS)
        if changed:
            setattr(self.comp_def, "unit", FrequencyUnit[val])

        duty_cycle = getattr(self.comp_def, "duty_cycle", 0.5)
        changed, val = bess_ui.slider_float("Duty Cycle", duty_cycle, 0.0, 1.0)
        if changed:
            setattr(self.comp_def, "duty_cycle", val)

        bess_ui.tree_pop()
