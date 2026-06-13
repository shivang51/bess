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
    FREQ_UNIT_INDEX = {
        str(v.name): index for index, v in enumerate(FrequencyUnit)
    }

    def __init__(self):
        super().__init__()
        self._ensure_runtime_state()

    def _ensure_runtime_state(self):
        if not hasattr(self, "temp_freq_tb"):
            self.temp_freq_tb = ""
        if not hasattr(self, "_duty_slider_options"):
            opts = widgets.SliderOptions()
            opts.step = 0.01
            opts.show_value = True
            opts.font_size = 8
            opts.knob_radius = opts.font_size / 2
            self._duty_slider_options = opts

    @override
    def get_type_name(self):
        return "ClockComp"

    @override
    def copy(self):
        cloned = copy.deepcopy(self)
        cloned._ensure_runtime_state()
        return cloned

    @override
    def calc_scale(self, scene_state: SceneState):
        scale = super().calc_scale(scene_state)
        # Extra space for frequency and duty cycle controls
        scale += vec2(16, 16)
        return scale

    @override
    def draw(self, context: SceneDrawContext):
        self._ensure_runtime_state()
        self.draw_background(context)
        self.draw_slots(context)

        layout = self._make_layout()
        comp_def = self.comp_def

        self.draw_freq_tb(context, layout, comp_def)
        self.draw_duty_cycle_slider(context, layout, comp_def)
        self.draw_freq_unit_dropdown(context, layout, comp_def)

    def _make_picking_id(self, info: int):
        picking_id = PickingId()
        picking_id.runtime_id = self.runtime_id
        picking_id.info = info
        return picking_id

    def _make_layout(self):
        transform = self.transform
        position = transform.position
        scale = transform.scale
        left = position.x - scale.x / 2
        top = position.y - scale.y / 2
        return left, top, top + self.get_slot_start_y(), position.z + 0.0001

    def draw_freq_tb(self, context: SceneDrawContext, layout, comp_def):
        picking_id = self._make_picking_id(1)
        left, _, slot_y, z = layout

        size = vec2(36, 12)
        x = left + 8 + size.x / 2
        y = slot_y

        freq = getattr(comp_def, "frequency", 1.0)
        [res, val] = widgets.text_box(
            picking_id,
            str(freq),
            vec3(x, y, z),
            size,
            context,
        )

        if res.changed:
            self.temp_freq_tb = val

        if res.submitted:
            try:
                val = float(self.temp_freq_tb)
                if val > 0:
                    setattr(comp_def, "frequency", val)
            except ValueError:
                pass
            self.temp_freq_tb = ""

    def draw_duty_cycle_slider(self, context: SceneDrawContext, layout, comp_def):
        picking_id = self._make_picking_id(2)
        left, _, slot_y, z = layout

        size = vec2(72, 12)
        x = left + 8 + size.x / 2
        y = slot_y + 16

        duty_cycle = getattr(comp_def, "duty_cycle", 0.5)

        [res, val] = widgets.slider_float(
            picking_id,
            duty_cycle,
            0,
            1,
            vec3(x, y, z),
            size,
            context,
            self._duty_slider_options,
        )

        if res.changed:
            try:
                val = float(val)
                if 0 <= val <= 1:
                    setattr(comp_def, "duty_cycle", val)
            except ValueError:
                pass

    def draw_freq_unit_dropdown(self, context: SceneDrawContext, layout, comp_def):
        picking_id = self._make_picking_id(3)
        left, _, slot_y, z = layout

        size = vec2(32, 12)
        x = left + size.x / 2 + 48
        y = slot_y

        unit = getattr(comp_def, "unit", FrequencyUnit.HZ)
        unit_name = str(unit.name)
        [res, val] = widgets.dropdown(
            picking_id,
            self.FREQ_UNIT_INDEX.get(unit_name, 0),
            self.FREQ_UNITS,
            vec3(x, y, z),
            size,
            context,
        )

        if res.changed:
            try:
                setattr(comp_def, "unit", FrequencyUnit[self.FREQ_UNITS[val]])
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
        transform = self.transform
        return transform.position.x - transform.scale.x / 2

    def top(self):
        transform = self.transform
        return transform.position.y - transform.scale.y / 2

    def bottom(self):
        transform = self.transform
        return transform.position.y + transform.scale.y / 2

    def top_left(self):
        transform = self.transform
        return vec2(
            transform.position.x - transform.scale.x / 2,
            transform.position.y - transform.scale.y / 2,
        )
