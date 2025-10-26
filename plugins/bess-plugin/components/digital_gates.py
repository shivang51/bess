from bessplug.api.sim_engine import ComponentDefinition
from bessplug.api.renderer.path import Path, PathProperties


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


def _init_paths():
    andPath = Path()
    andPath.move_to(0, 0)
    andPath.line_to(100, 0)
    andPath.quad_to(125, 100, 100, 200)
    andPath.line_to(0, 200)
    andPath.normalize()

    props = PathProperties()
    props.is_closed = True
    props.render_fill = True
    andPath.set_path_properties(props)

    return {"andPath": andPath}


_paths = _init_paths()

digital_gates: list[ComponentDefinition] = []
schematic_symbols: dict[int, Path] = {}

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
        schematic_symbols[def_gate.get_hash()] = _paths["andPath"]


__all__ = ["digital_gates", "schematic_symbols"]
