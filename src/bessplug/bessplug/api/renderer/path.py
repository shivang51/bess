from bessplug.bindings._bindings.renderer import Path as NativePath
from typing import List, Tuple, Union
from bessplug.bindings._bindings.renderer import Path, PathCommand, PathCommandKind, MoveTo, LineTo, QuadTo, CubicTo
from bessplug.bindings._bindings import UUID

Vec2 = Tuple[float, float]

class Path:
    """
    Pythonic wrapper for the native C++ Path class.
    """

    def __init__(self, native: NativePath | None = None):
        self._native = native or NativePath()

    # ---------------------------------------------------------------------
    # --- High-level API for constructing paths
    # ---------------------------------------------------------------------

    def move_to(self, pos: Vec2) -> "Path":
        """Move current position to (x, y)."""
        self._native.move_to(_vec2(pos))
        return self

    def line_to(self, pos: Vec2) -> "Path":
        """Draw a line from current position to (x, y)."""
        self._native.line_to(_vec2(pos))
        return self

    def quad_to(self, c: Vec2, pos: Vec2) -> "Path":
        """Draw a quadratic curve using control point c."""
        self._native.quad_to(_vec2(c), _vec2(pos))
        return self

    def cubic_to(self, c1: Vec2, c2: Vec2, pos: Vec2) -> "Path":
        """Draw a cubic Bezier curve."""
        self._native.cubic_to(_vec2(c1), _vec2(c2), _vec2(pos))
        return self

    # ---------------------------------------------------------------------
    # --- Command management
    # ---------------------------------------------------------------------

    def add_command(self, cmd: Union[PathCommand, dict]) -> "Path":
        """Add a PathCommand or dictionary-based command."""
        if isinstance(cmd, dict):
            cmd = self._dict_to_command(cmd)
        if not isinstance(cmd, PathCommand):
            raise TypeError("cmd must be PathCommand or dict")
        self._native.add_command(cmd)
        return self

    def get_commands(self) -> List[PathCommand]:
        """Return the list of underlying path commands."""
        return list(self._native.get_commands())

    def set_commands(self, cmds: List[Union[PathCommand, dict]]):
        """Replace path commands."""
        native_cmds = [
            self._dict_to_command(c) if isinstance(c, dict) else c
            for c in cmds
        ]
        self._native.set_commands(native_cmds)

    # ---------------------------------------------------------------------
    # --- Contours and UUID access
    # ---------------------------------------------------------------------

    @property
    def contours(self):
        """Return computed contours (list of list of PathPoints)."""
        return self._native.get_contours()

    @property
    def uuid(self) -> UUID:
        return self._native.uuid

    # ---------------------------------------------------------------------
    # --- Serialization helpers
    # ---------------------------------------------------------------------

    def to_dict(self) -> dict:
        """Serialize path into a JSON-serializable dict."""
        return {
            "uuid": int(self._native.uuid),
            "commands": [self._command_to_dict(c) for c in self.get_commands()],
        }

    @classmethod
    def from_dict(cls, data: dict) -> "Path":
        """Reconstruct path from a serialized dict."""
        path = cls()
        cmds = [cls._dict_to_command(c) for c in data.get("commands", [])]
        path._native.set_commands(cmds)
        return path

    # ---------------------------------------------------------------------
    # --- Internal helpers
    # ---------------------------------------------------------------------

    @staticmethod
    def _dict_to_command(data: dict) -> PathCommand:
        """Convert dictionary to a C++ PathCommand."""
        kind_str = data.get("kind")
        if kind_str is None:
            raise ValueError("PathCommand dict missing 'kind'")
        kind = PathCommandKind[kind_str]

        cmd = PathCommand()
        cmd.kind = kind
        cmd.z = float(data.get("z", 0.0))
        cmd.weight = float(data.get("weight", 1.0))
        cmd.id = int(data.get("id", -1))

        match kind:
            case PathCommandKind.Move:
                cmd.move.p = _vec2(data["p"])
            case PathCommandKind.Line:
                cmd.line.p = _vec2(data["p"])
            case PathCommandKind.Quad:
                cmd.quad.c = _vec2(data["c"])
                cmd.quad.p = _vec2(data["p"])
            case PathCommandKind.Cubic:
                cmd.cubic.c1 = _vec2(data["c1"])
                cmd.cubic.c2 = _vec2(data["c2"])
                cmd.cubic.p = _vec2(data["p"])
        return cmd

    @staticmethod
    def _command_to_dict(cmd: PathCommand) -> dict:
        """Convert a C++ PathCommand to dict."""
        base = {
            "kind": cmd.kind.name,
            "z": cmd.z,
            "weight": cmd.weight,
            "id": cmd.id,
        }

        match cmd.kind:
            case PathCommandKind.Move:
                base["p"] = tuple(cmd.move.p)
            case PathCommandKind.Line:
                base["p"] = tuple(cmd.line.p)
            case PathCommandKind.Quad:
                base["c"] = tuple(cmd.quad.c)
                base["p"] = tuple(cmd.quad.p)
            case PathCommandKind.Cubic:
                base["c1"] = tuple(cmd.cubic.c1)
                base["c2"] = tuple(cmd.cubic.c2)
                base["p"] = tuple(cmd.cubic.p)
        return base

    def __repr__(self):
        cmds = self.get_commands()
        return f"<PathWrapper cmds={len(cmds)} uuid={int(self.uuid):016x}>"

    def __str__(self):
        '''
        Returns SVG formated path data string.
        '''
        svg_parts = []
        for cmd in self.get_commands():
            match cmd.kind:
                case PathCommandKind.Move:
                    svg_parts.append(f"M {cmd.move.p.x} {cmd.move.p.y}")
                case PathCommandKind.Line:
                    svg_parts.append(f"L {cmd.line.p.x} {cmd.line.p.y}")
                case PathCommandKind.Quad:
                    svg_parts.append(f"Q {cmd.quad.c.x} {cmd.quad.c.y}, {cmd.quad.p.x} {cmd.quad.p.y}")
                case PathCommandKind.Cubic:
                    svg_parts.append(f"C {cmd.cubic.c1.x} {cmd.cubic.c1.y}, {cmd.cubic.c2.x} {cmd.cubic.c2.y}, {cmd.cubic.p.x} {cmd.cubic.p.y}")
        return " ".join(svg_parts)

# ---------------------------------------------------------------------
# Utility: convert Python tuple to glm::vec2
# ---------------------------------------------------------------------

def _vec2(v: Vec2):
    from bessplug.bindings._bindings import vec2
    return vec2(float(v[0]), float(v[1]))


__all__ = ["Path", "Vec2"]
