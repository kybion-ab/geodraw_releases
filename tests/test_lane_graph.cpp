/*******************************************************************************
 * File: test_lane_graph.cpp
 *
 * Description: Unit tests for lane-to-graph conversion.
 * Tests graph construction, metadata, and complexity scoring.
 *
 *
 ******************************************************************************/

#include "geodraw/graph/lane_graph.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>

using namespace geodraw::graph;
using Catch::Approx;
using json = nlohmann::json;

static bool approxEqual(double a, double b, double epsilon = 1e-6) {
    return std::abs(a - b) < epsilon;
}

TEST_CASE("Lane graph - empty input") {
    std::vector<LaneData> lanes;
    auto result = buildLaneGraph(lanes);

    CHECK(result.graph.empty());
    CHECK(result.graph.nodeCount() == 0);
    CHECK(result.graph.edgeCount() == 0);
    CHECK(result.edgeToLaneId.empty());
}

TEST_CASE("Lane graph - single lane") {
    LaneData lane;
    lane.id = 42;
    lane.mapElementId = 1;
    lane.geometry = {
        {0.0, 0.0, 0.0},
        {10.0, 0.0, 0.0},
        {20.0, 0.0, 0.0}
    };

    auto result = buildLaneGraph({lane});

    // Should have 2 nodes (start and end) and 1 edge
    CHECK(result.graph.nodeCount() == 2);
    CHECK(result.graph.edgeCount() == 1);
    REQUIRE(result.edgeToLaneId.size() == 1);

    auto edgeIds = result.graph.edgeIds();
    REQUIRE(edgeIds.size() == 1);

    const Edge* edge = result.graph.getEdge(edgeIds[0]);
    REQUIRE(edge != nullptr);

    auto laneId = edge->metadata.get<int64_t>("lane_id");
    REQUIRE(laneId.has_value());
    CHECK(laneId.value() == 42);

    auto length = edge->metadata.get<double>("length");
    REQUIRE(length.has_value());
    CHECK(approxEqual(length.value(), 20.0));

    auto avgCurvature = edge->metadata.get<double>("avg_curvature");
    REQUIRE(avgCurvature.has_value());
    CHECK(approxEqual(avgCurvature.value(), 0.0)); // Straight line

    auto isTurn = edge->metadata.get<bool>("is_turn");
    REQUIRE(isTurn.has_value());
    CHECK(isTurn.value() == false);
}

TEST_CASE("Lane graph - connected lanes") {
    LaneData lane1;
    lane1.id = 1;
    lane1.geometry = {{0.0, 0.0, 0.0}, {10.0, 0.0, 0.0}};

    LaneData lane2;
    lane2.id = 2;
    lane2.geometry = {{10.0, 0.0, 0.0}, {20.0, 0.0, 0.0}}; // Same as lane1 end

    auto result = buildLaneGraph({lane1, lane2});

    // Should have 3 nodes (start1, shared junction, end2) and 2 edges
    CHECK(result.graph.nodeCount() == 3);
    CHECK(result.graph.edgeCount() == 2);
}

TEST_CASE("Lane graph - merge lane (two lanes converging)") {
    LaneData lane1;
    lane1.id = 1;
    lane1.geometry = {{0.0, 5.0, 0.0}, {10.0, 0.0, 0.0}};

    LaneData lane2;
    lane2.id = 2;
    lane2.geometry = {{0.0, -5.0, 0.0}, {10.0, 0.0, 0.0}}; // Same endpoint as lane1

    LaneGraphOptions options;
    options.connectionThreshold = 2.0;
    auto result = buildLaneGraph({lane1, lane2}, options);

    CHECK(result.graph.nodeCount() == 3);
    CHECK(result.graph.edgeCount() == 2);

    int mergeCount = 0;
    result.graph.forEachNode([&](const Node& node) {
        auto isMerge = node.metadata.get<bool>("is_merge");
        if (isMerge.value_or(false)) {
            ++mergeCount;
            CHECK(result.graph.inDegree(node.id) == 2);
        }
    });
    CHECK(mergeCount == 1);
}

TEST_CASE("Lane graph - split lane (one lane diverging)") {
    LaneData lane1;
    lane1.id = 1;
    lane1.geometry = {{0.0, 0.0, 0.0}, {10.0, 0.0, 0.0}};

    LaneData lane2;
    lane2.id = 2;
    lane2.geometry = {{10.0, 0.0, 0.0}, {20.0, 5.0, 0.0}};

    LaneData lane3;
    lane3.id = 3;
    lane3.geometry = {{10.0, 0.0, 0.0}, {20.0, -5.0, 0.0}};

    auto result = buildLaneGraph({lane1, lane2, lane3});

    CHECK(result.graph.nodeCount() == 4);
    CHECK(result.graph.edgeCount() == 3);

    int splitCount = 0;
    result.graph.forEachNode([&](const Node& node) {
        auto isSplit = node.metadata.get<bool>("is_split");
        if (isSplit.value_or(false)) {
            ++splitCount;
            CHECK(result.graph.outDegree(node.id) == 2);
        }
    });
    CHECK(splitCount == 1);
}

