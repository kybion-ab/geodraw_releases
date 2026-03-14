# geodraw_releases

GeoDraw is a modern C++ library for building interactive visualization tools
for geometry and algorithms with minimal boilerplate.

It provides a simple, callback-based API for rendering geometric primitives
(lines, triangles, meshes, point clouds) in interactive scenes with an
integrated camera system, command palette, and optional Dear ImGui support.

GeoDraw is designed to be embedded as a reusable component in larger systems,
or used to rapidly construct small, focused visualization tools.

## Features

- Lightweight callback-based rendering API (no subclassing required)
- Priority-based draw/update callback system
- Integrated 3D camera with orbit, pan, and zoom
- Command palette with key binding support
- Dear ImGui panel integration
- Earth/map tile rendering with OpenStreetMap support
- Scenario and trajectory visualization for autonomous driving research
- Python bindings via pybind11
- AddressSanitizer-ready build option

## Prerequisites

**macOS:**
```bash
brew install cmake glm nlohmann-json glfw
# Optional: Python bindings
brew install python pybind11
```

**Linux (Ubuntu/Debian):**
```bash
sudo apt install cmake libglm-dev nlohmann-json3-dev libglfw3-dev
# Optional: Python bindings
sudo apt install python3-dev pybind11-dev
```

## Building

```bash
cmake -B build
cmake --build build
```

Output binaries land in `build/bin/`. To disable Python bindings or ImGui:
```bash
cmake -B build -DGEODRAW_WITH_PYTHON=OFF -DGEODRAW_WITH_IMGUI=OFF
```

Full list of options:

| Option | Default | Description |
|---|---|---|
| `GEODRAW_WITH_IMGUI` | ON | Build demo/app targets that use ImGui |
| `GEODRAW_WITH_PYTHON` | ON | Build Python bindings |
| `GEODRAW_BUILD_APPS` | ON | Build applications and demos |
| `GEODRAW_BUILD_TESTS` | ON | Build tests |
| `GEODRAW_USE_ASAN` | OFF | Enable AddressSanitizer |

## Running a demo

```bash
./build/bin/demo_hello_world
```

## Using GeoDraw in your own CMake project

**Option A — add_subdirectory:**
```cmake
add_subdirectory(path/to/geodraw_releases)
target_link_libraries(my_app PRIVATE geodraw)
```

**Option B — FetchContent:**
```cmake
include(FetchContent)
FetchContent_Declare(geodraw GIT_REPOSITORY <repo-url> GIT_TAG main)
FetchContent_MakeAvailable(geodraw)
target_link_libraries(my_app PRIVATE geodraw)
```

In both cases, linking against `geodraw` automatically propagates include paths and transitive dependencies (glm, glfw, nlohmann_json, imgui).

## Repository layout

```
include/   — public headers
lib/       — pre-built shared libraries (macOS / Linux)
demos/     — example source files
app/       — first-party applications
tests/     — unit and integration tests
python/    — Python bindings
data/      — assets used by demos and tests
external/  — bundled third-party headers (imgui, protozero, vtzero)
```
