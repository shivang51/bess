from bessplug.api.sim_engine import (
    AnalogComponentDefinition,
    AnalogStampContext,
)

from .common import SingleTerminalAnalogComponent, TwoTerminalAnalogComponent


class AnalogTestPointComponent(SingleTerminalAnalogComponent):
    def __init__(self, comp_name: str = "TP"):
        super().__init__(comp_name)

    def stamp(self, context: AnalogStampContext) -> None:
        del context


class VoltageProbeComponent(TwoTerminalAnalogComponent):
    def __init__(self, comp_name: str = "VProbe"):
        super().__init__(comp_name)

    def stamp(self, context: AnalogStampContext) -> None:
        del context


class CurrentProbeComponent(TwoTerminalAnalogComponent):
    def __init__(self, comp_name: str = "IProbe"):
        super().__init__(comp_name)

    def stamp(self, context: AnalogStampContext) -> None:
        if not self._connected() or self._shorted():
            return

        branch_name = f"{self.name()}:{int(self.get_uuid())}"
        context.add_voltage_source(self._terminals[0], self._terminals[1], 0.0, branch_name)

    def voltage_source_count(self) -> int:
        return 1 if self._connected() and not self._shorted() else 0


analog_test_point_def = AnalogComponentDefinition(
    1,
    ["TP"],
    lambda: AnalogTestPointComponent("TP"),
)
analog_test_point_def.name = "Analog Test Point"
analog_test_point_def.group_name = "Analog"

differential_voltage_probe_def = AnalogComponentDefinition(
    2,
    ["+", "-"],
    lambda: VoltageProbeComponent("VProbe"),
)
differential_voltage_probe_def.name = "Differential Voltage Probe"
differential_voltage_probe_def.group_name = "Analog"

inline_current_probe_def = AnalogComponentDefinition(
    2,
    ["IN", "OUT"],
    lambda: CurrentProbeComponent("IProbe"),
)
inline_current_probe_def.name = "Inline Current Probe"
inline_current_probe_def.group_name = "Analog"


__all__ = [
    "analog_test_point_def",
    "differential_voltage_probe_def",
    "inline_current_probe_def",
]
