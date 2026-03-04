/*******************************************************************************
 * File: bindings.cpp
 *
 * Description: Python bindings for GeoDraw visualization.
 * Provides two APIs:
 *
 *   QuickDraw (original): Build a scene, call show(), window closes.
 *     scene = geodraw.Scene(); scene.add_point(...); geodraw.show(scene)
 *
 *   App (full loop): Event-driven callbacks, keybindings, ImGui panels.
 *     app = geodraw.App(1280, 720, "title")
 *     @app.add_update_callback()
 *     def update(dt): ...
 *     app.run()
 *
 ******************************************************************************/

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <pybind11/functional.h>

#include "geodraw/scene/scene.hpp"
#include "geodraw/ui/quick_view.hpp"
#include "geodraw/geometry/geometry.hpp"
#include "geodraw/app/app.hpp"
#include "geodraw/modules/gltf/gltf_loader.hpp"

#ifdef GEODRAW_HAS_IMGUI
#include "geodraw/ui/imgui_plugin.h"
#include "geodraw/ui/imgui_helpers.hpp"
#include "geodraw/ui/tooltip_system.hpp"
#endif

#include <GLFW/glfw3.h>

#include <vector>
#include <tuple>
#include <string>
#include <stdexcept>
#include <iostream>
#include <memory>

namespace py = pybind11;

namespace {

// =============================================================================
// Helper functions for converting Python data to C++ types
// =============================================================================

geodraw::Pos3 toPos3(const py::sequence& seq) {
    if (py::len(seq) < 3) {
        throw std::invalid_argument("Position must have at least 3 elements (x, y, z)");
    }
    return geodraw::Pos3(
        seq[0].cast<double>(),
        seq[1].cast<double>(),
        seq[2].cast<double>()
    );
}

glm::vec3 toColor(const py::sequence& seq) {
    if (py::len(seq) < 3) {
        throw std::invalid_argument("Color must have at least 3 elements (r, g, b)");
    }
    return glm::vec3(
        seq[0].cast<float>(),
        seq[1].cast<float>(),
        seq[2].cast<float>()
    );
}

std::vector<geodraw::Pos3> toPos3Vector(py::array_t<double> arr) {
    auto buf = arr.request();
    if (buf.ndim != 2 || buf.shape[1] < 3) {
        throw std::invalid_argument("Array must be Nx3 (N points, 3 coordinates each)");
    }
    std::vector<geodraw::Pos3> result;
    result.reserve(buf.shape[0]);
    auto ptr = static_cast<double*>(buf.ptr);
    size_t stride = buf.shape[1];
    for (ssize_t i = 0; i < buf.shape[0]; ++i) {
        result.emplace_back(ptr[i * stride + 0], ptr[i * stride + 1], ptr[i * stride + 2]);
    }
    return result;
}

std::vector<geodraw::Pos3> toPos3VectorFromList(const py::list& lst) {
    std::vector<geodraw::Pos3> result;
    result.reserve(py::len(lst));
    for (const auto& item : lst) {
        result.push_back(toPos3(item.cast<py::sequence>()));
    }
    return result;
}

std::vector<unsigned int> toIndexVector(py::array_t<uint32_t> arr) {
    auto buf = arr.request();
    if (buf.ndim != 1) {
        throw std::invalid_argument("Index array must be 1-dimensional");
    }
    auto ptr = static_cast<uint32_t*>(buf.ptr);
    return std::vector<unsigned int>(ptr, ptr + buf.shape[0]);
}

} // anonymous namespace

// =============================================================================
// PyTooltipBuilder - fluent tag builder for Python geometry tooltip metadata
// =============================================================================

// Python-facing fluent builder for geometry tooltip tagging.
// Stores DrawCmd* (into Scene::cmds). Only valid until scene.clear().
class PyTooltipBuilder {
public:
    explicit PyTooltipBuilder(geodraw::DrawCmd* cmd) : cmd_(cmd) {}
    PyTooltipBuilder with_id(uint64_t id) {
        if (cmd_) cmd_->objectId = id;
        return *this;
    }
    PyTooltipBuilder with_type(const std::string& type) {
        if (cmd_) cmd_->objectType = type;
        return *this;
    }
private:
    geodraw::DrawCmd* cmd_;
};

// Context passed to Python tooltip provider callbacks.
struct PyTooltipContext {
    uint64_t object_id;
    std::string object_type;
    std::tuple<float, float, float> world_point;
};

// =============================================================================
// PyLoadedModel - Python wrapper for gltf::LoadedModel
// =============================================================================

class PyLoadedModel {
public:
    explicit PyLoadedModel(std::shared_ptr<geodraw::gltf::LoadedModel> model)
        : model_(std::move(model)) {}

    bool is_valid() const { return model_ && model_->isValid(); }

    std::string name() const { return model_ ? model_->name : ""; }

    std::string model_type() const {
        if (!model_) return "unknown";
        switch (model_->modelType) {
            case geodraw::gltf::ModelType::VEHICLE:    return "vehicle";
            case geodraw::gltf::ModelType::CYCLIST:    return "cyclist";
            case geodraw::gltf::ModelType::PEDESTRIAN: return "pedestrian";
            default:                                    return "unknown";
        }
    }

    py::tuple bounds() const {
        if (!model_) {
            return py::make_tuple(py::make_tuple(0.0, 0.0, 0.0),
                                  py::make_tuple(0.0, 0.0, 0.0));
        }
        const auto& mn = model_->bounds.min.pos;
        const auto& mx = model_->bounds.max.pos;
        return py::make_tuple(py::make_tuple(mn.x, mn.y, mn.z),
                              py::make_tuple(mx.x, mx.y, mx.z));
    }

    const geodraw::gltf::LoadedModel& get() const { return *model_; }

private:
    std::shared_ptr<geodraw::gltf::LoadedModel> model_;
};

// =============================================================================
// PyScene - Python wrapper for Scene (owning + reference-proxy modes)
// =============================================================================

class PyScene {
public:
    PyScene() = default;

    // Reference proxy mode: wraps an externally-owned Scene*.
    // The caller (PyApp) is responsible for the Scene's lifetime.
    static PyScene fromRef(geodraw::Scene* ref) {
        PyScene s;
        s.ref_ = ref;
        return s;
    }

    void clear() { sceneRef().clear(); }

    PyTooltipBuilder add_point(py::sequence pos,
                               float radius = 0.5f,
                               py::sequence color = py::make_tuple(1.0f, 1.0f, 1.0f),
                               float alpha = 1.0f) {
        auto b = sceneRef().Add(toPos3(pos), radius, alpha, toColor(color));
        return PyTooltipBuilder{b.get()};
    }

    void add_points(py::array_t<double> points,
                    float radius = 0.5f,
                    py::sequence color = py::make_tuple(1.0f, 1.0f, 1.0f),
                    float alpha = 1.0f) {
        auto positions = toPos3Vector(points);
        glm::vec3 col = toColor(color);
        for (const auto& pos : positions) {
            sceneRef().Add(pos, radius, alpha, col);
        }
    }

