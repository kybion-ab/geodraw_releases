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

} // namespace http
} // namespace earth
} // namespace geodraw
