from bessplug.api.common.time import TimeNS
from bessplug.api.sim_engine import (
    PinState,
    LogicState,
    SlotsGroupInfo,
)

from bessplug.api.sim_engine.driver import DigCompDef, DigCompSimData
from bessplug.api.sim_engine import SlotCategory


def _simulate_tristate_buffer(data: DigCompSimData) -> DigCompSimData:
    output_states = []

    inputs = data.input_states
    enable_input = inputs[1]
    changed = False
    for inp in inputs[:-1]:
        new_output = PinState()
        if enable_input.state == LogicState.HIGH:
            new_output = inp.copy()
            new_output.last_change_time_ns = data.sim_time
        else:
            new_output.state = LogicState.HIGH_Z

        if new_output.state != data.prev_state.output_states[len(output_states)].state:
            changed = True
        output_states.append(new_output)

    data.output_states = output_states
    data.sim_dependants = changed

    return data


input_slots = SlotsGroupInfo()
input_slots.count = 2
input_slots.names = ["A", "Enable"]
input_slots.categories = [(1, SlotCategory.ENABLE)]
input_slots.is_resizeable = False  # FIXME: Should be true to allow multiple data inputs

output_slots = SlotsGroupInfo()
output_slots.count = 1
output_slots.names = ["Y"]

tristate_buffer_def = DigCompDef.from_sim_fn(
    name="Tri-State Buffer",
    group_name="Digital Gates",
    inputs=input_slots,
    outputs=output_slots,
    prop_delay=TimeNS(2),
    sim_function=_simulate_tristate_buffer,
)

__all__ = ["tristate_buffer_def"]