    PyTooltipBuilder add_line(py::sequence start,
                              py::sequence end,
                              float thickness = 1.0f,
                              py::sequence color = py::make_tuple(1.0f, 1.0f, 1.0f),
                              float alpha = 1.0f) {
        geodraw::Line3 line(toPos3(start), toPos3(end));
        auto b = sceneRef().Add(line, thickness, geodraw::LineStyle::Solid, alpha, toColor(color));
        return PyTooltipBuilder{b.get()};
    }

    PyTooltipBuilder add_polyline(py::object points,
                                  float thickness = 1.0f,
                                  py::sequence color = py::make_tuple(1.0f, 1.0f, 1.0f),
                                  float alpha = 1.0f) {
        geodraw::Polyline3 polyline;
        if (py::isinstance<py::array>(points)) {
            polyline.points = toPos3Vector(points.cast<py::array_t<double>>());
        } else {
            polyline.points = toPos3VectorFromList(points.cast<py::list>());
        }
        auto b = sceneRef().Add(polyline, thickness, geodraw::LineStyle::Solid, alpha, toColor(color));
        return PyTooltipBuilder{b.get()};
    }

    void add_lines(py::array_t<double> lines,
                   float thickness = 1.0f,
                   py::sequence color = py::make_tuple(1.0f, 1.0f, 1.0f),
                   float alpha = 1.0f) {
        auto buf = lines.request();
        if (buf.ndim != 3 || buf.shape[1] != 2 || buf.shape[2] < 3) {
            throw std::invalid_argument("Lines array must be Nx2x3 (N lines, 2 endpoints, 3 coords)");
        }
        auto ptr = static_cast<double*>(buf.ptr);
        glm::vec3 col = toColor(color);
        for (ssize_t i = 0; i < buf.shape[0]; ++i) {
            size_t base = i * 6;
            geodraw::Line3 line(
                geodraw::Pos3(ptr[base + 0], ptr[base + 1], ptr[base + 2]),
                geodraw::Pos3(ptr[base + 3], ptr[base + 4], ptr[base + 5])
            );
            sceneRef().Add(line, thickness, geodraw::LineStyle::Solid, alpha, col);
        }
    }

    PyTooltipBuilder add_bbox(py::sequence min_corner,
                              py::sequence max_corner,
                              float thickness = 1.0f,
                              py::sequence color = py::make_tuple(1.0f, 1.0f, 1.0f),
                              float alpha = 1.0f,
                              bool filled = false) {
        geodraw::BBox3 bbox(toPos3(min_corner), toPos3(max_corner));
        auto b = sceneRef().Add(bbox, thickness, geodraw::LineStyle::Solid, alpha, toColor(color), true, filled);
        return PyTooltipBuilder{b.get()};
    }

    PyTooltipBuilder add_oriented_bbox(py::sequence center,
                                       py::sequence half_extents,
                                       py::object rotation = py::none(),
                                       float thickness = 1.0f,
                                       py::sequence color = py::make_tuple(1.0f, 1.0f, 1.0f),
                                       float alpha = 1.0f,
                                       bool filled = false) {
        auto c = toPos3(center);
        auto he = toPos3(half_extents);

        geodraw::RBox3 rbox;
        rbox.pose.t = c;

        if (!rotation.is_none()) {
            auto rot = rotation.cast<py::array_t<double>>();
            auto buf = rot.request();
            if (buf.ndim != 2 || buf.shape[0] != 3 || buf.shape[1] != 3) {
                throw std::invalid_argument("Rotation must be 3x3 matrix");
            }
            auto ptr = static_cast<double*>(buf.ptr);
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) {
                    rbox.pose.R[i][j] = static_cast<float>(ptr[j * 3 + i]);
                }
            }
        } else {
            rbox.pose.R = glm::mat3(1.0f);
        }

        rbox.bbox.min = geodraw::Pos3(-he.pos.x, -he.pos.y, -he.pos.z);
        rbox.bbox.max = geodraw::Pos3(he.pos.x, he.pos.y, he.pos.z);
        auto b = sceneRef().Add(rbox, thickness, geodraw::LineStyle::Solid, alpha, toColor(color), true, filled);
        return PyTooltipBuilder{b.get()};
    }

    PyTooltipBuilder add_triangle(py::sequence vertices,
                                  py::sequence color = py::make_tuple(1.0f, 1.0f, 1.0f),
                                  float alpha = 1.0f,
                                  bool filled = true) {
        geodraw::Triangle3 tri(toPos3(vertices[0]), toPos3(vertices[1]), toPos3(vertices[2]));
        auto b = sceneRef().Add(tri, 1.0f, geodraw::LineStyle::Solid, alpha, toColor(color), true, filled);
        return PyTooltipBuilder{b.get()};
    }

    PyTooltipBuilder add_mesh(py::array_t<double> vertices,
                              py::array_t<uint32_t> indices,
                              py::sequence color = py::make_tuple(1.0f, 1.0f, 1.0f),
                              float alpha = 1.0f,
                              bool filled = true) {
        geodraw::Mesh3 mesh;
        mesh.vertices = toPos3Vector(vertices);
        mesh.indices = toIndexVector(indices);
        if (mesh.indices.size() % 3 != 0) {
            throw std::invalid_argument("Index count must be multiple of 3 (triangles)");
        }
        auto b = sceneRef().Add(mesh, 1.0f, geodraw::LineStyle::Solid, alpha, toColor(color), true, filled);
        return PyTooltipBuilder{b.get()};
    }

    void add_axes(py::sequence position,
                  float scale = 1.0f,
                  float thickness = 2.0f) {
        auto pos = toPos3(position);
        sceneRef().Add(geodraw::Line3(pos, geodraw::Pos3(pos.pos.x + scale, pos.pos.y, pos.pos.z)),
                       thickness, geodraw::LineStyle::Solid, 1.0f, glm::vec3(1.0f, 0.0f, 0.0f));
        sceneRef().Add(geodraw::Line3(pos, geodraw::Pos3(pos.pos.x, pos.pos.y + scale, pos.pos.z)),
                       thickness, geodraw::LineStyle::Solid, 1.0f, glm::vec3(0.0f, 1.0f, 0.0f));
        sceneRef().Add(geodraw::Line3(pos, geodraw::Pos3(pos.pos.x, pos.pos.y, pos.pos.z + scale)),
                       thickness, geodraw::LineStyle::Solid, 1.0f, glm::vec3(0.0f, 0.0f, 1.0f));
    }

    void add_pose(py::sequence position,
                  py::array_t<double> rotation,
                  float scale = 1.0f,
                  float thickness = 2.0f) {
        geodraw::Pose3 pose;
        pose.t = toPos3(position);
        auto buf = rotation.request();
        if (buf.ndim != 2 || buf.shape[0] != 3 || buf.shape[1] != 3) {
            throw std::invalid_argument("Rotation must be 3x3 matrix");
        }
        auto ptr = static_cast<double*>(buf.ptr);
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                pose.R[i][j] = static_cast<float>(ptr[j * 3 + i]);
            }
        }
        sceneRef().Add(pose, scale, thickness);
    }

    PyTooltipBuilder add_ground(py::sequence center,
                                double width,
                                double height,
                                py::sequence color = py::make_tuple(0.5f, 0.5f, 0.5f),
                                float alpha = 1.0f) {
        auto c = toPos3(center);
        geodraw::Ground ground;
        ground.pose.x = c.pos.x;
        ground.pose.y = c.pos.y;
        ground.pose.z = c.pos.z;
        ground.pose.yaw = 0.0;
        ground.pose.pitch = 0.0;
        ground.pose.roll = 0.0;
        ground.width = width;
        ground.height = height;
        auto b = sceneRef().Add(ground, 1.0f, 0.0f, geodraw::LineStyle::Solid, alpha,
                                toColor(color), glm::vec3(0, 0, 1), true, true);
        return PyTooltipBuilder{b.get()};
    }

    void add_loaded_model(const PyLoadedModel& model, float alpha = 1.0f) {
        const auto& lm = model.get();
        for (size_t i = 0; i < lm.meshes.size(); ++i) {
            GLuint tex   = i < lm.textureHandles.size() ? lm.textureHandles[i] : 0;
            glm::vec3 color = i < lm.colors.size() ? lm.colors[i] : glm::vec3(1.0f);
            sceneRef().Add(lm.meshes[i], 1.0f, geodraw::LineStyle::Solid,
                           alpha, color, /*depthTest=*/true, /*filled=*/true, tex);
        }
    }

    const geodraw::Scene& getScene() const {
        return ref_ ? *ref_ : scene_;
    }

    geodraw::Scene& getMutableScene() { return sceneRef(); }

