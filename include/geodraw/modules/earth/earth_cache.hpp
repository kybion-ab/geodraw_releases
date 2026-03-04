/*******************************************************************************
 * File: earth_cache.hpp
 *
 * Description: Tile caching system with memory and disk layers.
 * Provides LRU eviction to manage cache size and persistent storage
 * between sessions.
 *
 * Why required: Network tile fetching is slow and expensive. Caching
 * avoids repeated requests for the same tiles and provides offline
 * capability for previously viewed areas.
 *
 *
 ******************************************************************************/

#pragma once

#include "earth_tiles.hpp"
#include "geodraw/log/log.hpp"
#include <filesystem>
#include <fstream>
#include <list>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace geodraw {
namespace earth {

// =============================================================================
// Cache Configuration
// =============================================================================

/**
 * Configuration for the tile cache
 */
struct CacheConfig {
    std::string cachePath = (std::filesystem::path(std::getenv("HOME")) / ".cache" / "geodraw" / "tiles").string();  // Base cache directory
    size_t maxCacheSizeMB = 512;                     // Max disk cache size in MB
    size_t maxMemoryCacheTiles = 1024;               // Max tiles in memory
    bool persistCache = true;                        // Keep cache between sessions
};

// =============================================================================
// Tile Cache
// =============================================================================

/**
 * Two-level tile cache with memory and disk layers
 *
 * Memory cache: Fast access, limited size, LRU eviction
 * Disk cache: Persistent, larger, LRU eviction based on file modification time
 *
 * Thread-safe: All public methods are protected by mutex
 */
class TileCache {
public:
    explicit TileCache(const CacheConfig& config = CacheConfig())
        : config_(config) {
        if (config_.persistCache) {
            std::filesystem::create_directories(config_.cachePath);
        }
    }

    ~TileCache() = default;

    // Non-copyable
    TileCache(const TileCache&) = delete;
    TileCache& operator=(const TileCache&) = delete;

    // =========================================================================
    // Cache Operations
    // =========================================================================

    /**
     * Check if a tile is in cache (memory or disk)
     *
     * @param coord Tile coordinate
     * @param layerId Layer identifier (e.g., "satellite", "streets")
     * @param cachePrefix Provider cache prefix (e.g., "maptiler", "mapbox")
     */
    bool contains(const TileCoord& coord, const std::string& layerId,
                  const std::string& cachePrefix = "") const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string key = makeKey(coord, layerId, cachePrefix);

        // Check memory cache
        if (memoryCache_.count(key) > 0) {
            return true;
        }

        // Check disk cache
        if (config_.persistCache) {
            std::string path = tilePath(coord, layerId, cachePrefix);
            return std::filesystem::exists(path);
        }

        return false;
    }

    /**
     * Get tile from cache
     *
     * Checks memory first, then disk. If found on disk, promotes to memory.
     *
     * @param coord Tile coordinate
     * @param layerId Layer identifier (e.g., "satellite", "streets")
     * @param cachePrefix Provider cache prefix (e.g., "maptiler", "mapbox")
     * @return Tile data if found, nullopt otherwise
     */
    std::optional<std::vector<unsigned char>> get(const TileCoord& coord,
                                                   const std::string& layerId,
                                                   const std::string& cachePrefix = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string key = makeKey(coord, layerId, cachePrefix);

        // Check memory cache first
        auto memIt = memoryCache_.find(key);
        if (memIt != memoryCache_.end()) {
            // Move to front of LRU list (most recently used)
            lruList_.splice(lruList_.begin(), lruList_, memIt->second.lruIt);
            log_verbose("Cache memory hit: " + key);
            return memIt->second.data;
        }

        // Check disk cache
        if (config_.persistCache) {
            std::string path = tilePath(coord, layerId, cachePrefix);
            bool exists = std::filesystem::exists(path);
            log_spam("Cache checking disk: " + path + " exists=" + std::to_string(exists));
            if (exists) {
                auto data = loadFromDisk(path);
                if (data.has_value()) {
                    // Promote to memory cache
                    putInMemory(key, *data);
                    log_verbose("Cache disk hit: " + key + " size=" + std::to_string(data->size()));
                    return data;
                }
            }
        }

        log_verbose("Cache miss: " + key);
        return std::nullopt;
    }

