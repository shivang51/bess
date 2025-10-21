from bessplug.api.sim_engine import ComponentDefinition, ComponentState, PinState, LogicState, PinDetail

def _simulate_d_latch(inputs: list[PinState], simTime: float, oldState: ComponentState) -> ComponentState:
    newState = oldState.copy()
    newState.input_states = inputs.copy() 

    d_input = inputs[0]
    clk_input = inputs[1]
    enable_input = inputs[2]

    if enable_input.state != LogicState.HIGH:
        return newState

    last_clk_state = oldState.input_states[1].state

    isRisingEdge = clk_input.state == LogicState.HIGH and last_clk_state == LogicState.LOW

    if not isRisingEdge:
        return newState

    if isRisingEdge:
        newOutput = d_input.copy()
        if newOutput.state != oldState.output_states[0].state:
            newOutput.last_change_time_ns = simTime
            newState.set_output_state(0, newOutput)
            newState.changed = True

    return newState

_dLatchDef = ComponentDefinition.from_expressions( "D Latch", "Latches", 
                                                 3, 2, _simulate_d_latch, 2)

inp_pin_details = [
        PinDetail.for_input_pin("D"),
        PinDetail.for_input_pin("CLK"),
        PinDetail.for_input_pin("EN"),
]

out_pin_details = [
        PinDetail.for_output_pin("Q"),
        PinDetail.for_output_pin("Q'"),
]

_dLatchDef.input_pin_details = inp_pin_details
_dLatchDef.output_pin_details = out_pin_details


latches = [_dLatchDef]

__all__ = ["latches"]
