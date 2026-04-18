from bessplug.api.sim_engine import (
    AnalogComponentTrait,
    AnalogStampContext,
    ComponentDefinition,
    SlotsGroupType,
)

from .common import SingleTerminalAnalogComponent, TwoTerminalAnalogComponent, make_slots_info


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


analog_test_point_def = ComponentDefinition()
analog_test_point_def.name = "Analog Test Point"
analog_test_point_def.group_name = "Analog"
analog_test_point_def.input_slots_info = make_slots_info(SlotsGroupType.INPUT, 1, ["TP"])
analog_test_point_def.output_slots_info = make_slots_info(SlotsGroupType.OUTPUT, 0, [])
analog_test_point_def.set_analog_trait(
    AnalogComponentTrait(
        1,
        ["TP"],
        lambda: AnalogTestPointComponent("TP"),
    )
)

differential_voltage_probe_def = ComponentDefinition()
differential_voltage_probe_def.name = "Differential Voltage Probe"
differential_voltage_probe_def.group_name = "Analog"
differential_voltage_probe_def.input_slots_info = make_slots_info(
    SlotsGroupType.INPUT,
    1,
    ["+"],
)
differential_voltage_probe_def.output_slots_info = make_slots_info(
    SlotsGroupType.OUTPUT,
    1,
    ["-"],
)
differential_voltage_probe_def.set_analog_trait(
    AnalogComponentTrait(
        2,
        ["+", "-"],
        lambda: VoltageProbeComponent("VProbe"),
    )
)

inline_current_probe_def = ComponentDefinition()
inline_current_probe_def.name = "Inline Current Probe"
inline_current_probe_def.group_name = "Analog"
inline_current_probe_def.input_slots_info = make_slots_info(SlotsGroupType.INPUT, 1, ["IN"])
inline_current_probe_def.output_slots_info = make_slots_info(SlotsGroupType.OUTPUT, 1, ["OUT"])
inline_current_probe_def.set_analog_trait(
    AnalogComponentTrait(
        2,
        ["IN", "OUT"],
        lambda: CurrentProbeComponent("IProbe"),
    )
)


__all__ = [
    "analog_test_point_def",
    "differential_voltage_probe_def",
    "inline_current_probe_def",
]
