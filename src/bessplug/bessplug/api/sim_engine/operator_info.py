from bessplug.bindings._bindings.sim_engine import (
    OperatorInfo as NativeOperatorInfo,
)


class OperatorInfo:
    """
    Provides Pythonic accessors for operator metadata used by the simulation engine
    and authoring tools:
    - Name, category, timing (delay/setup/hold)
    - Input/output counts and characteristics
    - Stable content hash for equality/caching
    """

    def __init__(self, native: NativeOperatorInfo = None):
        """Wrap an existing native OperatorInfo."""
        self._native: NativeOperatorInfo = native or NativeOperatorInfo()

    @property
    def operator_symbol(self) -> str:
        return self._native.op

    @operator_symbol.setter
    def operator_symbol(self, value: str) -> None:
        self._native.op = value

    @property
    def should_negate_output(self) -> bool:
        return self._native.negate_output

    @should_negate_output.setter
    def should_negate_output(self, value: bool) -> None:
        self._native.negate_output = value


__all__ = ["OperatorInfo"]
