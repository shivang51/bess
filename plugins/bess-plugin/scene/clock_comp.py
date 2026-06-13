import copy
from typing import override
from bessplug.api import bess_ui
from bessplug.api.common import JVal, vec2, vec3
from bessplug.api.scene import (
    PickingId,
    SceneDrawContext,
    SceneState,
    SimulationSceneComponent,
    widgets,
)
from components.clock import FrequencyUnit


class ClockComp(SimulationSceneComponent):
    FREQ_UNITS = [str(v.name) for v in FrequencyUnit]

    def __init__(self):
        super().__init__()
        self.temp_freq_tb = ""

    @override
    def get_type_name(self):
        return "ClockComp"

    @override
    def copy(self):
        cloned = copy.deepcopy(self)
        return cloned

    @override
    def calc_scale(self, scene_state: SceneState):
        scale = super().calc_scale(scene_state)
        # Extra space for frequency and duty cycle controls
        scale += vec2(16, 16)
        return scale

    @override
    def draw(self, context: SceneDrawContext):
        self.draw_background(context)
        self.draw_slots(context)

        self.draw_freq_tb(context)
        self.draw_duty_cycle_slider(context)
        self.draw_freq_unit_dropdown(context)

    def draw_freq_tb(self, context: SceneDrawContext):
        id = PickingId()
        id.runtime_id = self.runtime_id
        id.info = 1

        size = vec2(36, 12)
        x = self.left() + 8 + size.x / 2
        y = self.top() + self.get_slot_start_y()

        freq = getattr(self.comp_def, "frequency", 1.0)
        [res, val] = widgets.text_box(
            id,
            str(freq),
            vec3(x, y, self.transform.position.z + 0.0001),
            size,
            context,
        )

        if res.changed:
            self.temp_freq_tb = val

        if res.submitted:
            try:
                val = float(self.temp_freq_tb)
                if val > 0:
                    setattr(self.comp_def, "frequency", val)
            except ValueError:
                pass
            self.temp_freq_tb = ""

    def draw_duty_cycle_slider(self, context: SceneDrawContext):
        id = PickingId()
        id.runtime_id = self.runtime_id
        id.info = 2

        size = vec2(72, 12)
        x = self.left() + 8 + size.x / 2
        y = self.top() + self.get_slot_start_y() + 16

        duty_cycle = getattr(self.comp_def, "duty_cycle", 0.5)
        opts = widgets.SliderOptions()
        opts.step = 0.01
        opts.show_value = True
        opts.font_size = 8
        opts.knob_radius = opts.font_size / 2

        [res, val] = widgets.slider_float(
            id,
            duty_cycle,
            0,
            1,
            vec3(x, y, self.transform.position.z + 0.0001),
            size,
            context,
            opts,
        )

        if res.changed:
            try:
                val = float(val)
                if 0 <= val <= 1:
                    setattr(self.comp_def, "duty_cycle", val)
            except ValueError:
                pass

    def draw_freq_unit_dropdown(self, context: SceneDrawContext):
        id = PickingId()
        id.runtime_id = self.runtime_id
        id.info = 3

        size = vec2(32, 12)
        x = self.left() + size.x / 2 + 48
        y = self.top() + self.get_slot_start_y()

        unit = getattr(self.comp_def, "unit", FrequencyUnit.HZ)
        [res, val] = widgets.dropdown(
            id,
            self.FREQ_UNITS.index(str(unit.name)),
            self.FREQ_UNITS,
            vec3(x, y, self.transform.position.z + 0.0001),
            size,
            context,
        )

        if res.changed:
            try:
                setattr(self.comp_def, "unit", FrequencyUnit[self.FREQ_UNITS[val]])
            except KeyError:
                pass

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

    def left(self):
        return self.transform.position.x - self.transform.scale.x / 2

    def top(self):
        return self.transform.position.y - self.transform.scale.y / 2

    def bottom(self):
        return self.transform.position.y + self.transform.scale.y / 2

    def top_left(self):
        return vec2(self.left(), self.top())
