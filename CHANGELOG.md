# Changelog

All notable changes to GeoDraw will be documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [0.1.1] — 2026-03-05

### Added
- Linux shared libraries (`.so` files) added under `lib/`, enabling Linux
  targets to link against GeoDraw without building from source.

### Fixed
- Improved submodule support: GeoDraw can now be added as a git submodule with
  no extra CMake configuration required.

## [0.1.0] — 2026-03-04

### Added
- Initial public release of the GeoDraw SDK (pre-built shared libraries for
  macOS and Linux).
- CMake integration: `add_subdirectory` and `FetchContent` both supported;
  linking against `geodraw` propagates headers and transitive dependencies
  (glm, glfw, nlohmann_json, imgui).
- Build options: `GEODRAW_WITH_IMGUI`, `GEODRAW_WITH_PYTHON`,
  `GEODRAW_BUILD_APPS`, `GEODRAW_BUILD_TESTS`, `GEODRAW_USE_ASAN`.
- Demo programs (`demo_hello_world`, `demo_add_cmd`, `demo_minor_mode`, …).
- First-party applications: `earth_viewer`, `scenario_generator`, and more.
- Optional Python bindings via pybind11.
- `README.md` with prerequisites, build instructions, and CMake integration
  guide.
