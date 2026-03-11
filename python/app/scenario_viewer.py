"""Scenario Viewer (Python)

Python mirror of app/scenario_viewer.cpp.

Standalone vehicle trajectory viewer. Loads a JSON scenario file and
displays vehicles, lanes, and road geometry with playback controls.

Usage (from repo root):
    python python/app/scenario_viewer.py [path/to/scenario.json]
"""

import sys
from pathlib import Path
import geodraw

_REPO_ROOT = Path(__file__).resolve().parent.parent.parent
DEFAULT_SCENARIO = str(_REPO_ROOT / "data" / "examples" / "tfrecord-00000-of-01000_4.json")
DEFAULT_CAR_MODEL = str(_REPO_ROOT / "data" / "glb_files" / "WhiteCar.glb")


def main():
    filepath = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_SCENARIO

    app = geodraw.App(1280, 720, "Vehicle Trajectory Viewer")

    scenario = geodraw.ScenarioPlugin()

    scenario.set_folder_pick_callback(
        lambda default_path: geodraw.pick_folder(default_path)
    )
    scenario.set_file_pick_callback(
        lambda default_path: geodraw.pick_file(default_path, [("JSON scenario", "json")])
    )

    car_model = app.load_glb(DEFAULT_CAR_MODEL)
    if car_model is not None and car_model.is_valid:
        scenario.set_car_model(car_model)

    app.add_module(scenario)
    scenario.activate_scenario(filepath, app)

    cam_traj = geodraw.CameraTrajectoryPlugin()
    app.add_module(cam_traj)
    app.activate_minor_mode(cam_traj.get_minor_mode())

    video_cap = geodraw.VideoCapturePlugin(cam_traj)
    app.add_module(video_cap)
    app.activate_minor_mode(video_cap.get_minor_mode())

    @app.set_imgui_callback()
    def draw_ui(gui):
        scenario.draw_imgui_panel(app)
        cam_traj.draw_imgui_panel(app)
        video_cap.draw_imgui_panel(app)

    app.run()


if __name__ == "__main__":
    main()
