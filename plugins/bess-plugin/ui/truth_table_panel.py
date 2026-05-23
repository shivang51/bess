import typing
from bessplug.api import sim_engine
import bessplug.api.bess_ui as bess_ui
from bessplug.api.common import UUID, vec2
from bessplug.api.sim_engine import ComponentBehaviorType, core
import bessplug
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
