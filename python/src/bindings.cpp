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

#include "geodraw/app/key.hpp"
#include "geodraw/modules/earth/earth_shape_edit.hpp"
#include "geodraw/modules/camera_trajectory/camera_trajectory_plugin.hpp"
#include "geodraw/modules/video_capture/video_capture_plugin.hpp"
#include "geodraw/modules/drive/scenario_plugin.hpp"

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
// PyRenderer - thin wrapper around geodraw::Renderer (ref proxy, not owned)
// =============================================================================

class PyRenderer {
public:
    explicit PyRenderer(geodraw::Renderer& r) : renderer_(r) {}

    unsigned int load_texture(const std::string& path) {
        return renderer_.loadTexture(path);
    }

    void unload_texture(const std::string& path) {
        renderer_.unloadTexture(path);
    }

private:
    geodraw::Renderer& renderer_;
};

// =============================================================================
// PyShape3 - Python wrapper for geodraw::Shape3 (mutable, editable geometry)
// =============================================================================

class PyShape3 {
public:
    PyShape3() = default;

    // Add a new polygon (ring). points: list of (x,y,z) or Nx3 array.
    // Each call creates a new polygon; the first ring is the outer boundary.
    void add_ring(py::object points) {
        geodraw::Polyline3 ring;
        if (py::isinstance<py::array>(points)) {
            ring.points = toPos3Vector(points.cast<py::array_t<double>>());
        } else {
            ring.points = toPos3VectorFromList(points.cast<py::list>());
        }
        shape_.polygons.push_back({ring});
    }

    // Add a hole to the last polygon (must have called add_ring first).
    void add_hole(py::object points) {
        if (shape_.polygons.empty()) {
            throw std::runtime_error("add_hole: no polygon yet; call add_ring first.");
        }
        geodraw::Polyline3 hole;
        if (py::isinstance<py::array>(points)) {
            hole.points = toPos3Vector(points.cast<py::array_t<double>>());
        } else {
            hole.points = toPos3VectorFromList(points.cast<py::list>());
        }
        shape_.polygons.back().push_back(hole);
    }

    // Add an isolated polyline.
    void add_line(py::object points) {
        geodraw::Polyline3 line;
        if (py::isinstance<py::array>(points)) {
            line.points = toPos3Vector(points.cast<py::array_t<double>>());
        } else {
            line.points = toPos3VectorFromList(points.cast<py::list>());
        }
        shape_.lines.push_back(line);
    }

    // Add an isolated point.
    void add_point(const py::sequence& pos) {
        shape_.points.push_back(toPos3(pos));
    }

    void clear() { shape_ = geodraw::Shape3{}; }
    bool is_empty() const { return shape_.isEmpty(); }

