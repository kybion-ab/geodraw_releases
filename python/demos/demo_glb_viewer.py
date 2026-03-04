"""Demo: GLB Model Viewer (Python)

Python mirror of demos/demo_glb_viewer.cpp.

Interactive viewer for glTF 2.0 binary (.glb) 3D models.
Loads all .glb files from data/glb_files/ and lets you switch between them.

Controls:
  LEFT/RIGHT ARROW - Switch between models
  1-8              - Select model directly
  B                - Toggle bounding box display
  +/-              - Adjust model transparency

Note: wireframe mode is not available through the Python API.

Run from the repo root:
    python python/demos/demo_glb_viewer.py
"""

import glob
import os
import geodraw
from pathlib import Path


def find_glb_files(dir_path):
    """Return sorted list of .glb file paths in dir_path."""
    pattern = os.path.join(dir_path, "*.glb")
    return sorted(glob.glob(pattern))


def main():
    app = geodraw.App(1280, 720, "Demo GLB Viewer")

    root = os.environ.get("GEODRAW_ROOT")
    if not root:
        raise RuntimeError("GEODRAW_ROOT not set")

    glb_dir = Path(root) / "data" / "glb_files"
    paths = find_glb_files(glb_dir)

    if not paths:
        print(f"Error: No .glb files found in {glb_dir}")
        print("Please ensure .glb files exist in data/glb_files/ directory")
        return

    print(f"Found {len(paths)} .glb files")

    # Load all models upfront
    models = []
    for path in paths:
        model = app.load_glb(path)
        if model is not None and model.is_valid:
            models.append(model)
            print(f"Loaded: {model.name}")
        else:
            print(f"Failed to load: {path}")
            models.append(None)

    valid_models = [m for m in models if m is not None]
    if not valid_models:
        print("Error: No models could be loaded")
        return

    # Mutable state (use lists so lambdas can mutate)
    state = {
        "index": 0,
        "alpha": [1.0],
        "show_bounds": [True],
    }

    def current_model():
        return models[state["index"]]

    def next_model():
        state["index"] = (state["index"] + 1) % len(models)
        m = current_model()
        print(f"Switched to: {m.name if m else '(invalid)'}")

    def prev_model():
        state["index"] = (state["index"] - 1) % len(models)
        m = current_model()
        print(f"Switched to: {m.name if m else '(invalid)'}")

    def select_model(idx_1based):
        idx = idx_1based - 1
        if 0 <= idx < len(models):
            state["index"] = idx
            m = current_model()
            print(f"Selected model {idx_1based}: {m.name if m else '(invalid)'}")

    def increase_alpha():
        state["alpha"][0] = min(1.0, state["alpha"][0] + 0.1)
        print(f"Model alpha: {state['alpha'][0]:.1f}")

    def decrease_alpha():
        state["alpha"][0] = max(0.0, state["alpha"][0] - 0.1)
        print(f"Model alpha: {state['alpha'][0]:.1f}")

    def toggle_bounds():
        state["show_bounds"][0] = not state["show_bounds"][0]

    # Register commands
    app.add_cmd("next-model",  next_model,  "Switch to next model",     key=geodraw.Key.RIGHT)
    app.add_cmd("prev-model",  prev_model,  "Switch to previous model", key=geodraw.Key.LEFT)

    app.add_cmd("select-model-1", lambda: select_model(1), "Select model 1", key=geodraw.Key.D1)
    app.add_cmd("select-model-2", lambda: select_model(2), "Select model 2", key=geodraw.Key.D2)
    app.add_cmd("select-model-3", lambda: select_model(3), "Select model 3", key=geodraw.Key.D3)
    app.add_cmd("select-model-4", lambda: select_model(4), "Select model 4", key=geodraw.Key.D4)
    app.add_cmd("select-model-5", lambda: select_model(5), "Select model 5", key=geodraw.Key.D5)
    app.add_cmd("select-model-6", lambda: select_model(6), "Select model 6", key=geodraw.Key.D6)
    app.add_cmd("select-model-7", lambda: select_model(7), "Select model 7", key=geodraw.Key.D7)
    app.add_cmd("select-model-8", lambda: select_model(8), "Select model 8", key=geodraw.Key.D8)

    app.add_cmd("increase-alpha", increase_alpha, "Increase transparency", key=geodraw.Key.EQUAL)
    app.add_cmd("decrease-alpha", decrease_alpha, "Decrease transparency", key=geodraw.Key.MINUS)
    app.add_cmd("toggle-bounds",  toggle_bounds,  "Toggle bounding box",   key=geodraw.Key.B)

    # Frame camera to first valid model
    first = current_model()
    if first is not None:
        bounds = first.bounds
        app.auto_frame(bounds[0], bounds[1])

    app.request_continuous_update("glb_viewer.display")

    @app.add_update_callback()
    def update(dt):
        scene = app.scene()
        scene.clear()

        model = current_model()
        if model is None or not model.is_valid:
            return

        scene.add_loaded_model(model, alpha=state["alpha"][0])

        if state["show_bounds"][0]:
            bounds = model.bounds
            scene.add_bbox(bounds[0], bounds[1],
                           color=(0.0, 1.0, 0.0), thickness=2.0)

    app.run()


if __name__ == "__main__":
    main()
