/*******************************************************************************
 * File: tile_provider.hpp
 *
 * Description: Abstract interface for map tile providers.
 * Enables multiple external map APIs (MapTiler, Mapbox, Google Maps, OSM, etc.)
 * to plug into GeoDraw through a common interface.
 *
 * Design Goals:
 * - Provider-agnostic tile fetching (URL building, authentication)
 * - Provider-specific rate limiting configuration
 * - Extensible for custom tile formats and parsing
 * - Clean separation between provider logic and fetch/cache infrastructure
 *
 *
 ******************************************************************************/

#pragma once

#include "geodraw/export/export.hpp"
#include "geodraw/modules/earth/earth_tiles.hpp"
#include <memory>
#include <string>
#include <vector>
#include <utility>

namespace geodraw {
namespace earth {

// =============================================================================
// Tile Layer Configuration
// =============================================================================

/**
 * Configuration for a tile layer within a provider
 *
 * Each provider offers one or more layers (e.g., satellite, streets, terrain).
 * This struct describes a single layer's capabilities and format.
 */
struct TileLayerConfig {
    std::string id;           // Internal identifier (e.g., "satellite")
    std::string displayName;  // Human-readable name (e.g., "Satellite Imagery")
    std::string format;       // File format: "png", "jpg", "webp", "pbf"
    int tileSize = 256;       // Tile dimensions in pixels
    int minZoom = 0;          // Minimum available zoom level
    int maxZoom = 22;         // Maximum available zoom level

    /**
     * Get file extension for this layer
     */
    const char* extension() const {
        if (format == "jpg" || format == "jpeg") return "jpg";
        if (format == "png") return "png";
        if (format == "webp") return "webp";
        if (format == "pbf") return "pbf";
        if (format == "terrain-rgb") return "png";
        if (format == "terrain") return "terrain";
        return "png";
    }

    /**
     * Check if this layer contains vector data (PBF)
     */
    bool isVectorLayer() const {
        return format == "pbf";
    }

    /**
     * Check if this layer contains raster data (PNG/JPG/WebP)
     */
    bool isRasterLayer() const {
        return format == "png" || format == "jpg" || format == "jpeg" ||
               format == "webp" || format == "terrain-rgb";
    }

    /**
     * Check if this layer contains terrain mesh data (quantized mesh)
     */
    bool isTerrainMeshLayer() const {
        return format == "terrain";
    }
};

// =============================================================================
// Provider Rate Limits
// =============================================================================

/**
 * Rate limiting configuration for a provider
 *
 * Different providers have different API rate limits. This struct allows
 * each provider to specify its limits for proper rate limiting.
 */
struct ProviderRateLimits {
    double tokensPerSecond = 10.0;   // Token bucket refill rate
    int maxBurstTokens = 15;         // Maximum burst capacity
    int maxConcurrentRequests = 6;   // Parallel request limit

    /**
     * Default rate limits (conservative values that work for most providers)
     */
    static ProviderRateLimits defaults() {
        return {10.0, 15, 6};
    }

    /**
     * MapTiler free tier limits
     */
    static ProviderRateLimits mapTilerFree() {
        return {8.0, 12, 6};
    }
};

// =============================================================================
// Tile Provider Abstract Base Class
// =============================================================================

/**
 * Abstract base class for map tile providers
 *
 * Implementations of this class enable different map APIs to integrate
 * with GeoDraw's tile fetching and caching infrastructure.
 *
 * The provider is responsible for:
 * - Building tile URLs with proper authentication
 * - Providing rate limit configuration
 * - Defining available layers
 * - Optionally handling custom tile parsing
 *
 * The provider is NOT responsible for:
 * - HTTP fetching (handled by TileRequestManager)
 * - Caching (handled by TileCache)
 * - GPU upload (handled by EarthLayer)
 *
 * Example implementation:
 *   class MapTilerProvider : public TileProvider {
 *   public:
 *       MapTilerProvider(std::string apiKey);
 *       std::string name() const override { return "MapTiler"; }
 *       // ... implement other methods
 *   };
 *
 * Example usage:
 *   auto provider = std::make_shared<MapTilerProvider>("YOUR_API_KEY");
 *   EarthLayerConfig config;
 *   config.provider = provider;
 *   config.layerId = "satellite";
 */
class TileProvider {
public:
    virtual ~TileProvider() = default;

