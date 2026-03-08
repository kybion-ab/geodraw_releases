"""Demo: Shape Edit — local coordinates (Python)

Python mirror of app/demo_shape_edit.cpp.

Demonstrates shape editing in a local (non-geographic) coordinate system.
No earth tiles, API keys, or network access required.

Controls:
  CTRL+S       — Activate / deactivate shape editing mode
  CTRL+Click   — Add point
  CTRL+R       — Commit points as ring (polygon)
  CTRL+L       — Commit points as line
  CTRL+P       — Commit points as isolated points
  CTRL+Z / Y   — Undo / Redo
  CTRL+D       — Delete nearest point
  CTRL+I       — Split nearest edge (insert point)
  CTRL+M       — Move nearest point (hold and drag)
  CTRL+O       — Save shape to edited_shape.shape
  CTRL+F       — Load shape from edited_shape.shape
  H            — Show all keybindings

The shape is stored in scene-local XY-plane coordinates (z ≈ 0).
CTRL+O/F save and load the raw coordinates using the .shape format.

Run from the repo root:
    python python/demos/demo_shape_edit.py
"""

import geodraw


def draw_ground_and_grid(scene, extent=10.0, step=1.0):
    """Opaque ground plane + reference grid on the XY plane."""
    scene.add_ground(
        (0, 0, 0), width=extent * 2, height=extent * 2,
        color=(0.12, 0.12, 0.12), alpha=1.0,
    )
    grid_color = (0.28, 0.28, 0.28)
    x = -extent
    while x <= extent + 0.01:
        scene.add_polyline(
            [(x, -extent, 0), (x, extent, 0)], thickness=1.0, color=grid_color
        )
        x += step
    y = -extent
    while y <= extent + 0.01:
        scene.add_polyline(
            [(-extent, y, 0), (extent, y, 0)], thickness=1.0, color=grid_color
        )
        y += step


def main():
    app = geodraw.App(1280, 720, "Demo Shape Edit")

    editor = geodraw.ShapeEditor()
    editor.register_commands(app)

    @app.add_update_callback()
    def update(dt):
        scene = app.scene()
        scene.clear()
        draw_ground_and_grid(scene)
        editor.add_to_scene(scene)

    @app.add_draw_callback()
    def draw():
        editor.handle_input(app)

    app.run()


if __name__ == "__main__":
    main()
