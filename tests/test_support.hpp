#pragma once

// Small helpers shared by the Phase 2 test programs. No external test framework.

#include "movie.hpp"
#include "normalize.hpp"
#include "prefix_index.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace test_support {

inline std::size_t checks = 0;

inline void check(bool condition, const std::string& message) {
    ++checks;
    if (!condition) {
        throw std::runtime_error(message);
    }
}

inline Movie makeMovie(const std::string& id, const std::string& title, int year = -1) {
    return {id, title, "movie", year, normalizeTitle(title)};
}

// Independent answer: scan every movie, then apply the canonical sort.
inline std::vector<MovieId> expectedMatches(const std::vector<Movie>& movies,
                                            const std::string& raw_prefix,
                                            std::size_t limit) {
    const auto prefix = normalizeTitle(raw_prefix);
    std::vector<MovieId> ids;
    if (prefix.empty()) {
        return ids;
    }
    for (MovieId id = 0; id < movies.size(); ++id) {
        if (movies[id].normalized_title.compare(0, prefix.size(), prefix) == 0) {
            ids.push_back(id);
        }
    }
    std::stable_sort(ids.begin(), ids.end(), MovieIdLess{movies});
    if (ids.size() > limit) {
        ids.resize(limit);
    }
    return ids;
}

inline void checkAgainstScan(const PrefixIndex& index, const std::vector<Movie>& movies,
                             const std::string& prefix, std::size_t limit = 10) {
    check(index.search(prefix, limit).movie_ids == expectedMatches(movies, prefix, limit),
          index.name() + " differs from scan for: " + prefix.substr(0, 80)
              + " limit " + std::to_string(limit));
}

} // namespace test_support
