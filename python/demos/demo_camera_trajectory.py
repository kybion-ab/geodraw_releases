"""Demo: Camera Trajectory (Python)

Python mirror of app/demo_camera_trajectory.cpp.

Demonstrates the CameraTrajectoryPlugin for recording and replaying
camera paths.

Pattern — three lines after App setup:
  cam_traj = geodraw.CameraTrajectoryPlugin()
  app.add_module(cam_traj)
  app.activate_minor_mode(cam_traj.get_minor_mode())

The plugin's ImGui panel (shown via cam_traj.draw_imgui_panel(app))
provides record / playback / speed controls.

Usage:
  1. Add keyframes by positioning the viewer and pressing 'Add' in the panel
  2. Bake interpolated frames
  3. View the trajectory
  4. Save as CSV file

Run from the repo root:
    python python/demos/demo_camera_trajectory.py
"""

import geodraw

TRIANGLE = [(1, 1, 0), (3, 2, 0), (1, 2, 1)]
ORANGE = (0.95, 0.5, 0.0)


def main():
    app = geodraw.App(1280, 720, "Demo Camera Trajectory")

    cam_traj = geodraw.CameraTrajectoryPlugin()
    app.add_module(cam_traj)
    app.activate_minor_mode(cam_traj.get_minor_mode())

    @app.set_imgui_callback()
    def draw_ui(gui):
        cam_traj.draw_imgui_panel(app)

    @app.add_update_callback()
    def update(dt):
        scene = app.scene()
        scene.clear()
        scene.add_axes((0, 0, 0), scale=2.0, thickness=5.0)
        scene.add_triangle(TRIANGLE, color=ORANGE)

    app.run()


if __name__ == "__main__":
    main()
