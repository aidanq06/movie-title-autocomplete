#include "app.hpp"
#include "imdb_loader.hpp"

#include <charconv>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
void usage(std::ostream& out) {
    out << "Movie Title Autocomplete - Trie vs Ternary Search Tree\n"
        << "Usage: movie_autocomplete --data PATH [--max-movies N]\n"
        << "                          [--search PREFIX [--algorithm trie|tst|both] | --benchmark [--csv FILE]]\n"
        << "  --help              Display this help\n"
        << "  --data PATH         Decompressed IMDb-format TSV file\n"
        << "  --max-movies N      Select up to N eligible movies (default: 150000; 0 allowed)\n"
        << "  --search PREFIX     Return up to ten matches and exit\n"
        << "  --algorithm NAME    Structure used by --search: trie (default), tst, or both\n"
        << "  --benchmark         Run the full performance benchmark and exit\n"
        << "  --csv FILE          With --benchmark, also write per-query measurements to FILE\n"
        << "With --data and neither --search nor --benchmark, an interactive menu starts.\n"
        << "Exit codes: 0 success, 1 file or I/O failure, 2 invalid arguments, 3 results differ.\n"
        << "Examples:\n"
        << "  ./build/movie_autocomplete --data data/sample_movies.tsv\n"
        << "  ./build/movie_autocomplete --data data/sample_movies.tsv --search \"star\" --algorithm both\n"
        << "  ./build/movie_autocomplete --data data/title.basics.tsv --max-movies 150000 --benchmark\n";
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

struct Options {
    std::string path;
    std::string prefix;
    std::string csv_path;
    std::size_t max_movies = 150000;
    AlgorithmChoice algorithm = AlgorithmChoice::Trie; // Phase 1's --search behavior.
    bool has_search = false;
    bool has_algorithm = false;
    bool has_benchmark = false;
    bool show_help = false;
};

Options parseOptions(int argc, char* argv[]) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string option = argv[i];
        if (option == "--help") {
            options.show_help = true;
            return options;
        }
        if (option == "--benchmark") {
            options.has_benchmark = true;
            continue;
        }
        if (option != "--data" && option != "--max-movies" && option != "--search"
            && option != "--algorithm" && option != "--csv") {
            throw std::invalid_argument("Unknown option: " + option);
        }
        if (i + 1 == argc) {
            throw std::invalid_argument("Missing value for " + option);
        }
        const std::string value = argv[++i];
        if (option == "--data") {
            options.path = value;
        } else if (option == "--max-movies") {
            options.max_movies = parseCount(value);
        } else if (option == "--search") {
            options.prefix = value;
            options.has_search = true;
        } else if (option == "--csv") {
            options.csv_path = value;
        } else {
            const auto algorithm = parseAlgorithmChoice(value);
            if (!algorithm) {
                throw std::invalid_argument("--algorithm must be trie, tst, or both, not: " + value);
            }
            options.algorithm = *algorithm;
            options.has_algorithm = true;
        }
    }
    if (options.path.empty()) {
        throw std::invalid_argument("--data PATH is required");
    }
    if (options.has_search && options.has_benchmark) {
        throw std::invalid_argument("--search and --benchmark cannot be used together");
    }
    if (options.has_algorithm && !options.has_search) {
        throw std::invalid_argument("--algorithm only applies to --search");
    }
    if (!options.csv_path.empty() && !options.has_benchmark) {
        throw std::invalid_argument("--csv only applies to --benchmark");
    }
    return options;
}
} // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc == 1) {
            usage(std::cout);
            return kExitSuccess;
        }
        const Options options = parseOptions(argc, argv);
        if (options.show_help) {
            usage(std::cout);
            return kExitSuccess;
        }

        // The one load of the dataset. Every index below shares this vector.
        const auto loaded = loadImdbMovies(options.path, options.max_movies);
        printLoadSummary(loaded, options.max_movies, std::cout);
        if (options.has_search) {
            return runSearchCommand(loaded, options.prefix, options.algorithm, std::cout);
        }
        if (options.has_benchmark) {
            return runBenchmarkCommand(loaded, options.csv_path, std::cout, std::cerr);
        }
        return runInteractiveMenu(loaded, options.path, std::cin, std::cout);
    } catch (const std::invalid_argument& error) {
        std::cerr << "Error: " << error.what() << "\n\n";
        usage(std::cerr);
        return kExitBadArguments;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return kExitFailure;
    }
}