    /**
     * Store tile in cache (both memory and disk if persistent)
     *
     * @param coord Tile coordinate
     * @param layerId Layer identifier (e.g., "satellite", "streets")
     * @param cachePrefix Provider cache prefix (e.g., "maptiler", "mapbox")
     * @param data Tile data to store
     */
    void put(const TileCoord& coord, const std::string& layerId,
             const std::string& cachePrefix,
             const std::vector<unsigned char>& data) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string key = makeKey(coord, layerId, cachePrefix);

        // Store in memory
        putInMemory(key, data);

        // Store on disk if persistent
        if (config_.persistCache) {
            std::string path = tilePath(coord, layerId, cachePrefix);
            saveToDisk(path, data);
        }
    }

    /**
     * Remove a specific tile from cache
     *
     * @param coord Tile coordinate
     * @param layerId Layer identifier (e.g., "satellite", "streets")
     * @param cachePrefix Provider cache prefix (e.g., "maptiler", "mapbox")
     */
    void remove(const TileCoord& coord, const std::string& layerId,
                const std::string& cachePrefix = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string key = makeKey(coord, layerId, cachePrefix);

        // Remove from memory
        auto memIt = memoryCache_.find(key);
        if (memIt != memoryCache_.end()) {
            lruList_.erase(memIt->second.lruIt);
            memoryCache_.erase(memIt);
        }

        // Remove from disk
        if (config_.persistCache) {
            std::string path = tilePath(coord, layerId, cachePrefix);
            std::filesystem::remove(path);
        }
    }

    /**
     * Clear all cached tiles
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);

        // Clear memory cache
        memoryCache_.clear();
        lruList_.clear();

        // Clear disk cache
        if (config_.persistCache) {
            std::filesystem::remove_all(config_.cachePath);
            std::filesystem::create_directories(config_.cachePath);
        }
    }

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * Get number of tiles in memory cache
     */
    size_t memoryTileCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return memoryCache_.size();
    }

    /**
     * Get approximate memory usage in bytes
     */
    size_t memoryUsageBytes() const {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t total = 0;
        for (const auto& [key, entry] : memoryCache_) {
            total += entry.data.size();
        }
        return total;
    }

    /**
     * Get disk cache usage in bytes
     */
    size_t diskUsageBytes() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!config_.persistCache) {
            return 0;
        }

        size_t total = 0;
        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(config_.cachePath)) {
                if (entry.is_regular_file()) {
                    total += entry.file_size();
                }
            }
        } catch (const std::exception&) {
            // Directory might not exist yet
        }
        return total;
    }

    /**
     * Get number of tiles on disk
     */
    size_t diskTileCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!config_.persistCache) {
            return 0;
        }

        size_t count = 0;
        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(config_.cachePath)) {
                if (entry.is_regular_file() && entry.path().extension() == ".tile") {
                    count++;
                }
            }
        } catch (const std::exception&) {
            // Directory might not exist yet
        }
        return count;
    }

    /**
     * Get cache configuration
     */
    const CacheConfig& config() const { return config_; }

    // =========================================================================
    // Maintenance
    // =========================================================================

    /**
     * Evict oldest tiles from disk cache to stay under size limit
     *
     * Call this periodically to manage disk space.
     */
    void evictDiskIfNeeded() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!config_.persistCache) {
            return;
        }

        size_t maxBytes = config_.maxCacheSizeMB * 1024 * 1024;
        size_t currentBytes = 0;

        // Collect all files with their modification times
        std::vector<std::pair<std::filesystem::path, std::filesystem::file_time_type>> files;

        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(config_.cachePath)) {
                if (entry.is_regular_file()) {
                    currentBytes += entry.file_size();
                    files.push_back({entry.path(), entry.last_write_time()});
                }
            }
        } catch (const std::exception&) {
            return;
        }

        // If under limit, nothing to do
        if (currentBytes <= maxBytes) {
            return;
        }

        // Sort by modification time (oldest first)
        std::sort(files.begin(), files.end(),
                  [](const auto& a, const auto& b) {
                      return a.second < b.second;
                  });

        // Evict oldest files until under 90% of limit
        size_t targetBytes = static_cast<size_t>(maxBytes * 0.9);
        for (const auto& [path, _] : files) {
            if (currentBytes <= targetBytes) {
                break;
            }

            try {
                size_t fileSize = std::filesystem::file_size(path);
                std::filesystem::remove(path);
                currentBytes -= fileSize;
            } catch (const std::exception&) {
                // File may have been deleted already
            }
        }
    }

