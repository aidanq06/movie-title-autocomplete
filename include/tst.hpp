#pragma once

#include "prefix_index.hpp"

#include <limits>

// Ternary Search Tree over normalized movie titles. Every node holds one
// character plus three links: left/right lead to alternative characters at the
// same title position, middle advances to the next position.
//
// All nodes live in one vector and refer to each other by index, so a link stays
// valid when the vector reallocates. Links use std::size_t to match the Trie's
// edge targets, which keeps the memory comparison about structure, not link width.
class TernarySearchTree : public PrefixIndex {
public:
    void build(const std::vector<Movie>& movies) override;
    SearchResult search(const std::string& raw_prefix,
                        std::size_t limit = 10) const override;
    IndexStats stats() const override;
    std::string name() const override;

private:
    static constexpr std::size_t kNullNode = std::numeric_limits<std::size_t>::max();

    struct Node {
        explicit Node(char value) : character(value) {}

        char character;
        std::size_t left = kNullNode;
        std::size_t middle = kNullNode;
        std::size_t right = kNullNode;
        std::vector<MovieId> terminal_ids; // Movies whose whole key ends here.
    };

    std::vector<Node> nodes_;
    std::size_t root_ = kNullNode;

    std::size_t addNode(char character);
    void insert(const std::string& normalized_title, MovieId id);
};