    // Non-const accessor used by PyScene::add_shape() for the editable overload.
    geodraw::Shape3& getShape() { return shape_; }

private:
    geodraw::Shape3 shape_;
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
                                  bool filled = true,
                                  unsigned int texture_handle = 0) {
        geodraw::Triangle3 tri;
        bool has_uv = false;
        try {
            py::sequence v0 = vertices[0].cast<py::sequence>();
            if (py::len(v0) == 2) {
                v0[0].cast<py::sequence>();  // throws if v0[0] is a number
                has_uv = true;
            }
        } catch (...) {}

        if (has_uv) {
            auto parseUV = [](py::object item) -> std::pair<geodraw::Pos3, glm::vec2> {
                py::sequence pair = item.cast<py::sequence>();
                py::sequence pos  = pair[0].cast<py::sequence>();
                py::sequence uv   = pair[1].cast<py::sequence>();
                return {toPos3(pos), glm::vec2(uv[0].cast<float>(), uv[1].cast<float>())};
            };
            auto [p0, t0] = parseUV(vertices[0]);
            auto [p1, t1] = parseUV(vertices[1]);
            auto [p2, t2] = parseUV(vertices[2]);
            tri = geodraw::Triangle3(p0, p1, p2, t0, t1, t2);
        } else {
            tri = geodraw::Triangle3(toPos3(vertices[0]), toPos3(vertices[1]), toPos3(vertices[2]));
        }

        auto b = sceneRef().Add(tri, 1.0f, geodraw::LineStyle::Solid, alpha,
                                toColor(color), true, filled,
                                static_cast<GLuint>(texture_handle));
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

    // add_shape: editable rendering path. IMPORTANT: pyShape must remain alive
    // for as long as this scene is rendered (the scene holds a non-owning pointer).
    PyTooltipBuilder add_shape(PyShape3& pyShape,
                               float thickness = 1.0f,
                               py::sequence color = py::make_tuple(1.0f, 1.0f, 1.0f),
                               float alpha = 1.0f,
                               bool filled = true) {
        auto b = sceneRef().Add(pyShape.getShape(), thickness,
                                geodraw::LineStyle::Solid, alpha, toColor(color),
                                true, filled);
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
// PyIAppModuleBase - common base for Python-accessible IAppModule wrappers
// =============================================================================

class PyIAppModuleBase {
public:
    virtual geodraw::IAppModule& module() = 0;
    virtual ~PyIAppModuleBase() = default;
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

    // Returns (changed, (r, g, b))
    py::tuple color_edit3(std::string label, float r, float g, float b) {
        auto [changed, col] = geodraw::imgui::color_edit3(label.c_str(), r, g, b);
        return py::make_tuple(changed, py::make_tuple(col[0], col[1], col[2]));
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

    geodraw::App& getApp() { return app_; }

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

    void enable_editing(size_t scene_index = 0) {
        app_.enableEditing(scene_index);
    }

    void add_module(PyIAppModuleBase& m) {
        app_.addModule(m.module());
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

    PyRenderer renderer() {
        return PyRenderer(app_.getRenderer());
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
// PyCameraTrajectoryPlugin - Python wrapper for CameraTrajectoryPlugin
// =============================================================================

class PyCameraTrajectoryPlugin : public PyIAppModuleBase {
public:
    PyCameraTrajectoryPlugin() = default;

    geodraw::IAppModule& module() override { return plugin_; }

    geodraw::CameraTrajectoryPlugin& getCamTraj() { return plugin_; }

    geodraw::MinorMode* get_minor_mode() { return plugin_.getMinorMode(); }

#ifdef GEODRAW_HAS_IMGUI
    void draw_imgui_panel(PyApp& pyApp);
#endif

private:
    geodraw::CameraTrajectoryPlugin plugin_;
};

// =============================================================================
// PyVideoCapturePlugin - Python wrapper for VideoCapturePlugin
// =============================================================================

class PyVideoCapturePlugin : public PyIAppModuleBase {
public:
    // cam must outlive this object (enforced via py::keep_alive in pybind11).
    explicit PyVideoCapturePlugin(PyCameraTrajectoryPlugin& cam)
        : plugin_(cam.getCamTraj()) {}

    geodraw::IAppModule& module() override { return plugin_; }

    geodraw::MinorMode* get_minor_mode() { return plugin_.getMinorMode(); }

#ifdef GEODRAW_HAS_IMGUI
    void draw_imgui_panel(PyApp& pyApp);
#endif

private:
    geodraw::VideoCapturePlugin plugin_;
};

// =============================================================================
// PyScenarioPlugin - Python wrapper for ScenarioPlugin
// =============================================================================

class PyScenarioPlugin : public PyIAppModuleBase {
public:
    PyScenarioPlugin() = default;  // standalone mode only

    geodraw::IAppModule& module() override { return plugin_; }

    void activate_scenario(const std::string& filepath, PyApp& pyApp) {
        plugin_.activateScenario(filepath, pyApp.getApp());
    }

    void set_car_model(const PyLoadedModel& model) {
        plugin_.setCarModel(model.get());
    }

    bool is_loaded() const { return plugin_.isLoaded(); }

#ifdef GEODRAW_HAS_IMGUI
    void draw_imgui_panel(PyApp& pyApp) {
        plugin_.drawImGuiPanel(pyApp.getApp().camera, pyApp.getApp(), nullptr);
    }
#endif

private:
    geodraw::ScenarioPlugin plugin_;
};

// =============================================================================
// PyShapeEditor - Python wrapper for geodraw::ShapeEditor
// =============================================================================

class PyShapeEditor {
public:
    PyShapeEditor() = default;

    void register_commands(PyApp& pyApp) {
        editor_.registerCommands(pyApp.getApp());
    }

    void add_to_scene(PyScene& pyScene,
                      py::sequence origin = py::make_tuple(0.0, 0.0, 0.0)) {
        glm::dvec3 o(origin[0].cast<double>(),
                     origin[1].cast<double>(),
                     origin[2].cast<double>());
        editor_.addToScene(pyScene.getMutableScene(), o);
    }

    void handle_input(PyApp& pyApp) {
        editor_.handleInput(pyApp.getApp());
    }

    geodraw::MinorMode* get_minor_mode() { return editor_.getMinorMode(); }
    bool is_active() const { return editor_.isActive(); }

    void undo()           { editor_.undo(); }
    void redo()           { editor_.redo(); }
    bool can_undo() const { return editor_.canUndo(); }
    bool can_redo() const { return editor_.canRedo(); }

private:
    geodraw::ShapeEditor editor_;
};

#ifdef GEODRAW_HAS_IMGUI
// Out-of-line definitions: require PyApp to be fully declared.
void PyCameraTrajectoryPlugin::draw_imgui_panel(PyApp& pyApp) {
    // nullptr: context already set (Python runs single-process, single dylib).
    plugin_.drawImGuiPanel(pyApp.getApp().camera, pyApp.getApp(), nullptr);
}
void PyVideoCapturePlugin::draw_imgui_panel(PyApp& pyApp) {
    plugin_.drawImGuiPanel(pyApp.getApp(), nullptr);
}
#endif // GEODRAW_HAS_IMGUI

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
    // Renderer class
    // -------------------------------------------------------------------------

    py::class_<PyRenderer>(m, "Renderer", R"doc(
Thin wrapper around the OpenGL renderer for texture management.
Obtained via app.renderer(). Do not hold references beyond app lifetime.
)doc")
        .def("load_texture", &PyRenderer::load_texture, py::arg("path"),
             "Load a texture from file. Returns int handle (0 on error).")
        .def("unload_texture", &PyRenderer::unload_texture, py::arg("path"),
             "Unload a previously loaded texture.");

    // -------------------------------------------------------------------------
    // Shape3 class (mutable editable geometry)
    // -------------------------------------------------------------------------

    py::class_<PyShape3>(m, "Shape3", R"doc(
Mutable 3D shape for editable geometry.

Build the shape before entering the event loop, then add it to the scene
each frame via scene.add_shape(shape). The shape object must remain alive
for as long as it is rendered (the scene holds a non-owning reference).

Controls when app.enable_editing() is active:
    TAB   — enter / exit edit mode
    ENTER — commit edits
    ESC   — cancel edits (revert to last committed state)

Example:
    shape = geodraw.Shape3()
    shape.add_ring([(-5,-5,0), (-5,0,0), (0,0,0), (0,-5,0), (-5,-5,0)])

    @app.add_update_callback()
    def update(dt):
        scene = app.scene()
        scene.clear()
        scene.add_shape(shape).with_id(1).with_type("MyShape")
)doc")
        .def(py::init<>())
        .def("add_ring", &PyShape3::add_ring, py::arg("points"),
             "Add a new polygon ring (outer boundary). points: list of (x,y,z) or Nx3 array.")
        .def("add_hole", &PyShape3::add_hole, py::arg("points"),
             "Add a hole to the last polygon. Must call add_ring first.")
        .def("add_line", &PyShape3::add_line, py::arg("points"),
             "Add an isolated polyline. points: list of (x,y,z) or Nx3 array.")
        .def("add_point", &PyShape3::add_point, py::arg("pos"),
             "Add an isolated point.")
        .def("clear", &PyShape3::clear,
             "Remove all geometry from the shape.")
        .def("is_empty", &PyShape3::is_empty,
             "Return True if the shape contains no geometry.");

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
             py::arg("texture_handle") = 0u,
             R"doc(Add a single triangle.

vertices: list of 3 items, either:
  - Plain positions:  [(x,y,z), (x,y,z), (x,y,z)]
  - With UV coords:   [((x,y,z),(u,v)), ((x,y,z),(u,v)), ((x,y,z),(u,v))]

texture_handle: int returned by app.renderer().load_texture(). 0 = solid color.
)doc")
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
        .def("add_shape", &PyScene::add_shape,
             py::arg("shape"),
             py::arg("thickness") = 1.0f,
             py::arg("color") = py::make_tuple(1.0f, 1.0f, 1.0f),
             py::arg("alpha") = 1.0f,
             py::arg("filled") = true,
             R"doc(Add an editable Shape3 to the scene.

The shape object must remain alive for as long as it is rendered.
Use .with_id(n).with_type("Name") on the returned TooltipBuilder
so the editor can track and select this object.

Example:
    scene.add_shape(shape).with_id(1).with_type("MyShape")
)doc")
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
        .def("color_edit3", &PyGui::color_edit3,
             py::arg("label"), py::arg("r"), py::arg("g"), py::arg("b"),
             "Color picker widget. Returns (changed: bool, (r, g, b): tuple).")
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
        .def("enable_editing", &PyApp::enable_editing,
             py::arg("scene_index") = 0,
             R"doc(Activate click-to-select and drag editing for Shape3 objects.

Must be called once after app creation. Only objects added via
scene.add_shape() participate in editing.

Controls:
    TAB   — enter / exit edit mode
    ENTER — commit edits
    ESC   — cancel edits (revert to last committed state)
)doc")
        .def("add_module", &PyApp::add_module,
             py::arg("module"),
             "Attach a plugin module (CameraTrajectoryPlugin, VideoCapturePlugin, etc.) to the app.")
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
        .def("renderer", &PyApp::renderer,
             "Get the renderer for texture management (load_texture / unload_texture).")
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
    // CameraTrajectoryPlugin class
    // -------------------------------------------------------------------------

    py::class_<PyIAppModuleBase>(m, "IAppModule"); // internal base, not user-visible

    py::class_<PyCameraTrajectoryPlugin, PyIAppModuleBase>(m, "CameraTrajectoryPlugin", R"doc(
Plugin for recording and playing back camera trajectories.

Usage:
    cam_traj = geodraw.CameraTrajectoryPlugin()
    app.add_module(cam_traj)
    app.activate_minor_mode(cam_traj.get_minor_mode())

    @app.set_imgui_callback()
    def draw_ui(gui):
        cam_traj.draw_imgui_panel(app)

The ImGui panel provides keyframe management (add/remove/sort),
baking (densify at FPS), playback controls, and CSV I/O.
)doc")
        .def(py::init<>())
        .def("get_minor_mode", &PyCameraTrajectoryPlugin::get_minor_mode,
             py::return_value_policy::reference,
             "Return the MinorMode for camera trajectory keybindings (available after add_module).")
#ifdef GEODRAW_HAS_IMGUI
        .def("draw_imgui_panel", &PyCameraTrajectoryPlugin::draw_imgui_panel,
             py::arg("app"),
             "Draw the camera trajectory control panel inside an imgui callback.")
#endif
        ;

    // -------------------------------------------------------------------------
    // VideoCapturePlugin class
    // -------------------------------------------------------------------------

    py::class_<PyVideoCapturePlugin, PyIAppModuleBase>(m, "VideoCapturePlugin", R"doc(
Plugin that captures video frames by stepping through a CameraTrajectoryPlugin's
baked poses and writing PNG files with glReadPixels.

The cam_traj argument must remain alive for the lifetime of this plugin.

Usage:
    cam_traj = geodraw.CameraTrajectoryPlugin()
    video_cap = geodraw.VideoCapturePlugin(cam_traj)
    app.add_module(cam_traj)
    app.add_module(video_cap)
    app.activate_minor_mode(video_cap.get_minor_mode())

    @app.set_imgui_callback()
    def draw_ui(gui):
        video_cap.draw_imgui_panel(app)
)doc")
        .def(py::init<PyCameraTrajectoryPlugin&>(),
             py::arg("cam_traj"),
             py::keep_alive<1, 2>(), // keep cam_traj alive at least as long as video_cap
             "Create a VideoCapturePlugin that hooks into cam_traj's playback.")
        .def("get_minor_mode", &PyVideoCapturePlugin::get_minor_mode,
             py::return_value_policy::reference,
             "Return the MinorMode for video capture keybindings (available after add_module).")
#ifdef GEODRAW_HAS_IMGUI
        .def("draw_imgui_panel", &PyVideoCapturePlugin::draw_imgui_panel,
             py::arg("app"),
             "Draw the video capture control panel inside an imgui callback.")
#endif
        ;

    // -------------------------------------------------------------------------
    // ScenarioPlugin class
    // -------------------------------------------------------------------------

    py::class_<PyScenarioPlugin, PyIAppModuleBase>(m, "ScenarioPlugin", R"doc(
Plugin for vehicle trajectory scenario visualization.

Usage:
    scenario = geodraw.ScenarioPlugin()
    car_model = app.load_glb("data/glb_files/GreenCar.glb")
    if car_model is not None and car_model.is_valid:
        scenario.set_car_model(car_model)
    app.add_module(scenario)
    scenario.activate_scenario("path/to/scene.json", app)

    @app.set_imgui_callback()
    def draw_ui(gui):
        scenario.draw_imgui_panel(app)
)doc")
        .def(py::init<>())
        .def("activate_scenario", &PyScenarioPlugin::activate_scenario,
             py::arg("filepath"), py::arg("app"),
             "Load and activate a scenario from a JSON file.")
        .def("set_car_model", &PyScenarioPlugin::set_car_model,
             py::arg("model"),
             "Set the 3D car model used to render vehicles (call before activate_scenario).")
        .def("is_loaded", &PyScenarioPlugin::is_loaded,
             "Return True if a scenario has been loaded.")
#ifdef GEODRAW_HAS_IMGUI
        .def("draw_imgui_panel", &PyScenarioPlugin::draw_imgui_panel,
             py::arg("app"),
             "Draw the scenario control panel (playback, visibility) inside an imgui callback.")
#endif
        ;

    // -------------------------------------------------------------------------
    // ShapeEditor class
    // -------------------------------------------------------------------------

    py::class_<PyShapeEditor>(m, "ShapeEditor", R"doc(
Shape editor for interactive polygon / line / point editing.

Create one instance, call register_commands(app) once, then call
add_to_scene(scene) from your update callback and handle_input(app)
from your draw callback.

Controls registered by register_commands():
    CTRL+S  — Activate / deactivate shape editing mode
    CTRL+R  — Commit current points as a ring (polygon)
    CTRL+L  — Commit current points as a line
    CTRL+P  — Commit current points as isolated points
    CTRL+Z  — Undo
    CTRL+Y  — Redo
    CTRL+D  — Delete nearest point
    CTRL+I  — Split nearest edge (insert point)
    CTRL+M  — Move nearest point (hold and drag)
    CTRL+O  — Save shape to file
    CTRL+F  — Load shape from file
)doc")
        .def(py::init<>())
        .def("register_commands", &PyShapeEditor::register_commands, py::arg("app"),
             "Register all shape editing commands with the app.")
        .def("add_to_scene", &PyShapeEditor::add_to_scene,
             py::arg("scene"),
             py::arg("origin") = py::make_tuple(0.0, 0.0, 0.0),
             "Add in-progress and committed geometry to the scene.")
        .def("handle_input", &PyShapeEditor::handle_input, py::arg("app"),
             "Process CTRL+click and drag input (call from a draw callback).")
        .def("get_minor_mode", &PyShapeEditor::get_minor_mode,
             py::return_value_policy::reference,
             "Return the MinorMode used for shape editing keybindings (or None if not yet registered).")
        .def("is_active", &PyShapeEditor::is_active,
             "Return True if shape editing mode is currently active.")
        .def("undo",     &PyShapeEditor::undo,     "Undo the last edit.")
        .def("redo",     &PyShapeEditor::redo,     "Redo the last undone edit.")
        .def("can_undo", &PyShapeEditor::can_undo, "Return True if there is a state to undo.")
        .def("can_redo", &PyShapeEditor::can_redo, "Return True if there is a state to redo.");

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

    using geodraw::Key;
    // Letters
    key_ns.attr("A") = geodraw::key(Key::A);  key_ns.attr("B") = geodraw::key(Key::B);
    key_ns.attr("C") = geodraw::key(Key::C);  key_ns.attr("D") = geodraw::key(Key::D);
    key_ns.attr("E") = geodraw::key(Key::E);  key_ns.attr("F") = geodraw::key(Key::F);
    key_ns.attr("G") = geodraw::key(Key::G);  key_ns.attr("H") = geodraw::key(Key::H);
    key_ns.attr("I") = geodraw::key(Key::I);  key_ns.attr("J") = geodraw::key(Key::J);
    key_ns.attr("K") = geodraw::key(Key::K);  key_ns.attr("L") = geodraw::key(Key::L);
    key_ns.attr("M") = geodraw::key(Key::M);  key_ns.attr("N") = geodraw::key(Key::N);
    key_ns.attr("O") = geodraw::key(Key::O);  key_ns.attr("P") = geodraw::key(Key::P);
    key_ns.attr("Q") = geodraw::key(Key::Q);  key_ns.attr("R") = geodraw::key(Key::R);
    key_ns.attr("S") = geodraw::key(Key::S);  key_ns.attr("T") = geodraw::key(Key::T);
    key_ns.attr("U") = geodraw::key(Key::U);  key_ns.attr("V") = geodraw::key(Key::V);
    key_ns.attr("W") = geodraw::key(Key::W);  key_ns.attr("X") = geodraw::key(Key::X);
    key_ns.attr("Y") = geodraw::key(Key::Y);  key_ns.attr("Z") = geodraw::key(Key::Z);

    // Digits
    key_ns.attr("D0") = geodraw::key(Key::D0); key_ns.attr("D1") = geodraw::key(Key::D1);
    key_ns.attr("D2") = geodraw::key(Key::D2); key_ns.attr("D3") = geodraw::key(Key::D3);
    key_ns.attr("D4") = geodraw::key(Key::D4); key_ns.attr("D5") = geodraw::key(Key::D5);
    key_ns.attr("D6") = geodraw::key(Key::D6); key_ns.attr("D7") = geodraw::key(Key::D7);
    key_ns.attr("D8") = geodraw::key(Key::D8); key_ns.attr("D9") = geodraw::key(Key::D9);

    // Special keys
    key_ns.attr("NONE")      = geodraw::key(Key::None);
    key_ns.attr("SPACE")     = geodraw::key(Key::Space);
    key_ns.attr("ENTER")     = geodraw::key(Key::Enter);
    key_ns.attr("ESCAPE")    = geodraw::key(Key::Escape);
    key_ns.attr("BACKSPACE")  = geodraw::key(Key::Backspace);
    key_ns.attr("TAB")       = geodraw::key(Key::Tab);
    key_ns.attr("DELETE")    = geodraw::key(Key::Delete);
    key_ns.attr("INSERT")    = geodraw::key(Key::Insert);
    key_ns.attr("HOME")      = geodraw::key(Key::Home);
    key_ns.attr("END")       = geodraw::key(Key::End);
    key_ns.attr("PAGE_UP")   = geodraw::key(Key::PageUp);
    key_ns.attr("PAGE_DOWN") = geodraw::key(Key::PageDown);
    key_ns.attr("EQUAL")     = geodraw::key(Key::Equal);
    key_ns.attr("MINUS")     = geodraw::key(Key::Minus);

    // Arrow keys
    key_ns.attr("LEFT")      = geodraw::key(Key::Left);
    key_ns.attr("RIGHT")     = geodraw::key(Key::Right);
    key_ns.attr("UP")        = geodraw::key(Key::Up);
    key_ns.attr("DOWN")      = geodraw::key(Key::Down);

    // Function keys
    key_ns.attr("F1")  = geodraw::key(Key::F1);  key_ns.attr("F2")  = geodraw::key(Key::F2);
    key_ns.attr("F3")  = geodraw::key(Key::F3);  key_ns.attr("F4")  = geodraw::key(Key::F4);
    key_ns.attr("F5")  = geodraw::key(Key::F5);  key_ns.attr("F6")  = geodraw::key(Key::F6);
    key_ns.attr("F7")  = geodraw::key(Key::F7);  key_ns.attr("F8")  = geodraw::key(Key::F8);
    key_ns.attr("F9")  = geodraw::key(Key::F9);  key_ns.attr("F10") = geodraw::key(Key::F10);
    key_ns.attr("F11") = geodraw::key(Key::F11); key_ns.attr("F12") = geodraw::key(Key::F12);

    m.attr("Key") = key_ns;

    // -------------------------------------------------------------------------
    // Modifier key constants
    // -------------------------------------------------------------------------

    auto mod_ns = py::module_::import("types").attr("SimpleNamespace")();
    mod_ns.attr("NONE")  = geodraw::Mod::None;
    mod_ns.attr("SHIFT") = geodraw::Mod::Shift;
    mod_ns.attr("CTRL")  = geodraw::Mod::Ctrl;
    mod_ns.attr("ALT")   = geodraw::Mod::Alt;
    mod_ns.attr("SUPER") = geodraw::Mod::Super;
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
