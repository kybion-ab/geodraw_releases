/*******************************************************************************
 * File: tile_rate_limiter.hpp
 *
 * Description: Rate-limited tile request management system.
 * Provides centralized request scheduling with:
 * - Token bucket rate limiting (max requests per second)
 * - Concurrent request limiting (max in-flight requests)
 * - Priority queue (visible tiles first, closer LODs prioritized)
 * - Exponential backoff for HTTP 429 responses
 * - Retry-After header support
 *
 * Design Goals:
 * - Prevent API key suspension from MapTiler (rate limit 429 errors)
 * - Smooth interaction during camera movement (non-blocking)
 * - Load tiles in priority order (visible/closer tiles first)
 *
 *
 ******************************************************************************/

#pragma once

#include "earth_tiles.hpp"
#include "earth_http.hpp"
#include <chrono>
#include <functional>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <atomic>
#include <thread>
#include <condition_variable>

namespace geodraw {
namespace earth {

// =============================================================================
// Rate Limiter Configuration
// =============================================================================

/**
 * Configuration for the rate-limited request manager
 */
struct RateLimiterConfig {
    // === Concurrency limits ===
    int maxConcurrentRequests = 6;    // Max HTTP requests in flight at once

    // === Rate limiting (token bucket) ===
    double tokensPerSecond = 8.0;     // New tokens added per second
    double maxTokens = 12.0;          // Max tokens in bucket (burst capacity)

    // === Retry configuration ===
    double initialBackoffSeconds = 1.0;  // First retry delay
    double maxBackoffSeconds = 30.0;     // Maximum retry delay
    double backoffMultiplier = 2.0;      // Exponential multiplier
    int maxRetries = 5;                  // Max retries before giving up

    // === Request timeout ===
    int requestTimeoutMs = 10000;        // HTTP request timeout
};

// =============================================================================
// Token Bucket Rate Limiter
// =============================================================================

/**
 * Token bucket algorithm for rate limiting
 *
 * Tokens are added at a fixed rate up to a maximum capacity.
 * Each request consumes one token. If no tokens are available,
 * the request must wait.
 *
 * Why token bucket:
 * - Simple and robust
 * - Allows short bursts (up to maxTokens)
 * - Smooths out request rate over time
 * - Easy to reason about and tune
 */
class TokenBucket {
public:
    /**
     * Create a token bucket
     * @param tokensPerSecond Rate at which tokens are added
     * @param maxTokens Maximum tokens (burst capacity)
     */
    TokenBucket(double tokensPerSecond = 8.0, double maxTokens = 12.0)
        : tokensPerSecond_(tokensPerSecond)
        , maxTokens_(maxTokens)
        , tokens_(maxTokens)  // Start full
        , lastRefill_(std::chrono::steady_clock::now())
    {}

    /**
     * Try to consume a token
     * @return true if token was available and consumed
     */
    bool tryConsume() {
        refill();
        if (tokens_ >= 1.0) {
            tokens_ -= 1.0;
            return true;
        }
        return false;
    }

    /**
     * Get current token count (for debugging/stats)
     */
    double availableTokens() const {
        return tokens_;
    }

    /**
     * Get time until next token is available
     * @return Seconds until a token will be available (0 if available now)
     */
    double timeUntilAvailable() const {
        if (tokens_ >= 1.0) return 0.0;
        double needed = 1.0 - tokens_;
        return needed / tokensPerSecond_;
    }

    /**
     * Reset the bucket (e.g., after a long pause)
     */
    void reset() {
        tokens_ = maxTokens_;
        lastRefill_ = std::chrono::steady_clock::now();
    }

private:
    void refill() {
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - lastRefill_).count();

        tokens_ = std::min(maxTokens_, tokens_ + elapsed * tokensPerSecond_);
        lastRefill_ = now;
    }

