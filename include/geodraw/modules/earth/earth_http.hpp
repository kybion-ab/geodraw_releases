/*******************************************************************************
 * File: earth_http.hpp
 *
 * Description: Header for HTTP tile fetching functionality.
 * Declares async fetch functions and request manager.
 *
 *
 ******************************************************************************/

#pragma once

#include "geodraw/export/export.hpp"
#include "earth_tiles.hpp"
#include <future>
#include <functional>
#include <mutex>
#include <queue>
#include <set>
#include <string>
#include <vector>

namespace geodraw {
namespace earth {
namespace http {

// =============================================================================
// Fetch Result
// =============================================================================

/**
 * Result of a tile fetch operation
 */
struct FetchResult {
    bool success = false;
    std::vector<unsigned char> data;
    int httpCode = 0;
    std::string errorMessage;

    // === Rate limiting support ===
    int retryAfterSeconds = 0;  // From Retry-After header (0 = not present)
};

// =============================================================================
// Fetch Functions
// =============================================================================

/**
 * Synchronously fetch a tile from URL
 *
 * Blocks until request completes or times out.
 *
 * @param url Full URL to fetch
 * @param timeoutMs Request timeout in milliseconds
 * @return FetchResult with data or error
 */
GEODRAW_API FetchResult fetchTileSync(const std::string& url, int timeoutMs = 10000,
    const std::vector<std::pair<std::string,std::string>>& headers = {});

/**
 * Asynchronously fetch a tile from URL
 *
 * Returns immediately with a future that will contain the result.
 *
 * @param url Full URL to fetch
 * @param timeoutMs Request timeout in milliseconds
 * @param headers Optional HTTP request headers (name, value) pairs
 * @return Future that resolves to FetchResult
 */
GEODRAW_API std::future<FetchResult> fetchTileAsync(const std::string& url, int timeoutMs = 10000,
    const std::vector<std::pair<std::string,std::string>>& headers = {});

// =============================================================================
// Request Manager
// =============================================================================

/**
 * @deprecated Use TileRequestManager from tile_rate_limiter.hpp instead.
 * TileRequestManager provides rate limiting, exponential backoff, and better
 * HTTP 429 handling. This class will be removed in a future release.
 *
 * Manages multiple concurrent tile requests with queuing.
 */
class [[deprecated("Use TileRequestManager from tile_rate_limiter.hpp instead")]] GEODRAW_API RequestManager {
public:
    using CompletionCallback = std::function<void(TileCoord, const std::string& layerId, FetchResult)>;

    /**
     * Create request manager
     * @param maxConcurrent Maximum concurrent HTTP requests (default: 4)
     */
    explicit RequestManager(int maxConcurrent = 4);

    ~RequestManager();

    // Non-copyable
    RequestManager(const RequestManager&) = delete;
    RequestManager& operator=(const RequestManager&) = delete;

    /**
     * Queue a tile fetch request
     *
     * If the tile is already pending or active, this is a no-op.
     *
     * @param coord Tile coordinate
     * @param layerId Layer identifier (e.g., "satellite", "streets")
     * @param url Pre-built URL for the tile
     * @param timeoutMs Request timeout in milliseconds
     */
    void enqueue(const TileCoord& coord,
                 const std::string& layerId,
                 const std::string& url,
                 int timeoutMs = 10000);

    /**
     * Process completed requests and start new ones
     *
     * Call this every frame to:
     * - Check for completed requests and invoke callbacks
     * - Start new requests from the queue if slots available
     */
    void update();

    /**
     * Set callback for completed requests
     */
    void setCompletionCallback(CompletionCallback callback);

    /**
     * Get number of active (in-flight) requests
     */
    int activeCount() const;

    /**
     * Get number of pending (queued) requests
     */
    int pendingCount() const;

    /**
     * Cancel all pending requests
     *
     * Active requests will complete but no new ones will be started.
     */
    void cancelAll();

private:
    void processQueue();

    struct PendingRequest {
        TileCoord coord;
        std::string layerId;
        std::string url;
        int timeoutMs;
    };

    struct ActiveRequest {
        TileCoord coord;
        std::string layerId;
        std::future<FetchResult> future;
    };

    int maxConcurrent_;
    std::queue<PendingRequest> pending_;
    std::vector<ActiveRequest> active_;
    std::set<std::string> pendingKeys_;  // For deduplication
    std::set<std::string> activeKeys_;   // For deduplication
    mutable std::mutex mutex_;
    CompletionCallback completionCallback_;
};

} // namespace http
} // namespace earth
} // namespace geodraw
