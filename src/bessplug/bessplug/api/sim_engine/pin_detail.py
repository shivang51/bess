from bessplug.bindings import _bindings as _b
from bessplug.bindings._bindings.sim_engine import PinDetail as NativePinDetail
from .enums import PinType, ExtendedPinType

class PinDetail:
    """Pin detail metadata.

    Wraps the native `PinDetail` and exposes:
    - type: Pin type (input/output)
    - name: Pin name
    - extended_type: Extended pin type information
    """

    
    @staticmethod
    def from_fields(
        pinType: PinType,
        name: str,
        extended_type: ExtendedPinType
    ) -> "PinDetail":
        """Create a PinDetail from fields.

        - type: Pin type (input/output)
        - name: Pin name
        - extended_type: Extended pin type information
        """
        pin_detail = PinDetail()
        pin_detail.type = pinType
        pin_detail.name = name
        pin_detail.extended_type = extended_type
        return pin_detail

    @staticmethod
    def for_input_pin(
        name: str,
        extended_type: ExtendedPinType = ExtendedPinType.NONE
    ) -> "PinDetail":
        """Create a PinDetail from fields.

        - type: Pin type (input/output)
        - name: Pin name
        - extended_type: Extended pin type information
        """
        pin_detail = PinDetail()
        pin_detail.type = PinType.INPUT
        pin_detail.name = name
        pin_detail.extended_type = extended_type
        return pin_detail

    @staticmethod
    def for_output_pin(
        name: str,
        extended_type: ExtendedPinType = ExtendedPinType.NONE
    ) -> "PinDetail":
        """Create a PinDetail from fields.

        - type: Pin type (input/output)
        - name: Pin name
        - extended_type: Extended pin type information
        """
        pin_detail = PinDetail()
        pin_detail.type = PinType.OUTPUT
        pin_detail.name = name
        pin_detail.extended_type = extended_type
        return pin_detail

    def __init__(self, native: NativePinDetail | None = None) -> None:
        """Create a PinDetail.

        - native: Optional native instance to wrap
        """
        if native is None:
            self._native: NativePinDetail = _b.sim_engine.PinDetail()
        elif isinstance(native, PinDetail):
            self._native = native._native
        else:
            self._native = native

    @property
    def type(self) -> PinType:
        """Pin type (input/output)."""
        return self._native.type

    @type.setter
    def type(self, value: PinType) -> None:
        """Set the pin type (input/output)."""
        self._native.type = value.value

    @property
    def name(self) -> str:
        """Pin name."""
        return self._native.name

    @name.setter
    def name(self, value: str) -> None:
        """Set the pin name."""
        self._native.name = value

    @property
    def extended_type(self) -> ExtendedPinType:
        """Extended pin type information."""
        return self._native.extended_type

    @extended_type.setter
    def extended_type(self, value: ExtendedPinType) -> None:
        """Set the extended pin type information."""
        self._native.extended_type = value.value


__all__ = ["PinDetail"]
