/*******************************************************************************
 * File: test_graph.cpp
 *
 * Description: Unit tests for the graph module.
 * Tests node/edge CRUD, adjacency queries, metadata, and algorithms.
 *
 *
 ******************************************************************************/

#include "geodraw/graph/graph.hpp"
#include "geodraw/graph/graph_algorithms.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <set>

using namespace geodraw::graph;
using Catch::Approx;

static bool approxEqual(double a, double b, double epsilon = 1e-9) {
    return std::abs(a - b) < epsilon;
}

TEST_CASE("Metadata operations") {
    Metadata m;
    CHECK(m.empty());
    CHECK(m.size() == 0);

    m.set("name", std::string("test"));
    m.set("count", int64_t(42));
    m.set("weight", 3.14);
    m.set("active", true);

    CHECK(!m.empty());
    CHECK(m.size() == 4);
    CHECK(m.has("name"));
    CHECK(m.has("count"));
    CHECK(!m.has("nonexistent"));

    auto name = m.get<std::string>("name");
    REQUIRE(name.has_value());
    CHECK(name.value() == "test");

    auto count = m.get<int64_t>("count");
    REQUIRE(count.has_value());
    CHECK(count.value() == 42);

    auto weight = m.get<double>("weight");
    REQUIRE(weight.has_value());
    CHECK(approxEqual(weight.value(), 3.14));

    auto active = m.get<bool>("active");
    REQUIRE(active.has_value());
    CHECK(active.value() == true);

    // Get with wrong type returns nullopt
    CHECK(!m.get<int64_t>("name").has_value());

    // Get nonexistent key returns nullopt
    CHECK(!m.get<std::string>("missing").has_value());

    m.remove("count");
    CHECK(!m.has("count"));
    CHECK(m.size() == 3);

    m.clear();
    CHECK(m.empty());
    CHECK(m.size() == 0);
}

TEST_CASE("Node operations") {
    Graph g;
    CHECK(g.empty());
    CHECK(g.nodeCount() == 0);

    NodeId n1 = g.addNode();
    NodeId n2 = g.addNode();
    NodeId n3 = g.addNode();

    CHECK(n1 != InvalidNodeId);
    CHECK(n2 != InvalidNodeId);
    CHECK(n3 != InvalidNodeId);
    CHECK(n1 != n2);
    CHECK(n2 != n3);
    CHECK(g.nodeCount() == 3);
    CHECK(!g.empty());

    NodeId n100 = g.addNode(100);
    CHECK(n100 == 100);
    CHECK(g.nodeCount() == 4);

    CHECK(g.hasNode(n1));
    CHECK(g.hasNode(n2));
    CHECK(g.hasNode(100));
    CHECK(!g.hasNode(999));

    Node* node = g.getNode(n1);
    REQUIRE(node != nullptr);
    CHECK(node->id == n1);

    const Graph& cg = g;
    CHECK(cg.getNode(n1) != nullptr);
    CHECK(g.getNode(999) == nullptr);

    node->metadata.set("label", std::string("Node 1"));
    CHECK(g.getNode(n1)->metadata.get<std::string>("label").value() == "Node 1");

    auto ids = g.nodeIds();
    CHECK(ids.size() == 4);
    std::set<NodeId> idSet(ids.begin(), ids.end());
    CHECK(idSet.count(n1) == 1);
    CHECK(idSet.count(n2) == 1);
    CHECK(idSet.count(n3) == 1);
    CHECK(idSet.count(100) == 1);

    CHECK(g.removeNode(n2));
    CHECK(!g.hasNode(n2));
    CHECK(g.nodeCount() == 3);
    CHECK(!g.removeNode(n2));  // Already removed
    CHECK(!g.removeNode(999)); // Never existed
}