    double tokensPerSecond_;
    double maxTokens_;
    double tokens_;
    std::chrono::steady_clock::time_point lastRefill_;
};

// =============================================================================
// Tile Request with Priority and Retry State
// =============================================================================

/**
 * A tile request with priority information and retry state
 *
 * Requests are ordered by priority:
 * 1. Lower priority number = higher priority (processed first)
 * 2. Closer zoom levels get higher priority
 * 3. Screen-space error can be used to prioritize visible tiles
 */
struct TileRequest {
    TileCoord coord;
    std::string layerId;  // Layer identifier (e.g., "satellite", "streets")
    std::string url;      // Pre-built URL from provider
    std::vector<std::pair<std::string,std::string>> headers;  // Optional HTTP request headers

    // === Priority (lower = higher priority) ===
    float priority = 0.0f;         // 0 = highest priority, higher = lower priority
    bool isVisible = true;         // Currently visible tiles get priority

    // === Retry state ===
    int retryCount = 0;
    std::chrono::steady_clock::time_point nextAllowedTime;

    // === Request tracking ===
    std::chrono::steady_clock::time_point queuedTime;

    /**
     * Generate unique key for deduplication
     */
    std::string key() const {
        return layerId + "/" + coord.key();
    }

    /**
     * Check if this request is allowed to be sent now
     */
    bool canSendNow() const {
        return std::chrono::steady_clock::now() >= nextAllowedTime;
    }

    /**
     * Calculate effective priority (considers visibility and zoom)
     * Lower value = higher priority
     */
    float effectivePriority() const {
        float p = priority;

        // Invisible tiles get much lower priority
        if (!isVisible) {
            p += 1000.0f;
        }

        // Prefer tiles at moderate zoom levels (not too coarse, not too detailed)
        // This helps show something quickly while loading details
        int zoom = coord.z;
        if (zoom < 5) {
            p -= (5 - zoom) * 10.0f;  // Coarse tiles first for overview
        }

        return p;
    }
};

/**
 * Comparator for priority queue (min-heap by effective priority)
 */
struct TileRequestComparator {
    bool operator()(const TileRequest& a, const TileRequest& b) const {
        // Return true if a has LOWER priority than b (for min-heap)
        return a.effectivePriority() > b.effectivePriority();
    }
};

// =============================================================================
// Rate-Limited Tile Request Manager
// =============================================================================

/**
 * Centralized manager for tile HTTP requests with rate limiting
 *
 * Features:
 * - Token bucket rate limiting (prevents request storms)
 * - Concurrent request limiting (doesn't overwhelm network)
 * - Priority queue (loads important tiles first)
 * - Exponential backoff for 429 errors
 * - Retry-After header support
 *
 * Usage:
 *   RateLimiterConfig config;
 *   config.maxConcurrentRequests = 6;
 *   config.tokensPerSecond = 8.0;
 *
 *   TileRequestManager manager(config);
 *   manager.setCompletionCallback([](TileCoord, const std::string& layerId, http::FetchResult) {
 *       // Handle completed request
 *   });
 *
 *   // Queue tile requests (call from update loop)
 *   std::string url = provider->buildTileUrl(TileCoord{10, 163, 395}, "satellite");
 *   manager.enqueue(TileCoord{10, 163, 395}, "satellite",
 *                   url, 0.0f, true);  // priority=0, visible=true
 *
 *   // Process queue (call every frame)
 *   manager.update();
 *
 * Thread safety:
 * - All public methods are thread-safe
 * - Callbacks are invoked from the calling thread (update())
 */
class TileRequestManager {
public:
    using CompletionCallback = std::function<void(TileCoord, const std::string& layerId, http::FetchResult)>;

    /**
     * Create request manager with configuration
     */
    explicit TileRequestManager(const RateLimiterConfig& config = RateLimiterConfig{})
        : config_(config)
        , rateLimiter_(config.tokensPerSecond, config.maxTokens)
        , shutdown_(false)
    {}

    ~TileRequestManager() {
        shutdown();
    }

    // Non-copyable
    TileRequestManager(const TileRequestManager&) = delete;
    TileRequestManager& operator=(const TileRequestManager&) = delete;

