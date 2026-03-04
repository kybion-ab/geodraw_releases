"""Demo: Tooltip System (Python)

Python mirror of app/demo_tooltip.cpp.

Demonstrates hovering over geometry to display a tooltip popup.

Three ingredients:
  1. app.enable_tooltips(scene)
       Hooks into the ImGui frame to perform ray-picking on the given scene
       and draw a tooltip popup when the ray hits tagged geometry.

  2. scene.add_triangle(...).with_id(n).with_type("Name")
       Tags a rendered item so the picking system can identify it.
       id   — unique integer within the scene (used in TooltipContext)
       type — string key matched to a registered provider

  3. app.register_tooltip_provider("Name", fn)
       Maps a type string to content.  fn receives a TooltipContext
       (object_id, hit position, …) and returns the text to display.

Hover the mouse over any tagged geometry to see its tooltip.

Run from the repo root:
    python python/demos/demo_tooltip.py
"""

import geodraw


def main():
    app = geodraw.App(1280, 720, "Demo Tooltip System")

    app.enable_tooltips(app.scene())

    app.register_tooltip_provider(
        "Triangle",
        lambda ctx: "Orange Triangle",
    )

    TRIANGLE = [(1, 1, 0), (3, 2, 0), (1, 2, 1)]
    ORANGE = (0.95, 0.5, 0.0)

    @app.add_update_callback()
    def update(dt):
        scene = app.scene()
        scene.clear()
        scene.add_axes((0, 0, 0), scale=2.0, thickness=5.0)
        # Tagged triangle — .with_id() + .with_type() enable tooltip picking.
        scene.add_triangle(TRIANGLE, color=ORANGE).with_id(1).with_type("Triangle")

    app.run()


if __name__ == "__main__":
    main()
