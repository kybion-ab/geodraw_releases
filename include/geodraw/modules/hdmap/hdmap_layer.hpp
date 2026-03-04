/*******************************************************************************
 * File: hdmap_layer.hpp
 *
 * Description: HD map visualization layer with picking support.
 * Renders HD map objects (lanes, barriers, links) in the scene and supports
 * mouse-based object selection.
 *
 *
 ******************************************************************************/

#pragma once

#include "hdmap_provider.hpp"
#include "hdmap_types.hpp"
#include "geodraw/modules/earth/earth_coords.hpp"
#include <functional>
#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <set>
#include <vector>

namespace geodraw {
namespace hdmap {

// =============================================================================
// Rendering Style Configuration
// =============================================================================

/**
 * Style for rendering lanes
 */
struct HDLaneStyle {
    glm::vec4 normalColor{0.4f, 0.6f, 0.8f, 0.7f};       // Light blue
    glm::vec4 selectedColor{1.0f, 0.8f, 0.0f, 0.9f};     // Yellow
    glm::vec4 hoveredColor{0.6f, 0.8f, 1.0f, 0.8f};      // Lighter blue
    float lineWidth = 2.0f;
    float selectedLineWidth = 4.0f;
    bool showCenterline = true;
    bool showBoundaries = false;
    bool fillPolygon = true;
    float fillOpacity = 0.3f;
};

/**
 * Style for rendering lane markers
 */
struct HDLaneMarkerStyle {
    glm::vec4 whiteColor{1.0f, 1.0f, 1.0f, 0.9f};
    glm::vec4 yellowColor{1.0f, 0.85f, 0.0f, 0.9f};
    glm::vec4 selectedColor{0.0f, 1.0f, 0.5f, 1.0f};     // Green
    float lineWidth = 1.5f;
    float selectedLineWidth = 3.0f;
    float dashLength = 3.0f;    // For dashed markers
    float gapLength = 3.0f;
};

/**
 * Style for rendering barriers
 */
struct HDBarrierStyle {
    glm::vec4 guardrailColor{0.6f, 0.6f, 0.6f, 0.8f};    // Gray
    glm::vec4 concreteColor{0.7f, 0.7f, 0.65f, 0.8f};    // Light gray
    glm::vec4 fenceColor{0.5f, 0.4f, 0.3f, 0.6f};        // Brown
    glm::vec4 curbColor{0.4f, 0.4f, 0.4f, 0.8f};         // Dark gray
    glm::vec4 selectedColor{1.0f, 0.3f, 0.3f, 1.0f};     // Red
    float lineWidth = 2.0f;
    float selectedLineWidth = 4.0f;
};

/**
 * Style for rendering links (road network)
 */
struct HDLinkStyle {
    glm::vec4 motorwayColor{0.8f, 0.4f, 0.4f, 0.6f};     // Red
    glm::vec4 primaryColor{0.8f, 0.6f, 0.2f, 0.6f};      // Orange
    glm::vec4 secondaryColor{0.9f, 0.9f, 0.4f, 0.6f};    // Yellow
    glm::vec4 residentialColor{0.9f, 0.9f, 0.9f, 0.5f};  // White
    glm::vec4 selectedColor{0.5f, 0.0f, 1.0f, 0.9f};     // Purple
    float lineWidth = 3.0f;
    float selectedLineWidth = 6.0f;
};

/**
 * Combined style configuration for HD map layer
 */
struct HDMapLayerStyle {
    HDLaneStyle lane;
    HDLaneMarkerStyle laneMarker;
    HDBarrierStyle barrier;
    HDLinkStyle link;

    // Visibility toggles
    bool showLanes = true;
    bool showLaneMarkers = true;
    bool showBarriers = true;
    bool showLinks = false;  // Off by default (can be cluttered)
};

// =============================================================================
// Layer Configuration
// =============================================================================

/**
 * Configuration for HD map layer
 */
struct HDMapLayerConfig {
    std::shared_ptr<HDMapProvider> provider;  // The HD map provider
    earth::GeoReference reference;            // Geographic reference for rendering
    HDMapLayerStyle style;                    // Rendering style

    // Tile loading
    int maxVisibleTiles = 50;                 // Maximum tiles to render
    bool autoLoadTiles = true;                // Auto-load tiles as camera moves

    // Picking
    float pickingTolerance = 5.0f;            // Pixels for picking tolerance
    bool enablePicking = true;
};

// =============================================================================
// Selection State
// =============================================================================

/**
 * Selection state for HD map objects
 */
struct HDMapSelection {
    std::set<HDObjectId> selectedIds;         // Currently selected object IDs
    HDObjectId hoveredId = 0;                 // Object under mouse cursor (0 = none)

    bool isSelected(HDObjectId id) const {
        return selectedIds.count(id) > 0;
    }

    bool isHovered(HDObjectId id) const {
        return hoveredId == id && id != 0;
    }

    void select(HDObjectId id, bool addToSelection = false) {
        if (!addToSelection) {
            selectedIds.clear();
        }
        if (id != 0) {
            selectedIds.insert(id);
        }
    }

    void deselect(HDObjectId id) {
        selectedIds.erase(id);
    }

    void clearSelection() {
        selectedIds.clear();
    }

