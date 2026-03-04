/*******************************************************************************
 * File: debug_goals.cpp
 *
 * Description: Diagnostic binary for verifying goal reachability via the
 * directed lane graph. Loads a scenario JSON, builds the lane graph, and for
 * each vehicle with a goal reports whether a directed Dijkstra path exists from
 * the vehicle's lane end-node to the goal's lane node.
 *
 * Usage: debug_goals --input <path-to-scenario.json>
 *
 ******************************************************************************/

#include "geodraw/modules/drive/trajectory_json.hpp"
#include "geodraw/modules/drive/trajectory_data.hpp"
#include "geodraw/modules/drive/goal_assignment.hpp"
#include "geodraw/graph/lane_graph.hpp"
#include "geodraw/graph/graph_algorithms.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>

using namespace geodraw;
using namespace geodraw::graph;

namespace {

double dist2D(const glm::dvec3& a, const glm::dvec3& b) {
    double dx = a.x - b.x, dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

// Closest point on a centerline to a query position; returns arc-length.
double closestArcLength(const glm::dvec3& q, const std::vector<glm::dvec3>& pts) {
    double best = std::numeric_limits<double>::max();
    double arcBest = 0.0, cum = 0.0;
    for (size_t i = 0; i + 1 < pts.size(); ++i) {
        glm::dvec3 seg = pts[i + 1] - pts[i];
        double slen2 = seg.x * seg.x + seg.y * seg.y;
        double t = (slen2 > 1e-18)
            ? std::clamp((q.x - pts[i].x) * seg.x + (q.y - pts[i].y) * seg.y, 0.0, slen2) / slen2
            : 0.0;
        glm::dvec3 proj = pts[i] + t * seg;
        double dx = q.x - proj.x, dy = q.y - proj.y;
        double d = std::sqrt(dx * dx + dy * dy);
        if (d < best) { best = d; arcBest = cum + t * std::sqrt(slen2); }
        cum += std::sqrt(slen2);
    }
    return arcBest;
}

// Find the source node of the sub-lane edge that contains the given arc-length
// position on the lane with the given ID.
NodeId findSourceNode(
    int64_t laneId,
    double arcLen,
    const LaneGraphResult& gr)
{
    NodeId best = InvalidNodeId;
    double bestStart = -1.0;
    for (auto eid : gr.graph.edgeIds()) {
        const auto* e = gr.graph.getEdge(eid);
        if (!e) continue;
        auto lid = e->metadata.get<int64_t>("lane_id");
        if (!lid || lid.value() != laneId) continue;
        double eStart = e->metadata.get<double>("start_arc_length").value_or(0.0);
        double eLen   = e->metadata.get<double>("length").value_or(0.0);
        if (arcLen >= eStart - 0.5 && arcLen <= eStart + eLen + 0.5) {
            if (eStart > bestStart) { bestStart = eStart; best = e->source; }
        }
    }
    return best;
}

// Find the target node of the sub-lane edge that contains the given arc-length
// position on the lane with the given ID (goal: we need to arrive at the target).
NodeId findTargetNode(
    int64_t laneId,
    double arcLen,
    const LaneGraphResult& gr)
{
    NodeId best = InvalidNodeId;
    double bestStart = -1.0;
    for (auto eid : gr.graph.edgeIds()) {
        const auto* e = gr.graph.getEdge(eid);
        if (!e) continue;
        auto lid = e->metadata.get<int64_t>("lane_id");
        if (!lid || lid.value() != laneId) continue;
        double eStart = e->metadata.get<double>("start_arc_length").value_or(0.0);
        double eLen   = e->metadata.get<double>("length").value_or(0.0);
        if (arcLen >= eStart - 0.5 && arcLen <= eStart + eLen + 0.5) {
            if (eStart > bestStart) { bestStart = eStart; best = e->target; }
        }
    }
    return best;
}

} // namespace

int main(int argc, char* argv[]) {
    std::string inputFile;
    for (int i = 1; i < argc; ++i) {
        if ((std::strcmp(argv[i], "--input") == 0 || std::strcmp(argv[i], "-i") == 0)
            && i + 1 < argc) {
            inputFile = argv[++i];
        }
    }

    if (inputFile.empty()) {
        std::cerr << "Usage: debug_goals --input <scenario.json>\n";
        return 1;
    }

    std::cout << "Loading: " << inputFile << "\n";
    SceneData scene;
    try {
        scene = loadFromJson(inputFile);
    } catch (const std::exception& ex) {
        std::cerr << "Failed to load: " << ex.what() << "\n";
        return 1;
    }
    std::cout << "Vehicles: " << scene.vehicles.size()
              << "  Lanes: " << scene.lanes.size() << "\n\n";

    // Reassign goals using the fixed assignGoals (clears any pre-existing goals)
    for (auto& v : scene.vehicles) v.goalPosition = std::nullopt;
    auto assignResult = assignGoals(scene.vehicles, scene.lanes);
    std::cout << "Goals assigned: " << assignResult.goalsAssigned
              << "  skipped: " << assignResult.goalsSkipped << "\n\n";

    // Build lane graph
    std::vector<LaneData> laneData;
    laneData.reserve(scene.lanes.size());
    for (const auto& lg : scene.lanes) {
        LaneData ld;
        ld.id = static_cast<int64_t>(lg.id);
        for (const auto& p : lg.centerline)
            ld.geometry.push_back({p.x, p.y, p.z});
        laneData.push_back(std::move(ld));
    }
    LaneGraphOptions opts;
    opts.connectionThreshold = 2.0;
    auto gr = buildLaneGraph(laneData, opts);
    std::cout << "Graph: " << gr.graph.nodeCount() << " nodes, "
              << gr.graph.edgeCount() << " edges\n\n";

    int total = 0, reachable = 0, unreachable = 0, noGoal = 0;

    for (const auto& vt : scene.vehicles) {
        if (!vt.goalPosition || vt.positions.empty()) { ++noGoal; continue; }
        ++total;

        const glm::dvec3& vpos = vt.positions.front();
        const glm::dvec3& gpos = *vt.goalPosition;

        // Find which lane the vehicle and goal are on
        int vLaneIdx = -1; double vArc = 0.0, vDist = std::numeric_limits<double>::max();
        int gLaneIdx = -1; double gArc = 0.0, gDist = std::numeric_limits<double>::max();
        for (size_t i = 0; i < scene.lanes.size(); ++i) {
            const auto& cl = scene.lanes[i].centerline;
            if (cl.size() < 2) continue;
            double arc = closestArcLength(vpos, cl);
            glm::dvec3 tmp; // not used; just need distance
            // recompute distance
            double d = dist2D(vpos, cl.front()); // approximate - use centroid
            // proper: distance at the found arc point (skip - arc is enough for match)
            // Use a quick scan
            double bestD = std::numeric_limits<double>::max();
            for (size_t j = 0; j + 1 < cl.size(); ++j) {
                glm::dvec3 seg = cl[j+1] - cl[j];
                double slen2 = seg.x*seg.x + seg.y*seg.y;
                double t = (slen2>1e-18)
                    ? std::clamp((vpos.x-cl[j].x)*seg.x+(vpos.y-cl[j].y)*seg.y, 0.0, slen2)/slen2
                    : 0.0;
                glm::dvec3 p = cl[j] + t*seg;
                double dd = dist2D(vpos, p);
                if (dd < bestD) bestD = dd;
            }
            if (bestD < vDist) { vDist = bestD; vLaneIdx = (int)i; vArc = arc; }

            bestD = std::numeric_limits<double>::max();
            arc = closestArcLength(gpos, cl);
            for (size_t j = 0; j + 1 < cl.size(); ++j) {
                glm::dvec3 seg = cl[j+1] - cl[j];
                double slen2 = seg.x*seg.x + seg.y*seg.y;
                double t = (slen2>1e-18)
                    ? std::clamp((gpos.x-cl[j].x)*seg.x+(gpos.y-cl[j].y)*seg.y, 0.0, slen2)/slen2
                    : 0.0;
                glm::dvec3 p = cl[j] + t*seg;
                double dd = dist2D(gpos, p);
                if (dd < bestD) bestD = dd;
            }
            if (bestD < gDist) { gDist = bestD; gLaneIdx = (int)i; gArc = arc; }
        }

        if (vLaneIdx < 0 || gLaneIdx < 0) {
            std::cout << "  Vehicle " << vt.id << ": SKIP (no lane match)\n";
            ++unreachable;
            continue;
        }

        int64_t vLaneId = static_cast<int64_t>(scene.lanes[vLaneIdx].id);
        int64_t gLaneId = static_cast<int64_t>(scene.lanes[gLaneIdx].id);

        NodeId startNode = findSourceNode(vLaneId, vArc, gr);
        NodeId endNode   = findTargetNode(gLaneId, gArc, gr);

        if (startNode == InvalidNodeId || endNode == InvalidNodeId) {
            std::cout << "  Vehicle " << vt.id << ": UNREACHABLE (node not found:"
                      << " start=" << (startNode == InvalidNodeId ? "?" : "ok")
                      << " end=" << (endNode == InvalidNodeId ? "?" : "ok") << ")\n";
            ++unreachable;
            continue;
        }

        if (startNode == endNode) {
            ++reachable;
            continue;
        }

        auto path = dijkstraPath(gr.graph, startNode, endNode,
                                 weights::fromMetadata("length", 1.0));
        if (path.found) {
            ++reachable;
        } else {
            ++unreachable;
            std::cout << "  Vehicle " << vt.id << ": UNREACHABLE"
                      << "  vLane=" << vLaneId << " gLane=" << gLaneId
                      << "  startNode=" << startNode << " endNode=" << endNode << "\n";
        }
    }

    std::cout << "\n=== Summary ===\n";
    std::cout << "Total vehicles with goals: " << total << "\n";
    std::cout << "Reachable:   " << reachable << "\n";
    std::cout << "Unreachable: " << unreachable << "\n";
    std::cout << "No goal:     " << noGoal << "\n";

    return (unreachable == 0) ? 0 : 1;
}
