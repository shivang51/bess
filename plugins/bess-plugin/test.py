from bessplug.api.renderer.path import Path

p = Path()

p.move_to((0, 0)).line_to((100, 0)).line_to((100, 100)).line_to((0, 100)).line_to((0, 0))

print(p)
