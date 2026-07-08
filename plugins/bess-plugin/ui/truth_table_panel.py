import typing
import time
import bessplug
import bessplug.api.bess_ui as bess_ui
import threading
from bessplug.api.common import UUID, vec2
from bessplug.api.sim_engine import (
    ComponentBehaviorType,
    LogicState,
    PortDirection,
    core,
)
from bessplug.api.sim_engine.driver import DigCompDef, DigSimComp, Net


class ProgressInfo:
    def __init__(self):
        self.total_steps = 0
        self.current_step = 0
        self.latest_msg = ""
        self.completed = False

    def update(self, msg: str):
        self.current_step += 1
        self.latest_msg = msg

    def reset(self):
        self.total_steps = 0
        self.current_step = 0
        self.latest_msg = ""
        self.completed = False

    def get_progress(self) -> float:
        if self.total_steps == 0:
            return 0.0
        return self.current_step / self.total_steps


class TruthTablePanel:
    def __init__(self):
        self.name = "Truth Table Panel"
        self.is_open = False
        self._is_first = True

        self._nets: dict[UUID, Net] = {}
        self._selected_net_id: UUID | None = None

        self._comps: dict[UUID, DigSimComp] = {}
        self._inputs: list[UUID] = []
        self._outputs: list[UUID] = []

        self._fetched_comps = False
        self._progress_info: ProgressInfo = ProgressInfo()
        self._gen_thread = None

        self._table: list[tuple[list[int], list[int]]] = []
        self._table_regen = False  # true if table was regnerated and not yet displayed

        self._column_names = []

    def _gen_column_names(self):
        self._column_names = []

        # reverse indexing the names so idx-0 (lsb) is rightmost in the table
        for comp_id in self._inputs:
            comp = self._comps.get(comp_id)
            if not comp:
                continue

            dig_def = typing.cast(DigCompDef, comp.definition)
            for slot_idx in range(dig_def.output_slots_info.count):
                idx = dig_def.output_slots_info.count - 1 - slot_idx
                self._column_names.append(f"INP-{comp.name}-{idx}")

        for comp_id in self._outputs:
            comp = self._comps.get(comp_id)
            if not comp:
                continue

            dig_def = typing.cast(DigCompDef, comp.definition)
            for slot_idx in range(dig_def.input_slots_info.count):
                idx = dig_def.input_slots_info.count - 1 - slot_idx
                self._column_names.append(f"OUT-{comp.name}-{idx}")

    def _rev_states(self):
        # reversing the states so idx-0 (lsb) is rightmost in the table
        for inp_states, out_states in self._table:
            inp_states.reverse()
            out_states.reverse()

    def draw_table(self):
        if not self._table:
            return

        if self._table_regen:
            self._gen_column_names()
            self._rev_states()
            self._table_regen = False

        if not bess_ui.begin_table("TruthTable", len(self._column_names)):
            return

        for col_name in self._column_names:
            bess_ui.table_setup_column(col_name)

        bess_ui.table_headers_row()

        for inp_states, out_states in self._table:
            for state in inp_states:
                bess_ui.table_next_column()
                bess_ui.text(str(state))

            for state in out_states:
                bess_ui.table_next_column()
                bess_ui.text(str(state))

        bess_ui.end_table()

    def draw(self):
        if not self.is_open:
            return

        if self._is_first:
            self._is_first = False
            bess_ui.try_reg_dock(self.name, bess_ui.Dock.bottom)

        self.is_open = bess_ui.begin_panel(self.name, vec2(250, 250), self.is_open)

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

        if self._gen_thread and self._gen_thread.is_alive():
            if not self._progress_info.completed:
                bess_ui.text(
                    f"Progress: {self._progress_info.get_progress() * 100:.2f}%"
                )
                bess_ui.same_line()
                bess_ui.separator(True)
                bess_ui.same_line()
                bess_ui.text(self._progress_info.latest_msg)
        elif bess_ui.button("Calculate Table"):
            self._fetch_comps()
            self._gen_thread = threading.Thread(
                target=self._gen_truth_table, name="TruthTableGenThread", daemon=True
            )
            self._gen_thread.start()

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

        if self._progress_info.completed:
            self.draw_table()

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

        self._table = []
        table = []

        num_inputs = 0

        for comp_id in self._inputs:
            comp = self._comps.get(comp_id)
            if not comp:
                continue

            dig_def = typing.cast(DigCompDef, comp.definition)
            num_inputs += dig_def.output_slots_info.count

        num_rows = 2**num_inputs

        self._progress_info.reset()
        self._progress_info.total_steps = num_rows

        for i in range(num_rows):
            core.pause()

            input_states = [(i >> bit) & 1 for bit in range(num_inputs)]
            idx = 0

            for comp_id in self._inputs:
                comp = self._comps.get(comp_id)
                if not comp:
                    continue
                dig_def = typing.cast(DigCompDef, comp.definition)
                for slot_idx in range(dig_def.output_slots_info.count):
                    self._set_inp_comp_state(comp_id, slot_idx, input_states[idx])
                    idx += 1

            self._progress_info.update(f"Running for row {i+1} of {num_rows}")

            core.resume()

            while not core.is_sim_stable():
                self._progress_info.latest_msg = (
                    f"Waiting for sim to stabilize for row {i+1} of {num_rows}..."
                )
                time.sleep(0.01)

            output_states = []
            for out_id in self._outputs:
                states = self._get_output_comp_states(out_id)
                output_states = output_states + states

            table.append((input_states, output_states))
            self._progress_info.latest_msg = f"Completed row {i+1} of {num_rows}"

        self._progress_info.completed = True
        self._table = table
        self._table_regen = True

    def _set_inp_comp_state(self, comp_id: UUID, idx: int, state: int):
        comp = self._comps.get(comp_id)
        if not comp:
            return

        logic_state = LogicState.HIGH if state == 1 else LogicState.LOW

        core.set_out_slot_state(comp_id, idx, logic_state)

    def _get_output_comp_states(self, comp_id: UUID) -> list[int]:
        states = core.get_inp_slots_states(comp_id)

        return [1 if s.state == LogicState.HIGH else 0 for s in states]

    def _create_basic_io_circuit(self):
        inp = bessplug.cmds.add("Input")
        out = bessplug.cmds.add("Output")

        bessplug.cmds.connect(
            inp.result, PortDirection.OUTPUT, 0, out.result, PortDirection.INPUT, 0
        )

        bessplug.cmds.org_comps()
