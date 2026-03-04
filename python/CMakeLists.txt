# Python bindings for GeoDraw
# Requires: pybind11, Python 3.8+, NumPy

cmake_minimum_required(VERSION 3.18)

# Find Python and pybind11
find_package(Python 3.8 COMPONENTS Interpreter Development NumPy REQUIRED)
find_package(pybind11 CONFIG REQUIRED)

# Create the Python module
pybind11_add_module(_geodraw
    src/bindings.cpp
)

# Link against geodraw library
target_link_libraries(_geodraw PRIVATE geodraw)

# Include directories
# Note: ${CMAKE_SOURCE_DIR}/include is intentionally omitted — geodraw target
# already exposes include/ as PUBLIC, so it's inherited automatically.
target_include_directories(_geodraw PRIVATE
    ${Python_NumPy_INCLUDE_DIRS}
)

# C++17 required
target_compile_features(_geodraw PRIVATE cxx_std_17)

# Output .so to build tree so `import geodraw` works without touching the source tree.
# Standalone:  PYTHONPATH=<build>/python
# Embedded:    PYTHONPATH=<ROOT_BUILD>/python
set_target_properties(_geodraw PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/python/geodraw"
)

# Ensure _geodraw.so can find libgeodraw.so at runtime without LD_LIBRARY_PATH.
# _geodraw.so lives at <build>/python/geodraw/; libgeodraw.so lives at <build>/.
# $ORIGIN/../.. (Linux) and @loader_path/../.. (macOS) both resolve to <build>/.
if(APPLE)
    set_target_properties(_geodraw PROPERTIES
        BUILD_RPATH "@loader_path/../.."
    )
elseif(UNIX)
    set_target_properties(_geodraw PROPERTIES
        BUILD_RPATH "$ORIGIN/../.."
    )
endif()

# Copy __init__.py to build tree alongside the .so
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/geodraw/__init__.py"
    "${CMAKE_BINARY_DIR}/python/geodraw/__init__.py"
    COPYONLY
)
