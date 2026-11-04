#pragma once

#include "movie.hpp"

struct LoadResult {
    std::vector<Movie> movies;
    std::size_t rows_scanned = 0;     // Source data lines, excluding the header.
    std::size_t records_loaded = 0;   // Always equals movies.size().
    std::size_t malformed_rows = 0;   // Wrong field count or missing IMDb ID.
};

// Streams a decompressed TSV; throws std::runtime_error for file/header/I/O errors.
// Selects the first maxMovies eligible rows without deduplication.
LoadResult loadImdbMovies(const std::string& path, std::size_t maxMovies = 150000);