TEST_CASE("Edge operations") {
    Graph g;
    NodeId a = g.addNode();
    NodeId b = g.addNode();
    NodeId c = g.addNode();

    EdgeId e1 = g.addEdge(a, b);
    EdgeId e2 = g.addEdge(b, c);
    EdgeId e3 = g.addEdge(a, c);

    CHECK(e1 != InvalidEdgeId);
    CHECK(e2 != InvalidEdgeId);
    CHECK(e3 != InvalidEdgeId);
    CHECK(g.edgeCount() == 3);

    EdgeId e100 = g.addEdge(c, a, 100);
    CHECK(e100 == 100);
    CHECK(g.edgeCount() == 4);

    CHECK(g.hasEdge(e1));
    CHECK(g.hasEdge(100));
    CHECK(!g.hasEdge(999));
    CHECK(g.hasEdge(a, b));
    CHECK(g.hasEdge(b, c));
    CHECK(!g.hasEdge(b, a)); // Directed: a->b exists, b->a doesn't

    Edge* edge = g.getEdge(e1);
    REQUIRE(edge != nullptr);
    CHECK(edge->id == e1);
    CHECK(edge->source == a);
    CHECK(edge->target == b);

    const Edge* found = g.findEdge(a, b);
    REQUIRE(found != nullptr);
    CHECK(found->id == e1);
    CHECK(g.findEdge(b, a) == nullptr);

    // Edge to nonexistent nodes fails
    CHECK(g.addEdge(a, 999) == InvalidEdgeId);
    CHECK(g.addEdge(999, a) == InvalidEdgeId);

    edge->metadata.set("weight", 5.5);
    CHECK(approxEqual(g.getEdge(e1)->metadata.get<double>("weight").value(), 5.5));

    CHECK(g.removeEdge(e1));
    CHECK(!g.hasEdge(e1));
    CHECK(!g.hasEdge(a, b));
    CHECK(g.edgeCount() == 3);

    CHECK(g.removeEdge(b, c));
    CHECK(!g.hasEdge(b, c));
    CHECK(g.edgeCount() == 2);
}

TEST_CASE("Node removal cascades to edges") {
    Graph g;
    NodeId a = g.addNode();
    NodeId b = g.addNode();
    NodeId c = g.addNode();

    EdgeId e1 = g.addEdge(a, b);
    EdgeId e2 = g.addEdge(b, c);
    EdgeId e3 = g.addEdge(c, a);

    REQUIRE(g.nodeCount() == 3);
    REQUIRE(g.edgeCount() == 3);

    // Remove node b - should remove e1 (a->b) and e2 (b->c)
    g.removeNode(b);

    CHECK(g.nodeCount() == 2);
    CHECK(g.edgeCount() == 1);
    CHECK(!g.hasEdge(e1));
    CHECK(!g.hasEdge(e2));
    CHECK(g.hasEdge(e3)); // c->a still exists
}

TEST_CASE("Adjacency queries") {
    Graph g;
    NodeId a = g.addNode();
    NodeId b = g.addNode();
    NodeId c = g.addNode();
    NodeId d = g.addNode();

    //   a --> b --> c
    //   |           ^
    //   +-----------+
    //   d (isolated)
    g.addEdge(a, b);
    g.addEdge(b, c);
    g.addEdge(a, c);

    CHECK(g.outEdges(a).size() == 2);
    CHECK(g.outEdges(b).size() == 1);
    CHECK(g.outEdges(c).size() == 0);
    CHECK(g.outEdges(d).size() == 0);

    CHECK(g.inEdges(a).size() == 0);
    CHECK(g.inEdges(b).size() == 1);
    CHECK(g.inEdges(c).size() == 2);

    auto succA = g.successors(a);
    CHECK(succA.size() == 2);
    std::set<NodeId> succASet(succA.begin(), succA.end());
    CHECK(succASet.count(b) == 1);
    CHECK(succASet.count(c) == 1);

    auto predC = g.predecessors(c);
    CHECK(predC.size() == 2);
    std::set<NodeId> predCSet(predC.begin(), predC.end());
    CHECK(predCSet.count(a) == 1);
    CHECK(predCSet.count(b) == 1);

    auto neighborsB = g.neighbors(b);
    CHECK(neighborsB.size() == 2);
    std::set<NodeId> neighborsBSet(neighborsB.begin(), neighborsB.end());
    CHECK(neighborsBSet.count(a) == 1);
    CHECK(neighborsBSet.count(c) == 1);

    CHECK(g.outDegree(a) == 2);
    CHECK(g.inDegree(a) == 0);
    CHECK(g.degree(a) == 2);

    CHECK(g.outDegree(b) == 1);
    CHECK(g.inDegree(b) == 1);
    CHECK(g.degree(b) == 2);

    CHECK(g.outDegree(c) == 0);
    CHECK(g.inDegree(c) == 2);
    CHECK(g.degree(c) == 2);

    CHECK(g.degree(d) == 0);
}

TEST_CASE("Node and edge iteration") {
    Graph g;
    NodeId a = g.addNode();
    NodeId b = g.addNode();
    g.addEdge(a, b);

    int nodeCount = 0;
    g.forEachNode([&](const Node& n) {
        nodeCount++;
        CHECK(n.id != InvalidNodeId);
    });
    CHECK(nodeCount == 2);

    g.forEachNode([](Node& n) {
        n.metadata.set("visited", true);
    });
    CHECK(g.getNode(a)->metadata.get<bool>("visited").value() == true);
    CHECK(g.getNode(b)->metadata.get<bool>("visited").value() == true);

    int edgeCount = 0;
    g.forEachEdge([&](const Edge& e) {
        edgeCount++;
        CHECK(e.id != InvalidEdgeId);
    });
    CHECK(edgeCount == 1);
}