    /**
     * Queue a tile fetch request
     *
     * The request will be scheduled based on:
     * - Rate limiting (token bucket)
     * - Concurrent request limit
     * - Priority (visible tiles first)
     * - Retry backoff (if previously failed with 429)
     *
     * @param coord Tile coordinate
     * @param layerId Layer identifier (e.g., "satellite", "streets")
     * @param url Pre-built URL from the provider
     * @param priority Lower = higher priority (0 = highest)
     * @param isVisible Is this tile currently visible in the viewport?
     */
    void enqueue(const TileCoord& coord,
                 const std::string& layerId,
                 const std::string& url,
                 float priority = 0.0f,
                 bool isVisible = true,
                 std::vector<std::pair<std::string,std::string>> headers = {}) {
        std::lock_guard<std::mutex> lock(mutex_);

        // Generate unique key for deduplication
        std::string key = layerId + "/" + coord.key();

        // Skip if already pending or active
        if (pendingKeys_.count(key) > 0 || activeKeys_.count(key) > 0) {
            return;
        }

        // Check for existing retry state
        TileRequest request;
        request.coord = coord;
        request.layerId = layerId;
        request.url = url;
        request.headers = std::move(headers);
        request.priority = priority;
        request.isVisible = isVisible;
        request.queuedTime = std::chrono::steady_clock::now();

        auto retryIt = retryState_.find(key);
        if (retryIt != retryState_.end()) {
            request.retryCount = retryIt->second.retryCount;
            request.nextAllowedTime = retryIt->second.nextAllowedTime;
        }

        pendingQueue_.push(request);
        pendingKeys_.insert(key);
    }

    /**
     * Update visibility/priority for a pending request
     *
     * Use this when the camera moves and tile priorities change.
     * Note: This rebuilds the queue, so call sparingly.
     *
     * @param coord Tile coordinate
     * @param layerId Layer identifier (e.g., "satellite", "streets")
     * @param priority New priority
     * @param isVisible New visibility state
     */
    void updatePriority(const TileCoord& coord, const std::string& layerId,
                        float priority, bool isVisible) {
        std::lock_guard<std::mutex> lock(mutex_);

        std::string key = layerId + "/" + coord.key();

        // Skip if not pending (already active or not queued)
        if (pendingKeys_.count(key) == 0) {
            return;
        }

        // Rebuild queue with updated priority
        // This is O(n log n) but called infrequently
        std::priority_queue<TileRequest, std::vector<TileRequest>, TileRequestComparator> newQueue;

        while (!pendingQueue_.empty()) {
            TileRequest req = pendingQueue_.top();
            pendingQueue_.pop();

            if (req.key() == key) {
                req.priority = priority;
                req.isVisible = isVisible;
            }

            newQueue.push(req);
        }

        pendingQueue_ = std::move(newQueue);
    }

    /**
     * Process completed requests and start new ones
     *
     * Call this every frame to:
     * - Check for completed async requests
     * - Apply rate limiting and start new requests
     * - Handle 429 responses with backoff
     *
     * This is non-blocking and safe to call frequently.
     */
    void update() {
        std::lock_guard<std::mutex> lock(mutex_);

        // Process completed requests
        processCompletedRequests();

        // Try to start new requests (rate limited)
        processQueue();
    }

    /**
     * Set callback for completed requests
     * Callback is invoked from update() on the calling thread
     */
    void setCompletionCallback(CompletionCallback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        completionCallback_ = std::move(callback);
    }

    /**
     * Cancel all pending requests
     * Active requests will complete but won't trigger callbacks
     */
    void cancelAll() {
        std::lock_guard<std::mutex> lock(mutex_);

        // Clear pending queue
        while (!pendingQueue_.empty()) {
            pendingQueue_.pop();
        }
        pendingKeys_.clear();
        retryState_.clear();
    }

