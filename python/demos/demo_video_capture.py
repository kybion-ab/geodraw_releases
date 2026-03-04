"""Demo: Video Capture (Python)

Python mirror of app/demo_video_capture.cpp.

Layers VideoCapturePlugin on top of CameraTrajectoryPlugin.
Together they let you:
  1. Record a camera path (CameraTrajectoryPlugin)
  2. Replay it and capture each frame to disk (VideoCapturePlugin)
  3. Save as mp4

Pattern — two extra lines after camera-trajectory setup:
  video_cap = geodraw.VideoCapturePlugin(cam_traj)  # requires cam_traj reference
  app.add_module(video_cap)
  app.activate_minor_mode(video_cap.get_minor_mode())

Both plugins expose an ImGui panel; show them in the same callback.

Status: STUB — requires Python bindings not yet implemented.
Missing bindings:
  - geodraw.CameraTrajectoryPlugin (see demo_camera_trajectory.py)
  - geodraw.VideoCapturePlugin(cam_traj)
    - add_module / get_minor_mode / draw_imgui_panel

Run from the repo root:
    python python/demos/demo_video_capture.py
"""

import geodraw


# Intended implementation (requires missing bindings):
#
# def main():
#     app = geodraw.App(1280, 720, "Demo Video Capture")
#
#     TRIANGLE = [(1, 1, 0), (3, 2, 0), (1, 2, 1)]
#     ORANGE = (0.95, 0.5, 0.0)
#
#     # Camera trajectory plugin (record / playback).
#     cam_traj = geodraw.CameraTrajectoryPlugin()
#     app.add_module(cam_traj)
#     app.activate_minor_mode(cam_traj.get_minor_mode())
#
#     # Video capture plugin — hooks into trajectory playback.
#     video_cap = geodraw.VideoCapturePlugin(cam_traj)
#     app.add_module(video_cap)
#     app.activate_minor_mode(video_cap.get_minor_mode())
#
#     @app.set_imgui_callback()
#     def draw_ui(gui):
#         cam_traj.draw_imgui_panel(gui)
#         video_cap.draw_imgui_panel(gui)
#
#     @app.add_update_callback()
#     def update(dt):
#         scene = app.scene()
#         scene.clear()
#         scene.add_axes((0, 0, 0), scale=2.0, thickness=5.0)
#         scene.add_triangle(TRIANGLE, color=ORANGE)
#
#     app.run()


def main():
    raise NotImplementedError(
        "demo_video_capture requires missing Python bindings: "
        "geodraw.CameraTrajectoryPlugin and geodraw.VideoCapturePlugin "
        "(add_module, get_minor_mode, draw_imgui_panel)."
    )


if __name__ == "__main__":
    main()
