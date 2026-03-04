"""Demo: Multiple Scenes (Python)

2×3 grid of scenes, each showing a triangle in a different color.
Demonstrates: App.set_scene_grid, App.auto_frame_scene, multiple scenes.
"""
import geodraw

COLORS = [
    (1.0, 0.2, 0.2), (0.2, 1.0, 0.2), (0.2, 0.2, 1.0),
    (1.0, 0.6, 0.1), (0.1, 0.8, 0.8), (0.8, 0.1, 0.8),
]
TRIANGLE = [(1, 1, 0), (3, 2, 0), (1, 2, 1)]


def main():
    app = geodraw.App(1280, 720, "Demo Multiple Scenes")

    for _ in range(6):
        app.add_scene()
    app.set_scene_grid(2, 3)

    for i in range(6):
        app.auto_frame_scene(i, (1, 1, 0), (3, 2, 1))

    @app.add_update_callback()
    def update(dt):
        for i in range(6):
            s = app.scene(i)
            s.clear()
            s.add_triangle(TRIANGLE, color=COLORS[i])
            s.add_pose((0, 0, 0), [[1, 0, 0], [0, 1, 0], [0, 0, 1]], 2.0, 5.0);

    app.run()


if __name__ == "__main__":
    main()
