# GeoDraw Python Bindings

Python bindings for GeoDraw visualization, intended for debugging and visualization in Python code (e.g., PyTorch RL environments).

## Requirements

- Python 3.8+
- NumPy
- pybind11 (for building)
- GeoDraw C++ library (built as shared library)

## Building

From the geodraw root directory:

```bash
# Configure with Python bindings enabled
cmake -B build -DCMAKE_BUILD_TYPE=Release -DGEODRAW_WITH_PYTHON=ON

# Build
cmake --build build --target _geodraw
```

The Python module is built to `build/python/geodraw/_geodraw.cpython-*.so`.

### Building on Linux

Install the required system packages first:

```bash
sudo apt install build-essential cmake libgl1-mesa-dev libglfw3-dev \
    python3-dev python3-numpy pybind11-dev
```

> **Note:** If you previously did an in-source build (before the build directory was set
> up correctly), you may have a stale `.so` file at `python/geodraw/_geodraw*.so`. Delete
> it before rebuilding, otherwise Python will import the old artifact instead of the
> freshly-built one:
> ```bash
> rm -f python/geodraw/_geodraw*.so
> ```

## Running demos

```bash
PYTHONPATH=build/python python3 python/demos/demo_hello_world.py
```

If `libgeodraw.so.1` is not found, also set `LD_LIBRARY_PATH`:

```bash
LD_LIBRARY_PATH=build PYTHONPATH=build/python python3 python/demos/demo_hello_world.py
```

## Usage

Add the `build/python` directory to your Python path, then import `geodraw`:

```python
import sys
sys.path.insert(0, '/path/to/geodraw/build/python')

import geodraw
import numpy as np

# Create a scene
scene = geodraw.Scene()

# Add geometry using plain Python data
scene.add_point((0, 0, 0), radius=0.5, color=(1, 0, 0))
scene.add_line((0, 0, 0), (5, 0, 0), thickness=2.0)
scene.add_bbox((-1, -1, 0), (1, 1, 2), color=(0, 1, 0))

# NumPy arrays work too
points = np.array([[0, 0, 1], [1, 0, 1], [2, 0, 1]], dtype=np.float64)
scene.add_points(points, radius=0.3, color=(1, 0.5, 0))

# Show the scene (blocks until window is closed)
geodraw.show(scene, title="Debug View")
```

## API Reference

### Scene Methods

| Method | Description |
|--------|-------------|
| `add_point(pos, radius=0.5, color=(1,1,1), alpha=1.0)` | Add a point (rendered as sphere) |
| `add_points(points, ...)` | Add multiple points from Nx3 numpy array |
| `add_line(start, end, thickness=1.0, color=(1,1,1), alpha=1.0)` | Add a line segment |
| `add_lines(lines, ...)` | Add multiple lines from Nx2x3 numpy array |
| `add_polyline(points, thickness=1.0, color=(1,1,1), alpha=1.0)` | Add connected line segments |
| `add_bbox(min_corner, max_corner, ..., filled=False)` | Add axis-aligned bounding box |
| `add_oriented_bbox(center, half_extents, rotation=None, ..., filled=False)` | Add oriented bounding box |
| `add_triangle(p0, p1, p2, color=(1,1,1), alpha=1.0, filled=True)` | Add a triangle |
| `add_mesh(vertices, indices, color=(1,1,1), alpha=1.0, filled=True)` | Add triangle mesh |
| `add_axes(position, scale=1.0, thickness=2.0)` | Add coordinate axes (RGB=XYZ) |
| `add_pose(position, rotation, scale=1.0, thickness=2.0)` | Add pose visualization |
| `add_ground(center, width, height, color=(0.5,0.5,0.5), alpha=1.0)` | Add ground plane |
| `clear()` | Remove all geometry |

### Module Functions

| Function | Description |
|----------|-------------|
| `show(scene, title="GeoDraw", width=1280, height=720)` | Show scene in window (blocks) |
| `show([(label, scene), ...], ...)` | Show multiple scenes with toggle (keys 1-9) |

### Data Formats

- **Positions**: `(x, y, z)` tuple/list or numpy array
- **Colors**: `(r, g, b)` tuple with values 0-1
- **Vertices**: Nx3 numpy array (`float64`)
- **Indices**: 1D numpy array (`uint32`), length multiple of 3
- **Rotation matrices**: 3x3 numpy array (`float64`)

## Viewer Controls

- **Mouse drag**: Rotate camera (orbit)
- **Mouse scroll**: Zoom in/out
- **Right-click drag**: Pan camera
- **Q / ESC**: Close window
- **H**: Show all commands
- **1-9**: Toggle scenes (when showing multiple)

## Design Philosophy

> "GeoDraw Python bindings are for debugging and visualization, not for building applications."

The API is intentionally simple:
- Pass plain Python data (tuples, lists, numpy arrays)
- C++ owns all geometry types
- No type wrappers exposed to Python
- Blocking `show()` is acceptable for debug use
