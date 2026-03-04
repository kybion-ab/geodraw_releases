"""Demo: Hello World (Python)

Python mirror of app/demo_hello_world.cpp.

START HERE. The most basic GeoDraw application.

This demo walks through the fundamental pattern every GeoDraw app follows:
  1. Create an App window
  2. Register an update callback that populates the scene
  3. Call app.run() to enter the event loop

Geometry demonstrated:
  axes     — coordinate axes at the origin (X=red, Y=green, Z=blue)
  triangle — orange triangle in 3D

Controls: drag to orbit · scroll to zoom · right-drag to pan · H for help

Run from the repo root:
    python python/demos/demo_hello_world.py
"""

import geodraw


# Triangle vertices (created once, reused every frame).
TRIANGLE = [(1, 1, 0), (3, 2, 0), (1, 2, 1)]
ORANGE = (0.95, 0.5, 0.0)


def main():
    # -------------------------------------------------------------------------
    # Step 1: Create the application window.
    #
    # App(width, height, title) initialises the OpenGL window, the renderer,
    # and the event system. Orbit camera is active by default — drag to rotate.
    # -------------------------------------------------------------------------
    app = geodraw.App(1280, 720, "Demo Hello World")

    # -------------------------------------------------------------------------
    # Step 2: Register an update callback.
    #
    # The decorator wires `update` into the render loop. It runs whenever the
    # scene needs refreshing. `dt` is elapsed time in seconds since the
    # previous frame.
    # -------------------------------------------------------------------------
    @app.add_update_callback()
    def update(dt):
        scene = app.scene()
        scene.clear()

        # Reference frame at the origin.
        scene.add_axes((0, 0, 0), scale=2.0, thickness=5.0)

        # Orange triangle.
        scene.add_triangle(TRIANGLE, color=ORANGE)

    # -------------------------------------------------------------------------
    # Step 3: Enter the event loop.
    #
    # run() blocks until the user closes the window (Q / ESC / window X).
    # -------------------------------------------------------------------------
    app.run()


if __name__ == "__main__":
    main()