    void toggleSelection(HDObjectId id) {
        if (isSelected(id)) {
            deselect(id);
        } else {
            select(id, true);
        }
    }
};

// =============================================================================
// Callbacks
// =============================================================================

/**
 * Callback when an object is selected
 * @param objectId ID of selected object (0 if selection cleared)
 * @param object Pointer to object (nullptr if cleared)
 */
using SelectionCallback = std::function<void(HDObjectId objectId, const HDMapObject* object)>;

/**
 * Callback when mouse hovers over an object
 * @param objectId ID of hovered object (0 if none)
 * @param object Pointer to object (nullptr if none)
 */
using HoverCallback = std::function<void(HDObjectId objectId, const HDMapObject* object)>;

// =============================================================================
// HD Map Layer
// =============================================================================

/**
 * HD map visualization layer
 *
 * Renders HD map objects in the scene with support for:
 * - Mouse-based picking and selection
 * - Configurable rendering styles
 * - Automatic tile loading based on view frustum
 * - Multi-selection with Ctrl+click
 *
 * Usage:
 *   HDMapLayerConfig config;
 *   config.provider = hdmapProvider;
 *   config.reference = geoRef;
 *
 *   HDMapLayer layer(config);
 *
 *   // Set callbacks
 *   layer.setSelectionCallback([](HDObjectId id, const HDMapObject* obj) {
 *       if (obj) {
 *           std::cout << "Selected: " << getTypeName(*obj) << std::endl;
 *       }
 *   });
 *
 *   // In render loop:
 *   layer.update(cameraPosition);
 *   layer.render(viewMatrix, projMatrix);
 *
 *   // Handle mouse click:
 *   layer.pick(mouseX, mouseY, screenWidth, screenHeight, viewMatrix, projMatrix);
 */
class HDMapLayer {
public:
    explicit HDMapLayer(const HDMapLayerConfig& config);
    ~HDMapLayer();

    // Non-copyable
    HDMapLayer(const HDMapLayer&) = delete;
    HDMapLayer& operator=(const HDMapLayer&) = delete;

    // =========================================================================
    // Update and Rendering
    // =========================================================================

    /**
     * Update layer state (load/unload tiles based on camera position)
     *
     * @param cameraPosition Camera position in world coordinates
     */
    void update(const glm::vec3& cameraPosition);

    /**
     * Render the HD map layer
     *
     * @param viewMatrix View transformation matrix
     * @param projMatrix Projection matrix
     */
    void render(const glm::mat4& viewMatrix, const glm::mat4& projMatrix);

    // =========================================================================
    // Picking and Selection
    // =========================================================================

    /**
     * Pick object at screen coordinates
     *
     * @param screenX Screen X coordinate
     * @param screenY Screen Y coordinate
     * @param screenWidth Screen width
     * @param screenHeight Screen height
     * @param viewMatrix View matrix
     * @param projMatrix Projection matrix
     * @param addToSelection If true, add to selection (Ctrl+click behavior)
     * @return Pick result with hit information
     */
    HDPickResult pick(float screenX, float screenY,
                      int screenWidth, int screenHeight,
                      const glm::mat4& viewMatrix, const glm::mat4& projMatrix,
                      bool addToSelection = false);

    /**
     * Update hover state at screen coordinates (call on mouse move)
     *
     * @param screenX Screen X coordinate
     * @param screenY Screen Y coordinate
     * @param screenWidth Screen width
     * @param screenHeight Screen height
     * @param viewMatrix View matrix
     * @param projMatrix Projection matrix
     */
    void updateHover(float screenX, float screenY,
                     int screenWidth, int screenHeight,
                     const glm::mat4& viewMatrix, const glm::mat4& projMatrix);

    /**
     * Clear all selections
     */
    void clearSelection();

    /**
     * Get current selection state
     */
    const HDMapSelection& getSelection() const { return selection_; }

    /**
     * Get selected objects
     */
    std::vector<const HDMapObject*> getSelectedObjects() const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    /**
     * Set callback for selection changes
     */
    void setSelectionCallback(SelectionCallback callback);

    /**
     * Set callback for hover changes
     */
    void setHoverCallback(HoverCallback callback);

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * Get current style
     */
    HDMapLayerStyle& style() { return config_.style; }
    const HDMapLayerStyle& style() const { return config_.style; }

    /**
     * Get the provider
     */
    std::shared_ptr<HDMapProvider> getProvider() const { return config_.provider; }

    /**
     * Reload all tiles
     */
    void reloadTiles();

    /**
     * Get loaded tiles
     */
    const std::vector<HDMapTile>& getLoadedTiles() const { return loadedTiles_; }

    /**
     * Get statistics
     */
    int getLoadedTileCount() const { return static_cast<int>(loadedTiles_.size()); }
    int getVisibleObjectCount() const;

private:
    // Internal methods
    void loadTilesForView(const glm::vec3& cameraPosition);
    void buildRenderData();
    void renderLanes(const glm::mat4& viewMatrix, const glm::mat4& projMatrix);
    void renderLaneMarkers(const glm::mat4& viewMatrix, const glm::mat4& projMatrix);
    void renderBarriers(const glm::mat4& viewMatrix, const glm::mat4& projMatrix);
    void renderLinks(const glm::mat4& viewMatrix, const glm::mat4& projMatrix);

    glm::vec3 hdPointToWorld(const HDPoint3& point) const;
    HDPickResult pickObject(const glm::vec3& rayOrigin, const glm::vec3& rayDir) const;

    // Find object by ID in loaded tiles (returns nullptr if not found)
    const HDMapObject* findObjectById(HDObjectId id) const;

    // Configuration
    HDMapLayerConfig config_;

    // State
    HDMapSelection selection_;
    std::vector<HDMapTile> loadedTiles_;
    std::set<int64_t> loadedTileIds_;

    // Cached object for returning from findObjectById
    mutable std::optional<HDMapObject> cachedFoundObject_;

    // Callbacks
    SelectionCallback selectionCallback_;
    HoverCallback hoverCallback_;

    // Render data (cached for performance)
    bool renderDataDirty_ = true;
};

} // namespace hdmap
} // namespace geodraw
