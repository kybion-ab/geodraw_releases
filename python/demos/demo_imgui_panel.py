"""Demo: ImGui Panel (Python)

Python mirror of app/demo_imgui_panel.cpp.

Adds a floating ImGui control window to the hello-world scene.

Pattern — two lines on top of any existing App:
  @app.set_imgui_callback()
  def draw_ui(gui): ...   # gui is a Gui proxy with widget methods

Everything inside the callback runs inside a valid ImGui frame.
State mutated by the UI is read by the update callback every frame,
so the scene responds to controls in real time.

Adaptation note:
  The C++ demo uses ImGui::ColorEdit3 which is not yet bound in Python.
  This demo uses three gui.slider_float() calls for R, G, B instead.
  The visual result is equivalent.

Run from the repo root:
    python python/demos/demo_imgui_panel.py
"""

import geodraw


def main():
    app = geodraw.App(1280, 720, "Demo ImGui Panel")

    # Mutable color state (list, not tuple — Python floats are immutable).
    color = [0.95, 0.5, 0.0]  # orange

    TRIANGLE = [(1, 1, 0), (3, 2, 0), (1, 2, 1)]

    # -------------------------------------------------------------------------
    # ImGui callback — runs every frame inside a valid ImGui context.
    #
    # Note: gui.slider_float() returns (changed, new_value). We store the
    # new value back into `color` so the update callback sees the change.
    # -------------------------------------------------------------------------
    @app.set_imgui_callback()
    def draw_ui(gui):
        gui.begin("Color picking")
        gui.separator()

        changed_r, color[0] = gui.slider_float("R##color", color[0], 0.0, 1.0)
        changed_g, color[1] = gui.slider_float("G##color", color[1], 0.0, 1.0)
        changed_b, color[2] = gui.slider_float("B##color", color[2], 0.0, 1.0)

        gui.end()

    # Keep the scene refreshing while the panel is open.
    app.request_continuous_update("demo.imgui_panel")

    # -------------------------------------------------------------------------
    # Update callback — reads color set by the ImGui callback above.
    # -------------------------------------------------------------------------
    @app.add_update_callback()
    def update(dt):
        scene = app.scene()
        scene.clear()
        scene.add_axes((0, 0, 0), scale=2.0, thickness=5.0)
        scene.add_triangle(TRIANGLE, color=tuple(color))

    app.run()


if __name__ == "__main__":
    main()