private:
    geodraw::Scene  scene_;          // owning mode (QuickDraw)
    geodraw::Scene* ref_ = nullptr;  // reference proxy mode (App)

    geodraw::Scene& sceneRef() {
        return ref_ ? *ref_ : scene_;
    }
    const geodraw::Scene& sceneRef() const {
        return ref_ ? *ref_ : scene_;
    }
};

// =============================================================================
// PyToggle - mutable bool wrapper (since Python booleans are immutable)
// =============================================================================

struct PyToggle {
    bool value = false;
};

// =============================================================================
// PyGui - ImGui proxy for Python callbacks (Phase 2)
// =============================================================================

#ifdef GEODRAW_HAS_IMGUI
class PyGui {
public:
    bool begin(std::string title) {
        return geodraw::imgui::begin(title.c_str());
    }

    void end() {
        geodraw::imgui::end();
    }

    void text(std::string s) {
        geodraw::imgui::text(s.c_str());
    }

    bool button(std::string label) {
        return geodraw::imgui::button(label.c_str());
    }

    // Returns (changed, new_value)
    py::tuple checkbox(std::string label, bool v) {
        auto [changed, nv] = geodraw::imgui::checkbox(label.c_str(), v);
        return py::make_tuple(changed, nv);
    }

    // Returns (changed, new_value)
    py::tuple slider_float(std::string label, float v, float v_min, float v_max) {
        auto [changed, nv] = geodraw::imgui::slider_float(label.c_str(), v, v_min, v_max);
        return py::make_tuple(changed, nv);
    }

    // Returns (changed, new_text)
    py::tuple input_text(std::string label, std::string s) {
        auto [changed, ns] = geodraw::imgui::input_text(label.c_str(), s);
        return py::make_tuple(changed, ns);
    }

    // Returns (changed, new_value)
    py::tuple slider_int(std::string label, int v, int v_min, int v_max) {
        auto [changed, nv] = geodraw::imgui::slider_int(label.c_str(), v, v_min, v_max);
        return py::make_tuple(changed, nv);
    }

    // Returns (changed, new_value)
    py::tuple input_int(std::string label, int v) {
        auto [changed, nv] = geodraw::imgui::input_int(label.c_str(), v);
        return py::make_tuple(changed, nv);
    }

    void begin_disabled()  { geodraw::imgui::begin_disabled(); }
    void end_disabled()    { geodraw::imgui::end_disabled(); }

    void text_colored(std::string s, float r, float g, float b) {
        geodraw::imgui::text_colored(s.c_str(), r, g, b);
    }

    void text_disabled(std::string s) {
        geodraw::imgui::text_disabled(s.c_str());
    }

    void separator()                              { geodraw::imgui::separator(); }
    void same_line()                              { geodraw::imgui::same_line(); }
    void spacing()                                { geodraw::imgui::spacing(); }
    void set_next_window_pos(float x, float y)    { geodraw::imgui::set_next_window_pos(x, y); }
    void set_next_window_size(float w, float h)   { geodraw::imgui::set_next_window_size(w, h); }

    bool tree_node(std::string label) {
        return geodraw::imgui::tree_node(label.c_str());
    }

    void tree_pop() { geodraw::imgui::tree_pop(); }

    bool collapsing_header(std::string label) {
        return geodraw::imgui::collapsing_header(label.c_str());
    }
};
#endif // GEODRAW_HAS_IMGUI

// =============================================================================
// PyApp - Python wrapper for geodraw::App (full event-loop support)
// =============================================================================

class PyApp {
public:
    PyApp(int width, int height, std::string title)
        : app_(width, height, title.c_str()) {
        // Add a default scene so scene(0) works without explicit setup.
        app_.addScene();
    }

    // Run the main loop. Releases the GIL so the event loop can proceed.
    void run() {
        py::gil_scoped_release release;
        app_.run();
    }

    void quit() { app_.quit(); }

    // -------------------------------------------------------------------------
    // Commands and toggles
    // -------------------------------------------------------------------------

    /**
     * Register a command.
     * @param mode Optional MinorMode pointer (nullptr = global command)
     */
    void add_cmd(std::string name, py::function fn, std::string doc,
                 int key = -1, int mods = 0,
                 geodraw::MinorMode* mode = nullptr) {
        auto cb = makePyCb(fn);
        if (mode) {
            app_.addCmd(name.c_str(), cb, doc.c_str(), key, mods, true, *mode);
        } else {
            app_.addCmd(name.c_str(), cb, doc.c_str(), key, mods);
        }
    }

    /**
     * Register a toggle. Returns a PyToggle whose .value is updated when
     * the key is pressed. The PyApp holds a shared_ptr to keep it alive.
     * @param mode Optional MinorMode pointer (nullptr = global toggle)
     */
    std::shared_ptr<PyToggle> add_toggle(std::string name, std::string doc,
                                          int key = -1, int mods = 0,
                                          geodraw::MinorMode* mode = nullptr) {
        auto toggle = std::make_shared<PyToggle>();
        toggles_.push_back(toggle);
        if (mode) {
            app_.addToggle(name.c_str(), toggle->value, doc.c_str(), key, mods, true, *mode);
        } else {
            app_.addToggle(name.c_str(), toggle->value, doc.c_str(), key, mods);
        }
        return toggle;
    }