TEST_CASE("Lane graph - curved lane") {
    LaneData lane;
    lane.id = 1;
    lane.geometry = {
        {0.0, 0.0, 0.0},
        {5.0, 0.0, 0.0},
        {10.0, 0.0, 0.0},
        {10.0, 5.0, 0.0},
        {10.0, 10.0, 0.0}
    };

    auto result = buildLaneGraph({lane});
    auto edgeIds = result.graph.edgeIds();
    REQUIRE(edgeIds.size() == 1);
    const Edge* edge = result.graph.getEdge(edgeIds[0]);
    REQUIRE(edge != nullptr);

    auto maxCurvature = edge->metadata.get<double>("max_curvature");
    REQUIRE(maxCurvature.has_value());
    CHECK(maxCurvature.value() > 0.0);

    auto totalHeadingChange = edge->metadata.get<double>("total_heading_change");
    REQUIRE(totalHeadingChange.has_value());
    CHECK(totalHeadingChange.value() > M_PI / 4.0); // > 45 deg

    auto isTurn = edge->metadata.get<bool>("is_turn");
    REQUIRE(isTurn.has_value());
    CHECK(isTurn.value() == true);
}

TEST_CASE("Lane graph - connection threshold") {
    LaneData lane1;
    lane1.id = 1;
    lane1.geometry = {{0.0, 0.0, 0.0}, {10.0, 0.0, 0.0}};

    LaneData lane2;
    lane2.id = 2;
    lane2.geometry = {{10.0, 1.5, 0.0}, {20.0, 0.0, 0.0}}; // 1.5m away from lane1 end

    SECTION("With threshold 2.0, should connect") {
        LaneGraphOptions options;
        options.connectionThreshold = 2.0;
        auto result = buildLaneGraph({lane1, lane2}, options);
        CHECK(result.graph.nodeCount() == 3); // Connected
    }

    SECTION("With threshold 1.0, should not connect") {
        LaneGraphOptions options;
        options.connectionThreshold = 1.0;
        auto result = buildLaneGraph({lane1, lane2}, options);
        CHECK(result.graph.nodeCount() == 4); // Separate endpoints
    }
}

TEST_CASE("Lane graph - T-junction (mid-segment connection)") {
    // Lane A runs from (0,0) to (100,0) with interior points
    LaneData laneA;
    laneA.id = 1;
    laneA.geometry = {
        {0.0, 0.0, 0.0}, {25.0, 0.0, 0.0}, {50.0, 0.0, 0.0}, {75.0, 0.0, 0.0}, {100.0, 0.0, 0.0}
    };

    // Lane B starts near the MIDDLE of Lane A
    LaneData laneB;
    laneB.id = 2;
    laneB.geometry = {{50.0, 0.5, 0.0}, {50.0, 25.0, 0.0}, {50.0, 50.0, 0.0}};

    LaneGraphOptions options;
    options.connectionThreshold = 2.0;
    auto result = buildLaneGraph({laneA, laneB}, options);

    CHECK(result.graph.edgeCount() == 3);
    CHECK(result.graph.nodeCount() == 4);

    int splitCount = 0;
    result.graph.forEachNode([&](const Node& node) {
        auto isSplit = node.metadata.get<bool>("is_split");
        if (isSplit.value_or(false)) ++splitCount;
    });
    CHECK(splitCount == 1);

    // Lane A original ID should be preserved on both split edges
    int laneAEdges = 0, laneBEdges = 0;
    for (const auto& [edgeId, laneId] : result.edgeToLaneId) {
        if (laneId == 1) ++laneAEdges;
        if (laneId == 2) ++laneBEdges;
    }
    CHECK(laneAEdges == 2);
    CHECK(laneBEdges == 1);
}

TEST_CASE("Lane graph - T-junction merge (lane ends in middle of another)") {
    LaneData laneA;
    laneA.id = 1;
    laneA.geometry = {
        {0.0, 0.0, 0.0}, {25.0, 0.0, 0.0}, {50.0, 0.0, 0.0}, {75.0, 0.0, 0.0}, {100.0, 0.0, 0.0}
    };

    LaneData laneB;
    laneB.id = 2;
    laneB.geometry = {{50.0, -50.0, 0.0}, {50.0, -25.0, 0.0}, {50.0, 0.5, 0.0}};

    LaneGraphOptions options;
    options.connectionThreshold = 2.0;
    auto result = buildLaneGraph({laneA, laneB}, options);

    CHECK(result.graph.edgeCount() == 3);

    int mergeCount = 0;
    result.graph.forEachNode([&](const Node& node) {
        auto isMerge = node.metadata.get<bool>("is_merge");
        if (isMerge.value_or(false)) ++mergeCount;
    });
    CHECK(mergeCount == 1);
}

