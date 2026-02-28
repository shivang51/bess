from __future__ import annotations
import bessplug.api.common
__all__: list[str] = ['SchematicThemeColors', 'schematic']
class SchematicThemeColors:
    activeSignal: bessplug.api.common.vec4
    componentFill: bessplug.api.common.vec4
    componentStroke: bessplug.api.common.vec4
    connection: bessplug.api.common.vec4
    pin: bessplug.api.common.vec4
    text: bessplug.api.common.vec4
    def __init__(self) -> None:
        ...
schematic: SchematicThemeColors  # value = <bessplug.api.common.theme.SchematicThemeColors object>
