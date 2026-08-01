import copy
from typing import override
from bessplug.api import bess_ui
from bessplug.api.common import JVal, vec2
from bessplug.api.scene import (
    DropdownOption,
    Label,
    Dropdown,
    ContainerComp,
    Padding,
    SceneDrawContext,
    SceneState,
    SceneUIPrepareCtx,
    SimulationSceneComponent,
    Slider,
    TextBox,
    UIElementStyle,
    UILayoutAlignment,
    UILayoutDirection,
    UILayoutSelfAlignment,
    widgets,
)
from components.clock import FrequencyUnit


class ClockComp(SimulationSceneComponent):
    FREQ_UNITS = [str(v.name) for v in FrequencyUnit]
    FREQ_UNIT_INDEX = {str(v.name): index for index, v in enumerate(FrequencyUnit)}

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
        if not hasattr(self, "_controls_parent_id"):
            self._controls_parent_id = None

    @override
    def get_type_name(self):
        return "ClockComp"

    @override
    def copy(self):
        cloned = copy.deepcopy(self)
        cloned._ensure_runtime_state()
        cloned._controls_parent_id = None
        return cloned

    @override
    def draw(self, context: SceneDrawContext):
        self._ensure_runtime_state()
        self.draw_background(context)
        self.draw_slots(context)

    @override
    def to_json(self):
        data = super().to_json()
        return data

    @staticmethod
    @SimulationSceneComponent.deser
    def from_json(_: JVal):
        comp = ClockComp()
        return comp

    def _on_freq_tb_changed(self, val):
        self.temp_freq_tb = val

    def _on_freq_tb_submitted_cancel(self, val):
        try:
            val = float(val)
            if val > 0:
                setattr(self.comp_def, "frequency", val)
        except ValueError:
            pass
        self.temp_freq_tb = ""

    def _on_duty_cycle_changed(self, val):
        try:
            val = float(val)
            if 0 <= val <= 1:
                setattr(self.comp_def, "duty_cycle", val)
        except ValueError:
            pass

    def _on_freq_unit_changed(self, index, _):
        try:
            setattr(self.comp_def, "unit", FrequencyUnit[self.FREQ_UNITS[index]])
        except KeyError:
            pass

    @override
    def prepare_ui(self, ctx: SceneUIPrepareCtx):
        super().prepare_ui(ctx)

        prev_parent = ctx.parent_node

        ctx.parent_node = self.inp_slots_container.ui_node

        # FREQUENCY TB

        freq_row = ContainerComp.create(UILayoutDirection.horizontal)
        freq_row.main_axis_alignment = UILayoutAlignment.start
        freq_row.cross_axis_alignment = UILayoutAlignment.start
        freq_row.custom_style = UIElementStyle()
        freq_row.custom_style.align_self = UILayoutSelfAlignment.start
        freq_row.custom_style.padding = Padding.zero()
        freq_row.custom_style.margin = Padding.only_top(4)
        ctx.scene_state.add_ui_comp(freq_row)
        freq_row.prepare_ui(ctx)

        ctx.parent_node = freq_row.ui_node

        freq_label = Label.create("Frequency")
        freq_label.custom_style = UIElementStyle()
        freq_label.custom_style.padding = Padding.symmetric(3, 1)
        freq_label.custom_style.margin = Padding.zero()
        ctx.scene_state.add_ui_comp(freq_label)
        freq_label.prepare_ui(ctx)

        freq = getattr(self.comp_def, "frequency", 1.0)
        freq_tb = TextBox.create(str(freq), self._on_freq_tb_changed)

        freq_tb.set_submit_cb(self._on_freq_tb_submitted_cancel)
        freq_tb.set_cancel_cb(self._on_freq_tb_submitted_cancel)

        freq_tb.custom_style = UIElementStyle()
        freq_tb.custom_style.padding = Padding.symmetric(3, 1)
        freq_tb.custom_style.margin = Padding.only_left(6)

        freq_tb.set_tb_size(vec2(44, 0))

        ctx.scene_state.add_ui_comp(freq_tb)
        freq_tb.prepare_ui(ctx)

        ctx.parent_node = self.inp_slots_container.ui_node

        # DUTY CYCLE SLIDER
        duty_cycle = getattr(self.comp_def, "duty_cycle", 0.5)
        slider = Slider.create(
            "Duty Cycle", duty_cycle, 0.0, 1.0, self._on_duty_cycle_changed
        )
        slider.custom_style = UIElementStyle()
        slider.custom_style.padding = Padding.symmetric(0, 1)
        slider.custom_style.margin = Padding.vertical(4)
        slider.custom_style.align_self = UILayoutSelfAlignment.start

        slider.thumb_radius = 4
        slider.set_slider_size(vec2(32, 0))

        ctx.scene_state.add_ui_comp(slider)

        slider.prepare_ui(ctx)

        # FREQUENCY UNIT DROPDOWN
        unit = getattr(self.comp_def, "unit", FrequencyUnit.HZ)

        opts: list[DropdownOption] = []

        for name in self.FREQ_UNITS:
            opts.append(DropdownOption(name, True))

        dropdown = Dropdown.create(
            opts,
            self.FREQ_UNIT_INDEX.get(str(unit.name), 0),
            self._on_freq_unit_changed,
        )
        dropdown.custom_style = UIElementStyle()
        dropdown.custom_style.padding = Padding.symmetric(4, 1)
        dropdown.custom_style.margin = Padding.vertical(2)
        dropdown.custom_style.align_self = UILayoutSelfAlignment.start
        dropdown.custom_style.width = 44

        ctx.scene_state.add_ui_comp(dropdown)
        dropdown.prepare_ui(ctx)

        ctx.parent_node = prev_parent

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
