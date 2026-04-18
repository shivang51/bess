from __future__ import annotations

from bessplug.api.sim_engine import (
    ANALOG_UNCONNECTED_NODE,
    AnalogComponent,
    SlotsGroupInfo,
    SlotsGroupType,
)


def make_slots_info(
    group_type: SlotsGroupType,
    count: int,
    names: list[str],
) -> SlotsGroupInfo:
    info = SlotsGroupInfo()
    info.type = group_type
    info.is_resizeable = False
    info.count = count
    info.names = names
    return info


class TwoTerminalAnalogComponent(AnalogComponent):
    def __init__(self, comp_name: str):
        super().__init__()
        self._comp_name = comp_name
        self._terminals = [int(ANALOG_UNCONNECTED_NODE), int(ANALOG_UNCONNECTED_NODE)]

    def terminals(self) -> list[int]:
        return [self._terminals[0], self._terminals[1]]

    def set_terminal_node(self, terminal_idx: int, node: int) -> bool:
        if terminal_idx < 0 or terminal_idx >= len(self._terminals):
            return False

        self._terminals[terminal_idx] = int(node)
        return True

    def name(self) -> str:
        return self._comp_name

    def _connected(self) -> bool:
        return (
            self._terminals[0] != int(ANALOG_UNCONNECTED_NODE)
            and self._terminals[1] != int(ANALOG_UNCONNECTED_NODE)
        )

    def _shorted(self) -> bool:
        return self._terminals[0] == self._terminals[1]


class SingleTerminalAnalogComponent(AnalogComponent):
    def __init__(self, comp_name: str):
        super().__init__()
        self._comp_name = comp_name
        self._terminal = int(ANALOG_UNCONNECTED_NODE)

    def terminals(self) -> list[int]:
        return [self._terminal]

    def set_terminal_node(self, terminal_idx: int, node: int) -> bool:
        if terminal_idx != 0:
            return False

        self._terminal = int(node)
        return True

    def name(self) -> str:
        return self._comp_name

    def _connected(self) -> bool:
        return self._terminal != int(ANALOG_UNCONNECTED_NODE)


__all__ = [
    "make_slots_info",
    "SingleTerminalAnalogComponent",
    "TwoTerminalAnalogComponent",
]
