"""Demo: Editable Geometry (Python)

Python mirror of app/demo_editable.cpp.

Demonstrates the two-line editing workflow built into App:
  app.enable_editing()   — activates click-to-select + drag editing for Shape3

Only Shape3 objects participate in editing:
  - Add via scene.add_shape(shape) using a mutable Shape3.
  - Tag with .with_id(n).with_type("Name") so the editor can track the object.

Edit session controls (press TAB to enter edit mode):
  TAB   — enter / exit edit mode
  ENTER — commit edits (propagate changes to the source object)
  ESC   — cancel edits (revert to the last committed state)

Status: STUB — requires Python bindings not yet implemented.
Missing bindings:
  - app.enable_editing()
  - Shape3 as a mutable Python object with polygon/polyline construction
  - scene.add_shape(shape3).with_id(n).with_type(s)  (editable rendering path)

Run from the repo root:
    python python/demos/demo_editable.py
"""

import geodraw


# Intended implementation (requires missing bindings):
#
# def main():
#     app = geodraw.App(1280, 720, "Demo Editable Geometry")
#     app.enable_editing()
#
#     # Build an L-shaped editable polygon.
#     shape = geodraw.Shape3()
#     shape.add_ring([
#         (-5, -5, 0), (-5,  0, 0), (-3,  0, 0),
#         (-3, -3, 0), ( 0, -3, 0), ( 0, -5, 0),
#         (-5, -5, 0),  # close the ring
#     ])
#
#     TRIANGLE = [(1, 1, 0), (3, 2, 0), (1, 2, 1)]
#     ORANGE = (0.95, 0.5, 0.0)
#
#     @app.add_update_callback()
#     def update(dt):
#         scene = app.scene()
#         scene.clear()
#         scene.add_axes((0, 0, 0), scale=2.0, thickness=5.0)
#         scene.add_triangle(TRIANGLE, color=ORANGE)
#         # Editable path: add_shape(mutable Shape3) + tagging.
#         scene.add_shape(shape).with_id(1).with_type("EditableShape")
#
#     app.run()


def main():
    raise NotImplementedError(
        "demo_editable requires missing Python bindings: "
        "app.enable_editing(), the Shape3 Python class, "
        "and scene.add_shape() with .with_id() / .with_type() tagging."
    )


if __name__ == "__main__":
    main()