TEST_CASE("Utility methods") {
    Graph g;
    CHECK(g.empty());

    g.addNode();
    g.addNode();
    CHECK(!g.empty());

    g.clear();
    CHECK(g.empty());
    CHECK(g.nodeCount() == 0);
    CHECK(g.edgeCount() == 0);

    g.reserve(1000, 5000);
    CHECK(g.empty());
}

TEST_CASE("BFS traversal") {
    Graph g;
    NodeId n1 = g.addNode();
    NodeId n2 = g.addNode();
    NodeId n3 = g.addNode();
    NodeId n4 = g.addNode();
    NodeId n5 = g.addNode(); // Isolated

    g.addEdge(n1, n2);
    g.addEdge(n2, n3);
    g.addEdge(n2, n4);

    std::vector<NodeId> visited;
    bfs(g, n1, [&](NodeId node) {
        visited.push_back(node);
        return true;
    });

    CHECK(visited.size() == 4);
    CHECK(visited[0] == n1);
    CHECK(visited[1] == n2);

    visited.clear();
    bfs(g, n5, [&](NodeId node) {
        visited.push_back(node);
        return true;
    });
    CHECK(visited.size() == 1);
    CHECK(visited[0] == n5);

    visited.clear();
    bfs(g, n1, [&](NodeId node) {
        visited.push_back(node);
        return node != n2; // Stop when we see n2
    });
    CHECK(visited.size() == 2);
}

TEST_CASE("DFS traversal") {
    Graph g;
    NodeId n1 = g.addNode();
    NodeId n2 = g.addNode();
    NodeId n3 = g.addNode();
    NodeId n4 = g.addNode();

    g.addEdge(n1, n2);
    g.addEdge(n2, n3);
    g.addEdge(n2, n4);

    std::vector<NodeId> visited;
    dfs(g, n1, [&](NodeId node) {
        visited.push_back(node);
        return true;
    });

    CHECK(visited.size() == 4);
    CHECK(visited[0] == n1);
}

TEST_CASE("Dijkstra's algorithm") {
    Graph g;
    NodeId a = g.addNode();
    NodeId b = g.addNode();
    NodeId c = g.addNode();
    NodeId d = g.addNode();

    // a --5-- b
    // |       |
    // 1       2
    // |       |
    // c --1-- d
    // Shortest a->d: a->c->d (cost 2)
    g.getEdge(g.addEdge(a, b))->metadata.set("weight", 5.0);
    g.getEdge(g.addEdge(a, c))->metadata.set("weight", 1.0);
    g.getEdge(g.addEdge(b, d))->metadata.set("weight", 2.0);
    g.getEdge(g.addEdge(c, d))->metadata.set("weight", 1.0);

    auto weight = weights::fromMetadata("weight");
    auto result = dijkstra(g, a, weight);

    CHECK(approxEqual(result.distances[a], 0.0));
    CHECK(approxEqual(result.distances[b], 5.0));
    CHECK(approxEqual(result.distances[c], 1.0));
    CHECK(approxEqual(result.distances[d], 2.0));

    auto path = result.pathTo(d);
    REQUIRE(path.found);
    REQUIRE(path.path.size() == 3);
    CHECK(path.path[0] == a);
    CHECK(path.path[1] == c);
    CHECK(path.path[2] == d);
    CHECK(approxEqual(path.totalCost, 2.0));
}

TEST_CASE("Dijkstra path") {
    Graph g;
    NodeId a = g.addNode();
    NodeId b = g.addNode();
    NodeId c = g.addNode();

    g.getEdge(g.addEdge(a, b))->metadata.set("weight", 5.0);
    g.getEdge(g.addEdge(b, c))->metadata.set("weight", 3.0);

    auto weight = weights::fromMetadata("weight");
    auto path = dijkstraPath(g, a, c, weight);

    REQUIRE(path.found);
    REQUIRE(path.path.size() == 3);
    CHECK(path.path[0] == a);
    CHECK(path.path[1] == b);
    CHECK(path.path[2] == c);
    CHECK(approxEqual(path.totalCost, 8.0));

    // Path to self
    auto selfPath = dijkstraPath(g, a, a, weight);
    REQUIRE(selfPath.found);
    CHECK(selfPath.path.size() == 1);
    CHECK(selfPath.path[0] == a);
    CHECK(approxEqual(selfPath.totalCost, 0.0));

    // No path
    NodeId isolated = g.addNode();
    auto noPath = dijkstraPath(g, a, isolated, weight);
    CHECK(!noPath.found);
}

