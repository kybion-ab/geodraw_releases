/*******************************************************************************
 * File: graph_algorithms.hpp
 *
 * Description: Graph algorithms for traversal, shortest paths, connectivity,
 * and flow analysis.
 *
 *
 ******************************************************************************/

#pragma once

#include "geodraw/graph/graph.hpp"
#include <functional>
#include <unordered_map>
#include <vector>

namespace geodraw {
namespace graph {

// =============================================================================
// Function Types
// =============================================================================

/**
 * Function type for computing edge weights.
 * Takes an edge and returns a non-negative weight.
 */
using WeightFn = std::function<double(const Edge&)>;

/**
 * Function type for A* heuristic.
 * Estimates the cost from a node to the goal.
 */
using HeuristicFn = std::function<double(NodeId from, NodeId to)>;

/**
 * Function type for computing edge capacities in flow networks.
 */
using CapacityFn = std::function<double(const Edge&)>;

/**
 * Visitor function for BFS/DFS traversal.
 * Called for each visited node. Return false to stop traversal.
 */
using VisitorFn = std::function<bool(NodeId node)>;

// =============================================================================
// Weight Helpers
// =============================================================================

namespace weights {

/**
 * Unit weight function (all edges have weight 1).
 */
inline double unit(const Edge&) { return 1.0; }

/**
 * Create a weight function that reads weight from edge metadata.
 * @param key Metadata key to read.
 * @param defaultVal Value to use if key is not found.
 */
GEODRAW_API WeightFn fromMetadata(const std::string& key, double defaultVal = 1.0);

} // namespace weights

// =============================================================================
// Result Types
// =============================================================================

/**
 * Result of a path-finding algorithm.
 */
struct GEODRAW_API PathResult {
    bool found = false;
    std::vector<NodeId> path;
    std::vector<EdgeId> edges;
    double totalCost = 0.0;
};

/**
 * Result of Dijkstra's single-source shortest paths algorithm.
 */
struct GEODRAW_API DijkstraResult {
    std::unordered_map<NodeId, double> distances;
    std::unordered_map<NodeId, NodeId> predecessors;
    std::unordered_map<NodeId, EdgeId> predecessorEdges;

    /**
     * Reconstruct the path to a specific target node.
     */
    PathResult pathTo(NodeId target) const;
};

/**
 * Result of a max-flow algorithm.
 */
struct GEODRAW_API MaxFlowResult {
    double maxFlow = 0.0;
    std::unordered_map<EdgeId, double> flow;
    std::vector<NodeId> minCutSourceSide;
};

/**
 * Result of connected components analysis.
 */
struct GEODRAW_API ComponentsResult {
    size_t count = 0;
    std::unordered_map<NodeId, size_t> componentId;
    std::vector<std::vector<NodeId>> components;
};

// =============================================================================
// Traversal Algorithms
// =============================================================================

/**
 * Breadth-first search traversal.
 * @param graph The graph to traverse.
 * @param source Starting node.
 * @param visitor Function called for each visited node. Return false to stop.
 */
GEODRAW_API void bfs(const Graph& graph, NodeId source, VisitorFn visitor);

/**
 * Depth-first search traversal.
 * @param graph The graph to traverse.
 * @param source Starting node.
 * @param visitor Function called for each visited node. Return false to stop.
 */
GEODRAW_API void dfs(const Graph& graph, NodeId source, VisitorFn visitor);

// =============================================================================
// Shortest Path Algorithms
// =============================================================================

/**
 * Dijkstra's algorithm for single-source shortest paths.
 * @param graph The graph.
 * @param source Source node.
 * @param weight Edge weight function (must return non-negative values).
 * @return Distances and predecessor information for all reachable nodes.
 */
GEODRAW_API DijkstraResult dijkstra(const Graph& graph, NodeId source,
                                     WeightFn weight = weights::unit);

/**
 * Dijkstra's algorithm for point-to-point shortest path.
 * Terminates early when target is reached.
 * @param graph The graph.
 * @param source Source node.
 * @param target Target node.
 * @param weight Edge weight function (must return non-negative values).
 * @return The shortest path from source to target.
 */
GEODRAW_API PathResult dijkstraPath(const Graph& graph, NodeId source, NodeId target,
                                     WeightFn weight = weights::unit);

/**
 * A* algorithm for point-to-point shortest path with heuristic.
 * @param graph The graph.
 * @param source Source node.
 * @param target Target node.
 * @param weight Edge weight function (must return non-negative values).
 * @param heuristic Heuristic function estimating cost to target. Must be admissible.
 * @return The shortest path from source to target.
 */
GEODRAW_API PathResult astar(const Graph& graph, NodeId source, NodeId target,
                              WeightFn weight, HeuristicFn heuristic);

// =============================================================================
// Connectivity Algorithms
// =============================================================================

/**
 * Find weakly connected components (treating graph as undirected).
 * @param graph The graph.
 * @return Component assignment for each node and list of components.
 */
GEODRAW_API ComponentsResult connectedComponents(const Graph& graph);

/**
 * Find strongly connected components using Tarjan's algorithm.
 * @param graph The graph.
 * @return Component assignment for each node and list of components.
 */
GEODRAW_API ComponentsResult stronglyConnectedComponents(const Graph& graph);

/**
 * Check if the graph is weakly connected (all nodes reachable ignoring direction).
 * @param graph The graph.
 * @return true if the graph is connected.
 */
GEODRAW_API bool isConnected(const Graph& graph);

// =============================================================================
// Flow Algorithms
// =============================================================================

/**
 * Edmonds-Karp algorithm for maximum flow.
 * @param graph The graph.
 * @param source Source node.
 * @param sink Sink node.
 * @param capacity Edge capacity function.
 * @return Maximum flow value and flow assignment per edge.
 */
GEODRAW_API MaxFlowResult maxFlow(const Graph& graph, NodeId source, NodeId sink,
                                   CapacityFn capacity);

} // namespace graph
} // namespace geodraw
