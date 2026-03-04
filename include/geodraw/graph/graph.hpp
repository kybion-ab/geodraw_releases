/*******************************************************************************
 * File: graph.hpp
 *
 * Description: Generic directed graph class with adjacency list storage.
 * Supports node and edge CRUD operations, adjacency queries, and iteration.
 *
 *
 ******************************************************************************/

#pragma once

#include "geodraw/graph/graph_types.hpp"
#include <functional>
#include <unordered_map>
#include <vector>

namespace geodraw {
namespace graph {

/**
 * Generic directed graph with adjacency list storage.
 *
 * Nodes and edges are stored by ID and can have arbitrary metadata attached.
 * The graph is move-only to prevent expensive accidental copies.
 */
class GEODRAW_API Graph {
public:
    Graph() = default;
    ~Graph() = default;

    // Move-only
    Graph(Graph&&) = default;
    Graph& operator=(Graph&&) = default;
    Graph(const Graph&) = delete;
    Graph& operator=(const Graph&) = delete;

    // =========================================================================
    // Node Operations
    // =========================================================================

    /**
     * Add a node to the graph.
     * @param id Optional ID for the node. If InvalidNodeId, auto-generates an ID.
     * @return The ID of the added node.
     */
    NodeId addNode(NodeId id = InvalidNodeId);

    /**
     * Remove a node and all its incident edges.
     * @return true if the node was found and removed.
     */
    bool removeNode(NodeId id);

    /**
     * Check if a node exists in the graph.
     */
    bool hasNode(NodeId id) const;

    /**
     * Get a mutable pointer to a node.
     * @return Pointer to the node, or nullptr if not found.
     */
    Node* getNode(NodeId id);

    /**
     * Get a const pointer to a node.
     * @return Pointer to the node, or nullptr if not found.
     */
    const Node* getNode(NodeId id) const;

    /**
     * Get all node IDs in the graph.
     */
    std::vector<NodeId> nodeIds() const;

    /**
     * Get the number of nodes in the graph.
     */
    size_t nodeCount() const;

    // =========================================================================
    // Edge Operations
    // =========================================================================

    /**
     * Add an edge between two nodes.
     * Both nodes must already exist in the graph.
     * @param source Source node ID.
     * @param target Target node ID.
     * @param id Optional ID for the edge. If InvalidEdgeId, auto-generates an ID.
     * @return The ID of the added edge, or InvalidEdgeId if source/target don't exist.
     */
    EdgeId addEdge(NodeId source, NodeId target, EdgeId id = InvalidEdgeId);

    /**
     * Remove an edge by ID.
     * @return true if the edge was found and removed.
     */
    bool removeEdge(EdgeId id);

    /**
     * Remove an edge by source and target nodes.
     * @return true if the edge was found and removed.
     */
    bool removeEdge(NodeId source, NodeId target);

    /**
     * Check if an edge exists by ID.
     */
    bool hasEdge(EdgeId id) const;

    /**
     * Check if an edge exists between two nodes.
     */
    bool hasEdge(NodeId source, NodeId target) const;

    /**
     * Get a mutable pointer to an edge by ID.
     * @return Pointer to the edge, or nullptr if not found.
     */
    Edge* getEdge(EdgeId id);

    /**
     * Get a const pointer to an edge by ID.
     * @return Pointer to the edge, or nullptr if not found.
     */
    const Edge* getEdge(EdgeId id) const;

    /**
     * Find an edge by source and target nodes.
     * @return Pointer to the edge, or nullptr if not found.
     */
    const Edge* findEdge(NodeId source, NodeId target) const;

    /**
     * Get all edge IDs in the graph.
     */
    std::vector<EdgeId> edgeIds() const;

    /**
     * Get the number of edges in the graph.
     */
    size_t edgeCount() const;

    // =========================================================================
    // Adjacency Queries
    // =========================================================================

    /**
     * Get IDs of all outgoing edges from a node.
     */
    std::vector<EdgeId> outEdges(NodeId node) const;

    /**
     * Get IDs of all incoming edges to a node.
     */
    std::vector<EdgeId> inEdges(NodeId node) const;

    /**
     * Get IDs of all successor nodes (nodes reachable via outgoing edges).
     */
    std::vector<NodeId> successors(NodeId node) const;

    /**
     * Get IDs of all predecessor nodes (nodes with edges to this node).
     */
    std::vector<NodeId> predecessors(NodeId node) const;

    /**
     * Get IDs of all neighbor nodes (union of successors and predecessors).
     */
    std::vector<NodeId> neighbors(NodeId node) const;

    /**
     * Get the out-degree (number of outgoing edges) of a node.
     */
    size_t outDegree(NodeId node) const;

    /**
     * Get the in-degree (number of incoming edges) of a node.
     */
    size_t inDegree(NodeId node) const;

    /**
     * Get the total degree (in-degree + out-degree) of a node.
     */
    size_t degree(NodeId node) const;

    // =========================================================================
    // Iteration
    // =========================================================================

    /**
     * Iterate over all nodes (const version).
     */
    void forEachNode(const std::function<void(const Node&)>& fn) const;

    /**
     * Iterate over all nodes (mutable version).
     */
    void forEachNode(const std::function<void(Node&)>& fn);

    /**
     * Iterate over all edges (const version).
     */
    void forEachEdge(const std::function<void(const Edge&)>& fn) const;

    /**
     * Iterate over all edges (mutable version).
     */
    void forEachEdge(const std::function<void(Edge&)>& fn);

    // =========================================================================
    // Utility
    // =========================================================================

    /**
     * Remove all nodes and edges from the graph.
     */
    void clear();

    /**
     * Check if the graph is empty (no nodes).
     */
    bool empty() const;

    /**
     * Reserve capacity for nodes and edges.
     * This is a hint for performance optimization.
     */
    void reserve(size_t nodeCapacity, size_t edgeCapacity);

private:
    NodeId nextNodeId_ = 1;
    EdgeId nextEdgeId_ = 1;
    std::unordered_map<NodeId, Node> nodes_;
    std::unordered_map<EdgeId, Edge> edges_;
    std::unordered_map<NodeId, std::vector<EdgeId>> outAdjacency_;
    std::unordered_map<NodeId, std::vector<EdgeId>> inAdjacency_;
    std::unordered_map<NodeId, std::unordered_map<NodeId, EdgeId>> edgeLookup_;

    void removeEdgeFromAdjacency(EdgeId edgeId, NodeId source, NodeId target);
};

} // namespace graph
} // namespace geodraw
