#include "imdb_loader.hpp"
#include "trie.hpp"

#include <charconv>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
void usage(std::ostream& out) {
    out << "Movie Title Autocomplete - Phase 1 (Trie)\n"
        << "Usage: movie_autocomplete --data PATH [--max-movies N] [--search PREFIX]\n"
        << "  --help          Display this help\n"
        << "  --data PATH     Decompressed IMDb-format TSV file\n"
        << "  --max-movies N  Select up to N eligible movies (default: 150000; 0 allowed)\n"
        << "  --search PREFIX  Return up to ten matches and exit\n"
        << "Example: ./build/movie_autocomplete --data data/sample_movies.tsv --search \"star\"\n";
}

std::size_t parseCount(std::string_view value) {
    if (value.empty() || value.find_first_not_of("0123456789") != std::string_view::npos) {
        throw std::invalid_argument("--max-movies requires a nonnegative integer");
    }
    std::size_t count = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), count);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
        throw std::invalid_argument("--max-movies is too large");
    }
    return count;
}
} // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc == 1) {
            usage(std::cout);
            return 0;
        }
        std::string path;
        std::string prefix;
        std::size_t max_movies = 150000;
        bool has_search = false;
        for (int i = 1; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--help") {
                usage(std::cout);
                return 0;
            }
            if (option != "--data" && option != "--max-movies" && option != "--search") {
                throw std::invalid_argument("Unknown option: " + option);
            }
            if (i + 1 == argc) {
                throw std::invalid_argument("Missing value for " + option);
            }
            const std::string value = argv[++i];
            if (option == "--data") {
                path = value;
            } else if (option == "--max-movies") {
                max_movies = parseCount(value);
            } else {
                prefix = value;
                has_search = true;
            }
        }
        if (path.empty()) {
            throw std::invalid_argument("--data PATH is required");
        }

        const auto loaded = loadImdbMovies(path, max_movies);
        Trie trie;
        trie.build(loaded.movies);
        const auto stats = trie.stats();
        std::cout << "Loaded records: " << loaded.records_loaded << '\n'
                  << "Source rows scanned: " << loaded.rows_scanned << '\n'
                  << "Malformed rows skipped: " << loaded.malformed_rows << '\n'
                  << "Requested maximum: " << max_movies << '\n';
        if (loaded.records_loaded < max_movies) {
            std::cout << "Reached end of file before requested maximum.\n";
        }
        std::cout << "Trie nodes: " << stats.node_count << '\n'
                  << "Estimated index storage (bytes): " << stats.estimated_bytes << '\n';
        if (!has_search) {
            std::cout << "Supply --search PREFIX to run a Trie prefix search.\n";
            return 0;
        }

        const auto result = trie.search(prefix);
        std::cout << "Search prefix: \"" << prefix << "\"\n"
                  << "Matches: " << result.movie_ids.size() << '\n';
        for (MovieId id : result.movie_ids) {
            const auto& movie = loaded.movies[id];
            std::cout << "  " << movie.primary_title << " ("
                      << (movie.year == -1 ? "Unknown year" : std::to_string(movie.year))
                      << ") [" << movie.tconst << "]\n";
        }
        std::cout << "Nodes visited: " << result.nodes_visited << '\n';
        return 0;
    } catch (const std::invalid_argument& error) {
        std::cerr << "Error: " << error.what() << "\n\n";
        usage(std::cerr);
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