    // -------------------------------------------------------------------------
    // Callbacks (decorator factories)
    // -------------------------------------------------------------------------

    /**
     * Register an update callback. Usable as a decorator factory:
     *
     *   @app.add_update_callback(priority=Priority.BASE)
     *   def update(dt): ...
     *
     * Or called directly:
     *   app.add_update_callback(Priority.BASE, my_fn)
     *
     * The callback receives dt (float, seconds since last frame).
     */
    py::object add_update_callback(int priority = geodraw::Priority::BASE,
                                    py::object fn = py::none()) {
        auto registrar = [this, priority](py::function f) -> py::object {
            app_.addUpdateCallback([f](float dt) {
                py::gil_scoped_acquire acquire;
                try { f(dt); }
                catch (py::error_already_set& e) {
                    std::cerr << "Python error in update callback: " << e.what() << "\n";
                    e.restore();
                    PyErr_Clear();
                }
            }, priority);
            return py::object(f);
        };

        if (!fn.is_none()) {
            return registrar(fn.cast<py::function>());
        }
        return py::cpp_function(registrar);
    }

    /**
     * Register a draw callback. Usable as a decorator factory:
     *
     *   @app.add_draw_callback(priority=Priority.OVERLAY)
     *   def draw(): ...
     */
    py::object add_draw_callback(int priority = geodraw::Priority::BASE,
                                  py::object fn = py::none()) {
        auto registrar = [this, priority](py::function f) -> py::object {
            app_.addDrawCallback([f]() {
                py::gil_scoped_acquire acquire;
                try { f(); }
                catch (py::error_already_set& e) {
                    std::cerr << "Python error in draw callback: " << e.what() << "\n";
                    e.restore();
                    PyErr_Clear();
                }
            }, priority);
            return py::object(f);
        };

        if (!fn.is_none()) {
            return registrar(fn.cast<py::function>());
        }
        return py::cpp_function(registrar);
    }

    // -------------------------------------------------------------------------
    // Scene access
    // -------------------------------------------------------------------------

    /**
     * Get a reference proxy to the App's scene at the given index.
     * Auto-creates scenes if index >= current count.
     */
    PyScene scene(size_t index = 0) {
        while (app_.sceneCount() <= index) {
            app_.addScene();
        }
        return PyScene::fromRef(&app_.scene(index));
    }

    /**
     * Explicitly add a named scene. Returns the scene index.
     */
    size_t add_scene(std::string name = "default") {
        return app_.addScene(name);
    }

    /**
     * Activate N×M grid layout for simultaneous multi-scene rendering.
     */
    void set_scene_grid(int rows, int cols) {
        app_.setSceneGrid(rows, cols);
    }

    /**
     * Auto-frame the per-scene camera at index to the given bounding box.
     * Enables the dedicated camera for that scene.
     */
    void auto_frame_scene(size_t index, py::sequence bbox_min, py::sequence bbox_max) {
        auto mn = toPos3(bbox_min);
        auto mx = toPos3(bbox_max);
        app_.sceneCamera(index).autoFrame(
            glm::vec3(mn.pos.x, mn.pos.y, mn.pos.z),
            glm::vec3(mx.pos.x, mx.pos.y, mx.pos.z));
    }

    // -------------------------------------------------------------------------
    // Continuous update
    // -------------------------------------------------------------------------

    void request_update() {
        app_.requestUpdate();
    }

    void auto_frame(py::sequence bbox_min, py::sequence bbox_max) {
        auto mn = toPos3(bbox_min);
        auto mx = toPos3(bbox_max);
        app_.camera.autoFrame(
            glm::vec3(mn.pos.x, mn.pos.y, mn.pos.z),
            glm::vec3(mx.pos.x, mx.pos.y, mx.pos.z));
        app_.requestUpdate();
    }

    void request_continuous_update(std::string reason) {
        app_.requestContinuousUpdate(reason.c_str());
    }

    void cancel_continuous_update(std::string reason) {
        app_.cancelContinuousUpdate(reason.c_str());
    }

    // -------------------------------------------------------------------------
    // Minor modes
    // -------------------------------------------------------------------------

    geodraw::MinorMode* create_minor_mode(std::string name) {
        return &app_.createMinorMode(name);
    }

    void activate_minor_mode(geodraw::MinorMode* mode) {
        app_.activateMinorMode(*mode);
    }

    void deactivate_minor_mode(geodraw::MinorMode* mode) {
        app_.deactivateMinorMode(*mode);
    }

    void toggle_minor_mode(geodraw::MinorMode* mode) {
        app_.toggleMinorMode(*mode);
    }

    bool is_minor_mode_active(geodraw::MinorMode* mode) const {
        return app_.isMinorModeActive(*mode);
    }

    // -------------------------------------------------------------------------
    // ImGui support (Phase 2, guarded by GEODRAW_HAS_IMGUI)
    // -------------------------------------------------------------------------

#ifdef GEODRAW_HAS_IMGUI
    /**
     * Register an ImGui draw callback. Usable as a decorator factory:
     *
     *   @app.set_imgui_callback()
     *   def draw_ui(gui):
     *       gui.begin("Controls")
     *       gui.text("Hello")
     *       gui.end()
     *
     * Or called directly: app.set_imgui_callback()(my_fn)
     *
     * The callback receives a Gui proxy object with ImGui widget methods.
     */
    py::object set_imgui_callback() {
        return py::cpp_function([this](py::function fn) -> py::object {
            ensureImGuiPlugin();
            imgui_plugin_->setImGuiCallback([fn]() {
                py::gil_scoped_acquire acquire;
                try {
                    PyGui gui;
                    fn(gui);
                }
                catch (py::error_already_set& e) {
                    std::cerr << "Python error in imgui callback: " << e.what() << "\n";
                    e.restore();
                    PyErr_Clear();
                }
            });
            return py::object(fn);
        });
    }

    /**
     * Draw the master window listing all minor modes as checkboxes.
     * Call inside an imgui callback.
     */
    void draw_master_window(std::string title = "geodraw") {
        if (imgui_plugin_) {
            imgui_plugin_->drawMasterWindow(title);
        }
    }

    void enable_tooltips(PyScene& scene) {
        ensureImGuiPlugin();
        imgui_plugin_->enableTooltips(scene.getMutableScene());
    }

    void register_tooltip_provider(std::string objectType, py::function fn) {
        app_.tooltips().registerProvider(objectType,
            [fn](const geodraw::TooltipContext& ctx) -> geodraw::TooltipContent {
                py::gil_scoped_acquire acquire;
                try {
                    PyTooltipContext pyCtx{
                        ctx.objectId,
                        ctx.objectType,
                        {ctx.worldPoint.x, ctx.worldPoint.y, ctx.worldPoint.z}
                    };
                    auto result = fn(pyCtx);
                    return geodraw::TooltipContent::makeText(result.cast<std::string>());
                } catch (py::error_already_set& e) {
                    std::cerr << "Python error in tooltip provider: " << e.what() << "\n";
                    e.restore();
                    PyErr_Clear();
                    return geodraw::TooltipContent{};
                }
            });
    }
#endif // GEODRAW_HAS_IMGUI

