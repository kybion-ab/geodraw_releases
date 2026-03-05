cmake_minimum_required(VERSION 3.18)
project(GeoDraw VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ----------------------
# Options
# ----------------------
option(GEODRAW_WITH_IMGUI   "Build demo/app targets that use ImGui" ON)
option(GEODRAW_USE_ASAN     "Enable AddressSanitizer"               OFF)
option(GEODRAW_WITH_PYTHON  "Build Python bindings (requires pybind11)" ON)

# ----------------------
# Determine if top-level project
# ----------------------
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(_geodraw_is_top_level TRUE)
else()
    set(_geodraw_is_top_level FALSE)
endif()

option(GEODRAW_BUILD_APPS   "Build geodraw applications" ${_geodraw_is_top_level})
option(GEODRAW_BUILD_TESTS  "Build geodraw tests"        ${_geodraw_is_top_level})

# ----------------------
# Dependencies
# ----------------------
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)
find_package(glm REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(CURL REQUIRED)
find_package(ZLIB REQUIRED)

# ----------------------
# Pre-built GeoDraw library (binary-only)
# ----------------------
add_library(geodraw SHARED IMPORTED GLOBAL)
if(APPLE)
      set_target_properties(geodraw PROPERTIES
        IMPORTED_LOCATION "${CMAKE_CURRENT_LIST_DIR}/lib/macos/libgeodraw.1.dylib"
        IMPORTED_SONAME   "libgeodraw.1.dylib"
    )
    set(CMAKE_BUILD_RPATH "${CMAKE_CURRENT_LIST_DIR}/lib/macos")
    set(GEODRAW_LIB "${CMAKE_CURRENT_LIST_DIR}/lib/macos/libgeodraw.1.dylib")
elseif(UNIX)
    set_target_properties(geodraw PROPERTIES
        IMPORTED_LOCATION "${CMAKE_CURRENT_LIST__DIR}/lib/linux/libgeodraw.so.1"
        IMPORTED_SONAME   "libgeodraw.so.1"
    )
    set(CMAKE_BUILD_RPATH "${CMAKE_CURRENT_LIST_DIR}/lib/linux")
    set(GEODRAW_LIB "${CMAKE_CURRENT_LIST_DIR}/lib/macos/libgeodraw.1.dylib")
endif()

set_target_properties(geodraw PROPERTIES
    IMPORTED_LOCATION "${GEODRAW_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES
        "${CMAKE_CURRENT_LIST_DIR}/include;\
${CMAKE_CURRENT_LIST_DIR}/external/imgui;\
${CMAKE_CURRENT_LIST_DIR}/external/imgui/backends"
    INTERFACE_LINK_LIBRARIES "glm::glm;nlohmann_json::nlohmann_json;glfw"
)

# ----------------------
# ImGui static library (hidden symbols)
# ----------------------
if(GEODRAW_WITH_IMGUI)
    add_library(imgui STATIC
        external/imgui/imgui.cpp
        external/imgui/imgui_draw.cpp
        external/imgui/imgui_widgets.cpp
        external/imgui/imgui_tables.cpp
    )
    target_include_directories(imgui PUBLIC
        external/imgui
        external/imgui/backends
    )
    set_property(TARGET geodraw APPEND PROPERTY
        INTERFACE_LINK_LIBRARIES imgui)
    set_property(TARGET geodraw APPEND PROPERTY
        INTERFACE_COMPILE_DEFINITIONS GEODRAW_HAS_IMGUI)
endif()

# ----------------------
# Tests
# ----------------------
if(GEODRAW_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.7.1
    )
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    include(CTest)
    include(Catch)

    set(UNIT_TESTS
        camera_test clustering_test geojson_test test_graph test_lane_graph
        earth_coords_test earth_cache_test log_test
    )
    set(IMAGE_TEST image_test)
    set(TOOLS inspect_glb measure_model)
    set(INTERACTIVE_TESTS marching_squares_test)
    if(GEODRAW_WITH_IMGUI)
        list(APPEND INTERACTIVE_TESTS shape_test)
    endif()

    foreach(test ${UNIT_TESTS})
        add_executable(${test} tests/${test}.cpp)
        target_link_libraries(${test} PRIVATE geodraw Catch2::Catch2WithMain)
        set_target_properties(${test} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tests")
        if(GEODRAW_USE_ASAN)
            target_compile_options(${test} PRIVATE -fsanitize=address -g -O1)
            target_link_options(${test} PRIVATE -fsanitize=address)
        endif()
        catch_discover_tests(${test})
    endforeach()

    add_executable(${IMAGE_TEST} tests/${IMAGE_TEST}.cpp)
    target_link_libraries(${IMAGE_TEST} PRIVATE geodraw Catch2::Catch2)
    target_compile_definitions(${IMAGE_TEST} PRIVATE
        GEODRAW_TEST_IMAGE_PATH="${CMAKE_CURRENT_LIST_DIR}/data/images/baboon.png")
    set_target_properties(${IMAGE_TEST} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tests")
    if(GEODRAW_USE_ASAN)
        target_compile_options(${IMAGE_TEST} PRIVATE -fsanitize=address -g -O1)
        target_link_options(${IMAGE_TEST} PRIVATE -fsanitize=address)
    endif()
    catch_discover_tests(${IMAGE_TEST})

    foreach(tool ${TOOLS})
        add_executable(${tool} tests/${tool}.cpp)
        target_link_libraries(${tool} PRIVATE geodraw)
        set_target_properties(${tool} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tools")
        if(GEODRAW_USE_ASAN)
            target_compile_options(${tool} PRIVATE -fsanitize=address -g -O1)
            target_link_options(${tool} PRIVATE -fsanitize=address)
        endif()
    endforeach()

    foreach(test ${INTERACTIVE_TESTS})
        add_executable(${test} tests/${test}.cpp)
        target_link_libraries(${test} PRIVATE geodraw)
        set_target_properties(${test} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tests")
        if(GEODRAW_USE_ASAN)
            target_compile_options(${test} PRIVATE -fsanitize=address -g -O1)
            target_link_options(${test} PRIVATE -fsanitize=address)
        endif()
    endforeach()
endif() # GEODRAW_BUILD_TESTS

# ----------------------
# Applications / demos
# ----------------------
if(GEODRAW_BUILD_APPS)
    set(APPLICATIONS
        vehicle_glb_demo gizmo_demo earth_viewer
        scenario_generator debug_goals
    )
    set(DEMOS
        demo_hello_world demo_add_cmd demo_minor_mode demo_imgui_panel
        demo_tooltip demo_editable demo_camera_trajectory demo_video_capture
        demo_texture demo_quick_view demo_shape_edit demo_multiple_scenes
        demo_glb_viewer
    )
    if(GEODRAW_WITH_IMGUI)
        list(APPEND APPLICATIONS scenario_viewer rl_gui)
    endif()

    foreach(application ${APPLICATIONS})
        add_executable(${application} app/${application}.cpp)
        set_target_properties(${application} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
        target_link_libraries(${application} PRIVATE geodraw)
        if(GEODRAW_USE_ASAN)
            target_compile_options(${application} PRIVATE -fsanitize=address -g -O1)
            target_link_options(${application} PRIVATE -fsanitize=address)
        endif()
    endforeach()

    foreach(demo ${DEMOS})
        add_executable(${demo} demos/${demo}.cpp)
        set_target_properties(${demo} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
        target_link_libraries(${demo} PRIVATE geodraw)
        if(GEODRAW_USE_ASAN)
            target_compile_options(${demo} PRIVATE -fsanitize=address -g -O1)
            target_link_options(${demo} PRIVATE -fsanitize=address)
        endif()
    endforeach()

    if(GEODRAW_WITH_IMGUI)
        target_sources(rl_gui PRIVATE app/rl_gui_ui.cpp)
    endif()
endif() # GEODRAW_BUILD_APPS

# ----------------------
# Installation rules (top-level only)
# ----------------------
if(_geodraw_is_top_level)
    if(APPLE)
        install(FILES
            lib/macos/libgeodraw.0.1.dylib
            lib/macos/libgeodraw.1.dylib
            lib/macos/libgeodraw.dylib
            DESTINATION lib)
    elseif(UNIX)
        install(FILES
            lib/linux/libgeodraw.so.0.1
            lib/linux/libgeodraw.so.1
            lib/linux/libgeodraw.so
            DESTINATION lib)
    endif()
    install(DIRECTORY include/ DESTINATION include)
endif()

# ----------------------
# Python bindings (optional)
# ----------------------
if(GEODRAW_WITH_PYTHON)
    add_subdirectory(python)
endif()
