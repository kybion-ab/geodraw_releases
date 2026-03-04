/*******************************************************************************
 * File: hdmap_provider.hpp
 *
 * Description: Abstract interface for HD map data providers. Defines the
 * contract for HD map services like, HERE HD Live Map, TomTom HD Map, etc.
 *
 * Unlike TileProvider (which fetches raster/vector tiles for visualization),
 * HDMapProvider loads structured semantic data for autonomous driving.
 *
 *
 ******************************************************************************/

#pragma once

#include "hdmap_types.hpp"
#include "geodraw/modules/earth/earth_coords.hpp"
#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <vector>

namespace geodraw {
namespace hdmap {

// =============================================================================
// Provider Configuration
// =============================================================================

/**
 * Configuration for connecting to an HD map provider
 *
 * For local files:
 *   config.mapPath = "/path/to/map.mma";
 *
 * For server connections:
 *   config.apiEndpoint = "https://server.example.com";
 *   config.mapUuid = "4de0b294-0a5b-4671-9b20-81663ef5d1da";
 */
struct HDMapProviderConfig {
    std::string mapPath;            // Path to local map data (file/directory)
    std::string apiKey;             // API key for cloud services (optional)
    std::string apiEndpoint;        // API endpoint URL (optional)
    std::string mapUuid;            // Map UUID for server connections (optional)
    bool enableCaching = true;      // Cache loaded tiles in memory
    int maxCachedTiles = 100;       // Maximum tiles to keep in memory cache
};

/**
 * Provider capabilities flags
 */
struct HDMapCapabilities {
    bool supportsLanes = false;
    bool supportsLaneMarkers = false;
    bool supportsBarriers = false;
    bool supportsLinks = false;
    bool supportsRealTimeUpdates = false;
    bool supportsTileStreaming = false;
};

// =============================================================================
// Load Result
// =============================================================================

/**
 * Result of loading HD map data
 */
struct HDMapLoadResult {
    bool success = false;
    std::string errorMessage;
    std::vector<int64_t> loadedTileIds;
};

// =============================================================================
// Abstract Provider Interface
// =============================================================================

/**
 * Abstract base class for HD map data providers
 *
 * HD map providers load structured semantic map data (lanes, barriers, links)
 * as opposed to visualization tiles. The data is used for:
 * - Autonomous vehicle planning and perception
 * - Scene understanding and semantic visualization
 * - Object picking and selection in visualization tools
 *
 * Usage:
 *   auto hdmap = std::make_shared<HDProvider>(config);
 *   hdmap->initialize();
 *
 *   // Get tiles covering a location
 *   auto tileIds = hdmap->getTileIdsForLocation(earth::GeoCoord(59.3293, 18.0686));
 *
 *   // Load tiles
 *   for (auto tileId : tileIds) {
 *       auto tile = hdmap->loadTile(tileId);
 *       // Process lanes, barriers, etc.
 *   }
 */
class HDMapProvider {
public:
    virtual ~HDMapProvider() = default;

    // =========================================================================
    // Provider Identity
    // =========================================================================

    /**
     * Get the provider name (e.g., "HERE HD Live Map")
     */
    virtual std::string name() const = 0;

    /**
     * Get provider version string
     */
    virtual std::string version() const = 0;

    /**
     * Get provider capabilities
     */
    virtual HDMapCapabilities capabilities() const = 0;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * Initialize the provider with configuration
     * Must be called before any data loading operations
     *
     * @param config Provider configuration
     * @return true if initialization succeeded
     */
    virtual bool initialize(const HDMapProviderConfig& config) = 0;

    /**
     * Check if provider is initialized and ready
     */
    virtual bool isInitialized() const = 0;

    /**
     * Shut down the provider and release resources
     */
    virtual void shutdown() = 0;

    // =========================================================================
    // Tile Discovery
    // =========================================================================

    /**
     * Get tile IDs that cover a geographic location
     *
     * @param location Geographic coordinate (WGS84)
     * @return Vector of tile IDs covering this location
     */
    virtual std::vector<int64_t> getTileIdsForLocation(const earth::GeoCoord& location) const = 0;

