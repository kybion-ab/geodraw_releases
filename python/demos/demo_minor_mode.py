"""Demo: Minor Mode (Python)

Python mirror of app/demo_minor_mode.cpp.

A minor mode is a named container for keybindings.  Multiple minor modes
can be active simultaneously; when a key is pressed the app checks active
modes in reverse activation order (last activated wins).

Controls:
  1 — Toggle minor mode 1 (adds M → "Hello world!")
  2 — Toggle minor mode 2 (adds M → "Hi world!")
  H — Display help for currently active minor modes

Run from the repo root:
    python python/demos/demo_minor_mode.py
"""

import geodraw


ORANGE = (0.95, 0.5, 0.0)
TRIANGLE = [(1, 1, 0), (3, 2, 0), (1, 2, 1)]


def main():
    app = geodraw.App(1280, 720, "Demo Minor mode")

    # -------------------------------------------------------------------------
    # Minor mode 1 — "Hello world!" on M when active.
    # -------------------------------------------------------------------------
    mode1 = app.create_minor_mode("My minor mode 1")

    app.add_cmd(
        "say-hello",
        lambda: print("Hello world!"),
        "Say Hello World",
        key=geodraw.Key.M,
        mode=mode1,
    )
    app.add_cmd(
        "toggle-mode1",
        lambda: app.toggle_minor_mode(mode1),
        "Toggle hello minor mode",
        key=geodraw.Key.D1,
    )

    # -------------------------------------------------------------------------
    # Minor mode 2 — "Hi world!" on M when active.
    # -------------------------------------------------------------------------
    mode2 = app.create_minor_mode("My minor mode 2")

    app.add_cmd(
        "say-hi",
        lambda: print("Hi world!"),
        "Say Hi World",
        key=geodraw.Key.M,
        mode=mode2,
    )
    app.add_cmd(
        "toggle-mode2",
        lambda: app.toggle_minor_mode(mode2),
        "Toggle hi minormode",
        key=geodraw.Key.D2,
    )

    # -------------------------------------------------------------------------
    # Update callback — static scene with axes + orange triangle.
    # -------------------------------------------------------------------------
    @app.add_update_callback()
    def update(dt):
        scene = app.scene()
        scene.clear()
        scene.add_axes((0, 0, 0), scale=2.0, thickness=5.0)
        scene.add_triangle(TRIANGLE, color=ORANGE)

    app.run()


if __name__ == "__main__":
    main()
