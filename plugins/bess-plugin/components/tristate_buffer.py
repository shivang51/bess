from bessplug.api.sim_engine import (
    ComponentDefinition,
    ComponentState,
    PinState,
    LogicState,
    SlotsGroupInfo,
)
from bessplug.api.sim_engine.enums import SlotCategory
import datetime


def _simulate_tristate_buffer(
    inputs: list[PinState], simTime: float, oldState: ComponentState
) -> ComponentState:
    newState = oldState.copy()
    newState.input_states = inputs.copy()
    output_states = []
    enable_input = inputs[-1]
    changed = False
    for inp in inputs[:-1]:
        new_output = PinState()
        if enable_input.state == LogicState.HIGH:
            new_output = inp.copy()
            new_output.last_change_time_ns = simTime
        else:
            new_output.state = LogicState.HIGH_Z

        if new_output.state != oldState.output_states[len(output_states)].state:
            changed = True
        output_states.append(new_output)

    newState.output_states = output_states
    newState.is_changed = changed

    return newState


input_slots = SlotsGroupInfo()
input_slots.count = 2
input_slots.names = ["A", "Enable"]
input_slots.categories = [(1, SlotCategory.ENABLE)]

output_slots = SlotsGroupInfo()
output_slots.count = 1
output_slots.names = ["Y"]

tristate_buffer_def = ComponentDefinition.from_sim_fn(
    name="Tri-State Buffer",
    groupName="Digital Gates",
    inputs=input_slots,
    outputs=output_slots,
    sim_delay=datetime.timedelta(microseconds=0.01),
    simFn=_simulate_tristate_buffer,
)

__all__ = ["tristate_buffer_def"]
