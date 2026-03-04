/*******************************************************************************
 * File: graph_types.hpp
 *
 * Description: Core types for the GeoDraw graph module.
 * Provides NodeId, EdgeId, Metadata, Node, and Edge types for building
 * generic, algorithm-agnostic graphs with attachable metadata.
 *
 *
 ******************************************************************************/

#pragma once

#include "geodraw/export/export.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>

namespace geodraw {
namespace graph {

// =============================================================================
// ID Types
// =============================================================================

using NodeId = uint64_t;
using EdgeId = uint64_t;

constexpr NodeId InvalidNodeId = 0;
constexpr EdgeId InvalidEdgeId = 0;

// =============================================================================
// Metadata
// =============================================================================

/**
 * Type-safe metadata value supporting common types.
 */
using MetadataValue = std::variant<std::string, int64_t, double, bool>;

/**
 * Key-value metadata store for attaching arbitrary data to nodes and edges.
 */
class GEODRAW_API Metadata {
public:
    /**
     * Set a metadata value for the given key.
     * Overwrites any existing value with the same key.
     */
    void set(const std::string& key, MetadataValue value);

    /**
     * Get a metadata value by key, with type checking.
     * Returns std::nullopt if the key doesn't exist or the type doesn't match.
     */
    template<typename T>
    std::optional<T> get(const std::string& key) const {
        auto it = data_.find(key);
        if (it == data_.end()) {
            return std::nullopt;
        }
        if (auto* val = std::get_if<T>(&it->second)) {
            return *val;
        }
        return std::nullopt;
    }

    /**
     * Check if a key exists in the metadata.
     */
    bool has(const std::string& key) const;

    /**
     * Remove a key from the metadata.
     */
    void remove(const std::string& key);

    /**
     * Clear all metadata.
     */
    void clear();

    /**
     * Get the number of metadata entries.
     */
    size_t size() const;

    /**
     * Check if the metadata is empty.
     */
    bool empty() const;

private:
    std::unordered_map<std::string, MetadataValue> data_;
};

// =============================================================================
// Node and Edge
// =============================================================================

/**
 * Graph node with unique ID and attachable metadata.
 */
struct GEODRAW_API Node {
    NodeId id = InvalidNodeId;
    Metadata metadata;
};

/**
 * Directed edge connecting two nodes, with unique ID and attachable metadata.
 */
struct GEODRAW_API Edge {
    EdgeId id = InvalidEdgeId;
    NodeId source = InvalidNodeId;
    NodeId target = InvalidNodeId;
    Metadata metadata;
};

} // namespace graph
} // namespace geodraw
