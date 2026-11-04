#pragma once

#include "movie.hpp"

// IDs are positions in the shared vector. It must remain alive, at the same
// address, and unmodified until every index using it is destroyed or rebuilt.
// Populate normalized_title with normalizeTitle(primary_title) before build.
class PrefixIndex {
public:
    virtual ~PrefixIndex() = default;
    virtual void build(const std::vector<Movie>& movies) = 0;
    virtual SearchResult search(const std::string& raw_prefix,
                                std::size_t limit = 10) const = 0;
    virtual IndexStats stats() const = 0;
    virtual std::string name() const = 0;
};
