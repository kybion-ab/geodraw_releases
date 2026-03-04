"""Demo: QuickView as Debug Instrumentation (Python)

Python mirror of app/demo_quick_view.cpp.

Shows how to drop a geodraw visualisation into existing Python code as a
temporary debugging aid. The `debug` flag gates the entire block; set it to
False to restore production behaviour with zero overhead.

Run from the repo root:
    python python/demos/demo_quick_view.py
"""

import geodraw


# ---------------------------------------------------------------------------
# "Production" computation — a simple triangle.
# ---------------------------------------------------------------------------
def compute_triangle():
    """Return a list of (x, y, z) tuples forming a triangle."""
    return [(1, 1, 0), (3, 2, 0), (1, 2, 1)]


def main():
    # Run the production computation (unchanged).
    triangle = compute_triangle()

    # -------------------------------------------------------------------------
    # Debug block — flip this flag to False to skip the visualisation.
    # -------------------------------------------------------------------------
    debug = True

    if debug:
        print(f"Debug: visualising computed triangle")

        scene = geodraw.Scene()

        # Draw the triangle.
        orange=(0.95, 0.5, 0.0)
        scene.add_triangle(triangle, orange)

        # Reference frame at the origin.
        scene.add_axes((0, 0, 0), scale=2.0, thickness=5.0)

        # show() blocks until the viewer window is closed.
        geodraw.show(scene, title="Debugging: computed triangle")

    # The rest of the (hypothetical) production pipeline continues here.
    print(f"Pipeline continues...")


if __name__ == "__main__":
    main()
