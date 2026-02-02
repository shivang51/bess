from typing import override
from enum import Enum
from bessplug.api.common.time import TimeNS
from bessplug.api.sim_engine import (
    ComponentDefinition,
    ComponentState,
    PinState,
    LogicState,
    SlotsGroupInfo,
)

from bessplug.api.ui import ui_hook


class FrequencyUnit(Enum):
    HZ = "Hz"
    KHZ = "kHz"
    MHZ = "MHz"
    GHZ = "GHz"


class ClockDefinition(ComponentDefinition):
    def __init__(self):
        super().__init__()
        self.name = "New Clock"
        self.group_name = "IO"
        self.simulation_function = ClockDefinition._simulate_clock
        self.input_slots_info = SlotsGroupInfo()

        # FIXME: the mutation of set value should work, using workaround for now
        ouput_info = SlotsGroupInfo()
        ouput_info.count = 1
        self.output_slots_info = ouput_info

        self.sim_delay = TimeNS(0)
        self.should_auto_reschedule = True
        self.unit = FrequencyUnit.HZ
        self.frequency = 1.0
        self.duty_cycle = 0.5
        self.prev_logic_state = LogicState.LOW

    @override
    def clone(self) -> ComponentDefinition:
        cloned = ClockDefinition()
        cloned.name = self.name
        cloned.group_name = self.group_name
        cloned.sim_delay = self.sim_delay
        cloned.input_slots_info = self.input_slots_info
        cloned.output_slots_info = self.output_slots_info
        cloned.behavior_type = self.behavior_type
        cloned.should_auto_reschedule = self.should_auto_reschedule
        cloned.set_simulation_function(self.simulation_function)
        cloned.unit = self.unit
        cloned.frequency = self.frequency
        return cloned

    @override
    def get_reschedule_time(self, current_time_ns: TimeNS) -> TimeNS:
        if self.frequency <= 0:
            raise ValueError("Frequency must be greater than zero.")

        current_time = current_time_ns

        frequency_multiplier = {
            FrequencyUnit.HZ: 1,
            FrequencyUnit.KHZ: 1e3,
            FrequencyUnit.MHZ: 1e6,
            FrequencyUnit.GHZ: 1e9,
        }[self.unit]

        period = 1.0 / (self.frequency * frequency_multiplier)

        if self.prev_logic_state == LogicState.LOW:
            period = period * self.duty_cycle
        else:
            period = period * (1 - self.duty_cycle)

        next_sim_time = current_time + period
        return TimeNS(int(next_sim_time * 1e9))

    @override
    def on_state_change(
        self,
        old_state: ComponentState,
        new_state: ComponentState,
    ) -> None:
        self.prev_logic_state = new_state.output_states[0].state

    @staticmethod
    def _simulate_clock(
        _: list[PinState], simTime: float, oldState: ComponentState
    ) -> ComponentState:
        new_state = oldState.copy()
        new_state.output_states[0].invert()
        new_state.output_states[0].last_change_time_ns = simTime
        new_state.is_changed = True
        return new_state


# describes the properties to be shown in properties panel of the UI
def get_ui_hook():
    prop = ui_hook.PropertyDesc()
    prop.name = "Frequency"
    prop.type = ui_hook.PropertyDescType.float_t
    prop.default_value = 1.0
    prop.constraints = ui_hook.float_constraint(0.1, 5, 0.1)

    unit_prop = ui_hook.PropertyDesc()
    unit_prop.name = "Unit"
    unit_prop.type = ui_hook.PropertyDescType.enum_t
    unit_prop.default_value = FrequencyUnit.HZ.value
    unit_prop.constraints = ui_hook.EnumConstraintsBuilder.for_enum(FrequencyUnit)

    hook = ui_hook.UIHook()
    hook.add_property(prop)
    hook.add_property(unit_prop)
    return hook


clock_def = ClockDefinition()
clock_ui_hook = get_ui_hook()

__all__ = ["clock_def", "clock_ui_hook"]
