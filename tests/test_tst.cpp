#include "test_support.hpp"
#include "trie.hpp"
#include "tst.hpp"

#include <iostream>

using namespace test_support;

namespace {
void testEmptyIndex() {
    TernarySearchTree tst;
    check(tst.name() == "Ternary Search Tree", "index name");
    check(tst.stats().node_count == 0 && tst.stats().estimated_bytes == 0, "unbuilt stats");
    tst.build({});
    check(tst.stats().node_count == 0, "empty build has no nodes");
}

void testNodeCounts() {
    TernarySearchTree tst;
    const std::vector<Movie> movies = {makeMovie("a", "A"), makeMovie("ab", "AB"),
                                       makeMovie("ac", "AC"), makeMovie("empty", "!!!")};
    tst.build(movies);
    check(tst.stats().node_count == 3, "one node per distinct nonempty prefix");
    check(tst.stats().estimated_bytes > 0, "estimated bytes");

    // Each TST node stands for one distinct nonempty key prefix, exactly like a
    // non-root Trie node, whatever order the titles were inserted in.
    Trie trie;
    const std::vector<Movie> titles = {makeMovie("1", "cat"), makeMovie("2", "cap"),
                                       makeMovie("3", "car"), makeMovie("4", "cat"),
                                       makeMovie("5", "dog"), makeMovie("6", "cats")};
    trie.build(titles);
    tst.build(titles);
    check(tst.stats().node_count + 1 == trie.stats().node_count, "same prefixes as the Trie");
}

void testInsertSurvivesReallocation() {
    // Many sibling and deep inserts force nodes_ to reallocate during insertion.
    std::vector<Movie> movies;
    for (int i = 0; i < 5000; ++i) {
        movies.push_back(makeMovie("tt" + std::to_string(i), "movie " + std::to_string(i * 7919)));
    }
    Trie trie;
    TernarySearchTree tst;
    trie.build(movies);
    tst.build(movies);
    check(tst.stats().node_count + 1 == trie.stats().node_count, "no nodes lost on growth");
}
} // namespace

int main() {
    try {
        testEmptyIndex();
        testNodeCounts();
        testInsertSurvivesReallocation();
        std::cout << "Passed " << checks << " checks\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
