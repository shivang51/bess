from bessplug.api.common.time import TimeNS
from bessplug.api.sim_engine import (
    ComponentDefinition,
    ComponentState,
    PinState,
    SlotsGroupInfo,
)


def _simulate_seven_segment_display(
    inputs: list[PinState], _: float, oldState: ComponentState
) -> ComponentState:
    newState = oldState.copy()
    newState.input_states = inputs.copy()
    newState.is_changed = False
    return newState


input_slots = SlotsGroupInfo()
input_slots.count = 7
input_slots.names = ["A", "B", "C", "D", "E", "F", "G"]
output_slots = SlotsGroupInfo()
output_slots.count = 0

seven_seg_disp_def = ComponentDefinition.from_sim_fn(
    name="Seven Segment Display",
    group_name="IO",
    inputs=input_slots,
    outputs=output_slots,
    sim_delay=TimeNS(1),
    sim_function=_simulate_seven_segment_display,
)
seven_seg_disp_def.compute_hash()

__all__ = ["seven_seg_disp_def"]
