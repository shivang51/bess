import typing
import time
import bessplug
import bessplug.api.bess_ui as bess_ui
from bessplug.api.common import UUID, vec2
from bessplug.api.sim_engine import ComponentBehaviorType, LogicState, core
from bessplug.api.sim_engine.driver import DigCompDef, DigSimComp, Net


class TruthTablePanel:
    def __init__(self):
        self.name = "Truth Table Panel"
        self._is_open = True
        self._is_first = True

        self._nets: dict[UUID, Net] = {}
        self._selected_net_id: UUID | None = None

        self._comps: dict[UUID, DigSimComp] = {}
        self._inputs: list[UUID] = []
        self._outputs: list[UUID] = []

        self._fetched_comps = False

    def draw(self):
        if not self._is_open:
            return

        if self._is_first:
            self._is_first = False
            bess_ui.try_reg_dock(self.name, bess_ui.Dock.bottom)
            return

        self._is_open = bess_ui.begin_panel(self.name, vec2(250, 250), self._is_open)

        if bess_ui.button("Refresh Nets"):
            self._nets = core.get_nets()
            if len(self._nets) > 0:
                self._selected_net_id = next(iter(self._nets.keys()))

        bess_ui.same_line()

        bess_ui.separator(True)

        bess_ui.same_line()

        [changed, val] = bess_ui.combo_box(
            "Select Net",
            str(self._selected_net_id),
            [str(nid) for nid in self._nets.keys()],
        )

        if changed:
            self._selected_net_id = UUID.from_str(val)
            self._fetched_comps = False

        if not self._selected_net_id:
            bess_ui.end_panel()
            return

        if bess_ui.button("Calculate Table"):
            self._fetch_comps()

        if not self._fetched_comps:
            bess_ui.same_line()
            bess_ui.text("Press Calculate Table to start")
            bess_ui.end_panel()
            return

        if not self._comps:
            bess_ui.text("No components found in net")
            bess_ui.end_panel()
            return

        if not self._inputs:
            bess_ui.text("No inputs found in net")

        if not self._outputs:
            bess_ui.text("No outputs found in net")

        bess_ui.end_panel()

    def _fetch_comps(self):
        self._comps = {}
        self._inputs = []
        self._outputs = []

        if self._selected_net_id is None:
            return

        for id in self._nets[self._selected_net_id].get_components():
            comp = core.get_comp(id)

            if isinstance(comp, DigSimComp):
                self._comps[comp.uuid] = comp
                dig_def = typing.cast(DigCompDef, comp.definition)

                if dig_def.behavior_type == ComponentBehaviorType.INPUT:
                    self._inputs.append(comp.uuid)
                elif dig_def.behavior_type == ComponentBehaviorType.OUTPUT:
                    self._outputs.append(comp.uuid)

        self._fetched_comps = True

    def _gen_truth_table(self):
        if not self._inputs or not self._outputs:
            return []

        table = []

        num_inputs = len(self._inputs)
        num_rows = 2**num_inputs

        for i in range(num_rows):
            input_states = [(i >> bit) & 1 for bit in range(num_inputs)]
            for comp_id, state in zip(self._inputs, input_states):
                self._set_inp_comp_state(comp_id, 0, state)

            while not core.is_sim_stable():
                print("Waiting for sim to stabilize...")
                time.sleep(0.01)

            output_states = []
            for out_id in self._outputs:
                states = self._get_output_comp_states(out_id)
                output_states.append(states[0] if states else 0)

            table.append((input_states, output_states))

        return table

    def _set_inp_comp_state(self, comp_id: UUID, idx: int, state: int):
        comp = self._comps.get(comp_id)
        if not comp:
            return

        logic_state = LogicState.HIGH if state == 1 else LogicState.LOW

        bessplug.cmds.set_inp_state(comp_id, idx, logic_state)

    def _get_output_comp_states(self, comp_id: UUID) -> list[int]:
        comp = self._comps.get(comp_id)
        if not comp:
            return []

        comp = typing.cast(DigSimComp, comp)

        states = bessplug.cmds.get_inp_states(comp_id).result

        return [1 if s == LogicState.HIGH else 0 for s in states]