TEST_CASE("Lane graph complexity - empty graph") {
    Graph emptyGraph;
    auto score = computeComplexity(emptyGraph);

    CHECK(approxEqual(score.score, 0.0));
    CHECK(score.mergeCount == 0);
    CHECK(score.splitCount == 0);
    CHECK(score.turnCount == 0);
    CHECK(approxEqual(score.avgCurvature, 0.0));
    CHECK(approxEqual(score.networkDensity, 0.0));
}

TEST_CASE("Lane graph complexity - straight road has low complexity") {
    LaneData lane;
    lane.id = 1;
    lane.geometry = {{0.0, 0.0, 0.0}, {100.0, 0.0, 0.0}};

    auto result = buildLaneGraph({lane});
    auto score = computeComplexity(result.graph);

    CHECK(score.mergeCount == 0);
    CHECK(score.splitCount == 0);
    CHECK(score.turnCount == 0);
    CHECK(approxEqual(score.avgCurvature, 0.0));
    CHECK(score.score < 0.2);
}

TEST_CASE("Lane graph complexity - intersection has higher complexity") {
    std::vector<LaneData> lanes;

    // 4 incoming + 4 outgoing lanes at center
    for (int i = 0; i < 4; ++i) {
        double angle = i * M_PI / 2.0;
        LaneData incoming, outgoing;
        incoming.id = i + 1;
        incoming.geometry = {{20.0 * std::cos(angle), 20.0 * std::sin(angle), 0.0}, {0.0, 0.0, 0.0}};
        outgoing.id = i + 5;
        outgoing.geometry = {{0.0, 0.0, 0.0}, {20.0 * std::cos(angle), 20.0 * std::sin(angle), 0.0}};
        lanes.push_back(incoming);
        lanes.push_back(outgoing);
    }

    auto result = buildLaneGraph(lanes);
    auto score = computeComplexity(result.graph);

    CHECK((score.mergeCount > 0 || score.splitCount > 0));
    CHECK(score.networkDensity > 1.0);
    CHECK(score.score > 0.1);
}

TEST_CASE("Lane graph complexity score is in [0, 1]") {
    // Simple lane
    {
        LaneData lane;
        lane.id = 1;
        lane.geometry = {{0, 0, 0}, {10, 0, 0}};
        auto result = buildLaneGraph({lane});
        auto score = computeComplexity(result.graph);
        CHECK(score.score >= 0.0);
        CHECK(score.score <= 1.0);
    }

    // Multiple connected lanes
    {
        std::vector<LaneData> lanes;
        for (int i = 0; i < 5; ++i) {
            LaneData lane;
            lane.id = i;
            lane.geometry = {{i * 10.0, 0, 0}, {(i + 1) * 10.0, 0, 0}};
            lanes.push_back(lane);
        }
        auto result = buildLaneGraph(lanes);
        auto score = computeComplexity(result.graph);
        CHECK(score.score >= 0.0);
        CHECK(score.score <= 1.0);
    }
}

TEST_CASE("Lane graph - JSON loading") {
    static auto loadLanesFromJson = [](const std::string& filepath) {
        std::vector<LaneData> lanes;
        std::ifstream file(filepath);
        if (!file.is_open()) return lanes;
        json j;
        file >> j;
        if (!j.contains("roads")) return lanes;
        for (const auto& road : j["roads"]) {
            if (road.contains("type") && road["type"] == "lane" && road.contains("geometry")) {
                LaneData lane;
                lane.id = road.value("id", 0);
                lane.mapElementId = road.value("map_element_id", 0);
                for (const auto& point : road["geometry"]) {
                    LanePoint p;
                    p.x = point.value("x", 0.0);
                    p.y = point.value("y", 0.0);
                    p.z = point.value("z", 0.0);
                    lane.geometry.push_back(p);
                }
                if (lane.geometry.size() >= 2) lanes.push_back(lane);
            }
        }
        return lanes;
    };

    auto lanes = loadLanesFromJson("data/examples/tfrecord-00000-of-01000_4.json");

    if (lanes.empty()) {
        // No test data available - skip
        return;
    }

    CHECK(lanes.size() > 0);
    for (const auto& lane : lanes) {
        CHECK(lane.geometry.size() >= 2);
    }

    auto result = buildLaneGraph(lanes);
    CHECK(result.graph.nodeCount() > 0);
    CHECK(result.graph.edgeCount() > 0);

    auto score = computeComplexity(result.graph);
    CHECK(score.score >= 0.0);
    CHECK(score.score <= 1.0);
}