    // -------------------------------------------------------------------------
    // Misc
    // -------------------------------------------------------------------------

    py::object load_glb(const std::string& path) {
        auto result = geodraw::gltf::loadGLB(path, app_.getRenderer());
        if (!result || !result->isValid()) return py::none();
        return py::cast(PyLoadedModel(
            std::make_shared<geodraw::gltf::LoadedModel>(std::move(*result))));
    }

    int get_width() const  { return app_.getWidth(); }
    int get_height() const { return app_.getHeight(); }

private:
    geodraw::App app_;
    std::vector<std::shared_ptr<PyToggle>> toggles_;

#ifdef GEODRAW_HAS_IMGUI
    std::unique_ptr<ImGuiPlugin> imgui_plugin_;

    void ensureImGuiPlugin() {
        if (!imgui_plugin_) {
            imgui_plugin_ = std::make_unique<ImGuiPlugin>(app_);
        }
    }
#endif

    // Wrap a Python function in a GIL-acquiring void() lambda.
    static std::function<void()> makePyCb(py::function fn) {
        return [fn]() {
            py::gil_scoped_acquire acquire;
            try { fn(); }
            catch (py::error_already_set& e) {
                std::cerr << "Python error in command callback: " << e.what() << "\n";
                e.restore();
                PyErr_Clear();
            }
        };
    }
};

// =============================================================================
// Module-level show functions (QuickDraw API — unchanged)
// =============================================================================

void show_scene(const PyScene& scene,
                const std::string& title = "GeoDraw",
                int width = 1280,
                int height = 720) {
    py::gil_scoped_release release;
    geodraw::QuickView::show(scene.getScene(), title, width, height);
}

void show_scenes(const std::vector<std::pair<std::string, PyScene>>& scenes,
                 const std::string& title = "GeoDraw",
                 int width = 1280,
                 int height = 720) {
    std::vector<std::pair<std::string, geodraw::Scene>> cpp_scenes;
    cpp_scenes.reserve(scenes.size());
    for (const auto& [label, pyscene] : scenes) {
        cpp_scenes.emplace_back(label, pyscene.getScene());
    }
    py::gil_scoped_release release;
    geodraw::QuickView::show(cpp_scenes, title, width, height);
}

// =============================================================================
// Module definition
// =============================================================================

PYBIND11_MODULE(_geodraw, m) {
    m.doc() = R"doc(
GeoDraw Python bindings for debugging and visualization.

QuickDraw API (simple):
    scene = geodraw.Scene()
    scene.add_point((0, 0, 0), radius=0.5, color=(1, 0, 0))
    geodraw.show(scene)

App API (full loop):
    app = geodraw.App(1280, 720, "My App")

    @app.add_update_callback()
    def update(dt):
        s = app.scene()
        s.clear()
        s.add_point((0, 0, 0))

    app.run()
)doc";

    // -------------------------------------------------------------------------
    // LoadedModel class
    // -------------------------------------------------------------------------

    py::class_<PyLoadedModel>(m, "LoadedModel", R"doc(
A 3D model loaded from a .glb file, with full texture and material support.

Obtain via app.load_glb(path). Pass to scene.add_loaded_model() to render.

Properties:
    is_valid    -- True if the model loaded successfully
    name        -- Filename of the model
    model_type  -- Detected type: "vehicle", "cyclist", "pedestrian", or "unknown"
    bounds      -- ((min_x, min_y, min_z), (max_x, max_y, max_z)) bounding box
)doc")
        .def_property_readonly("is_valid",    &PyLoadedModel::is_valid,
            "True if the model was loaded successfully.")
        .def_property_readonly("name",        &PyLoadedModel::name,
            "Model filename.")
        .def_property_readonly("model_type",  &PyLoadedModel::model_type,
            "Detected type: 'vehicle', 'cyclist', 'pedestrian', or 'unknown'.")
        .def_property_readonly("bounds",      &PyLoadedModel::bounds,
            "Bounding box as ((min_x, min_y, min_z), (max_x, max_y, max_z)).");

    // -------------------------------------------------------------------------
    // TooltipBuilder and TooltipContext classes
    // -------------------------------------------------------------------------

    py::class_<PyTooltipBuilder>(m, "TooltipBuilder",
        "Fluent tag builder returned by scene.add_*() for tooltip metadata.")
        .def("with_id", &PyTooltipBuilder::with_id, py::arg("id"),
             "Set object ID for tooltip lookup.")
        .def("with_type", &PyTooltipBuilder::with_type, py::arg("type_name"),
             "Set object type for provider matching.");

    py::class_<PyTooltipContext>(m, "TooltipContext",
        "Context passed to tooltip provider callbacks.")
        .def_readonly("object_id",   &PyTooltipContext::object_id)
        .def_readonly("object_type", &PyTooltipContext::object_type)
        .def_readonly("world_point", &PyTooltipContext::world_point,
                      "Hit position as (x, y, z) tuple.");

    // -------------------------------------------------------------------------
    // Scene class (QuickDraw + App proxy)
    // -------------------------------------------------------------------------

    py::class_<PyScene>(m, "Scene", R"doc(
A collection of 3D geometry for visualization.

In QuickDraw mode: create, populate, pass to geodraw.show().
In App mode: obtained via app.scene(); cleared and re-populated each update.
)doc")
        .def(py::init<>())
        .def("clear", &PyScene::clear, "Remove all geometry from the scene")
        .def("add_point", &PyScene::add_point,
             py::arg("pos"),
             py::arg("radius") = 0.5f,
             py::arg("color") = py::make_tuple(1.0f, 1.0f, 1.0f),
             py::arg("alpha") = 1.0f,
             "Add a point (rendered as small sphere).")
        .def("add_points", &PyScene::add_points,
             py::arg("points"),
             py::arg("radius") = 0.5f,
             py::arg("color") = py::make_tuple(1.0f, 1.0f, 1.0f),
             py::arg("alpha") = 1.0f,
             "Add multiple points from Nx3 numpy array.")
        .def("add_line", &PyScene::add_line,
             py::arg("start"), py::arg("end"),
             py::arg("thickness") = 1.0f,
             py::arg("color") = py::make_tuple(1.0f, 1.0f, 1.0f),
             py::arg("alpha") = 1.0f,
             "Add a line segment between two points.")
        .def("add_polyline", &PyScene::add_polyline,
             py::arg("points"),
             py::arg("thickness") = 1.0f,
             py::arg("color") = py::make_tuple(1.0f, 1.0f, 1.0f),
             py::arg("alpha") = 1.0f,
             "Add a polyline. points: list of (x,y,z) or Nx3 numpy array.")
        .def("add_lines", &PyScene::add_lines,
             py::arg("lines"),
             py::arg("thickness") = 1.0f,
             py::arg("color") = py::make_tuple(1.0f, 1.0f, 1.0f),
             py::arg("alpha") = 1.0f,
             "Add multiple line segments from Nx2x3 numpy array.")
        .def("add_bbox", &PyScene::add_bbox,
             py::arg("min_corner"), py::arg("max_corner"),
             py::arg("thickness") = 1.0f,
             py::arg("color") = py::make_tuple(1.0f, 1.0f, 1.0f),
             py::arg("alpha") = 1.0f,
             py::arg("filled") = false,
             "Add an axis-aligned bounding box.")
        .def("add_oriented_bbox", &PyScene::add_oriented_bbox,
             py::arg("center"), py::arg("half_extents"),
             py::arg("rotation") = py::none(),
             py::arg("thickness") = 1.0f,
             py::arg("color") = py::make_tuple(1.0f, 1.0f, 1.0f),
             py::arg("alpha") = 1.0f,
             py::arg("filled") = false,
             "Add an oriented bounding box.")
        .def("add_triangle", &PyScene::add_triangle,
             py::arg("vertices"),
             py::arg("color") = py::make_tuple(1.0f, 1.0f, 1.0f),
             py::arg("alpha") = 1.0f,
             py::arg("filled") = true,
             "Add a single triangle.")
        .def("add_mesh", &PyScene::add_mesh,
             py::arg("vertices"), py::arg("indices"),
             py::arg("color") = py::make_tuple(1.0f, 1.0f, 1.0f),
             py::arg("alpha") = 1.0f,
             py::arg("filled") = true,
             "Add a triangle mesh (vertices: Nx3 float64, indices: uint32 array).")
        .def("add_axes", &PyScene::add_axes,
             py::arg("position"),
             py::arg("scale") = 1.0f,
             py::arg("thickness") = 2.0f,
             "Add coordinate axes (X=red, Y=green, Z=blue).")
        .def("add_pose", &PyScene::add_pose,
             py::arg("position"), py::arg("rotation"),
             py::arg("scale") = 1.0f,
             py::arg("thickness") = 2.0f,
             "Add a pose visualization (position + orientation axes).")
        .def("add_ground", &PyScene::add_ground,
             py::arg("center"), py::arg("width"), py::arg("height"),
             py::arg("color") = py::make_tuple(0.5f, 0.5f, 0.5f),
             py::arg("alpha") = 1.0f,
             "Add a ground plane quad.")
        .def("add_loaded_model", &PyScene::add_loaded_model,
             py::arg("model"), py::arg("alpha") = 1.0f,
             R"doc(Render all meshes from a LoadedModel (obtained via app.load_glb()).

Supports full texture and material data loaded by the C++ glTF pipeline.

Args:
    model: LoadedModel returned by app.load_glb()
    alpha: Overall opacity (0.0 = transparent, 1.0 = opaque)
)doc");

    // -------------------------------------------------------------------------
    // Toggle class
    // -------------------------------------------------------------------------

    py::class_<PyToggle, std::shared_ptr<PyToggle>>(m, "Toggle", R"doc(
Mutable boolean wrapper returned by App.add_toggle().

Python booleans are immutable, so this wrapper allows the App to flip
toggle.value when the associated key is pressed.

Example:
    grid = app.add_toggle("show-grid", "Show grid", key=Key.G)
    # Later: if grid.value: ...
)doc")
        .def(py::init<>())
        .def_readwrite("value", &PyToggle::value, "Current boolean state.");

    // -------------------------------------------------------------------------
    // MinorMode class (non-owning: App owns via unique_ptr)
    // -------------------------------------------------------------------------

    py::class_<geodraw::MinorMode>(m, "MinorMode", R"doc(
A named set of keybindings that take precedence when active.

Obtain via app.create_minor_mode("name"). Activate with app.activate_minor_mode(mode).
)doc")
        .def_readonly("name", &geodraw::MinorMode::name);

