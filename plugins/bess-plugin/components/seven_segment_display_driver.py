from bessplug.api.common.time import TimeNS
from bessplug.api.sim_engine import (
    ComponentDefinition,
    ComponentState,
    LogicState,
    PinState,
    SlotsGroupInfo,
)

_digit_to_seg_map = {
    0: 0b0111111,
    1: 0b0000110,
    2: 0b1011011,
    3: 0b1001111,
    4: 0b1100110,
    5: 0b1101101,
    6: 0b1111101,
    7: 0b0000111,
    8: 0b1111111,
    9: 0b1101111,
    10: 0b1110111,  # A
    11: 0b1111100,  # b
    12: 0b0111001,  # C
    13: 0b1011110,  # d
    14: 0b1111001,  # E
    15: 0b1110001,  # F
}


def _simulate_seven_seg_disp_driver(
    inputs: list[PinState], simTime: float, oldState: ComponentState
) -> ComponentState:
    new_state = oldState.copy()
    new_state.input_states = inputs.copy()
    connected: bool = any(oldState.input_connected)
    decoded = -1

    if connected:
        decoded = 0
        for i in range(len(inputs)):
            if inputs[i].state == LogicState.HIGH:
                decoded |= 1 << i

    seg_bits = _digit_to_seg_map.get(decoded, 0b0000000)

    changed = False
    for i in range(len(new_state.output_states)):
        if (seg_bits & (1 << i)) != 0:
            new_state.output_states[i].state = LogicState.HIGH
        else:
            new_state.output_states[i].state = LogicState.LOW

        this_changed = (
            new_state.output_states[i].state != oldState.output_states[i].state
        )

        if this_changed:
            new_state.output_states[i].last_change_time_ns = simTime

        changed = changed or this_changed

    new_state.is_changed = changed

    return new_state


input_slots = SlotsGroupInfo()
input_slots.names = ["A (LSB)", "B", "C", "D (MSB)"]
input_slots.count = len(input_slots.names)
output_slots = SlotsGroupInfo()
output_slots.count = 7
output_slots.names = ["a", "b", "c", "d", "e", "f", "g"]

seven_seg_disp_driver_def = ComponentDefinition.from_sim_fn(
    name="Seven Segment Display Driver",
    group_name="IO",
    inputs=input_slots,
    outputs=output_slots,
    sim_delay=TimeNS(2),
    sim_function=_simulate_seven_seg_disp_driver,
)
__all__ = ["seven_seg_disp_driver_def"]
