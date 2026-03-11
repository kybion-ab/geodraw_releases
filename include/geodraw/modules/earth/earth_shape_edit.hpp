#pragma once

#include "geodraw/export/export.hpp"

#include "geodraw/app/app.hpp"
#include "geodraw/app/app_module.hpp"
#include "geodraw/scene/scene.hpp"
#include "geodraw/geometry/geometry.hpp"
#include <glm/glm.hpp>

namespace geodraw {

// State snapshot for undo/redo
struct EditorState {
    std::vector<glm::dvec3> tempPoints;
    std::vector<Polyline3> tempHoles;  // Stored holes when editing outer ring
    Shape3 editedShape;
};

class GEODRAW_API ShapeEditor : public IAppModule {
public:
    ShapeEditor() = default;

    // IAppModule
    void attach(App& app) override;
    void detach(App& app) override;

    // Register all shape editing commands with the app
    void registerCommands(App& app);

    // Handle CTRL+click input (call from draw callback)
    void handleInput(App& app);

    // Add visualization to scene (call from update callback)
    // sceneOrigin: the current scene origin in absolute coordinates (e.g., targetECEF in GLOBE mode)
    void addToScene(Scene& scene, const glm::dvec3& sceneOrigin = glm::dvec3(0.0));

    // Minor mode accessor (matches CameraTrajectoryPlugin pattern)
    MinorMode* getMinorMode() { return mode_; }
    const MinorMode* getMinorMode() const { return mode_; }

#ifdef GEODRAW_HAS_IMGUI
    /// Draw the Shape Edit ImGui panel as a standalone window.
    /// Call inside imgui.setImGuiCallback() for non-drawPluginsPanel use.
    void drawImGuiPanel(App& app, ImGuiCtx imguiCtx = {});
#endif

    // Accessors
    bool isActive() const { return active_; }

    // Generic accessor (works for any coordinate system)
    const Shape3& getShape() const { return editedShape_; }
    Shape3& getShape()             { return editedShape_; }

    // Backward-compatible alias for earth/ECEF use
    const Shape3& getShapeECEF() const { return editedShape_; }
    Shape3& getShapeECEF()             { return editedShape_; }

    // Undo/Redo
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;

private:
#ifdef GEODRAW_HAS_IMGUI
    void drawPanelContents(App& app);
#endif

    void commitAsRing();
    void commitAsLine();
    void commitAsPoints();

    // Delete the point nearest to the cursor from the committed shape
    void deleteNearestPoint(App& app);

    // Split the nearest ring edge by inserting a point at the cursor projection
    void splitNearestEdge(App& app);

    // Save the edited shape as GeoJSON (converts from ECEF to WGS84)
    void saveAsGeoJSON(const std::string& filepath);

    // Save/load using .shape format (raw coordinates, no conversion)
    void saveAsShape(const std::string& filepath);
    void loadFromShape(const std::string& filepath, App& app);

    // Load from GeoJSON, converting WGS84 → ECEF when in GLOBE mode
    void loadFromGeoJSON(const std::string& filepath, App& app);

    // Undo/Redo helpers
    void pushState();
    void loadState(size_t index);

    bool active_ = false;
    std::vector<glm::dvec3> tempPoints_;  // In-progress points (absolute coords)
    std::vector<Polyline3> tempHoles_;    // Temporarily stored holes when editing outer ring
    bool wasClicking_ = false;
    Shape3 editedShape_;  // Committed shape (absolute coords)

    // Undo/Redo state
    std::vector<EditorState> history_;
    size_t historyIndex_ = 0;
    static constexpr size_t kMaxHistorySize = 50;

    // File path buffers for the ImGui panel
    char shapePath_[256]   = "edited_shape.shape";
    char geojsonPath_[256] = "edited_shape.geojson";

    // Minor mode for shape editing keybindings
    MinorMode* mode_ = nullptr;

    // Move point state - tracks which point is being dragged
    enum class MovePointType { None, TempPoint, Polygon, Line, Isolated };
    bool isMovingPoint_ = false;
    bool moveStartedThisFrame_ = false;  // Prevent immediate state save on key press
    MovePointType movePointType_ = MovePointType::None;
    size_t movePolygonIdx_ = 0;
    size_t moveRingIdx_ = 0;
    size_t moveLineIdx_ = 0;
    size_t movePointIdx_ = 0;
};

} // namespace geodraw