#ifdef GEODRAW_HAS_IMGUI
    // -------------------------------------------------------------------------
    // Gui class (ImGui proxy)
    // -------------------------------------------------------------------------

    py::class_<PyGui>(m, "Gui", R"doc(
ImGui proxy object passed to the imgui callback.

Methods map to common ImGui widgets. Returned by the imgui callback;
do not hold references to it outside the callback scope.
)doc")
        .def(py::init<>())
        .def("begin", &PyGui::begin, py::arg("title"),
             "Begin an ImGui window. Returns False if collapsed.")
        .def("end", &PyGui::end,
             "End an ImGui window (always call after begin).")
        .def("text", &PyGui::text, py::arg("s"),
             "Display text.")
        .def("button", &PyGui::button, py::arg("label"),
             "Display a button. Returns True when clicked.")
        .def("checkbox", &PyGui::checkbox, py::arg("label"), py::arg("v"),
             "Checkbox widget. Returns (changed: bool, new_value: bool).")
        .def("slider_float", &PyGui::slider_float,
             py::arg("label"), py::arg("v"), py::arg("v_min"), py::arg("v_max"),
             "Float slider. Returns (changed: bool, new_value: float).")
        .def("slider_int", &PyGui::slider_int,
             py::arg("label"), py::arg("v"), py::arg("v_min"), py::arg("v_max"),
             "Integer slider. Returns (changed: bool, new_value: int).")
        .def("input_text", &PyGui::input_text, py::arg("label"), py::arg("s"),
             "Text input. Returns (changed: bool, new_text: str).")
        .def("input_int", &PyGui::input_int, py::arg("label"), py::arg("v"),
             "Integer input field. Returns (changed: bool, new_value: int).")
        .def("begin_disabled", &PyGui::begin_disabled,
             "Begin a disabled (greyed-out) section.")
        .def("end_disabled", &PyGui::end_disabled,
             "End a disabled section opened by begin_disabled().")
        .def("text_colored", &PyGui::text_colored,
             py::arg("s"), py::arg("r"), py::arg("g"), py::arg("b"),
             "Display colored text (r/g/b in [0, 1]).")
        .def("text_disabled", &PyGui::text_disabled, py::arg("s"),
             "Display greyed-out (disabled) text.")
        .def("separator", &PyGui::separator, "Horizontal separator.")
        .def("same_line", &PyGui::same_line, "Place next item on the same line.")
        .def("spacing", &PyGui::spacing, "Add vertical spacing.")
        .def("set_next_window_pos", &PyGui::set_next_window_pos,
             py::arg("x"), py::arg("y"),
             "Set position of next window (first use only).")
        .def("set_next_window_size", &PyGui::set_next_window_size,
             py::arg("w"), py::arg("h"),
             "Set size of next window (first use only).")
        .def("tree_node", &PyGui::tree_node, py::arg("label"),
             "Begin a tree node. Returns True if open; call tree_pop() if so.")
        .def("tree_pop", &PyGui::tree_pop,
             "End a tree node opened by tree_node().")
        .def("collapsing_header", &PyGui::collapsing_header, py::arg("label"),
             "Collapsing section header. Returns True if open.");
