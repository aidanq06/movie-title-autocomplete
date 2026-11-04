#pragma once

#include "prefix_index.hpp"

#include <utility>

class Trie : public PrefixIndex {
public:
    void build(const std::vector<Movie>& movies) override;
    SearchResult search(const std::string& raw_prefix,
                        std::size_t limit = 10) const override;
    IndexStats stats() const override;
    std::string name() const override;

private:
    struct Node {
        std::vector<std::pair<char, std::size_t>> children;
        std::vector<MovieId> terminal_ids;
    };
    std::vector<Node> nodes_ = std::vector<Node>(1); // Root is always index zero.

    void insert(const std::string& normalized_title, MovieId id);
};