    /**
     * Get tile IDs within a geographic bounding box
     *
     * @param minLat Minimum latitude
     * @param minLon Minimum longitude
     * @param maxLat Maximum latitude
     * @param maxLon Maximum longitude
     * @return Vector of tile IDs within the bounds
     */
    virtual std::vector<int64_t> getTileIdsForBounds(
        double minLat, double minLon,
        double maxLat, double maxLon) const = 0;

    /**
     * Get all available tile IDs in the map
     * Use with caution - may be a large list
     */
    virtual std::vector<int64_t> getAllTileIds() const = 0;

    // =========================================================================
    // Tile Loading
    // =========================================================================

    /**
     * Load a tile synchronously
     *
     * @param tileId The tile identifier
     * @return HDMapTile with loaded data (empty if tile not found)
     */
    virtual HDMapTile loadTile(int64_t tileId) = 0;

    /**
     * Load a tile asynchronously
     *
     * @param tileId The tile identifier
     * @return Future that resolves to the loaded tile
     */
    virtual std::future<HDMapTile> loadTileAsync(int64_t tileId) = 0;

    /**
     * Load multiple tiles synchronously
     *
     * @param tileIds Vector of tile identifiers
     * @return Vector of loaded tiles (same order as input)
     */
    virtual std::vector<HDMapTile> loadTiles(const std::vector<int64_t>& tileIds) = 0;

    /**
     * Load all HD map objects within a geographic bounding box
     *
     * More efficient than loadTiles() when you don't need tile-based organization.
     * Objects are queried directly by bounds, not by tile ID.
     *
     * @param minLat Minimum latitude (south)
     * @param minLon Minimum longitude (west)
     * @param maxLat Maximum latitude (north)
     * @param maxLon Maximum longitude (east)
     * @return HDMapTile containing all objects within bounds (tileId = 0)
     */
    virtual HDMapTile loadObjectsInBounds(
        double minLat, double minLon,
        double maxLat, double maxLon) = 0;

    /**
     * Check if a tile is loaded in cache
     */
    virtual bool isTileCached(int64_t tileId) const = 0;

    /**
     * Evict a tile from cache
     */
    virtual void evictTile(int64_t tileId) = 0;

    /**
     * Clear all cached tiles
     */
    virtual void clearCache() = 0;

    // =========================================================================
    // Object Lookup (for picking support)
    // =========================================================================

    /**
     * Find an object by its GeoDraw ID
     *
     * @param objectId The HDObjectId assigned by GeoDraw
     * @return Pointer to object if found, nullptr otherwise
     */
    virtual const HDMapObject* findObject(HDObjectId objectId) const = 0;

    /**
     * Find an object by its provider-specific ID
     *
     * @param providerObjectId The original ID from the provider
     * @return Pointer to object if found, nullptr otherwise
     */
    virtual const HDMapObject* findObjectByProviderId(
        const std::string& providerObjectId) const = 0;

    // =========================================================================
    // Coordinate System Info
    // =========================================================================

    /**
     * Get the coordinate reference system used by this provider
     * @return CRS identifier (e.g., "EPSG:4326", "WGS84")
     */
    virtual std::string coordinateSystem() const { return "EPSG:4326"; }

    /**
     * Get the map's datum/epoch if applicable
     */
    virtual std::string mapDatum() const { return "WGS84"; }

protected:
    /**
     * Generate a unique GeoDraw object ID
     * Providers should use this to assign IDs to loaded objects
     */
    static HDObjectId generateObjectId() {
        static std::atomic<HDObjectId> nextId{1};
        return nextId++;
    }
};

// =============================================================================
// Factory Function
// =============================================================================

/**
 * Create an HD map provider by name
 *
 * @param providerName Provider name (e.g., "HERE HD Live Map")
 * @return Shared pointer to provider, or nullptr if unknown
 */
std::shared_ptr<HDMapProvider> createHDMapProvider(const std::string& providerName);

} // namespace hdmap
} // namespace geodraw
