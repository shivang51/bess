from bessplug.api.common import vec2
from bessplug.api.common import time
from bessplug.api.sim_engine import SlotsGroupInfo, OperatorInfo
from bessplug.api.scene.renderer import Path
from bessplug.api.scene import SchematicDiagram
import math

from bessplug.api.sim_engine.driver import CompDef, DigCompDef

_gates = {
    "BUF": {
        "name": "Buffer Gate",
        "input_pins": ["A"],
        "output_pins": ["Y"],
        "op": "$",
        "keep_io_count_eq": True,
    },
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
        "keep_io_count_eq": True,
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
Q {cx + r},{cy + r * s} {cx + r * m},{cy + r * m}
Q {cx + r * s},{cy + r} {cx},{cy + r}
Q {cx - r * s},{cy + r} {cx - r * m},{cy + r * m}
Q {cx - r},{cy + r * s} {cx - r},{cy}
Q {cx - r},{cy - r * s} {cx - r * m},{cy - r * m}
Q {cx - r * s},{cy - r} {cx},{cy - r}
Q {cx + r * s},{cy - r} {cx + r * m},{cy - r * m}
Q {cx + r},{cy - r * s} {cx + r},{cy}
""".strip()


def _init_paths():
    circle = Path.from_svg_str(quadratic_circle_path(cx=4, cy=4, r=4))
    circle.set_pos(vec2(92, 46))

    # AND Gate
    andPath = Path()
    andPath.move_to(0, 0)
    andPath.line_to(70, 0)
    andPath.quad_to(130, 50, 70, 100)
    andPath.line_to(0, 100)
    andDiagram = SchematicDiagram()
    andDiagram.add_path(andPath)
    andDiagram.show_name = False
    andDiagram.size = vec2(100, 100)

    # NAND Gate
    nandDiagram = SchematicDiagram()
    nandDiagram.show_name = False
    nandPath = Path()
    nandPath.move_to(0, 0)
    nandPath.line_to(62, 0)
    nandPath.quad_to(122, 50, 62, 100)
    nandPath.line_to(0, 100)
    nandDiagram.add_path(nandPath)
    nandDiagram.add_path(circle.copy())
    nandDiagram.size = vec2(100, 100)

    # OR Gate
    orDiagram = SchematicDiagram()
    orDiagram.show_name = False
    orPath = Path()
    orPath.move_to(0, 0)
    orPath.line_to(70, 0)
    orPath.quad_to(130, 50, 70, 100)
    orPath.line_to(0, 100)
    orPath.quad_to(30, 50, 0, 0)
    orDiagram.add_path(orPath)
    orDiagram.size = vec2(100, 100)

    # NOR Gate
    norDiagram = SchematicDiagram()
    norDiagram.show_name = False
    norPath = Path()
    norPath.move_to(0, 0)
    norPath.line_to(62, 0)
    norPath.quad_to(122, 50, 62, 100)
    norPath.line_to(0, 100)
    norPath.quad_to(30, 50, 0, 0)
    norDiagram.add_path(norPath)
    norDiagram.add_path(circle.copy())
    norDiagram.size = vec2(100, 100)

    # XOR
    xorDiagram = SchematicDiagram()
    xorDiagram.show_name = False

    # arc
    xorArcPath = Path()
    xorArcPath.move_to(0, 0)
    xorArcPath.quad_to(30, 50, 0, 100)

    # similar to or gate path. just shifted
    xorPath = Path()
    xorPath.move_to(0, 0)
    xorPath.line_to(60, 0)
    xorPath.quad_to(120, 50, 60, 100)
    xorPath.line_to(0, 100)
    xorPath.quad_to(30, 50, 0, 0)
    xorDiagram.add_path(xorArcPath.copy())
    xorDiagram.add_path(xorPath)
    xorDiagram.size = vec2(100, 100)

    # XNOR
    xnorDiagram = SchematicDiagram()
    xnorDiagram.show_name = False

    # similar to or gate path. just shifted
    xnorPath = Path()
    xnorPath.move_to(0, 0)
    xnorPath.line_to(52, 0)
    xnorPath.quad_to(112, 50, 52, 100)
    xnorPath.line_to(0, 100)
    xnorPath.quad_to(30, 50, 0, 0)

    xnorDiagram.add_path(xorArcPath.copy())
    xnorDiagram.add_path(xnorPath)
    xnorDiagram.add_path(circle.copy())
    xnorDiagram.size = vec2(100, 100)

    # BUF
    bufDiagram = SchematicDiagram()
    bufDiagram.show_name = False
    bufPath = Path()
    bufPath.move_to(0, 0)
    bufPath.line_to(100, 50)
    bufPath.line_to(0, 100)
    bufDiagram.add_path(bufPath)
    bufDiagram.size = vec2(100, 100)

    return {
        "AND": andDiagram,
        "NAND": nandDiagram,
        "OR": orDiagram,
        "NOR": norDiagram,
        "XOR": xorDiagram,
        "XNOR": xnorDiagram,
        "BUF": bufDiagram,
    }


_paths = _init_paths()


digital_gates: list[CompDef] = []
schematic_diagrams: dict[str, SchematicDiagram] = {}

for gate_key, gate_data in _gates.items():
    input_slots_info: SlotsGroupInfo = SlotsGroupInfo()
    input_slots_info.count = len(gate_data["input_pins"])
    input_slots_info.is_resizeable = True

    output_slots_info: SlotsGroupInfo = SlotsGroupInfo()
    output_slots_info.count = len(gate_data["output_pins"])

    opInfo = OperatorInfo()
    opInfo.op = gate_data["op"]
    opInfo.should_negate_output = gate_data.get("negate_output", False)

    def_gate = DigCompDef.from_operator(
        name=gate_data["name"],
        group_name="Digital Gates",
        inputs=input_slots_info,
        outputs=output_slots_info,
        prop_delay=time.TimeNS(2),
        op_info=opInfo,
    )
    def_gate.keep_io_count_eq = gate_data.get("keep_io_count_eq", False)
    digital_gates.append(def_gate)

    if _paths.get(gate_key) is None:
        continue

    dig = _paths[gate_key]
    dig.normalize_paths()
    dig.stroke_size = 1.0
    schematic_diagrams[def_gate.name] = dig


__all__ = ["digital_gates", "schematic_diagrams"]