#endif // GEODRAW_HAS_IMGUI

    // -------------------------------------------------------------------------
    // App class
    // -------------------------------------------------------------------------

    py::class_<PyApp>(m, "App", R"doc(
Full application loop wrapper.

Creates a window, runs the event loop, and dispatches registered callbacks.
Supports commands, toggles, minor modes, and (with ImGui) UI panels.

Example:
    import geodraw

    app = geodraw.App(1280, 720, "My App")

    toggle = app.add_toggle("show-grid", "Show grid", key=geodraw.Key.G)

    @app.add_update_callback()
    def update(dt):
        s = app.scene()
        s.clear()
        if toggle.value:
            s.add_axes((0, 0, 0))

    app.run()
)doc")
        .def(py::init<int, int, std::string>(),
             py::arg("width") = 1280,
             py::arg("height") = 720,
             py::arg("title") = "GeoDraw")
        .def("run", &PyApp::run,
             "Run the main event loop (blocks until window is closed).")
        .def("quit", &PyApp::quit,
             "Request the application to quit.")
        .def("scene", &PyApp::scene, py::arg("index") = 0,
             "Get a reference proxy to the scene at index (auto-creates if needed).")
        .def("add_scene", &PyApp::add_scene, py::arg("name") = "default",
             "Explicitly add a named scene. Returns the scene index.")
        .def("set_scene_grid", &PyApp::set_scene_grid,
             py::arg("rows"), py::arg("cols"),
             "Activate N×M grid layout for simultaneous multi-scene rendering.")
        .def("auto_frame_scene", &PyApp::auto_frame_scene,
             py::arg("index"), py::arg("bbox_min"), py::arg("bbox_max"),
             R"doc(Auto-frame the per-scene camera at index to the given bounding box.

Enables the dedicated camera for that scene.

Args:
    index:    Scene pair index
    bbox_min: (x, y, z) minimum corner of the scene bounds
    bbox_max: (x, y, z) maximum corner of the scene bounds
)doc")
        .def("add_cmd", &PyApp::add_cmd,
             py::arg("name"), py::arg("fn"), py::arg("doc"),
             py::arg("key") = -1, py::arg("mods") = 0,
             py::arg("mode") = nullptr,
             R"doc(Register a command callback.

Args:
    name: Command name (shown in M-x palette)
    fn:   Callable with no arguments
    doc:  Help text
    key:  GLFW key code (e.g. geodraw.Key.X), -1 for none
    mods: Modifier flags (e.g. geodraw.Mod.CTRL)
    mode: MinorMode to scope this command to, or None for global
)doc")
        .def("add_toggle", &PyApp::add_toggle,
             py::arg("name"), py::arg("doc"),
             py::arg("key") = -1, py::arg("mods") = 0,
             py::arg("mode") = nullptr,
             R"doc(Register a toggle. Returns a Toggle whose .value flips on keypress.

Args:
    name: Toggle name
    doc:  Help text
    key:  GLFW key code
    mods: Modifier flags
    mode: MinorMode to scope this toggle to, or None for global
)doc")
        .def("add_update_callback", &PyApp::add_update_callback,
             py::arg("priority") = geodraw::Priority::BASE,
             py::arg("fn") = py::none(),
             R"doc(Register an update callback (receives dt in seconds).

Usable as a decorator factory:

    @app.add_update_callback(priority=Priority.BASE)
    def update(dt): ...

Or directly: app.add_update_callback(Priority.BASE, my_fn)
)doc")
        .def("add_draw_callback", &PyApp::add_draw_callback,
             py::arg("priority") = geodraw::Priority::BASE,
             py::arg("fn") = py::none(),
             R"doc(Register a draw callback (no arguments).

Usable as a decorator factory:

    @app.add_draw_callback(priority=Priority.OVERLAY)
    def draw(): ...
)doc")
        .def("request_update", &PyApp::request_update,
             "Force a single redraw on the next frame (use when paused and scene changes).")
        .def("auto_frame", &PyApp::auto_frame,
             py::arg("bbox_min"), py::arg("bbox_max"),
             R"doc(Fit the camera to a bounding box and trigger a redraw.

Args:
    bbox_min: (x, y, z) minimum corner of the scene bounds
    bbox_max: (x, y, z) maximum corner of the scene bounds

Example:
    app.auto_frame((-80, -80, -2), (80, 80, 10))
)doc")
        .def("request_continuous_update", &PyApp::request_continuous_update,
             py::arg("reason"),
             "Request continuous redraws (app runs every frame while any reason is active).")
        .def("cancel_continuous_update", &PyApp::cancel_continuous_update,
             py::arg("reason"),
             "Cancel a continuous-update reason.")
        .def("create_minor_mode", &PyApp::create_minor_mode,
             py::arg("name"),
             py::return_value_policy::reference,
             "Create and return a named MinorMode (owned by App).")
        .def("activate_minor_mode", &PyApp::activate_minor_mode,
             py::arg("mode"),
             "Activate a minor mode.")
        .def("deactivate_minor_mode", &PyApp::deactivate_minor_mode,
             py::arg("mode"),
             "Deactivate a minor mode.")
        .def("toggle_minor_mode", &PyApp::toggle_minor_mode,
             py::arg("mode"),
             "Toggle a minor mode's active state.")
        .def("is_minor_mode_active", &PyApp::is_minor_mode_active,
             py::arg("mode"),
             "Return True if the minor mode is currently active.")
        .def("load_glb", &PyApp::load_glb, py::arg("path"),
             R"doc(Load a .glb file and return a LoadedModel.

Uses the C++ glTF pipeline: parses geometry, loads embedded textures to GPU,
applies coordinate-system conversion (Y-up → Z-up), and detects scale/units.

Returns None if the file could not be loaded or contains no valid meshes.

Args:
    path: Path to the .glb file

Example:
    model = app.load_glb("data/glb_files/GreenCar.glb")
    if model is not None and model.is_valid:
        s = app.scene()
        app.auto_frame(model.bounds[0], model.bounds[1])

        @app.add_draw_callback(geodraw.Priority.OVERLAY)
        def draw():
            s.clear()
            s.add_loaded_model(model)
)doc")
        .def("width",  &PyApp::get_width,  "Window width in pixels.")
        .def("height", &PyApp::get_height, "Window height in pixels.")
