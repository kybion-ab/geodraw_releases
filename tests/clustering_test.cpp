/*******************************************************************************
 * File: clustering_test.cpp
 *
 * Description: Unit tests for the MarkerClusterer class.
 * Tests screen-space intersection clustering behavior.
 *
 ******************************************************************************/

#include "geodraw/modules/clustering/marker_clusterer.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>

using namespace geodraw::clustering;
using Catch::Approx;

static bool approxEqual(float a, float b, float tolerance = 1e-4f) {
    return std::abs(a - b) < tolerance;
}

static bool approxEqual(const glm::vec2& a, const glm::vec2& b, float tolerance = 1e-4f) {
    return approxEqual(a.x, b.x, tolerance) && approxEqual(a.y, b.y, tolerance);
}

TEST_CASE("Clusterer - empty input") {
    MarkerClusterer clusterer;
    clusterer.update({}, {}, 100000.0);
    CHECK(clusterer.getClusters().empty());
}

TEST_CASE("Clusterer - single pin, no clustering") {
    MarkerClusterer clusterer;
    std::vector<std::optional<glm::vec2>> screenPos = {glm::vec2(100, 100)};
    std::vector<glm::dvec3> ecefPos = {glm::dvec3(1, 0, 0)};

    clusterer.update(screenPos, ecefPos, 100000.0);

    REQUIRE(clusterer.getClusters().size() == 1);
    CHECK(clusterer.getClusters()[0].isSingle());
    CHECK(clusterer.getClusters()[0].pinIndices[0] == 0);
}

TEST_CASE("Clusterer - two nearby pins cluster together") {
    ClusterConfig config;
    config.pinScreenRadius = 12.0f; // Pins cluster when centers < 60px apart (5*radius)
    MarkerClusterer clusterer(config);

    std::vector<std::optional<glm::vec2>> screenPos = {
        glm::vec2(100, 100),
        glm::vec2(150, 100) // 50 pixels apart, within 60px merge distance
    };
    std::vector<glm::dvec3> ecefPos = {glm::dvec3(1, 0, 0), glm::dvec3(1, 0, 0)};

    clusterer.update(screenPos, ecefPos, 100000.0);

    REQUIRE(clusterer.getClusters().size() == 1);
    CHECK(clusterer.getClusters()[0].count() == 2);
}

TEST_CASE("Clusterer - two distant pins don't cluster") {
    ClusterConfig config;
    config.pinScreenRadius = 12.0f;
    MarkerClusterer clusterer(config);

    std::vector<std::optional<glm::vec2>> screenPos = {
        glm::vec2(100, 100),
        glm::vec2(170, 100) // 70 pixels apart, > 60px merge distance
    };
    std::vector<glm::dvec3> ecefPos = {glm::dvec3(1, 0, 0), glm::dvec3(0, 1, 0)};

    clusterer.update(screenPos, ecefPos, 100000.0);

    REQUIRE(clusterer.getClusters().size() == 2);
    CHECK(clusterer.getClusters()[0].isSingle());
    CHECK(clusterer.getClusters()[1].isSingle());
}

TEST_CASE("Clusterer - always active regardless of altitude") {
    ClusterConfig config;
    config.pinScreenRadius = 12.0f;
    MarkerClusterer clusterer(config);

    std::vector<std::optional<glm::vec2>> screenPos = {
        glm::vec2(100, 100),
        glm::vec2(110, 100) // 10px apart, will cluster
    };
    std::vector<glm::dvec3> ecefPos = {glm::dvec3(1, 0, 0), glm::dvec3(1, 0, 0)};

    clusterer.update(screenPos, ecefPos, 100.0); // Very low altitude

    CHECK(clusterer.isClusteringActive());
    CHECK(clusterer.getClusters().size() == 1);
}

TEST_CASE("Clusterer - cluster center is average of pin positions") {
    ClusterConfig config;
    config.pinScreenRadius = 20.0f; // Large radius so 30px apart will cluster
    MarkerClusterer clusterer(config);

    std::vector<std::optional<glm::vec2>> screenPos = {
        glm::vec2(100, 100),
        glm::vec2(130, 100) // 30px apart, within 40px merge distance
    };
    std::vector<glm::dvec3> ecefPos = {glm::dvec3(1, 0, 0), glm::dvec3(0, 1, 0)};

    clusterer.update(screenPos, ecefPos, 100000.0);

    REQUIRE(clusterer.getClusters().size() == 1);
    glm::vec2 expectedCenter(115, 100);
    CHECK(approxEqual(clusterer.getClusters()[0].screenCenter, expectedCenter));
}

