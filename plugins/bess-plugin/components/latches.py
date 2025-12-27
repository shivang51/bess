from enum import Enum
import datetime
from bessplug.api.sim_engine import (
    ComponentDefinition,
    ComponentState,
    PinState,
    LogicState,
)
from bessplug.api.sim_engine.slots_group_info import SlotsGroupInfo


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


def _simulate_latch(
    inputs: list[PinState], simTime: float, oldState: ComponentState
) -> ComponentState:
    oldState = ComponentState(oldState)
    newState = oldState.copy()
    oldState = oldState
    newState.input_states = inputs.copy()

    aux_data: LatchAuxData = oldState.aux_data

    enable_input = inputs[aux_data.enable_pin_idx]

    if not enable_input.state == LogicState.HIGH:
        return newState._native

    latch_type = aux_data.type

    newQ = PinState()

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
            newQ = oldState.output_states[0].copy()
        else:
            newQ.state = LogicState.HIGH_Z
    elif latch_type == LatchType.T_LATCH:
        T = inputs[0]
        if T.state == LogicState.HIGH:
            oldQ = oldState.output_states[0]
            newQ.state = (
                LogicState.LOW if oldQ.state == LogicState.HIGH else LogicState.HIGH
            )
        else:
            newQ = oldState.output_states[0].copy()
    elif latch_type == LatchType.JK_LATCH:
        J = inputs[0]
        K = inputs[1]
        oldQ = oldState.output_states[0]
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

    if oldState.output_states[0].state == newQ.state:
        return newState._native

    newQ.last_change_time_ns = simTime
    newState.changed = True

    newQI = newQ.copy()
    newQI.invert()

    newState.set_output_state(0, newQ)
    newState.set_output_state(1, newQI)
    return newState._native


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


latches = []

for latch_type, details in latchDetails.items():
    input_slots_info: SlotsGroupInfo = SlotsGroupInfo()
    input_slots_info.count = len(details["input_pins"])
    input_slots_info.names = details["input_pins"]

    output_slots_info: SlotsGroupInfo = SlotsGroupInfo()
    output_slots_info.count = len(details["output_pins"])
    output_slots_info.names = details["output_pins"]

    latch = ComponentDefinition.from_sim_fn(
        name=details["name"],
        groupName="Latches",
        inputs=input_slots_info,
        outputs=output_slots_info,
        sim_delay=datetime.timedelta(microseconds=0.001),
        simFn=_simulate_latch,
    )
    latch.aux_data = details["aux_data"]
    latches.append(latch)

__all__ = ["latches"]
