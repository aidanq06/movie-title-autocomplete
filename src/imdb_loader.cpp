#include "imdb_loader.hpp"
#include "normalize.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace {
void splitFields(const std::string& line, std::vector<std::string_view>& fields) {
    fields.clear();
    std::size_t start = 0;
    for (;;) {
        const auto end = line.find('\t', start);
        if (end == std::string::npos) {
            fields.emplace_back(line.data() + start, line.size() - start);
            return;
        }
        fields.emplace_back(line.data() + start, end - start);
        start = end + 1;
    }
}

bool missing(std::string_view value) {
    return value.empty() || value == "\\N";
}

int parseYear(std::string_view value) {
    // IMDb documents YYYY. Reject signs, partial parses, overflow, and year zero.
    if (value.size() != 4) {
        return -1;
    }
    int year = 0;
    for (char ch : value) {
        if (ch < '0' || ch > '9') {
            return -1;
        }
        year = year * 10 + (ch - '0');
    }
    return year == 0 ? -1 : year;
}

void stripCarriageReturn(std::string& line) {
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
}
} // namespace

LoadResult loadImdbMovies(const std::string& path, std::size_t maxMovies) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Cannot open IMDb TSV: " + path);
    }
    std::string line;
    if (!std::getline(input, line)) {
        throw std::runtime_error("Cannot read IMDb TSV header: " + path);
    }
    stripCarriageReturn(line);
    // Tolerate an optional UTF-8 BOM before the first header name.
    if (line.compare(0, 3, "\xEF\xBB\xBF") == 0) {
        line.erase(0, 3);
    }
    std::vector<std::string_view> fields;
    splitFields(line, fields);
    const auto column_count = fields.size();
    constexpr std::array<std::string_view, 5> required = {
        "tconst", "titleType", "primaryTitle", "isAdult", "startYear"
    };
    std::array<std::size_t, required.size()> columns{};
    for (std::size_t i = 0; i < fields.size(); ++i) {
        if (fields[i].empty() || std::find(fields.begin(), fields.begin() + i, fields[i])
                                     != fields.begin() + i) {
            throw std::runtime_error("Malformed IMDb TSV header: empty or duplicate column in " + path);
        }
    }
    for (std::size_t i = 0; i < required.size(); ++i) {
        const auto found = std::find(fields.begin(), fields.end(), required[i]);
        if (found == fields.end()) {
            throw std::runtime_error("Malformed IMDb TSV header: missing "
                                     + std::string(required[i]) + " in " + path);
        }
        columns[i] = static_cast<std::size_t>(found - fields.begin());
    }

    LoadResult result;
    result.movies.reserve(std::min(maxMovies, std::size_t{150000}));
    while (result.movies.size() < maxMovies && std::getline(input, line)) {
        ++result.rows_scanned;
        stripCarriageReturn(line);
        splitFields(line, fields);
        if (fields.size() != column_count || missing(fields[columns[0]])) {
            ++result.malformed_rows;
            continue;
        }
        if (fields[columns[1]] != "movie" || fields[columns[3]] != "0"
            || missing(fields[columns[2]])) {
            continue;
        }
        std::string title(fields[columns[2]]);
        auto normalized = normalizeTitle(title);
        if (normalized.empty()) {
            continue;
        }
        result.movies.push_back({std::string(fields[columns[0]]), std::move(title),
                                 "movie", parseYear(fields[columns[4]]), std::move(normalized)});
    }
    if (input.bad() || (input.fail() && !input.eof())) {
        throw std::runtime_error("I/O error while reading IMDb TSV: " + path);
    }
    result.records_loaded = result.movies.size();
    return result;
}
