import bessplug.api.bess_ui as bess_ui
from bessplug.api.common import UUID, vec2
from bessplug.api.sim_engine import core
import bessplug
from bessplug.api.sim_engine.driver import Net


class TruthTablePanel:
    def __init__(self):
        self.name = "Truth Table Panel"
        self._is_open = True
        self._is_first = True

        self._nets: dict[UUID, Net] = {}
        self._selected_net_id: UUID | None = None
        self._net_names = []

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

        bess_ui.end_panel()
