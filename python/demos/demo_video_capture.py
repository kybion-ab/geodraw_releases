"""Demo: Video Capture (Python)

Python mirror of app/demo_video_capture.cpp.

Layers VideoCapturePlugin on top of CameraTrajectoryPlugin.
Together they let you:
  1. Record a camera path (CameraTrajectoryPlugin panel)
  2. Bake interpolated frames
  3. Replay and capture each frame to disk (VideoCapturePlugin panel)
  4. Save as mp4 via FFmpeg (button in the video capture panel)

Pattern — two extra lines after camera-trajectory setup:
  video_cap = geodraw.VideoCapturePlugin(cam_traj)  # requires cam_traj reference
  app.add_module(video_cap)
  app.activate_minor_mode(video_cap.get_minor_mode())

Both plugins expose an ImGui panel; show them in the same callback.

Run from the repo root:
    python python/demos/demo_video_capture.py
"""

import geodraw

TRIANGLE = [(1, 1, 0), (3, 2, 0), (1, 2, 1)]
ORANGE = (0.95, 0.5, 0.0)


def main():
    app = geodraw.App(1280, 720, "Demo Video Capture")

    # Camera trajectory plugin (record / playback).
    cam_traj = geodraw.CameraTrajectoryPlugin()
    app.add_module(cam_traj)
    app.activate_minor_mode(cam_traj.get_minor_mode())

    # Video capture plugin — hooks into trajectory playback.
    # cam_traj must remain alive for the lifetime of video_cap.
    video_cap = geodraw.VideoCapturePlugin(cam_traj)
    app.add_module(video_cap)
    app.activate_minor_mode(video_cap.get_minor_mode())

    @app.set_imgui_callback()
    def draw_ui(gui):
        cam_traj.draw_imgui_panel(app)
        video_cap.draw_imgui_panel(app)

    @app.add_update_callback()
    def update(dt):
        scene = app.scene()
        scene.clear()
        scene.add_axes((0, 0, 0), scale=2.0, thickness=5.0)
        scene.add_triangle(TRIANGLE, color=ORANGE)

    app.run()


if __name__ == "__main__":
    main()