    // =========================================================================
    // Required Methods (must implement)
    // =========================================================================

    /**
     * Get the provider's name (e.g., "MapTiler", "Mapbox", "OpenStreetMap")
     *
     * Used for logging, debugging, and cache directory naming.
     */
    virtual std::string name() const = 0;

    /**
     * Get available tile layers offered by this provider
     *
     * Returns a list of layer configurations describing each available layer.
     * Layer IDs should be stable strings suitable for use in code.
     *
     * Example:
     *   return {
     *       {"satellite", "Satellite Imagery", "jpg", 256, 0, 20},
     *       {"streets", "Street Map (Vector)", "pbf", 512, 0, 14},
     *       {"terrain", "Terrain RGB", "png", 256, 0, 12}
     *   };
     */
    virtual std::vector<TileLayerConfig> availableLayers() const = 0;

    /**
     * Build URL for a specific tile request
     *
     * @param coord Tile coordinate (z/x/y)
     * @param layerId Layer identifier from availableLayers()
     * @return Full URL for fetching the tile
     */
    virtual std::string buildTileUrl(const TileCoord& coord,
                                      const std::string& layerId) const = 0;

    /**
     * Get rate limiting configuration for this provider
     *
     * Returns the provider's API rate limits. The TileRequestManager
     * will use these to throttle requests appropriately.
     */
    virtual ProviderRateLimits rateLimits() const = 0;

    // =========================================================================
    // Optional Methods (have defaults)
    // =========================================================================

    /**
     * Get HTTP headers for authentication
     *
     * Some providers use header-based authentication (e.g., Bearer token)
     * instead of URL query parameters. Override this to provide auth headers.
     *
     * Default: empty (no auth headers)
     *
     * @return Vector of (header-name, header-value) pairs
     */
    virtual std::vector<std::pair<std::string, std::string>>
        authHeaders() const { return {}; }

    /**
     * Get HTTP headers required for a specific tile layer
     *
     * Override to provide layer-specific request headers (e.g., Accept header
     * for quantized mesh terrain tiles which require a specific content type).
     *
     * Default: empty (no additional headers)
     *
     * @param layerId Layer identifier from availableLayers()
     * @return Vector of (header-name, header-value) pairs
     */
    virtual std::vector<std::pair<std::string, std::string>>
        buildTileHeaders(const std::string& /*layerId*/) const { return {}; }

    /**
     * Get cache subdirectory name
     *
     * Override to customize the disk cache directory structure.
     * Default uses the provider name.
     *
     * @return Subdirectory name for this provider's cached tiles
     */
    virtual std::string cachePrefix() const { return name(); }

    /**
     * Refresh available layers from provider API
     *
     * Some providers offer dynamic layer discovery via API.
     * Override this method to fetch layer information from the provider.
     *
     * Default: returns false (static layer configuration)
     *
     * @return true if layers were refreshed, false if using static config
     */
    virtual bool refreshLayers() { return false; }

    // =========================================================================
    // Helper Methods
    // =========================================================================

    /**
     * Get configuration for a specific layer by ID
     *
     * @param layerId Layer identifier to look up
     * @return Pointer to layer config, or nullptr if not found
     */
    const TileLayerConfig* getLayer(const std::string& layerId) const {
        auto layers = availableLayers();
        for (const auto& layer : layers) {
            if (layer.id == layerId) {
                // Return a pointer to a static copy
                static thread_local TileLayerConfig result;
                result = layer;
                return &result;
            }
        }
        return nullptr;
    }

    /**
     * Check if a layer ID is valid for this provider
     */
    bool hasLayer(const std::string& layerId) const {
        return getLayer(layerId) != nullptr;
    }

    /**
     * Get the default layer ID (first layer in the list)
     */
    std::string defaultLayerId() const {
        auto layers = availableLayers();
        return layers.empty() ? "" : layers[0].id;
    }
};

// =============================================================================
// Provider Factory (for future use)
// =============================================================================

/**
 * Create a provider by name
 *
 * This factory function will be extended as more providers are added.
 * Currently only MapTiler is supported.
 *
 * @param providerName Name of the provider (e.g., "MapTiler")
 * @param apiKey API key or access token
 * @return Shared pointer to provider, or nullptr if not found
 */
GEODRAW_API std::shared_ptr<TileProvider> createProvider(const std::string& providerName,
                                                         const std::string& apiKey);

} // namespace earth
} // namespace geodraw