TEST_CASE("Clusterer - off-screen pins (nullopt) are excluded") {
    MarkerClusterer clusterer;

    std::vector<std::optional<glm::vec2>> screenPos = {
        glm::vec2(100, 100),
        std::nullopt,        // Off-screen
        glm::vec2(200, 200)
    };
    std::vector<glm::dvec3> ecefPos = {
        glm::dvec3(1, 0, 0),
        glm::dvec3(0, 1, 0),
        glm::dvec3(0, 0, 1)
    };

    clusterer.update(screenPos, ecefPos, 10000.0);
    // 100px apart with default 12px radius — two separate clusters
    CHECK(clusterer.getClusters().size() == 2);
}

TEST_CASE("Clusterer - all clusters have same screen radius") {
    ClusterConfig config;
    config.pinScreenRadius = 15.0f;
    MarkerClusterer clusterer(config);

    std::vector<std::optional<glm::vec2>> screenPos;
    std::vector<glm::dvec3> ecefPos;
    for (int i = 0; i < 8; ++i) {
        screenPos.push_back(glm::vec2(100 + i * 5, 100)); // 5px apart
        ecefPos.push_back(glm::dvec3(1, 0, 0));
    }

    clusterer.update(screenPos, ecefPos, 100000.0);

    REQUIRE(clusterer.getClusters().size() == 1);
    CHECK(approxEqual(clusterer.getClusters()[0].screenRadius, config.pinScreenRadius, 0.1f));
}

TEST_CASE("Clusterer - transitive clustering (chain)") {
    ClusterConfig config;
    config.pinScreenRadius = 12.0f; // Merge at 60px
    MarkerClusterer clusterer(config);

    // A-B-C chain: A-B and B-C are nearby, but A-C are far
    std::vector<std::optional<glm::vec2>> screenPos = {
        glm::vec2(100, 100), // A
        glm::vec2(150, 100), // B (50px from A)
        glm::vec2(200, 100)  // C (50px from B, 100px from A)
    };
    std::vector<glm::dvec3> ecefPos = {
        glm::dvec3(1, 0, 0), glm::dvec3(0, 1, 0), glm::dvec3(0, 0, 1)
    };

    clusterer.update(screenPos, ecefPos, 100000.0);

    REQUIRE(clusterer.getClusters().size() == 1);
    CHECK(clusterer.getClusters()[0].count() == 3);
}

TEST_CASE("Clusterer - cluster color by count") {
    glm::vec3 color1 = getClusterColor(1);   // Single - red
    glm::vec3 color3 = getClusterColor(3);   // 2-5 - green
    glm::vec3 color10 = getClusterColor(10); // 6-20 - yellow
    glm::vec3 color50 = getClusterColor(50); // 21+ - red-orange

    CHECK(color1.r > 0.9f);
    CHECK(color1.g < 0.1f);
    CHECK(color3.g > color3.r);
    CHECK(color3.g > 0.5f);
    CHECK(color10.r > 0.9f);
    CHECK(color10.g > 0.7f);
    CHECK(color50.r > 0.9f);
    CHECK(color50.g > 0.2f);
    CHECK(color50.g < 0.5f);
}

TEST_CASE("Clusterer - ECEF center is average of pin positions") {
    ClusterConfig config;
    config.pinScreenRadius = 50.0f; // Large radius to ensure clustering
    MarkerClusterer clusterer(config);

    std::vector<std::optional<glm::vec2>> screenPos = {
        glm::vec2(100, 100),
        glm::vec2(150, 150) // Within 100px merge distance
    };
    std::vector<glm::dvec3> ecefPos = {
        glm::dvec3(6378137.0, 0.0, 0.0),
        glm::dvec3(6378137.0, 100.0, 0.0)
    };

    clusterer.update(screenPos, ecefPos, 100000.0);

    REQUIRE(clusterer.getClusters().size() == 1);
    glm::dvec3 expectedCenter(6378137.0, 50.0, 0.0);
    glm::dvec3 actualCenter = clusterer.getClusters()[0].ecefCenter;
    CHECK(std::abs(actualCenter.x - expectedCenter.x) < 1.0);
    CHECK(std::abs(actualCenter.y - expectedCenter.y) < 1.0);
    CHECK(std::abs(actualCenter.z - expectedCenter.z) < 1.0);
}

TEST_CASE("Clusterer - update returns true when clustering changes") {
    MarkerClusterer clusterer;

    std::vector<std::optional<glm::vec2>> screenPos = {glm::vec2(100, 100)};
    std::vector<glm::dvec3> ecefPos = {glm::dvec3(1, 0, 0)};

    bool firstUpdate = clusterer.update(screenPos, ecefPos, 100000.0);
    bool sameUpdate = clusterer.update(screenPos, ecefPos, 100000.0);

    screenPos.push_back(glm::vec2(200, 200));
    ecefPos.push_back(glm::dvec3(0, 1, 0));
    bool changedUpdate = clusterer.update(screenPos, ecefPos, 100000.0);

    CHECK(firstUpdate);   // First update from empty should show change
    CHECK(!sameUpdate);   // Same state shouldn't show change
    CHECK(changedUpdate); // Added pin should show change
}
