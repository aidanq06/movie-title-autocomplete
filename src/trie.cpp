#include "trie.hpp"
#include "normalize.hpp"

#include <algorithm>

void Trie::insert(const std::string& normalized_title, MovieId id) {
    std::size_t node = 0;
    for (char ch : normalized_title) {
        const auto& children = nodes_[node].children;
        const auto found = std::lower_bound(children.begin(), children.end(), ch,
            [](const auto& edge, char key) { return edge.first < key; });
        if (found != children.end() && found->first == ch) {
            node = found->second;
        } else {
            const auto position = static_cast<std::size_t>(found - children.begin());
            const auto child = nodes_.size();
            nodes_.emplace_back();
            // Growing nodes_ invalidates references/iterators: reacquire by index.
            auto& updated_children = nodes_[node].children;
            updated_children.insert(updated_children.begin() + position, {ch, child});
            node = child;
        }
    }
    nodes_[node].terminal_ids.push_back(id);
}

void Trie::build(const std::vector<Movie>& movies) {
    nodes_ = std::vector<Node>(1); // Release old nodes and their dynamic storage.
    for (MovieId id = 0; id < movies.size(); ++id) {
        if (!movies[id].normalized_title.empty()) {
            insert(movies[id].normalized_title, id);
        }
    }
    for (auto& node : nodes_) {
        std::stable_sort(node.terminal_ids.begin(), node.terminal_ids.end(), MovieIdLess{movies});
    }
}

SearchResult Trie::search(const std::string& raw_prefix, std::size_t limit) const {
    SearchResult result;
    if (limit == 0) {
        return result;
    }
    const auto prefix = normalizeTitle(raw_prefix);
    if (prefix.empty()) {
        return result;
    }
    std::size_t node = 0;
    result.nodes_visited = 1; // Root; count each reached node once per query.
    for (char ch : prefix) {
        const auto& children = nodes_[node].children;
        const auto found = std::lower_bound(children.begin(), children.end(), ch,
            [](const auto& edge, char key) { return edge.first < key; });
        if (found == children.end() || found->first != ch) {
            return result;
        }
        node = found->second;
        ++result.nodes_visited;
    }

    const auto append_terminal = [&](std::size_t index) {
        for (MovieId id : nodes_[index].terminal_ids) {
            result.movie_ids.push_back(id);
            if (result.movie_ids.size() == limit) break;
        }
    };
    append_terminal(node); // A title precedes any longer title with this prefix.
    if (result.movie_ids.size() == limit) {
        return result;
    }

    // Explicit DFS frames avoid call-stack overflow for unusually long titles.
    struct Frame { std::size_t node; std::size_t next_child; };
    std::vector<Frame> stack = {{node, 0}};
    while (!stack.empty()) {
        auto& frame = stack.back();
        const auto& children = nodes_[frame.node].children;
        if (frame.next_child == children.size()) {
            stack.pop_back();
            continue;
        }
        const auto child = children[frame.next_child++].second;
        ++result.nodes_visited;
        append_terminal(child);
        if (result.movie_ids.size() == limit) {
            return result;
        }
        stack.push_back({child, 0});
    }
    return result;
}

IndexStats Trie::stats() const {
    IndexStats result;
    result.node_count = nodes_.size();
    result.estimated_bytes = nodes_.capacity() * sizeof(Node);
    for (const auto& node : nodes_) {
        result.estimated_bytes += node.children.capacity() * sizeof(std::pair<char, std::size_t>);
        result.estimated_bytes += node.terminal_ids.capacity() * sizeof(MovieId);
    }
    return result;
}

std::string Trie::name() const {
    return "Trie";
}
