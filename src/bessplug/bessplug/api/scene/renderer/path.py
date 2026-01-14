from bessplug.bindings._bindings.scene.renderer import Path as NativePath
from typing import List, Union
from bessplug.bindings._bindings.scene.renderer import (
    Path,
    PathCommand,
    PathCommandKind,
    PathProperties as NativePathProperties,
)
from bessplug.bindings._bindings import UUID
from bessplug.api.common.math import Vec2


class PathProperties:
    """
    Wrapper for path rendering properties.
    Attributes:
        - render_stroke (bool): Whether to render the stroke.
        - render_fill (bool): Whether to render the fill.
        - is_closed (bool): Whether the path is closed.
        - rounded_joints (bool): Whether to use rounded joints.
    """

    def __init__(self, native: NativePathProperties | None = None):
        self._native = native or NativePathProperties()

    @property
    def render_stroke(self) -> bool:
        return self._native.render_stroke

    @render_stroke.setter
    def render_stroke(self, value: bool):
        self._native.render_stroke = value

    @property
    def render_fill(self) -> bool:
        return self._native.render_fill

    @render_fill.setter
    def render_fill(self, value: bool):
        self._native.render_fill = value

    @property
    def is_closed(self) -> bool:
        return self._native.is_closed

    @is_closed.setter
    def is_closed(self, value: bool):
        self._native.is_closed = value

    @property
    def rounded_joints(self) -> bool:
        return self._native.rounded_joints

    @rounded_joints.setter
    def rounded_joints(self, value: bool):
        self._native.rounded_joints = value


