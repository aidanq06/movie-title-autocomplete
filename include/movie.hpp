#pragma once

#include <cstddef>
#include <string>
#include <tuple>
#include <vector>

using MovieId = std::size_t;

struct Movie {
    std::string tconst;
    std::string primary_title;
    std::string title_type;
    int year = -1;
    std::string normalized_title;
};

struct SearchResult {
    std::vector<MovieId> movie_ids;
    std::size_t nodes_visited = 0;
};

struct IndexStats {
    std::size_t node_count = 0;
    std::size_t estimated_bytes = 0;
};

// Shared bytewise ordering for both indexes and correctness oracles.
// Exact ties remain equivalent; use stable_sort to retain source order.
inline bool movieLess(const Movie& lhs, const Movie& rhs) {
    return std::tie(lhs.normalized_title, lhs.primary_title, lhs.year, lhs.tconst)
         < std::tie(rhs.normalized_title, rhs.primary_title, rhs.year, rhs.tconst);
}

struct MovieIdLess {
    const std::vector<Movie>& movies;

    bool operator()(MovieId lhs, MovieId rhs) const {
        return movieLess(movies[lhs], movies[rhs]);
    }
};
