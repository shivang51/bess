import datetime
from typing import override
from enum import Enum
from bessplug.api.sim_engine import (
    ComponentDefinition,
    ComponentState,
    PinState,
    LogicState,
    SlotsGroupInfo,
)


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

        # FIXME: the mutation of set type should work, using workaround for now
        ouput_info = SlotsGroupInfo()
        ouput_info.count = 1
        self.output_slots_info = ouput_info

        self.sim_delay = datetime.timedelta(seconds=0)
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
    def get_reschedule_time(
        self, current_time_ns: datetime.timedelta
    ) -> datetime.timedelta:
        if self.frequency <= 0:
            raise ValueError("Frequency must be greater than zero.")

        current_time = current_time_ns.total_seconds()

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
        return datetime.timedelta(seconds=next_sim_time)

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


clock_def = ClockDefinition()

__all__ = ["clock_def"]