class Path:
    """
    Pythonic wrapper for the native C++ Path class.
    Available Commands:
        - move_to(pos: Vec2)
        - line_to(pos: Vec2)
        - quad_to(c: Vec2, pos: Vec2)
        - cubic_to(c1: Vec2, c2: Vec2, pos: Vec2)
    Path Commands can also be added directly using add_command().
    Path properties can be accessed and modified via get_path_properties() and set_path_properties().
    """

    def __init__(self, native: NativePath | None = None):
        self._native = native or NativePath()

    # ---------------------------------------------------------------------
    # --- High-level API for constructing paths
    # ---------------------------------------------------------------------

    def move_to_vec(self, pos: Vec2) -> "Path":
        """Move current position to Vec2Vec2(x, y)."""
        self._native.move_to(pos.native)
        return self

    def move_to(self, x: float, y: float) -> "Path":
        """Move current position to Vec2(x, y)."""
        return self.move_to_vec(Vec2(x, y))

    def line_to_vec(self, pos: Vec2) -> "Path":
        """Draw a line from current position to Vec2Vec2(x, y)."""
        self._native.line_to(pos.native)
        return self

    def line_to(self, x: float, y: float) -> "Path":
        """Draw a line from current position to Vec2(x, y)."""
        return self.line_to_vec(Vec2(x, y))

    def quad_to_vec(self, c: Vec2, pos: Vec2) -> "Path":
        """Draw a quadratic curve using control point c."""
        self._native.quad_to(c.native, pos.native)
        return self

    def quad_to(self, cx: float, cy: float, x: float, y: float) -> "Path":
        """Draw a quadratic curve using control point Vec2(cx, cy)."""
        return self.quad_to_vec(Vec2(cx, cy), Vec2(x, y))

    def cubic_to_vec(self, c1: Vec2, c2: Vec2, pos: Vec2) -> "Path":
        """Draw a cubic Bezier curve."""
        self._native.cubic_to(c1.native, c2.native, pos.native)
        return self

    def cubic_to(
        self, c1x: float, c1y: float, c2x: float, c2y: float, x: float, y: float
    ) -> "Path":
        """Draw a cubic Bezier curve."""
        return self.cubic_to_vec(Vec2(c1x, c1y), Vec2(c2x, c2y), Vec2(x, y))

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

    def add_commands(self, cmds):
        """Return the list of underlying path commands."""
        cmds_ = self.get_commands()
        native_cmds = [
            self._dict_to_command(c) if isinstance(c, dict) else c for c in cmds
        ]

        cmds_.extend(native_cmds)
        self._native.set_commands(cmds_)

    def get_commands(self) -> List[PathCommand]:
        """Return the list of underlying path commands."""
        return list(self._native.get_commands())

    def set_commands(self, cmds: List[Union[PathCommand, dict]]):
        """Replace path commands."""
        native_cmds = [
            self._dict_to_command(c) if isinstance(c, dict) else c for c in cmds
        ]
        self._native.set_commands(native_cmds)

    def set_path_properties(self, properties: PathProperties) -> "Path":
        """Set path rendering properties."""
        self._native.set_props(properties._native)
        return self

    @property
    def properties(self) -> PathProperties:
        """Get path rendering properties."""
        return self._native.get_props_ref()

    @properties.setter
    def properties(self, properties: PathProperties):
        """Set path rendering properties."""
        self._native.set_props(properties._native)

    def get_path_props_copy(self) -> PathProperties:
        """Get copy of path rendering properties wrapped in PathProperties."""
        return PathProperties(self._native.get_props())

    def scale(self, sx: float, sy: float):
        """Scale path by (sx, sy)."""
        cmds = self.get_commands()
        for cmd in cmds:
            match cmd.kind:
                case PathCommandKind.Move:
                    p = cmd.move.p
                    p.x *= sx
                    p.y *= sy
                case PathCommandKind.Line:
                    p = cmd.line.p
                    p.x *= sx
                    p.y *= sy
                case PathCommandKind.Quad:
                    c = cmd.quad.c
                    p = cmd.quad.p
                    c.x *= sx
                    c.y *= sy
                    p.x *= sx
                    p.y *= sy
                case PathCommandKind.Cubic:
                    c1 = cmd.cubic.c1
                    c2 = cmd.cubic.c2
                    p = cmd.cubic.p
                    c1.x *= sx
                    c1.y *= sy
                    c2.x *= sx
                    c2.y *= sy
                    p.x *= sx
                    p.y *= sy

        self.set_commands(cmds)

    def calc_bounds(self) -> tuple[float, float]:
        cmds = self.get_commands()
        max_w = 0.0
        max_h = 0.0
        pos_x = 0.0
        pos_y = 0.0
        for cmd in cmds:
            match cmd.kind:
                case PathCommandKind.Move:
                    p = cmd.move.p
                    max_w = max(max_w, abs(p.x))
                    max_h = max(max_h, abs(p.y))
                case PathCommandKind.Line:
                    p = cmd.line.p
                    max_w = max(max_w, abs(p.x))
                    max_h = max(max_h, abs(p.y))
                case PathCommandKind.Quad:
                    p = cmd.quad.p
                    max_w = max(max_w, abs(p.x))
                    max_h = max(max_h, abs(p.y))
                case PathCommandKind.Cubic:
                    p = cmd.cubic.p
                    max_w = max(max_w, abs(p.x))
                    max_h = max(max_h, abs(p.y))

        return max_w, max_h

    def normalize(self):
        """Normalize path commands."""
        [max_w, max_h] = self.calc_bounds()
        self.scale(1.0 / max_w if max_w > 0 else 1.0, 1.0 / max_h if max_h > 0 else 1.0)

    def get_bounds(self) -> Vec2:
        """Return (min_x, min_y, max_x, max_y) bounds of the path."""
        return Vec2.from_native(self._native.get_bounds())

    def set_bounds(self, bounds: Vec2):
        """Set (min_x, min_y, max_x, max_y) bounds of the path."""
        self._native.set_bounds(bounds._native)

    def calc_set_bounds(self):
        """Calculate and set bounds of the path."""
        self._native.set_bounds(Vec2(*self.calc_bounds())._native)

    def set_lowest_pos(self, pos: Vec2):
        self._native.set_lowest_pos(pos._native)

    def get_lowest_pos(self) -> Vec2:
        return Vec2.from_native(self._native.get_lowest_pos())

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

    @staticmethod
    def from_svg_str(svg_data: str) -> "Path":
        """
        Parse an SVG path 'd' string and return a Path instance.
        Supports: M/m, Z/z, L/l, H/h, V/v, C/c, S/s, Q/q, T/t, A/a
        Converts arcs to cubic beziers and collapses degenerate cubics to straight lines.
        """
        import re
        import math

        # --- Helpers -------------------------------------------------------
        def _tok_regex():
            # match commands or numbers (supports scientific notation)
            return re.compile(
                r"[MmZzLlHhVvCcSsQqTtAa]|[-+]?(?:\d+\.\d*|\.\d+|\d+)(?:[eE][-+]?\d+)?"
            )

        def _read_number():
            nonlocal i
            if i >= len(tokens):
                raise ValueError("Unexpected end of path data (expected number).")
            tok = tokens[i]
            if re.fullmatch(r"[MmZzLlHhVvCcSsQqTtAa]", tok):
                raise ValueError(f"Unexpected command '{tok}' where number expected.")
            i += 1
            return float(tok)

        def _make_point(x, y, relative):
            if relative:
                return (cur_x + x, cur_y + y)
            else:
                return Vec2(x, y)

        def _dist(ax, ay, bx, by):
            return math.hypot(bx - ax, by - ay)

        def _distance_point_to_line(px, py, ax, ay, bx, by):
            # distance from P to line AB
            vx = bx - ax
            vy = by - ay
            wx = px - ax
            wy = py - ay
            denom = vx * vx + vy * vy
            if denom == 0.0:
                return math.hypot(px - ax, py - ay)
            t_proj = (vx * wx + vy * wy) / denom
            projx = ax + t_proj * vx
            projy = ay + t_proj * vy
            return math.hypot(px - projx, py - projy)

        def _is_degenerate_cubic(
            sx, sy, c1x, c1y, c2x, c2y, ex, ey, rel_tol=1e-6, abs_tol=1e-3
        ):
            """
            True if cubic (S, C1, C2, E) is effectively a straight line.
            Uses:
              - distance of control pts to S->E line
              - handle lengths relative to segment length
              - absolute clamp
            Tweak abs_tol based on your coordinate scale (1e-3 is typical for pixel-ish coords).
            """
            d1 = _distance_point_to_line(c1x, c1y, sx, sy, ex, ey)
            d2 = _distance_point_to_line(c2x, c2y, sx, sy, ex, ey)
            h1 = _dist(c1x, c1y, sx, sy)
            h2 = _dist(c2x, c2y, ex, ey)
            seg_len = _dist(sx, sy, ex, ey)
            rel = rel_tol * (seg_len if seg_len > 0 else 1.0)
            if (
                d1 <= max(abs_tol, rel)
                and d2 <= max(abs_tol, rel)
                and h1 <= max(abs_tol, rel)
                and h2 <= max(abs_tol, rel)
            ):
                return True
            if (
                abs(c1x - sx) <= abs_tol
                and abs(c1y - sy) <= abs_tol
                and abs(c2x - ex) <= abs_tol
                and abs(c2y - ey) <= abs_tol
            ):
                return True
            return False

        def _arc_to_cubics(x1, y1, rx, ry, phi_deg, large_arc, sweep, x2, y2):
            """
            Converts an SVG elliptical arc to a list of cubic bezier segments.
            Returns list of (c1x,c1y,c2x,c2y, ex,ey).
            Based on the SVG spec algorithm (center parameterization) and splitting
            into segments <= 90 degrees. Handles radii correction.
            """
            # degenerate straight-line fallback
            if rx == 0 or ry == 0:
                return [(x1, y1, x2, y2, x2, y2)]

            phi = math.radians(phi_deg % 360.0)
            cos_phi = math.cos(phi)
            sin_phi = math.sin(phi)

            # Step 1: transform to prime coords
            dx2 = (x1 - x2) / 2.0
            dy2 = (y1 - y2) / 2.0
            x1p = cos_phi * dx2 + sin_phi * dy2
            y1p = -sin_phi * dx2 + cos_phi * dy2

            rx_abs = abs(rx)
            ry_abs = abs(ry)
            if rx_abs == 0 or ry_abs == 0:
                return [(x1, y1, x2, y2, x2, y2)]

            # Step 2: correct radii if too small
            lam = (x1p * x1p) / (rx_abs * rx_abs) + (y1p * y1p) / (ry_abs * ry_abs)
            if lam > 1.0:
                scale = math.sqrt(lam)
                rx_abs *= scale
                ry_abs *= scale

            # Step 3: compute center prime (cx', cy')
            rx2 = rx_abs * rx_abs
            ry2 = ry_abs * ry_abs
            x1p2 = x1p * x1p
            y1p2 = y1p * y1p

            # safe factor for sqrt argument
            num = max(0.0, rx2 * ry2 - rx2 * y1p2 - ry2 * x1p2)
            denom = rx2 * y1p2 + ry2 * x1p2
            if denom == 0:
                cc = 0.0
            else:
                cc = math.sqrt(num / denom) if num > 0 else 0.0
                cc = -cc if large_arc == sweep else cc

            cxp = cc * (rx_abs * y1p / ry_abs)
            cyp = cc * (-ry_abs * x1p / rx_abs)

            # Step 4: center Vec2(cx, cy)
            cx = cos_phi * cxp - sin_phi * cyp + (x1 + x2) / 2.0
            cy = sin_phi * cxp + cos_phi * cyp + (y1 + y2) / 2.0

            # Step 5: angles
            def _angle(u_x, u_y, v_x, v_y):
                dot = u_x * v_x + u_y * v_y
                det = u_x * v_y - u_y * v_x
                return math.atan2(det, dot)

            ux = (x1p - cxp) / rx_abs
            uy = (y1p - cyp) / ry_abs
            vx = (-x1p - cxp) / rx_abs
            vy = (-y1p - cyp) / ry_abs

            theta1 = math.atan2(uy, ux)
            delta = _angle(ux, uy, vx, vy)

            # adjust by sweep flag
            if not sweep and delta > 0:
                delta -= 2 * math.pi
            elif sweep and delta < 0:
                delta += 2 * math.pi

            # split into segments with angle <= pi/2
            segs = max(1, int(math.ceil(abs(delta) / (math.pi / 2.0))))
            delta_per = delta / segs

            out = []
            for i_seg in range(segs):
                t1 = theta1 + i_seg * delta_per
                t2 = t1 + delta_per

                def _point(t):
                    ct = math.cos(t)
                    st = math.sin(t)
                    x = cx + rx_abs * cos_phi * ct - ry_abs * sin_phi * st
                    y = cy + rx_abs * sin_phi * ct + ry_abs * cos_phi * st
                    return x, y

                def _deriv(t):
                    ct = math.cos(t)
                    st = math.sin(t)
                    dxdt = -rx_abs * cos_phi * st - ry_abs * sin_phi * ct
                    dydt = -rx_abs * sin_phi * st + ry_abs * cos_phi * ct
                    return dxdt, dydt

                p1x, p1y = _point(t1)
                p2x, p2y = _point(t2)
                d1x, d1y = _deriv(t1)
                d2x, d2y = _deriv(t2)

                dt = t2 - t1
                if abs(dt) < 1e-12:
                    # tiny segment -> straight
                    out.append((p1x, p1y, p2x, p2y, p2x, p2y))
                    continue

                k = (4.0 / 3.0) * math.tan(dt / 4.0)
                c1x = p1x + k * d1x
                c1y = p1y + k * d1y
                c2x = p2x - k * d2x
                c2y = p2y - k * d2y

                out.append((c1x, c1y, c2x, c2y, p2x, p2y))

            return out

        # --- Tokenize -----------------------------------------------------
        tokens = _tok_regex().findall(svg_data.replace(",", " "))
        if not tokens:
            return Path()  # empty Path

        # param counts per command
        param_count = {
            "M": 2,
            "L": 2,
            "H": 1,
            "V": 1,
            "C": 6,
            "S": 4,
            "Q": 4,
            "T": 2,
            "A": 7,
            "Z": 0,
        }

        i = 0
        cur_x = 0.0
        cur_y = 0.0
        sub_x = None
        sub_y = None
        prev_cmd = None
        prev_cubic_ctrl = None
        prev_quad_ctrl = None

        # We'll build an intermediate list of commands to allow post-processing.
        # Each entry: ("M", x,y) | ("L", x,y) | ("C", c1x,c1y,c2x,c2y, ex,ey) |
        # ("Q", cx,cy, ex,ey) | ("Z",)
        cmds = []

        # --- Parse loop ---------------------------------------------------
        while i < len(tokens):
            tok = tokens[i]
            if re.fullmatch(r"[MmZzLlHhVvCcSsQqTtAa]", tok):
                i += 1
                cmd = tok
            else:
                # implicit repetition of previous command
                if prev_cmd is None:
                    raise ValueError("Path data must begin with a command")
                cmd = prev_cmd

            absolute = cmd.isupper()
            C = cmd.upper()

            if C == "Z":
                # close path
                cmds.append(("Z",))
                # set current point to subpath start (we'll resolve when emitting)
                prev_cubic_ctrl = None
                prev_quad_ctrl = None
                prev_cmd = cmd
                continue

            # process one or more parameter groups for this command
            need = param_count[C]

            while True:
                # if not enough tokens left for a full parameter group, break
                if need > 0:
                    # peek next token: if it's a command letter, break
                    if i >= len(tokens):
                        break
                    if re.fullmatch(r"[MmZzLlHhVvCcSsQqTtAa]", tokens[i]):
                        break

                if C == "M":
                    # MoveTo consumes one pair and subsequent pairs are treated as implicit L
                    x = _read_number()
                    y = _read_number()
                    px, py = _make_point(x, y, not absolute)
                    cmds.append(("M", px, py))
                    cur_x, cur_y = px, py
                    sub_x, sub_y = px, py
                    prev_cubic_ctrl = None
                    prev_quad_ctrl = None
                    # subsequent pairs become L/l
                    prev_cmd = "L" if absolute else "l"
                    cmd = prev_cmd
                    absolute = cmd.isupper()
                    C = "L"
                    continue

                if C == "L":
                    x = _read_number()
                    y = _read_number()
                    px, py = _make_point(x, y, not absolute)
                    cmds.append(("L", px, py))
                    cur_x, cur_y = px, py
                    prev_cubic_ctrl = None
                    prev_quad_ctrl = None
                    prev_cmd = cmd
                    continue

                if C == "H":
                    x = _read_number()
                    px = x if absolute else (cur_x + x)
                    py = cur_y
                    cmds.append(("L", px, py))
                    cur_x, cur_y = px, py
                    prev_cubic_ctrl = None
                    prev_quad_ctrl = None
                    prev_cmd = cmd
                    continue

                if C == "V":
                    y = _read_number()
                    py = y if absolute else (cur_y + y)
                    px = cur_x
                    cmds.append(("L", px, py))
                    cur_x, cur_y = px, py
                    prev_cubic_ctrl = None
                    prev_quad_ctrl = None
                    prev_cmd = cmd
                    continue

                if C == "C":
                    x1 = _read_number()
                    y1 = _read_number()
                    x2 = _read_number()
                    y2 = _read_number()
                    x = _read_number()
                    y = _read_number()
                    p1 = _make_point(x1, y1, not absolute)
                    p2 = _make_point(x2, y2, not absolute)
                    p = _make_point(x, y, not absolute)
                    cmds.append(("C", p1[0], p1[1], p2[0], p2[1], p[0], p[1]))
                    prev_cubic_ctrl = (p2[0], p2[1])
                    prev_quad_ctrl = None
                    cur_x, cur_y = p
                    prev_cmd = cmd
                    continue

                if C == "S":
                    x2 = _read_number()
                    y2 = _read_number()
                    x = _read_number()
                    y = _read_number()
                    if (
                        prev_cmd is not None
                        and prev_cmd.upper() in ("C", "S")
                        and prev_cubic_ctrl is not None
                    ):
                        refl_x = 2 * cur_x - prev_cubic_ctrl[0]
                        refl_y = 2 * cur_y - prev_cubic_ctrl[1]
                        c1x, c1y = refl_x, refl_y
                    else:
                        c1x, c1y = cur_x, cur_y
                    p2 = _make_point(x2, y2, not absolute)
                    p = _make_point(x, y, not absolute)
                    cmds.append(("C", c1x, c1y, p2[0], p2[1], p[0], p[1]))
                    prev_cubic_ctrl = (p2[0], p2[1])
                    prev_quad_ctrl = None
                    cur_x, cur_y = p
                    prev_cmd = cmd
                    continue

                if C == "Q":
                    cx = _read_number()
                    cy = _read_number()
                    x = _read_number()
                    y = _read_number()
                    cpt = _make_point(cx, cy, not absolute)
                    p = _make_point(x, y, not absolute)
                    cmds.append(("Q", cpt[0], cpt[1], p[0], p[1]))
                    prev_quad_ctrl = (cpt[0], cpt[1])
                    prev_cubic_ctrl = None
                    cur_x, cur_y = p
                    prev_cmd = cmd
                    continue

                if C == "T":
                    x = _read_number()
                    y = _read_number()
                    if (
                        prev_cmd is not None
                        and prev_cmd.upper() in ("Q", "T")
                        and prev_quad_ctrl is not None
                    ):
                        refl_x = 2 * cur_x - prev_quad_ctrl[0]
                        refl_y = 2 * cur_y - prev_quad_ctrl[1]
                        cpt = (refl_x, refl_y)
                    else:
                        cpt = (cur_x, cur_y)
                    p = _make_point(x, y, not absolute)
                    cmds.append(("Q", cpt[0], cpt[1], p[0], p[1]))
                    prev_quad_ctrl = cpt
                    prev_cubic_ctrl = None
                    cur_x, cur_y = p
                    prev_cmd = cmd
                    continue

                if C == "A":
                    rx = _read_number()
                    ry = _read_number()
                    xrot = _read_number()
                    laf = int(_read_number())
                    sf = int(_read_number())
                    x = _read_number()
                    y = _read_number()
                    p = _make_point(x, y, not absolute)
                    # Convert arc to cubic segments
                    segs = _arc_to_cubics(
                        cur_x, cur_y, rx, ry, xrot, laf != 0, sf != 0, p[0], p[1]
                    )
                    # append as cubic entries; degenerate handling will be done later
                    for c1x, c1y, c2x, c2y, ex, ey in segs:
                        cmds.append(("C", c1x, c1y, c2x, c2y, ex, ey))
                        cur_x, cur_y = ex, ey
                    prev_cubic_ctrl = (segs[-1][2], segs[-1][3]) if segs else None
                    prev_quad_ctrl = None
                    prev_cmd = cmd
                    continue

                # Unknown/unsupported should not happen
                raise ValueError(f"Unsupported command {C}")

            # done repetitions for this explicit command
            prev_cmd = cmd

        # --- Post-process intermediate cmds --------------------------------
        # Convert degenerate cubics to straight lines, drop zero-length lines, remove duplicate consecutive points.
        simplified = []
        last_pt = None
        subpath_start = None

        for entry in cmds:
            if entry[0] == "M":
                _, x, y = entry
                # Emit move only if different from last
                if last_pt is None or (
                    abs(x - last_pt.x) > 1e-12 or abs(y - last_pt.y) > 1e-12
                ):
                    simplified.append(("M", x, y))
                    last_pt = Vec2(x, y)
                    subpath_start = Vec2(x, y)
                else:
                    # duplicate move: ignore
                    subpath_start = Vec2(x, y)
                    # last_pt stays same
                continue

            if entry[0] == "L":
                _, x, y = entry
                if (
                    last_pt is not None
                    and abs(x - last_pt.x) < 1e-12
                    and abs(y - last_pt.y) < 1e-12
                ):
                    # zero-length line -> skip
                    continue
                simplified.append(("L", x, y))
                last_pt = Vec2(x, y)
                continue

            if entry[0] == "C":
                _, c1x, c1y, c2x, c2y, ex, ey = entry
                s = last_pt if last_pt is not None else Vec2(0.0, 0.0)
                sx = s.x
                sy = s.y
                # If cubic is degenerate -> replace with L
                if _is_degenerate_cubic(
                    sx, sy, c1x, c1y, c2x, c2y, ex, ey, rel_tol=1e-6, abs_tol=1e-3
                ):
                    # skip if zero-length
                    if abs(ex - sx) < 1e-12 and abs(ey - sy) < 1e-12:
                        # skip degenerate
                        last_pt = Vec2(ex, ey)
                        continue
                    simplified.append(("L", ex, ey))
                    last_pt = Vec2(ex, ey)
                else:
                    simplified.append(("C", c1x, c1y, c2x, c2y, ex, ey))
                    last_pt = Vec2(ex, ey)
                continue

            if entry[0] == "Q":
                _, cx, cy, ex, ey = entry
                s = last_pt if last_pt is not None else Vec2(0.0, 0.0)
                sx = s.x
                sy = s.y
                # Convert small quadratics (control nearly on line) to L
                dctrl = _distance_point_to_line(cx, cy, sx, sy, ex, ey)
                if dctrl <= max(1e-3, 1e-6 * _dist(sx, sy, ex, ey)):
                    # small quad -> line
                    if abs(ex - sx) < 1e-12 and abs(ey - sy) < 1e-12:
                        last_pt = Vec2(ex, ey)
                        continue
                    simplified.append(("L", ex, ey))
                    last_pt = Vec2(ex, ey)
                else:
                    simplified.append(("Q", cx, cy, ex, ey))
                    last_pt = Vec2(ex, ey)
                continue

            if entry[0] == "Z":
                simplified.append(("Z",))
                # after close, last_pt will become subpath_start
                last_pt = subpath_start
                continue

        # --- Emit to real Path API ------------------------------------------
        path = Path()
        cur_x = cur_y = 0.0
        sub_x = sub_y = None

        for e in simplified:
            if e[0] == "M":
                _, x, y = e
                # use move_to_vec
                path.move_to_vec(Vec2(x, y))
                cur_x, cur_y = x, y
                sub_x, sub_y = x, y
            elif e[0] == "L":
                _, x, y = e
                path.line_to_vec(Vec2(x, y))
                cur_x, cur_y = x, y
            elif e[0] == "C":
                _, c1x, c1y, c2x, c2y, ex, ey = e
                path.cubic_to_vec(Vec2(c1x, c1y), Vec2(c2x, c2y), Vec2(ex, ey))
                cur_x, cur_y = ex, ey
            elif e[0] == "Q":
                _, cx, cy, ex, ey = e
                path.quad_to_vec(Vec2(cx, cy), Vec2(ex, ey))
                cur_x, cur_y = ex, ey
            elif e[0] == "Z":
                if hasattr(path, "close"):
                    path.properties.is_closed = True
                else:
                    # fallback: line to subpath start
                    if sub_x is not None and sub_y is not None:
                        path.line_to_vec(Vec2(sub_x, sub_y))
                # current point becomes subpath start
                cur_x, cur_y = sub_x, sub_y
            else:
                # should not reach here
                raise RuntimeError(f"Unhandled simplified command {e[0]}")

        return path

    def copy(self):
        """Return a deep copy of this Path."""
        new_path = Path()
        new_path.set_commands(self.get_commands())
        new_path.set_path_properties(self.get_path_props_copy())
        new_path.set_bounds(self.get_bounds())
        new_path.set_lowest_pos(self.get_lowest_pos())
        return new_path

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
                cmd.move.p = data["p"].native
            case PathCommandKind.Line:
                cmd.line.p = data["p"].native
            case PathCommandKind.Quad:
                cmd.quad.c = data["c"].native
                cmd.quad.p = data["p"].native
            case PathCommandKind.Cubic:
                cmd.cubic.c1 = data["c1"].native
                cmd.cubic.c2 = data["c2"].native
                cmd.cubic.p = data["p"].native
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
        """
        Returns SVG formated path data string.
        """
        svg_parts = []
        for cmd in self.get_commands():
            match cmd.kind:
                case PathCommandKind.Move:
                    svg_parts.append(f"M {cmd.move.p.x} {cmd.move.p.y}")
                case PathCommandKind.Line:
                    svg_parts.append(f"L {cmd.line.p.x} {cmd.line.p.y}")
                case PathCommandKind.Quad:
                    svg_parts.append(
                        f"Q {cmd.quad.c.x} {cmd.quad.c.y}, {cmd.quad.p.x} {cmd.quad.p.y}"
                    )
                case PathCommandKind.Cubic:
                    svg_parts.append(
                        f"C {cmd.cubic.c1.x} {cmd.cubic.c1.y}, {cmd.cubic.c2.x} {cmd.cubic.c2.y}, {cmd.cubic.p.x} {cmd.cubic.p.y}"
                    )
        return " ".join(svg_parts)


# ---------------------------------------------------------------------
# Utility: convert Python tuple to glm::vec2
# ---------------------------------------------------------------------


__all__ = ["Path", "PathProperties"]
