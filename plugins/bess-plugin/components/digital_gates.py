from bessplug.api.common.math import Vec2
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
""".strip()


def _init_paths():
    circle = Path.from_svg_str(quadratic_circle_path(cx=4, cy=4, r=4))
    circle.set_bounds(Vec2(8, 8))
    circle.set_lowest_pos(Vec2(92, 46))
    circle.properties.render_fill = True

    andPath = Path()
    andPath.move_to(0, 0)
    andPath.line_to(70, 0)
    andPath.quad_to(130, 50, 70, 100)
    andPath.line_to(0, 100)
    andPath.properties.is_closed = True
    andPath.properties.render_fill = True
    andPath.calc_set_bounds()
    andPath.set_bounds(Vec2(100, 100))

    andDiagram = SchematicDiagram()
    andDiagram.add_path(andPath)
    andDiagram.show_name = False
    andDiagram.size = (100, 100)

    nandDiagram = SchematicDiagram()
    nandDiagram.show_name = False
    nandPath = Path()
    nandPath.move_to(0, 0)
    nandPath.line_to(62, 0)
    nandPath.quad_to(122, 50, 62, 100)
    nandPath.line_to(0, 100)
    nandPath.calc_set_bounds()
    nandPath.set_bounds(Vec2(92, 100))
    nandPath.set_lowest_pos(Vec2(0, 0))
    nandPath.properties.is_closed = True
    nandPath.properties.render_fill = True
    nandDiagram.add_path(nandPath)
    nandDiagram.add_path(circle.copy())
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

    if gate_key == "AND":
        def_gate.set_alt_input_counts([2, 3, 4])

    if gate_key in _paths:
        schematic_symbols[def_gate.get_hash()] = _paths[gate_key]
        print(
            f"Assigned schematic symbol for {gate_key} gate with hash {def_gate.get_hash()}."
        )


__all__ = ["digital_gates", "schematic_symbols"]