    /**
     * Graceful shutdown - wait for active requests to complete
     */
    void shutdown() {
        shutdown_.store(true);
        cancelAll();

        // Wait for active requests (with timeout)
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!active_.empty() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            std::lock_guard<std::mutex> lock(mutex_);
            processCompletedRequests();
        }
    }

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * Get number of active (in-flight) requests
     */
    int activeCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return static_cast<int>(active_.size());
    }

    /**
     * Get number of pending (queued) requests
     */
    int pendingCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return static_cast<int>(pendingQueue_.size());
    }

    /**
     * Get number of tiles in retry backoff
     */
    int retryCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return static_cast<int>(retryState_.size());
    }

    /**
     * Get available tokens in rate limiter
     */
    double availableTokens() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return rateLimiter_.availableTokens();
    }

    /**
     * Get statistics summary
     */
    struct Stats {
        int activeRequests;
        int pendingRequests;
        int tilesInBackoff;
        double availableTokens;
        int totalRetries;
        int total429Errors;
    };

    Stats getStats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return {
            static_cast<int>(active_.size()),
            static_cast<int>(pendingQueue_.size()),
            static_cast<int>(retryState_.size()),
            rateLimiter_.availableTokens(),
            totalRetries_,
            total429Errors_
        };
    }

private:
    // =========================================================================
    // Internal Types
    // =========================================================================

    struct ActiveRequest {
        TileCoord coord;
        std::string layerId;
        std::string key;
        std::string url;  // Preserved for retries
        std::vector<std::pair<std::string,std::string>> headers;  // Preserved for retries
        std::future<http::FetchResult> future;
    };

    struct RetryInfo {
        int retryCount = 0;
        std::chrono::steady_clock::time_point nextAllowedTime;
    };

    // =========================================================================
    // Internal Methods
    // =========================================================================

    void processCompletedRequests() {
        // Check for completed async requests (non-blocking)
        auto it = active_.begin();
        while (it != active_.end()) {
            // Check if future is ready (non-blocking)
            if (it->future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
                try {
                    http::FetchResult result = it->future.get();
                    handleCompletedRequest(it->coord, it->layerId, it->key, it->url, it->headers, result);
                } catch (const std::exception& e) {
                    // Future threw an exception
                    http::FetchResult errorResult;
                    errorResult.success = false;
                    errorResult.errorMessage = std::string("Exception: ") + e.what();
                    handleCompletedRequest(it->coord, it->layerId, it->key, it->url, it->headers, errorResult);
                }

                activeKeys_.erase(it->key);
                it = active_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void handleCompletedRequest(const TileCoord& coord, const std::string& layerId,
                                 const std::string& key, const std::string& url,
                                 const std::vector<std::pair<std::string,std::string>>& headers,
                                 http::FetchResult& result) {
        // Handle HTTP 429 - rate limited
        if (result.httpCode == 429) {
            total429Errors_++;
            handleRateLimitError(coord, layerId, key, url, headers, result);
            return;
        }

        // Success or non-retryable error - clear retry state
        retryState_.erase(key);

        // Invoke callback
        if (completionCallback_ && !shutdown_.load()) {
            completionCallback_(coord, layerId, std::move(result));
        }
    }

    void handleRateLimitError(const TileCoord& coord, const std::string& layerId,
                               const std::string& key, const std::string& url,
                               const std::vector<std::pair<std::string,std::string>>& headers,
                               const http::FetchResult& result) {
        // Get or create retry state
        auto& retry = retryState_[key];
        retry.retryCount++;
        totalRetries_++;

        // Check if we should give up
        if (retry.retryCount > config_.maxRetries) {
            retryState_.erase(key);

            // Report as failed
            http::FetchResult failResult;
            failResult.success = false;
            failResult.httpCode = 429;
            failResult.errorMessage = "Rate limited - max retries exceeded";

            if (completionCallback_ && !shutdown_.load()) {
                completionCallback_(coord, layerId, std::move(failResult));
            }
            return;
        }

        // Calculate backoff delay
        double delaySeconds = config_.initialBackoffSeconds;

        // Use Retry-After header if present
        if (result.retryAfterSeconds > 0) {
            delaySeconds = static_cast<double>(result.retryAfterSeconds);
        } else {
            // Exponential backoff: 1s, 2s, 4s, 8s, ... capped at maxBackoffSeconds
            for (int i = 1; i < retry.retryCount; ++i) {
                delaySeconds *= config_.backoffMultiplier;
            }
        }

        // Cap at maximum
        delaySeconds = std::min(delaySeconds, config_.maxBackoffSeconds);

        // Set next allowed time
        retry.nextAllowedTime = std::chrono::steady_clock::now() +
            std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                std::chrono::duration<double>(delaySeconds));

        // Re-queue the request to the deferred queue
        // It will be moved to pending queue when the backoff expires
        TileRequest retryRequest;
        retryRequest.coord = coord;
        retryRequest.layerId = layerId;
        retryRequest.url = url;
        retryRequest.headers = headers;
        retryRequest.retryCount = retry.retryCount;
        retryRequest.nextAllowedTime = retry.nextAllowedTime;
        retryRequest.priority = 0.0f;  // High priority for retry
        retryRequest.isVisible = true;
        retryRequest.queuedTime = std::chrono::steady_clock::now();

        deferredQueue_.push(retryRequest);
        pendingKeys_.insert(key);  // Mark as pending to prevent duplicate enqueues
    }

    void processQueue() {
        auto now = std::chrono::steady_clock::now();

        // Process pending requests
        while (!pendingQueue_.empty() &&
               active_.size() < static_cast<size_t>(config_.maxConcurrentRequests)) {

            TileRequest request = pendingQueue_.top();

            // Check if request is still in backoff
            if (request.nextAllowedTime > now) {
                // Skip this request for now, but don't remove it
                // Move to a deferred queue
                deferredQueue_.push(request);
                pendingQueue_.pop();
                continue;
            }

            // Check rate limit
            if (!rateLimiter_.tryConsume()) {
                // No tokens available - stop processing
                break;
            }

            // Remove from pending
            pendingQueue_.pop();
            pendingKeys_.erase(request.key());

            // Start async fetch using the pre-built URL from the provider
            ActiveRequest activeReq;
            activeReq.coord = request.coord;
            activeReq.layerId = request.layerId;
            activeReq.key = request.key();
            activeReq.url = request.url;  // Preserve for retry
            activeReq.headers = request.headers;  // Preserve for retry
            activeReq.future = http::fetchTileAsync(request.url, config_.requestTimeoutMs, request.headers);

            activeKeys_.insert(activeReq.key);
            active_.push_back(std::move(activeReq));
        }

        // Re-check deferred requests
        while (!deferredQueue_.empty()) {
            TileRequest request = deferredQueue_.top();

            if (request.nextAllowedTime <= now) {
                // Ready to retry - move back to main queue
                deferredQueue_.pop();
                pendingQueue_.push(request);
            } else {
                // Still in backoff - stop checking
                break;
            }
        }
    }

    // =========================================================================
    // State
    // =========================================================================

    RateLimiterConfig config_;
    mutable TokenBucket rateLimiter_;
    std::atomic<bool> shutdown_;

    // Request queues
    std::priority_queue<TileRequest, std::vector<TileRequest>, TileRequestComparator> pendingQueue_;
    std::priority_queue<TileRequest, std::vector<TileRequest>, TileRequestComparator> deferredQueue_;
    std::vector<ActiveRequest> active_;

    // Deduplication
    std::unordered_set<std::string> pendingKeys_;
    std::unordered_set<std::string> activeKeys_;

    // Retry tracking
    std::unordered_map<std::string, RetryInfo> retryState_;

    // Callback
    CompletionCallback completionCallback_;

    // Thread safety
    mutable std::mutex mutex_;

    // Statistics
    int totalRetries_ = 0;
    int total429Errors_ = 0;
};

} // namespace earth
} // namespace geodraw
