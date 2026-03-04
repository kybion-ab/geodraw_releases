"""Demo: Add keyboard commands and toggles (Python)

Python mirror of app/demo_add_cmd.cpp.

Demonstrates app.add_cmd() and app.add_toggle():
  - Press R to rotate the triangle 0.1 rad around the Z axis.
  - Press T to toggle triangle visibility.
  - Press H to see registered commands and their help text.

Run from the repo root:
    python python/demos/demo_add_cmd.py
"""

import math
import geodraw


ORANGE = (0.95, 0.5, 0.0)


def rotate_z(x, y, angle):
    """Rotate point (x, y) around the origin by `angle` radians."""
    c, s = math.cos(angle), math.sin(angle)
    return (c * x - s * y, s * x + c * y)


def main():
    app = geodraw.App(1280, 720, "Demo Add commands and toggles")

    # Mutable triangle state (Python lists are mutable, unlike tuples).
    triangle = [[1.0, 1.0, 0.0], [3.0, 2.0, 0.0], [1.0, 2.0, 1.0]]

    # -------------------------------------------------------------------------
    # Register a command.
    #
    # add_cmd(name, fn, doc, key) registers a void callback that fires when
    # the key is pressed. The command also appears in the H help overlay.
    # -------------------------------------------------------------------------
    def rotate_slightly():
        angle = 0.1  # radians per keypress
        for v in triangle:
            x, y = rotate_z(v[0], v[1], angle)
            v[0], v[1] = x, y
        print("Rotating slightly")

    app.add_cmd(
        "rotate-slightly",
        rotate_slightly,
        "Rotate the triangle slightly",
        key=geodraw.Key.R,
    )

    # -------------------------------------------------------------------------
    # Register a toggle.
    #
    # add_toggle(name, doc, key) returns a Toggle whose .value attribute flips
    # each time the key is pressed.  Python booleans are immutable, so the
    # Toggle wrapper is needed.
    # -------------------------------------------------------------------------
    show_tri = app.add_toggle(
        "toggle-triangle",
        "Toggle triangle",
        key=geodraw.Key.T,
    )

    # -------------------------------------------------------------------------
    # Update callback — rebuilds the scene every frame.
    # -------------------------------------------------------------------------
    @app.add_update_callback()
    def update(dt):
        scene = app.scene()
        scene.clear()
        scene.add_axes((0, 0, 0), scale=2.0, thickness=5.0)
        if show_tri.value:
            scene.add_triangle(triangle, color=ORANGE)

    app.run()


if __name__ == "__main__":
    main()
