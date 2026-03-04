"""Demo: Textured Geometry (Python)

Python mirror of app/demo_texture.cpp.

Demonstrates loading PNG textures and applying them to geometry.
The demo falls back gracefully to solid colours if texture files are missing.

Texture files expected (relative to the repo root at run time):
  data/textures/brick.png — applied to the triangle

Pattern:
  tex = app.renderer().load_texture("path/to/file.png")  # returns int handle, 0 on error
  scene.add_triangle(vertices, ..., texture_handle=tex)

load_texture() returns 0 if the file cannot be opened; passing 0 as the
texture_handle causes the geometry to render with its solid colour instead,
so no special fallback logic is required in the scene update.

Triangle UV coordinates are required for texture sampling:
  vertices_uv = [
      ((1, 1, 0), (0.0, 0.0)),
      ((3, 2, 0), (1.0, 0.0)),
      ((1, 2, 1), (0.5, 1.0)),
  ]

Status: STUB — requires Python bindings not yet implemented.
Missing bindings:
  - app.renderer() → Renderer object
  - Renderer.load_texture(path) → int
  - Renderer.unload_texture(path)
  - scene.add_triangle(..., texture_handle=int)  (UV-aware overload)

Run from the repo root:
    python python/demos/demo_texture.py
"""

import geodraw


# Intended implementation (requires missing bindings):
#
# def main():
#     app = geodraw.App(1280, 720, "Demo Textured Geometry")
#
#     tex = app.renderer().load_texture("data/textures/brick.png")
#     if tex == 0:
#         print("Note: brick.png not found — triangle will be solid orange.")
#
#     # UV coordinates enable texture sampling.
#     TRIANGLE_UV = [
#         ((1, 1, 0), (0.0, 0.0)),
#         ((3, 2, 0), (1.0, 0.0)),
#         ((1, 2, 1), (0.5, 1.0)),
#     ]
#     ORANGE = (0.95, 0.5, 0.0)
#
#     @app.add_update_callback()
#     def update(dt):
#         scene = app.scene()
#         scene.clear()
#         scene.add_axes((0, 0, 0), scale=2.0, thickness=5.0)
#         scene.add_triangle(TRIANGLE_UV, color=ORANGE, texture_handle=tex)
#
#     app.run()
#
#     if tex != 0:
#         app.renderer().unload_texture("data/textures/brick.png")


def main():
    raise NotImplementedError(
        "demo_texture requires missing Python bindings: "
        "app.renderer(), Renderer.load_texture(), Renderer.unload_texture(), "
        "and scene.add_triangle() with texture_handle and UV coordinates."
    )


if __name__ == "__main__":
    main()
