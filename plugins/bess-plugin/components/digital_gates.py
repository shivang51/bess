from bessplug.api.sim_engine import ComponentDefinition
from bessplug.api.renderer.path import Path, PathProperties
import math

from bessplug.plugin import SchematicDiagram


_gates = {
    "AND": {
        "name": "AND Gate",
        "input_pins": ["A", "B"],
        "output_pins": ["Y"],
        "op": "*",
    },
    "OR": {
        "name": "OR Gate",
        "input_pins": ["A", "B"],
        "output_pins": ["Y"],
        "op": "+",
    },
    "NOT": {
        "name": "NOT Gate",
        "input_pins": ["A"],
        "output_pins": ["Y"],
        "op": "!",
    },
    "NAND": {
        "name": "NAND Gate",
        "input_pins": ["A", "B"],
        "output_pins": ["Y"],
        "op": "*",
        "negate_output": True,
    },
    "NOR": {
        "name": "NOR Gate",
        "input_pins": ["A", "B"],
        "output_pins": ["Y"],
        "op": "+",
        "negate_output": True,
    },
    "XOR": {
        "name": "XOR Gate",
        "input_pins": ["A", "B"],
        "output_pins": ["Y"],
        "op": "^",
    },
    "XNOR": {
        "name": "XNOR Gate",
        "input_pins": ["A", "B"],
        "output_pins": ["Y"],
        "op": "^",
        "negate_output": True,
    },
}


def quadratic_circle_path(cx=0.0, cy=0.0, r=50.0):
    m = math.sqrt(2) / 2
    s = math.sqrt(2) - 1

    return f"""
M {cx + r},{cy}
Q {cx + r},{cy + r*s} {cx + r*m},{cy + r*m}
Q {cx + r*s},{cy + r} {cx},{cy + r}
Q {cx - r*s},{cy + r} {cx - r*m},{cy + r*m}
Q {cx - r},{cy + r*s} {cx - r},{cy}
Q {cx - r},{cy - r*s} {cx - r*m},{cy - r*m}
Q {cx - r*s},{cy - r} {cx},{cy - r}
Q {cx + r*s},{cy - r} {cx + r*m},{cy - r*m}
Q {cx + r},{cy - r*s} {cx + r},{cy}
Z
""".strip()


def _init_paths():
    circle = Path.from_svg_str(quadratic_circle_path(cx=104, cy=100, r=4))
    andPath = Path()
    andPath.move_to(0, 0)
    andPath.line_to(70, 0)
    andPath.quad_to(130, 50, 70, 100)
    andPath.line_to(0, 100)
    props = andPath.get_path_properties()
    props.is_closed = True
    props.render_fill = True

    andDiagram = SchematicDiagram()
    andDiagram.add_path(andPath)
    andDiagram.size = (100, 100)

    nandDiagram = SchematicDiagram()
    nandDiagram.add_path(andPath)
    nandDiagram.add_path(circle)
    nandDiagram.size = (100, 100)

    return {
        "AND": andDiagram,
        "NAND": nandDiagram,
    }


_paths = _init_paths()

digital_gates: list[ComponentDefinition] = []
schematic_symbols: dict[int, SchematicDiagram] = {}

for gate_key, gate_data in _gates.items():
    def_gate = ComponentDefinition.from_operator(
        name=gate_data["name"],
        category="Py Digital Gates",
        input_count=len(gate_data["input_pins"]),
        output_count=len(gate_data["output_pins"]),
        delay_ns=1,
        op=gate_data["op"],
    )
    def_gate.negate = gate_data.get("negate_output", False)
    digital_gates.append(def_gate)

    if gate_key in _paths:
        schematic_symbols[def_gate.get_hash()] = _paths[gate_key]


__all__ = ["digital_gates", "schematic_symbols"]
