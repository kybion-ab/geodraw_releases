/*******************************************************************************
 * File: earth_cache_test.cpp
 *
 * Description: Test for earth tile caching and HTTP fetching.
 *
 *
 ******************************************************************************/

#include "geodraw/modules/earth/earth_coords.hpp"
#include "geodraw/modules/earth/earth_cache.hpp"
#include "geodraw/modules/earth/earth_http.hpp"
#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <chrono>
#include <mutex>

using namespace geodraw::earth;

static const std::string SATELLITE = "satellite";

TEST_CASE("Earth cache - basic operations") {
    CacheConfig config;
    config.cachePath = "/tmp/geodraw_test_cache";
    config.maxMemoryCacheTiles = 10;
    config.persistCache = true;

    TileCache cache(config);
    cache.clear(); // Start fresh

    TileCoord coord(10, 163, 395);

    CHECK(!cache.contains(coord, SATELLITE, "")); // Initially empty

    std::vector<unsigned char> testData = {'T', 'E', 'S', 'T', 'D', 'A', 'T', 'A'};
    cache.put(coord, SATELLITE, "", testData);

    CHECK(cache.contains(coord, SATELLITE, ""));

    auto retrieved = cache.get(coord, SATELLITE, "");
    REQUIRE(retrieved.has_value());
    CHECK(retrieved->size() == testData.size());
    CHECK(*retrieved == testData);

    cache.clear();
}

TEST_CASE("Earth cache - LRU eviction") {
    CacheConfig config;
    config.cachePath = "/tmp/geodraw_test_cache_lru";
    config.maxMemoryCacheTiles = 3;
    config.persistCache = false; // Memory only

    TileCache cache(config);
    std::vector<unsigned char> data = {'D', 'A', 'T', 'A'};

    for (int i = 0; i < 4; ++i) {
        cache.put(TileCoord(10, i, 0), SATELLITE, "", data);
    }

    CHECK(!cache.contains(TileCoord(10, 0, 0), SATELLITE, "")); // First evicted (LRU)
    CHECK(cache.contains(TileCoord(10, 3, 0), SATELLITE, ""));  // Last present
    CHECK(cache.memoryTileCount() == 3);
}

TEST_CASE("Earth cache - disk persistence") {
    std::string cachePath = "/tmp/geodraw_test_cache_persist";
    TileCoord coord(12, 1000, 2000);
    std::vector<unsigned char> data = {'P', 'E', 'R', 'S', 'I', 'S', 'T'};

    // Write
    {
        CacheConfig config;
        config.cachePath = cachePath;
        config.persistCache = true;
        TileCache cache(config);
        cache.clear();
        cache.put(coord, SATELLITE, "", data);
    }

    // Read back (simulates app restart)
    {
        CacheConfig config;
        config.cachePath = cachePath;
        config.persistCache = true;
        TileCache cache(config);

        REQUIRE(cache.contains(coord, SATELLITE, ""));
        auto retrieved = cache.get(coord, SATELLITE, "");
        REQUIRE(retrieved.has_value());
        CHECK(*retrieved == data);

        cache.clear();
    }
}

TEST_CASE("Earth cache - HTTP fetch", "[network]") {
    std::string testUrl = "https://httpbin.org/bytes/100";
    auto result = http::fetchTileSync(testUrl, 5000);

    if (!result.success) {
        SUCCEED("Network unavailable: " + result.errorMessage);
        return;
    }

    CHECK(result.httpCode == 200);
    CHECK(result.data.size() == 100);
}

TEST_CASE("Earth cache - async fetch", "[network]") {
    auto future = http::fetchTileAsync("https://httpbin.org/bytes/50", 5000);

    auto status = future.wait_for(std::chrono::seconds(10));
    REQUIRE(status == std::future_status::ready);

    auto result = future.get();
    if (!result.success) {
        SUCCEED("Network unavailable: " + result.errorMessage);
        return;
    }

    CHECK(result.data.size() == 50);
}
