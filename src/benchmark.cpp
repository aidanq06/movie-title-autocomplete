#include "benchmark.hpp"
#include "normalize.hpp"

#include <algorithm>
#include <random>
#include <set>
#include <utility>

namespace {
// Movie IDs in canonical order. Because the order starts with the normalized title,
// all titles sharing a prefix sit in one contiguous range, which gives an answer
// that is independent of both index implementations.
class SortedScan {
public:
    explicit SortedScan(const std::vector<Movie>& movies) : movies_(movies) {
        sorted_.reserve(movies.size());
        for (MovieId id = 0; id < movies.size(); ++id) {
            if (!movies[id].normalized_title.empty()) {
                sorted_.push_back(id);
            }
        }
        std::stable_sort(sorted_.begin(), sorted_.end(), MovieIdLess{movies});
    }

    const std::vector<MovieId>& sorted() const { return sorted_; }

    std::size_t countMatches(const std::string& prefix) const {
        const auto range = matchRange(prefix);
        return static_cast<std::size_t>(range.second - range.first);
    }

    std::vector<MovieId> matches(const std::string& prefix, std::size_t limit) const {
        const auto range = matchRange(prefix);
        const auto count = std::min(limit, static_cast<std::size_t>(range.second - range.first));
        return std::vector<MovieId>(range.first, range.first + static_cast<std::ptrdiff_t>(count));
    }

private:
    using Iterator = std::vector<MovieId>::const_iterator;

    std::pair<Iterator, Iterator> matchRange(const std::string& prefix) const {
        const auto begin = std::lower_bound(sorted_.begin(), sorted_.end(), prefix,
            [&](MovieId id, const std::string& key) { return movies_[id].normalized_title < key; });
        const auto end = std::partition_point(begin, sorted_.end(), [&](MovieId id) {
            return movies_[id].normalized_title.compare(0, prefix.size(), prefix) == 0;
        });
        return {begin, end};
    }

    const std::vector<Movie>& movies_;
    std::vector<MovieId> sorted_;
};

// std::uniform_int_distribution and std::shuffle differ between standard libraries.
// mt19937's raw output does not, so these queries are the same on every platform.
class WorkloadBuilder {
public:
    WorkloadBuilder(const std::vector<Movie>& movies, const SortedScan& scan, std::uint32_t seed)
        : movies_(movies), scan_(scan), rng_(seed) {}

    std::vector<BenchmarkQuery> take() { return std::move(queries_); }

    // The prefixes shared by the most titles. Selected first so they keep this label.
    void addMostCommon(std::size_t target, std::size_t min_length, std::size_t max_length) {
        std::vector<std::pair<std::size_t, std::string>> counted;
        for (std::size_t length = min_length; length <= max_length; ++length) {
            std::string current;
            std::size_t count = 0;
            const auto flush = [&] {
                if (count >= 2) {
                    counted.emplace_back(count, current);
                }
            };
            for (MovieId id : scan_.sorted()) {
                const auto& title = movies_[id].normalized_title;
                if (title.size() < length || title[length - 1] == ' ') {
                    continue;
                }
                if (title.compare(0, length, current) == 0 && current.size() == length) {
                    ++count;
                } else {
                    flush();
                    current = title.substr(0, length);
                    count = 1;
                }
            }
            flush();
        }
        std::sort(counted.begin(), counted.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.first != rhs.first ? lhs.first > rhs.first : lhs.second < rhs.second;
        });
        std::size_t added = 0;
        for (const auto& entry : counted) {
            if (added == target) {
                break;
            }
            added += add(entry.second, QueryCategory::Common) ? 1 : 0;
        }
    }

    // Random prefixes of a title; max_matches filters for uncommon ones.
    void addRandom(QueryCategory category, std::size_t target, std::size_t min_length,
                   std::size_t max_length, std::size_t max_matches = SIZE_MAX) {
        fill(target, [&] {
            const auto prefix = randomPrefix(min_length, max_length);
            if (!prefix.empty() && scan_.countMatches(prefix) <= max_matches) {
                add(prefix, category);
            }
        });
    }

    // A real prefix followed by a character that makes it match nothing. Checked
    // against the data, so the category label is always true.
    void addNoMatch(std::size_t target) {
        static constexpr char kSuffixes[] = {'q', 'x', 'z', '0', '7'};
        fill(target, [&] {
            auto prefix = randomPrefix(2, 7);
            if (prefix.empty()) {
                return;
            }
            prefix.push_back(kSuffixes[rng_() % (sizeof(kSuffixes))]);
            if (scan_.countMatches(prefix) == 0) {
                add(prefix, QueryCategory::NoMatch);
            }
        });
    }

private:
    bool add(const std::string& prefix, QueryCategory category) {
        if (prefix.empty() || !used_.insert(prefix).second) {
            return false;
        }
        queries_.push_back({prefix, category});
        return true;
    }

    std::string randomPrefix(std::size_t min_length, std::size_t max_length) {
        const auto& sorted = scan_.sorted();
        const auto& title = movies_[sorted[rng_() % sorted.size()]].normalized_title;
        const std::size_t length = min_length + rng_() % (max_length - min_length + 1);
        if (title.size() < length) {
            return {};
        }
        // Normalizing trims a trailing space, which would otherwise duplicate a query.
        auto prefix = normalizeTitle(title.substr(0, length));
        return prefix.size() >= min_length ? prefix : std::string{};
    }

    // Small datasets may run out of unique prefixes, so attempts are bounded.
    template <typename Attempt>
    void fill(std::size_t target, Attempt attempt) {
        const auto goal = queries_.size() + target;
        for (std::size_t tries = 0; tries < target * 500 && queries_.size() < goal; ++tries) {
            attempt();
        }
    }

    const std::vector<Movie>& movies_;
    const SortedScan& scan_;
    std::mt19937 rng_;
    std::set<std::string> used_;
    std::vector<BenchmarkQuery> queries_;
};

std::vector<BenchmarkQuery> buildWorkload(const std::vector<Movie>& movies, const SortedScan& scan,
                                          std::uint32_t seed) {
    if (scan.sorted().empty()) {
        return {};
    }
    WorkloadBuilder builder(movies, scan, seed);
    builder.addMostCommon(15, 3, 5);
    builder.addRandom(QueryCategory::Short, 20, 1, 2);
    builder.addRandom(QueryCategory::Medium, 30, 3, 5);
    builder.addRandom(QueryCategory::Long, 30, 6, 10);
    builder.addRandom(QueryCategory::Rare, 15, 3, 10, 3);
    builder.addNoMatch(20);
    return builder.take();
}
} // namespace

const char* queryCategoryName(QueryCategory category) {
    switch (category) {
    case QueryCategory::Short: return "short";
    case QueryCategory::Medium: return "medium";
    case QueryCategory::Long: return "long";
    case QueryCategory::Common: return "common";
    case QueryCategory::Rare: return "rare";
    case QueryCategory::NoMatch: return "no-match";
    }
    return "unknown";
}

std::vector<BenchmarkQuery> generateQueryWorkload(const std::vector<Movie>& movies, std::uint32_t seed) {
    const SortedScan scan(movies);
    return buildWorkload(movies, scan, seed);
}
