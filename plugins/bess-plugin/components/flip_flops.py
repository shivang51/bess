from enum import Enum
from bessplug.api.common.time import TimeNS
from bessplug.api.sim_engine import (
    PinState,
    LogicState,
)
from bessplug.api.sim_engine import SlotCategory
from bessplug.api.sim_engine import SlotsGroupInfo
from bessplug.api.sim_engine.driver import DigCompDef, DigCompSimData

CLK_PIN_NAME = "CLK"
CLR_PIN_NAME = "CLR"


class FlipFlopType(Enum):
    JK = "JK"
    D = "D"
    SR = "SR"
    T = "T"


_flip_flops = {
    FlipFlopType.JK: {
        "name": "JK Flip Flop",
        "input_pins": ["J", CLK_PIN_NAME, "K", CLR_PIN_NAME],
        "output_pins": ["Q", "Q'"],
    },
    FlipFlopType.D: {
        "name": "D Flip Flop",
        "input_pins": ["D", CLK_PIN_NAME, CLR_PIN_NAME],
        "output_pins": ["Q", "Q'"],
    },
    FlipFlopType.SR: {
        "name": "SR Flip Flop",
        "input_pins": ["S", CLK_PIN_NAME, "R", CLR_PIN_NAME],
        "output_pins": ["Q", "Q'"],
    },
    FlipFlopType.T: {
        "name": "T Flip Flop",
        "input_pins": ["T", CLK_PIN_NAME, CLR_PIN_NAME],
        "output_pins": ["Q", "Q'"],
    },
}


class FlipFlopAuxData:
    def __init__(
        self, flip_flop_type: FlipFlopType, clk_pin_idx: int, clr_pin_idx: int
    ):
        self.flip_flop_type = flip_flop_type
        self.clk_pin_idx = clk_pin_idx
        self.clr_pin_idx = clr_pin_idx

    def __repr__(self) -> str:
        return f"FlipFlopAuxData(type={self.flip_flop_type}, clk_pin_idx={self.clk_pin_idx}, clr_pin_idx={self.clr_pin_idx})"

    @staticmethod
    def from_dict(type: FlipFlopType, data: dict) -> "FlipFlopAuxData":
        return FlipFlopAuxData(
            flip_flop_type=type,
            clk_pin_idx=data["input_pins"].index(CLK_PIN_NAME),
            clr_pin_idx=data["input_pins"].index(CLR_PIN_NAME),
        )


def _simulate_flip_flop(state: DigCompSimData) -> DigCompSimData:
    enum_key = FlipFlopType[state.expressions[0]]
    aux_data: FlipFlopAuxData = _flip_flops[enum_key]["aux_data"]

    inputs = state.input_states
    clr_input = inputs[aux_data.clr_pin_idx]
    if clr_input.state == LogicState.HIGH:
        newQ = PinState()
        newQ.state = LogicState.LOW
        newQ.last_change_time_ns = state.sim_time
        inv = newQ.copy()
        inv.invert()
        state.output_states = [newQ, inv]
        state.sim_dependants = True
        return state

    clk_input = inputs[aux_data.clk_pin_idx]

    prev_state = state.prev_state
    old_clk_input = prev_state.input_states[aux_data.clk_pin_idx]

    is_rising_edge = (
        old_clk_input.state != LogicState.HIGH and clk_input.state == LogicState.HIGH
    )

    if not is_rising_edge:
        return state

    current_q = prev_state.output_states[0]
    newQ = PinState()

    ff_type: FlipFlopType = aux_data.flip_flop_type

    if ff_type == FlipFlopType.JK:
        J = inputs[0]
        K = inputs[2]
        if J.state == LogicState.HIGH and K.state == LogicState.LOW:
            newQ.state = LogicState.HIGH
        elif J.state == LogicState.LOW and K.state == LogicState.HIGH:
            newQ.state = LogicState.LOW
        elif J.state == LogicState.HIGH and K.state == LogicState.HIGH:
            inv = current_q.copy()
            inv.invert()
            newQ.state = inv.state
        else:
            newQ = current_q.copy()
    elif ff_type == FlipFlopType.D:
        D = inputs[0]
        newQ = D.copy()
    elif ff_type == FlipFlopType.SR:
        S = inputs[0]
        R = inputs[2]
        if S.state == LogicState.HIGH and R.state == LogicState.LOW:
            newQ.state = LogicState.HIGH
        elif S.state == LogicState.LOW and R.state == LogicState.HIGH:
            newQ.state = LogicState.LOW
        elif S.state == LogicState.LOW and R.state == LogicState.LOW:
            newQ = current_q.copy()
        else:
            newQ.state = LogicState.HIGH_Z
    elif ff_type == FlipFlopType.T:
        T = inputs[0]
        if T.state == LogicState.HIGH:
            newQ.state = (
                LogicState.LOW
                if current_q.state == LogicState.HIGH
                else LogicState.HIGH
            )
        else:
            newQ = current_q.copy()
    else:
        raise ValueError(f"Unsupported flip-flop type: {ff_type}")

    if current_q.state == newQ.state:
        return state

    newQ.last_change_time_ns = state.sim_time
    newQInv = newQ.copy()
    newQInv.invert()
    state.output_states = [newQ, newQInv]
    state.sim_dependants = True
    return state


flip_flops = []

for ff_type, ff_data in _flip_flops.items():
    aux_data = FlipFlopAuxData.from_dict(ff_type, ff_data)
    inp_grp_info = SlotsGroupInfo()
    inp_grp_info.count = len(ff_data["input_pins"])
    inp_grp_info.names = ff_data["input_pins"]
    inp_grp_info.categories = [
        (aux_data.clk_pin_idx, SlotCategory.CLOCK),
        (aux_data.clr_pin_idx, SlotCategory.CLEAR),
    ]

    _flip_flops[ff_type]["aux_data"] = aux_data

    out_grp_info = SlotsGroupInfo()
    out_grp_info.count = len(ff_data["output_pins"])
    out_grp_info.names = ff_data["output_pins"]

    def_ff = DigCompDef()
    def_ff.name = ff_data["name"]
    def_ff.group_name = "Flip Flops"
    def_ff.input_slots_info = inp_grp_info
    def_ff.output_slots_info = out_grp_info
    def_ff.prop_delay = TimeNS(2)
    def_ff.sim_fn = _simulate_flip_flop
    def_ff.output_expressions = [ff_type.name]

    flip_flops.append(def_ff)

__all__ = ["flip_flops"]
