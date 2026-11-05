#include "tst.hpp"
#include "normalize.hpp"

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

SearchResult TernarySearchTree::search(const std::string& raw_prefix, std::size_t limit) const {
    SearchResult result;
    if (limit == 0) {
        return result;
    }
    const auto prefix = normalizeTitle(raw_prefix);
    if (prefix.empty()) {
        return result;
    }

    // Find the node that matches the last prefix character. nodes_visited counts
    // every node examined, including left/right nodes passed on the way down.
    std::size_t node = root_;
    std::size_t position = 0;
    while (node != kNullNode) {
        ++result.nodes_visited;
        const Node& current = nodes_[node];
        const char ch = prefix[position];
        if (ch < current.character) {
            node = current.left;
        } else if (ch > current.character) {
            node = current.right;
        } else if (position + 1 == prefix.size()) {
            break;
        } else {
            node = current.middle;
            ++position;
        }
    }
    if (node == kNullNode) {
        return result;
    }

    const auto append_terminal = [&](const Node& terminal) {
        for (MovieId id : terminal.terminal_ids) {
            result.movie_ids.push_back(id);
            if (result.movie_ids.size() == limit) {
                break;
            }
        }
    };
    // The endpoint's left/right links hold other characters at the same position,
    // which do not share this prefix. Only its own IDs and middle subtree match.
    append_terminal(nodes_[node]);
    if (result.movie_ids.size() == limit) {
        return result;
    }

    // In-order walk: left subtree, this node's IDs, middle subtree, right subtree.
    // An explicit stack keeps very long titles from overflowing the call stack.
    enum class Step : unsigned char { Left, Center, Right };
    struct Frame {
        std::size_t node;
        Step next;
    };
    std::vector<Frame> stack;
    const auto enter = [&](std::size_t child) {
        if (child != kNullNode) {
            ++result.nodes_visited;
            stack.push_back({child, Step::Left});
        }
    };
    enter(nodes_[node].middle);
    while (!stack.empty()) {
        // enter() may reallocate the stack, so update the frame before calling it.
        Frame& frame = stack.back();
        const Node& current = nodes_[frame.node];
        switch (frame.next) {
        case Step::Left:
            frame.next = Step::Center;
            enter(current.left);
            break;
        case Step::Center:
            append_terminal(current);
            if (result.movie_ids.size() == limit) {
                return result;
            }
            frame.next = Step::Right;
            enter(current.middle);
            break;
        case Step::Right:
            // Nothing remains here after the right subtree, so drop this frame first.
            stack.pop_back();
            enter(current.right);
            break;
        }
    }
    return result;
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