TEST_CASE("A* algorithm") {
    Graph g;
    NodeId a = g.addNode();
    NodeId b = g.addNode();
    NodeId c = g.addNode();
    NodeId d = g.addNode();

    g.getEdge(g.addEdge(a, b))->metadata.set("weight", 1.0);
    g.getEdge(g.addEdge(b, d))->metadata.set("weight", 1.0);
    g.getEdge(g.addEdge(a, c))->metadata.set("weight", 2.0);
    g.getEdge(g.addEdge(c, d))->metadata.set("weight", 1.0);

    auto weight = weights::fromMetadata("weight");
    auto heuristic = [](NodeId, NodeId) { return 0.0; }; // Zero heuristic (= Dijkstra)

    auto path = astar(g, a, d, weight, heuristic);

    REQUIRE(path.found);
    REQUIRE(path.path.size() == 3);
    CHECK(path.path[0] == a);
    CHECK(path.path[1] == b);
    CHECK(path.path[2] == d);
    CHECK(approxEqual(path.totalCost, 2.0));
}

TEST_CASE("Connected components") {
    Graph g;
    NodeId a = g.addNode();
    NodeId b = g.addNode();
    NodeId c = g.addNode();
    g.addEdge(a, b);
    g.addEdge(b, c);

    NodeId d = g.addNode();
    NodeId e = g.addNode();
    g.addEdge(d, e);

    NodeId f = g.addNode(); // Isolated

    auto result = connectedComponents(g);

    CHECK(result.count == 3);
    CHECK(result.components.size() == 3);

    CHECK(result.componentId[a] == result.componentId[b]);
    CHECK(result.componentId[b] == result.componentId[c]);
    CHECK(result.componentId[d] == result.componentId[e]);

    CHECK(result.componentId[a] != result.componentId[d]);
    CHECK(result.componentId[a] != result.componentId[f]);
    CHECK(result.componentId[d] != result.componentId[f]);
}

TEST_CASE("Strongly connected components") {
    Graph g;
    NodeId a = g.addNode();
    NodeId b = g.addNode();
    NodeId c = g.addNode();
    NodeId d = g.addNode();

    // a <-> b -> c <-> d
    g.addEdge(a, b);
    g.addEdge(b, a); // a and b form SCC
    g.addEdge(b, c);
    g.addEdge(c, d);
    g.addEdge(d, c); // c and d form SCC

    auto result = stronglyConnectedComponents(g);

    CHECK(result.count == 2);
    CHECK(result.componentId[a] == result.componentId[b]);
    CHECK(result.componentId[c] == result.componentId[d]);
    CHECK(result.componentId[a] != result.componentId[c]);
}

TEST_CASE("isConnected") {
    Graph g;
    CHECK(isConnected(g)); // Empty graph is connected

    NodeId a = g.addNode();
    CHECK(isConnected(g)); // Single node

    NodeId b = g.addNode();
    CHECK(!isConnected(g)); // Two isolated nodes

    g.addEdge(a, b);
    CHECK(isConnected(g));

    NodeId c = g.addNode();
    CHECK(!isConnected(g)); // c is isolated

    g.addEdge(b, c);
    CHECK(isConnected(g));
}

TEST_CASE("Max flow") {
    Graph g;
    NodeId s = g.addNode(); // Source
    NodeId a = g.addNode();
    NodeId b = g.addNode();
    NodeId t = g.addNode(); // Sink

    // s -> a (cap 10), s -> b (cap 10), a -> b (cap 2), a -> t (cap 10), b -> t (cap 10)
    // Max flow = 20
    g.getEdge(g.addEdge(s, a))->metadata.set("capacity", 10.0);
    g.getEdge(g.addEdge(s, b))->metadata.set("capacity", 10.0);
    g.getEdge(g.addEdge(a, b))->metadata.set("capacity", 2.0);
    g.getEdge(g.addEdge(a, t))->metadata.set("capacity", 10.0);
    g.getEdge(g.addEdge(b, t))->metadata.set("capacity", 10.0);

    auto capacity = weights::fromMetadata("capacity");
    auto result = maxFlow(g, s, t, capacity);

    CHECK(approxEqual(result.maxFlow, 20.0));

    bool sourceInCut = false;
    for (NodeId n : result.minCutSourceSide) {
        if (n == s) sourceInCut = true;
    }
    CHECK(sourceInCut);
}

TEST_CASE("Weight from metadata") {
    Graph g;
    NodeId a = g.addNode();
    NodeId b = g.addNode();
    Edge* e = g.getEdge(g.addEdge(a, b));

    auto weight = weights::fromMetadata("weight", 999.0);

    CHECK(approxEqual(weight(*e), 999.0)); // No metadata set - should return default

    e->metadata.set("weight", 5.5);
    CHECK(approxEqual(weight(*e), 5.5));

    e->metadata.set("weight", int64_t(42));
    CHECK(approxEqual(weight(*e), 42.0)); // int64_t should convert
}