private:
    // =========================================================================
    // Internal Types
    // =========================================================================

    struct CacheEntry {
        std::vector<unsigned char> data;
        std::list<std::string>::iterator lruIt;  // Position in LRU list
    };

    // =========================================================================
    // Internal Methods
    // =========================================================================

    std::string makeKey(const TileCoord& coord, const std::string& layerId,
                        const std::string& cachePrefix) const {
        // Format: cachePrefix/layerId/z/x/y
        std::string key = cachePrefix.empty() ? layerId : (cachePrefix + "/" + layerId);
        return key + "/" + coord.key();
    }

    std::string tilePath(const TileCoord& coord, const std::string& layerId,
                         const std::string& cachePrefix) const {
        // Create directory structure: cachePath/cachePrefix/layerId/z/x/y.tile
        std::string path = config_.cachePath + "/";
        if (!cachePrefix.empty()) {
            path += cachePrefix + "/";
        }
        path += layerId + "/" +
                std::to_string(coord.z) + "/" +
                std::to_string(coord.x) + "/" +
                std::to_string(coord.y) + ".tile";
        return path;
    }

    void putInMemory(const std::string& key, const std::vector<unsigned char>& data) {
        // Check if already in cache
        auto it = memoryCache_.find(key);
        if (it != memoryCache_.end()) {
            // Update existing entry and move to front
            it->second.data = data;
            lruList_.splice(lruList_.begin(), lruList_, it->second.lruIt);
            return;
        }

        // Evict if at capacity
        while (memoryCache_.size() >= config_.maxMemoryCacheTiles && !lruList_.empty()) {
            // Remove oldest (back of LRU list)
            std::string oldestKey = lruList_.back();
            lruList_.pop_back();
            memoryCache_.erase(oldestKey);
        }

        // Add new entry at front
        lruList_.push_front(key);
        CacheEntry entry;
        entry.data = data;
        entry.lruIt = lruList_.begin();
        memoryCache_[key] = std::move(entry);
    }

    std::optional<std::vector<unsigned char>> loadFromDisk(const std::string& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) {
            return std::nullopt;
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<unsigned char> buffer(size);
        if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
            return std::nullopt;
        }

        // Update file modification time (for LRU)
        try {
            std::filesystem::last_write_time(path, std::filesystem::file_time_type::clock::now());
        } catch (const std::exception&) {
            // Ignore errors updating timestamp
        }

        return buffer;
    }

    void saveToDisk(const std::string& path, const std::vector<unsigned char>& data) {
        // Create parent directories
        std::filesystem::path filePath(path);
        std::filesystem::create_directories(filePath.parent_path());

        std::ofstream file(path, std::ios::binary);
        if (file) {
            file.write(reinterpret_cast<const char*>(data.data()), data.size());
        }
    }

    // =========================================================================
    // State
    // =========================================================================

    CacheConfig config_;
    mutable std::mutex mutex_;

    // Memory cache with LRU eviction
    std::unordered_map<std::string, CacheEntry> memoryCache_;
    std::list<std::string> lruList_;  // Front = most recent, back = oldest
};

} // namespace earth
} // namespace geodraw
