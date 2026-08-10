from typing import override
from enum import Enum
from bessplug.api.common.time import TimeNS
from bessplug.api.sim_engine import (
    ConnectionState,
    LogicState,
    SlotsGroupInfo,
)

from bessplug.api.sim_engine.driver import CompDef, DigCompDef, DigCompSimData


class FrequencyUnit(Enum):
    mHz = "mHz"
    HZ = "Hz"
    KHZ = "kHz"


class ClockDefinition(DigCompDef):
    def __init__(self):
        super().__init__()
        self.name = "Clock"
        self.group_name = "IO"
        self.sim_fn = ClockDefinition._simulate_clock
        self.input_slots_info = SlotsGroupInfo()

        # FIXME: the mutation of set value should work,
        # using workaround for now
        ouput_info = SlotsGroupInfo()
        ouput_info.count = 1
        self.output_slots_info = ouput_info

        self.sim_delay = TimeNS(0)
        self.auto_reschedule = True
        self.unit = FrequencyUnit.HZ
        self.frequency = 1.0
        self.duty_cycle = 0.5

    @override
    def get_type_name(self) -> str:
        return "clk_compdef"

    @override
    def to_json(self):
        data = super().to_json()
        data["unit"] = self.unit.value
        data["frequency"] = self.frequency
        data["duty_cycle"] = self.duty_cycle
        return data

    @override
    def clone(self) -> CompDef:
        cloned = ClockDefinition()
        cloned.name = self.name
        cloned.group_name = self.group_name
        cloned.sim_delay = self.sim_delay
        cloned.input_slots_info = self.input_slots_info
        cloned.output_slots_info = self.output_slots_info
        cloned.behavior_type = self.behavior_type
        cloned.auto_reschedule = self.auto_reschedule
        cloned.sim_fn = ClockDefinition._simulate_clock
        cloned.unit = self.unit
        cloned.frequency = self.frequency
        cloned.duty_cycle = self.duty_cycle
        return cloned

    def _period_seconds(self) -> float:
        if self.frequency <= 0:
            raise ValueError("Frequency must be greater than zero.")

        frequency_multiplier = {
            FrequencyUnit.mHz: 1e-3,
            FrequencyUnit.HZ: 1,
            FrequencyUnit.KHZ: 1e3,
        }[self.unit]

        return 1.0 / (self.frequency * frequency_multiplier)

    def _state_duration(self, state: LogicState) -> TimeNS:
        period = self._period_seconds()
        phase_fraction = (
            self.duty_cycle
            if state == LogicState.HIGH
            else 1.0 - self.duty_cycle
        )
        return TimeNS(int(period * phase_fraction * 1e9))

    @override
    def get_initial_sim_delay(self) -> TimeNS:
        return self._state_duration(LogicState.LOW)

    @override
    def get_self_sim_delay(self) -> TimeNS:
        return self._state_duration(LogicState.HIGH)

    @override
    def get_self_sim_delay_after(
        self, completed_self_simulations: int
    ) -> TimeNS:
        current_state = (
            LogicState.HIGH
            if completed_self_simulations % 2 == 1
            else LogicState.LOW
        )
        return self._state_duration(current_state)

    @staticmethod
    def _simulate_clock(data: DigCompSimData) -> DigCompSimData:
        data.output_states[0].state = (
            LogicState.LOW
            if data.prev_state.output_states[0].state == LogicState.HIGH
            else LogicState.HIGH
        )
        data.output_states[0].conn_state = ConnectionState.DRIVEN
        data.output_states[0].last_change_time_ns = data.sim_time
        data.sim_dependants = True
        return data


clock_def = ClockDefinition()

__all__ = ["clock_def"]
