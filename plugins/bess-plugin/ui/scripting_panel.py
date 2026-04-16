import bessplug.api.bess_ui as bess_ui
from bessplug.api.common import vec2
import bessplug


class ScriptingPanel:
    def __init__(self):
        self.name = "Scripting Panel"
        self._is_open = True
        self._cmd_str = 'bessplug.cmds.set_inp_state("INP_0", 0, LogicState.HIGH)'
        self._cmd_res: bessplug.cmds.CmdResult | None = None

        self._script_str = ""

    def draw(self):
        if not self._is_open:
            return

        self._is_open = bess_ui.begin_panel(self.name, vec2(250, 250), self._is_open)

        self._draw_single_line_cmd()
        self._draw_script_editor()

        bess_ui.end_panel()

    def _draw_script_editor(self):
        clicked = False
        status = bessplug.cmds.get_async_script_status()
        if status.is_running:
            bess_ui.text("Script is running...")
        else:
            clicked = bess_ui.button("Run Script")

        [changed, val] = bess_ui.input_text_multiline("Script Editor", self._script_str)

        if changed:
            self._script_str = val

        if clicked:
            print(self._script_str)
            bessplug.cmds.exec_script_async(self._script_str)

    def _draw_single_line_cmd(self):
        [changed, val] = bess_ui.input_text("Cmd", self._cmd_str)

        if changed:
            self._cmd_str = val

        bess_ui.same_line()

        if bess_ui.button("Execute"):
            self._cmd_res = bessplug.cmds.exec(self._cmd_str)
            print("Res ", self._cmd_res)

        bess_ui.text(str(self._cmd_res))
        pass