#ifdef GEODRAW_HAS_IMGUI
        .def("set_imgui_callback", &PyApp::set_imgui_callback,
             R"doc(Register an ImGui draw callback. Returns a decorator.

    @app.set_imgui_callback()
    def draw_ui(gui):
        gui.begin("Controls")
        gui.text("Hello, world!")
        gui.end()
)doc")
        .def("draw_master_window", &PyApp::draw_master_window,
             py::arg("title") = "geodraw",
             "Draw the master window listing all minor modes as checkboxes.")
        .def("enable_tooltips", &PyApp::enable_tooltips,
             py::arg("scene"),
             "Enable automatic tooltip picking and rendering for a scene.")
        .def("register_tooltip_provider", &PyApp::register_tooltip_provider,
             py::arg("object_type"), py::arg("fn"),
             R"doc(Register a tooltip provider for an object type.

fn receives a TooltipContext and must return a string.

Example:
    app.register_tooltip_provider(
        "Triangle",
        lambda ctx: f"Triangle {ctx.object_id}"
    )
)doc")
#endif
        ;

    // -------------------------------------------------------------------------
    // QuickDraw show functions
    // -------------------------------------------------------------------------

    m.def("show", &show_scene,
          py::arg("scene"),
          py::arg("title") = "GeoDraw",
          py::arg("width") = 1280,
          py::arg("height") = 720,
          R"doc(Show a scene in a window (blocks until closed).

Use Q or ESC to close. Mouse: drag=orbit, scroll=zoom, right-drag=pan.
)doc");

    m.def("show", &show_scenes,
          py::arg("scenes"),
          py::arg("title") = "GeoDraw",
          py::arg("width") = 1280,
          py::arg("height") = 720,
          "Show multiple labeled scenes with toggle controls (1-9 keys).");

    // -------------------------------------------------------------------------
    // Key constants
    // -------------------------------------------------------------------------

    auto key_ns = py::module_::import("types").attr("SimpleNamespace")();

    // Letters
    key_ns.attr("A") = GLFW_KEY_A;  key_ns.attr("B") = GLFW_KEY_B;
    key_ns.attr("C") = GLFW_KEY_C;  key_ns.attr("D") = GLFW_KEY_D;
    key_ns.attr("E") = GLFW_KEY_E;  key_ns.attr("F") = GLFW_KEY_F;
    key_ns.attr("G") = GLFW_KEY_G;  key_ns.attr("H") = GLFW_KEY_H;
    key_ns.attr("I") = GLFW_KEY_I;  key_ns.attr("J") = GLFW_KEY_J;
    key_ns.attr("K") = GLFW_KEY_K;  key_ns.attr("L") = GLFW_KEY_L;
    key_ns.attr("M") = GLFW_KEY_M;  key_ns.attr("N") = GLFW_KEY_N;
    key_ns.attr("O") = GLFW_KEY_O;  key_ns.attr("P") = GLFW_KEY_P;
    key_ns.attr("Q") = GLFW_KEY_Q;  key_ns.attr("R") = GLFW_KEY_R;
    key_ns.attr("S") = GLFW_KEY_S;  key_ns.attr("T") = GLFW_KEY_T;
    key_ns.attr("U") = GLFW_KEY_U;  key_ns.attr("V") = GLFW_KEY_V;
    key_ns.attr("W") = GLFW_KEY_W;  key_ns.attr("X") = GLFW_KEY_X;
    key_ns.attr("Y") = GLFW_KEY_Y;  key_ns.attr("Z") = GLFW_KEY_Z;

    // Digits
    key_ns.attr("D0") = GLFW_KEY_0; key_ns.attr("D1") = GLFW_KEY_1;
    key_ns.attr("D2") = GLFW_KEY_2; key_ns.attr("D3") = GLFW_KEY_3;
    key_ns.attr("D4") = GLFW_KEY_4; key_ns.attr("D5") = GLFW_KEY_5;
    key_ns.attr("D6") = GLFW_KEY_6; key_ns.attr("D7") = GLFW_KEY_7;
    key_ns.attr("D8") = GLFW_KEY_8; key_ns.attr("D9") = GLFW_KEY_9;

    // Special keys
    key_ns.attr("NONE")      = -1;
    key_ns.attr("SPACE")     = GLFW_KEY_SPACE;
    key_ns.attr("ENTER")     = GLFW_KEY_ENTER;
    key_ns.attr("ESCAPE")    = GLFW_KEY_ESCAPE;
    key_ns.attr("BACKSPACE")  = GLFW_KEY_BACKSPACE;
    key_ns.attr("TAB")       = GLFW_KEY_TAB;
    key_ns.attr("DELETE")    = GLFW_KEY_DELETE;
    key_ns.attr("INSERT")    = GLFW_KEY_INSERT;
    key_ns.attr("HOME")      = GLFW_KEY_HOME;
    key_ns.attr("END")       = GLFW_KEY_END;
    key_ns.attr("PAGE_UP")   = GLFW_KEY_PAGE_UP;
    key_ns.attr("PAGE_DOWN") = GLFW_KEY_PAGE_DOWN;
    key_ns.attr("EQUAL")     = GLFW_KEY_EQUAL;
    key_ns.attr("MINUS")     = GLFW_KEY_MINUS;

    // Arrow keys
    key_ns.attr("LEFT")      = GLFW_KEY_LEFT;
    key_ns.attr("RIGHT")     = GLFW_KEY_RIGHT;
    key_ns.attr("UP")        = GLFW_KEY_UP;
    key_ns.attr("DOWN")      = GLFW_KEY_DOWN;

    // Function keys
    key_ns.attr("F1")  = GLFW_KEY_F1;  key_ns.attr("F2")  = GLFW_KEY_F2;
    key_ns.attr("F3")  = GLFW_KEY_F3;  key_ns.attr("F4")  = GLFW_KEY_F4;
    key_ns.attr("F5")  = GLFW_KEY_F5;  key_ns.attr("F6")  = GLFW_KEY_F6;
    key_ns.attr("F7")  = GLFW_KEY_F7;  key_ns.attr("F8")  = GLFW_KEY_F8;
    key_ns.attr("F9")  = GLFW_KEY_F9;  key_ns.attr("F10") = GLFW_KEY_F10;
    key_ns.attr("F11") = GLFW_KEY_F11; key_ns.attr("F12") = GLFW_KEY_F12;

    m.attr("Key") = key_ns;

    // -------------------------------------------------------------------------
    // Modifier key constants
    // -------------------------------------------------------------------------

    auto mod_ns = py::module_::import("types").attr("SimpleNamespace")();
    mod_ns.attr("NONE")  = 0;
    mod_ns.attr("SHIFT") = GLFW_MOD_SHIFT;
    mod_ns.attr("CTRL")  = GLFW_MOD_CONTROL;
    mod_ns.attr("ALT")   = GLFW_MOD_ALT;
    mod_ns.attr("SUPER") = GLFW_MOD_SUPER;
    m.attr("Mod") = mod_ns;

    // -------------------------------------------------------------------------
    // Priority constants
    // -------------------------------------------------------------------------

    auto prio_ns = py::module_::import("types").attr("SimpleNamespace")();
    prio_ns.attr("BASE")      = geodraw::Priority::BASE;
    prio_ns.attr("OVERLAY")   = geodraw::Priority::OVERLAY;
    prio_ns.attr("BEFORE_UI") = geodraw::Priority::BEFORE_UI;
    prio_ns.attr("UI")        = geodraw::Priority::UI;
    prio_ns.attr("AFTER_UI")  = geodraw::Priority::AFTER_UI;
    m.attr("Priority") = prio_ns;
}
