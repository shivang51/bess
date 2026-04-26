from bessplug.api.common.time import TimeNS
from bessplug.api.sim_engine import SlotsGroupInfo
from bessplug.api.sim_engine.driver import DigCompDef, DigCompSimData


def _simulate_seven_segment_display(data: DigCompSimData) -> DigCompSimData:
    data.sim_dependants = False
    return data


input_slots = SlotsGroupInfo()
input_slots.count = 7
input_slots.names = ["A", "B", "C", "D", "E", "F", "G"]
output_slots = SlotsGroupInfo()
output_slots.count = 0

seven_seg_disp_def = DigCompDef.from_sim_fn(
    name="Seven Segment Display",
    group_name="IO",
    inputs=input_slots,
    outputs=output_slots,
    prop_delay=TimeNS(1),
    sim_function=_simulate_seven_segment_display,
)

__all__ = ["seven_seg_disp_def"]
