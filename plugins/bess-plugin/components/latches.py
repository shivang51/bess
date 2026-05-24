from enum import Enum
from bessplug.api.common import time
from bessplug.api.sim_engine import (
    PinState,
    LogicState,
    SlotsGroupInfo,
)
from bessplug.api.sim_engine.driver import DigCompDef, DigCompSimData


class LatchType(Enum):
    D_LATCH = "D Latch"
    SR_LATCH = "SR Latch"
    T_LATCH = "T Latch"
    JK_LATCH = "JK Latch"


class LatchAuxData:
    def __init__(self, latch_type: LatchType, enable_pin_idx: int):
        self.type: LatchType = latch_type
        self.enable_pin_idx = enable_pin_idx

    def __repr__(self) -> str:
        return f"LatchAuxData(type={self.type}, enable_pin_idx={self.enable_pin_idx})"


latchDetails = {
    LatchType.D_LATCH: {
        "name": "D Latch",
        "input_pins": ["D", "EN"],
        "output_pins": ["Q", "Q'"],
        "aux_data": LatchAuxData(LatchType.D_LATCH, enable_pin_idx=1),
    },
    LatchType.SR_LATCH: {
        "name": "SR Latch",
        "input_pins": ["S", "R", "EN"],
        "output_pins": ["Q", "Q'"],
        "aux_data": LatchAuxData(LatchType.SR_LATCH, enable_pin_idx=2),
    },
    LatchType.T_LATCH: {
        "name": "T Latch",
        "input_pins": ["T", "EN"],
        "output_pins": ["Q", "Q'"],
        "aux_data": LatchAuxData(LatchType.T_LATCH, enable_pin_idx=1),
    },
    LatchType.JK_LATCH: {
        "name": "JK Latch",
        "input_pins": ["J", "K", "EN"],
        "output_pins": ["Q", "Q'"],
        "aux_data": LatchAuxData(LatchType.JK_LATCH, enable_pin_idx=2),
    },
}


def _simulate_latch(state: DigCompSimData) -> DigCompSimData:
    enum_key = LatchType[state.expressions[0]]
    aux_data: LatchAuxData = latchDetails[enum_key]["aux_data"]

    enable_input = state.input_states[aux_data.enable_pin_idx]

    if not enable_input.state == LogicState.HIGH:
        return state

    latch_type = aux_data.type

    newQ = PinState()

    inputs = state.input_states
    prev_state = state.prev_state
    if latch_type == LatchType.D_LATCH:
        newQ = inputs[0].copy()
    elif latch_type == LatchType.SR_LATCH:
        S = inputs[0]
        R = inputs[1]
        if S.state == LogicState.HIGH and R.state == LogicState.LOW:
            newQ.state = LogicState.HIGH
        elif S.state == LogicState.LOW and R.state == LogicState.HIGH:
            newQ.state = LogicState.LOW
        elif S.state == LogicState.LOW and R.state == LogicState.LOW:
            newQ = prev_state.output_states[0].copy()
        else:
            newQ.state = LogicState.HIGH_Z
    elif latch_type == LatchType.T_LATCH:
        T = inputs[0]
        if T.state == LogicState.HIGH:
            oldQ = prev_state.output_states[0]
            newQ.state = (
                LogicState.LOW if oldQ.state == LogicState.HIGH else LogicState.HIGH
            )
        else:
            newQ = prev_state.output_states[0].copy()
    elif latch_type == LatchType.JK_LATCH:
        J = inputs[0]
        K = inputs[1]
        oldQ = prev_state.output_states[0]
        if J.state == LogicState.HIGH and K.state == LogicState.LOW:
            newQ.state = LogicState.HIGH
        elif J.state == LogicState.LOW and K.state == LogicState.HIGH:
            newQ.state = LogicState.LOW
        elif J.state == LogicState.HIGH and K.state == LogicState.HIGH:
            newQ.state = (
                LogicState.LOW if oldQ.state == LogicState.HIGH else LogicState.HIGH
            )
        else:
            newQ = oldQ.copy()
    else:
        raise ValueError(f"Unsupported latch type: {latch_type}")

    if prev_state.output_states[0].state == newQ.state:
        return state

    newQ.last_change_time_ns = state.sim_time
    state.sim_dependants = True

    newQI = newQ.copy()
    newQI.invert()

    state.output_states = [newQ, newQI]
    return state


latches = []

for latch_type, details in latchDetails.items():
    input_slots_info: SlotsGroupInfo = SlotsGroupInfo()
    input_slots_info.count = len(details["input_pins"])
    input_slots_info.names = details["input_pins"]

    output_slots_info: SlotsGroupInfo = SlotsGroupInfo()
    output_slots_info.count = len(details["output_pins"])
    output_slots_info.names = details["output_pins"]

    latch = DigCompDef()
    latch.name = details["name"]
    latch.group_name = "Latches"
    latch.input_slots_info = input_slots_info
    latch.output_slots_info = output_slots_info
    latch.prop_delay = time.TimeNS(2)
    latch.sim_fn = _simulate_latch
    latch.output_expressions = [latch_type.name]
    latches.append(latch)

__all__ = ["latches"]
