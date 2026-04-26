from typing import override

from bessplug import Plugin
from bessplug.api.common.time import TimeNS
from bessplug.api.sim_engine import LogicState, PinState, SlotsGroupInfo, sim_functions
from bessplug.api.sim_engine.driver import (
    CompDef,
    DigCompDef,
    DigCompSimData,
    SimFnData,
)

# from components import seven_segment_display, seven_segment_display_driver
# from components.alu_74LS181 import dm74ls181
# from components.combinational_circuits import combinational_circuits
# from components.digital_gates import (
#     digital_gates,
#     schematic_diagrams as digital_gates_schematics,
# )
# from components.flip_flops import flip_flops
# from components.latches import latches
# from components.tristate_buffer import tristate_buffer_def
# from scene.digital_gate_comp import DigitalGateComp
# from scene.output_comp import OutputComp
# from scene.seven_seg_disp_comp import SevenSegDispComp
from ui.scripting_panel import ScriptingPanel


class AndGateDef(DigCompDef):
    def __init__(self):
        super().__init__()
        self.input_slots_info = SlotsGroupInfo()
        self.input_slots_info.count = 2
        self.input_slots_info.is_resizeable = True

        self.output_slots_info = SlotsGroupInfo()
        self.output_slots_info.count = 1

        self.name = "AND Gate"
        self.group_name = "Digital Gates"

        self.prop_delay = TimeNS(2)
        self.op_info.op = "&"

        self.sim_fn = sim_functions.expr_eval_sim_func

    def _sim_fn(self, data: DigCompSimData) -> DigCompSimData:
        is_high = all([state.state == LogicState.HIGH for state in data.input_states])

        new_state = PinState()

        if is_high:
            new_state.state = LogicState.HIGH
        else:
            new_state.state = LogicState.LOW

        if new_state.state != data.prev_state.output_states[0]:
            data.output_states = [new_state]

        data.output_states[0].last_change_time_ns = data.sim_time

        return data


class BessPlugin(Plugin):
    def __init__(self):
        super().__init__()
        self.name = "BESS Plugin"
        self.version = "1.0.0.dev"
        self.scripting_panel = ScriptingPanel()

    @override
    def on_components_reg_load(self) -> list[CompDef]:
        return [AndGateDef()]
        # return [
        #     *latches,
        #     *digital_gates,
        #     *flip_flops,
        #     *combinational_circuits,
        #     tristate_buffer_def,
        #     seven_segment_display.seven_seg_disp_def,
        #     seven_segment_display_driver.seven_seg_disp_driver_def,
        #     dm74ls181,
        #     # clock.clock_def, WILL ADD BACK LATER (POST UI HOOK ARCHITECUTRE)
        # ]

    @override
    def has_sim_comp(self, base_hash) -> bool:
        return False
        # return (
        #     base_hash == 15124334025293992558
        #     or digital_gates_schematics.get(int(base_hash), None) is not None
        #     or seven_segment_display.seven_seg_disp_def.get_hash() == base_hash
        # )

    @override
    def get_sim_comp(self, component_def):
        base_hash = component_def.get_hash()
        if not self.has_sim_comp(base_hash):
            return None

        # if base_hash == 15124334025293992558:
        #     return OutputComp()
        # elif seven_segment_display.seven_seg_disp_def.get_hash() == base_hash:
        #     return SevenSegDispComp()
        # else:
        #     return DigitalGateComp.from_component_def(component_def)

    @override
    def draw_ui(self):
        self.scripting_panel.draw()


plugin_hwd = BessPlugin()

if __name__ == "__main__":
    print("This is a plugin module and cannot be run directly.")
