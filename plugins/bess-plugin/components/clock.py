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

    @override
    def clone(self) -> ComponentDefinition:
        print("Cloning clock definition...")
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
        print("Getting reschedule time for clock...")

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
        next_sim_time = current_time + period
        return datetime.timedelta(seconds=next_sim_time)

    @staticmethod
    def _simulate_clock(
        inputs: list[PinState], simTime: float, oldState: ComponentState
    ) -> ComponentState:
        """
        Simulation of a simple clock component that toggles its output state
        at regular intervals defined by the clock period.
        """
        CLOCK_PERIOD = 1.0  # Clock period in seconds

        newState = oldState.copy()
        newState.input_states = inputs.copy()

        # Determine the number of clock cycles that have passed
        elapsed_time = simTime
        num_cycles = int(elapsed_time / CLOCK_PERIOD)

        # Toggle output state based on the number of cycles
        if num_cycles % 2 == 0:
            output_state = LogicState.LOW
        else:
            output_state = LogicState.HIGH

        # Set the output pin state (assuming single output pin at index 0)
        newState.output_states = [PinState(state=output_state)]

        return newState


clock_def = ClockDefinition()
print(clock_def.should_auto_reschedule)

__all__ = ["clock_def"]
