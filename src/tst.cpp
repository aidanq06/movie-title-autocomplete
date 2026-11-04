#include "tst.hpp"

#include <algorithm>

std::size_t TernarySearchTree::addNode(char character) {
    nodes_.emplace_back(character);
    return nodes_.size() - 1;
}

void TernarySearchTree::insert(const std::string& normalized_title, MovieId id) {
    if (root_ == kNullNode) {
        root_ = addNode(normalized_title[0]);
    }
    std::size_t node = root_;
    std::size_t position = 0;
    // addNode can reallocate nodes_, so this loop only holds indices. Each new
    // child index is stored in a local before the parent is looked up again.
    for (;;) {
        const char ch = normalized_title[position];
        const char current = nodes_[node].character;
        if (ch < current) {
            std::size_t next = nodes_[node].left;
            if (next == kNullNode) {
                next = addNode(ch);
                nodes_[node].left = next;
            }
            node = next;
        } else if (ch > current) {
            std::size_t next = nodes_[node].right;
            if (next == kNullNode) {
                next = addNode(ch);
                nodes_[node].right = next;
            }
            node = next;
        } else if (position + 1 == normalized_title.size()) {
            nodes_[node].terminal_ids.push_back(id);
            return;
        } else {
            ++position;
            std::size_t next = nodes_[node].middle;
            if (next == kNullNode) {
                next = addNode(normalized_title[position]);
                nodes_[node].middle = next;
            }
            node = next;
        }
    }
}

void TernarySearchTree::build(const std::vector<Movie>& movies) {
    nodes_ = std::vector<Node>(); // Release old nodes and their dynamic storage.
    root_ = kNullNode;
    for (MovieId id = 0; id < movies.size(); ++id) {
        if (!movies[id].normalized_title.empty()) {
            insert(movies[id].normalized_title, id);
        }
    }
    // Same canonical order as the Trie, independent of insertion order.
    for (auto& node : nodes_) {
        std::stable_sort(node.terminal_ids.begin(), node.terminal_ids.end(), MovieIdLess{movies});
    }
}

SearchResult TernarySearchTree::search(const std::string&, std::size_t) const {
    return {}; // Prefix search is the next milestone.
}

IndexStats TernarySearchTree::stats() const {
    IndexStats result;
    result.node_count = nodes_.size();
    // Same capacity convention as the Trie. Links live inside Node, so only the
    // node vector and terminal ID vectors own memory here.
    result.estimated_bytes = nodes_.capacity() * sizeof(Node);
    for (const auto& node : nodes_) {
        result.estimated_bytes += node.terminal_ids.capacity() * sizeof(MovieId);
    }
    return result;
}

std::string TernarySearchTree::name() const {
    return "Ternary Search Tree";
}
