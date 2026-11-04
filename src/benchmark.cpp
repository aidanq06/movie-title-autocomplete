#include "benchmark.hpp"
#include "format_utils.hpp"
#include "normalize.hpp"
#include "trie.hpp"
#include "tst.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <numeric>
#include <ostream>
#include <random>
#include <set>
#include <stdexcept>
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

using Clock = std::chrono::steady_clock;
using IndexPair = std::array<std::unique_ptr<PrefixIndex>, 2>;

constexpr std::size_t kNoResultCap = std::numeric_limits<std::size_t>::max();
constexpr std::size_t kMaxMismatchDetails = 20;

double elapsedMicroseconds(Clock::time_point start, Clock::time_point stop) {
    return std::chrono::duration<double, std::micro>(stop - start).count();
}

double meanOf(const std::vector<double>& values) {
    return values.empty() ? 0.0
                          : std::accumulate(values.begin(), values.end(), 0.0)
                                / static_cast<double>(values.size());
}

double medianOf(std::vector<double> values) {
    if (values.empty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const auto middle = values.size() / 2;
    return values.size() % 2 == 1 ? values[middle] : (values[middle - 1] + values[middle]) / 2.0;
}

// Each structure builds a fresh index in turn. The order alternates so neither
// always runs right after the other's allocations, and destruction is not timed.
IndexPair measureBuilds(const std::array<const IndexFactory*, 2>& factories,
                        const std::vector<Movie>& movies, std::size_t repetitions,
                        BenchmarkReport& report) {
    IndexPair live;
    for (std::size_t repetition = 0; repetition < repetitions; ++repetition) {
        for (std::size_t slot = 0; slot < 2; ++slot) {
            const std::size_t which = repetition % 2 == 0 ? slot : 1 - slot;
            live[which].reset();
            auto index = (*factories[which])();
            const auto start = Clock::now();
            index->build(movies);
            const auto stop = Clock::now();
            report.algorithms[which].build_ms.push_back(elapsedMicroseconds(start, stop) / 1000.0);
            live[which] = std::move(index);
        }
    }
    for (std::size_t which = 0; which < 2; ++which) {
        auto& algorithm = report.algorithms[which];
        algorithm.name = live[which]->name();
        algorithm.median_build_ms = medianOf(algorithm.build_ms);
        algorithm.stats = live[which]->stats();
    }
    return live;
}

void warmUp(const IndexPair& indexes, const std::vector<BenchmarkQuery>& queries,
            std::size_t limit, std::size_t passes) {
    volatile std::size_t sink = 0; // Keeps the optimizer from removing the searches.
    for (std::size_t pass = 0; pass < passes; ++pass) {
        for (const auto& index : indexes) {
            for (const auto& query : queries) {
                sink = sink + index->search(query.prefix, limit).movie_ids.size();
            }
        }
    }
}

// One Fisher-Yates permutation per pass, shared by both structures in that pass.
void shuffleOrder(std::vector<std::size_t>& order, std::mt19937& rng) {
    for (std::size_t i = order.size(); i > 1; --i) {
        std::swap(order[i - 1], order[rng() % i]);
    }
}

void recordMismatch(BenchmarkReport& report, std::string compared, const BenchmarkQuery& query,
                    std::size_t limit, const std::vector<MovieId>& first,
                    const std::vector<MovieId>& second, std::size_t& counter) {
    auto mismatch = compareResultLists(query.prefix, limit, first, second);
    if (!mismatch) {
        return;
    }
    ++counter;
    if (report.mismatch_details.size() < kMaxMismatchDetails) {
        mismatch->compared = std::move(compared);
        report.mismatch_details.push_back(std::move(*mismatch));
    }
}

// Times every query with one limit. Only the search call sits between the clock
// reads; result checks, moves, and frees all happen outside the timed region.
void measureQueries(const IndexPair& indexes, const std::vector<BenchmarkQuery>& queries,
                    const SortedScan& scan, std::size_t limit, std::size_t passes,
                    std::mt19937& rng, BenchmarkReport& report, std::vector<QueryRecord>& records) {
    const std::size_t query_count = queries.size();
    std::array<std::vector<std::vector<double>>, 2> samples;
    samples.fill(std::vector<std::vector<double>>(query_count));
    std::array<std::vector<SearchResult>, 2> results;
    results.fill(std::vector<SearchResult>(query_count));

    std::vector<std::vector<MovieId>> expected;
    expected.reserve(query_count);
    for (const auto& query : queries) {
        expected.push_back(scan.matches(query.prefix, limit));
    }

    std::vector<std::size_t> order(query_count);
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::vector<bool> identical(query_count, true);

    for (std::size_t pass = 0; pass < passes; ++pass) {
        shuffleOrder(order, rng);
        for (std::size_t slot = 0; slot < 2; ++slot) {
            const std::size_t which = pass % 2 == 0 ? slot : 1 - slot;
            const PrefixIndex& index = *indexes[which];
            for (std::size_t q : order) {
                const auto start = Clock::now();
                SearchResult result = index.search(queries[q].prefix, limit);
                const auto stop = Clock::now();
                samples[which][q].push_back(elapsedMicroseconds(start, stop));
                results[which][q] = std::move(result);
            }
        }
        for (std::size_t q = 0; q < query_count; ++q) {
            const auto before = report.cross_mismatches;
            ++report.comparisons;
            recordMismatch(report, report.algorithms[0].name + " vs " + report.algorithms[1].name,
                           queries[q], limit, results[0][q].movie_ids, results[1][q].movie_ids,
                           report.cross_mismatches);
            identical[q] = identical[q] && before == report.cross_mismatches;
            for (std::size_t which = 0; which < 2; ++which) {
                recordMismatch(report, report.algorithms[which].name + " vs sorted scan",
                               queries[q], limit, results[which][q].movie_ids, expected[q],
                               report.algorithms[which].scan_mismatches);
            }
        }
    }

    for (std::size_t q = 0; q < query_count; ++q) {
        QueryRecord record;
        record.prefix = queries[q].prefix;
        record.category = queries[q].category;
        record.limit = limit;
        record.total_matches = scan.countMatches(queries[q].prefix);
        record.results_identical = identical[q];
        for (std::size_t which = 0; which < 2; ++which) {
            record.latency[which] = summarizeLatencies(samples[which][q]);
            record.nodes_visited[which] = results[which][q].nodes_visited;
            record.results[which] = results[which][q].movie_ids.size();
        }
        records.push_back(std::move(record));
    }

    // Aggregates pool every sample of every query, so all queries weigh the same.
    for (std::size_t which = 0; which < 2; ++which) {
        std::vector<double> pooled;
        pooled.reserve(query_count * passes);
        double nodes = 0;
        double returned = 0;
        for (std::size_t q = 0; q < query_count; ++q) {
            pooled.insert(pooled.end(), samples[which][q].begin(), samples[which][q].end());
            nodes += static_cast<double>(results[which][q].nodes_visited);
            returned += static_cast<double>(results[which][q].movie_ids.size());
        }
        auto& summary = limit == kNoResultCap ? report.algorithms[which].exhaustive
                                              : report.algorithms[which].limited;
        summary.latency = summarizeLatencies(std::move(pooled));
        summary.mean_nodes_visited = nodes / static_cast<double>(query_count);
        summary.mean_results = returned / static_cast<double>(query_count);
    }
}

std::vector<CategoryReport> summarizeCategories(const std::vector<QueryRecord>& records,
                                                std::size_t limit) {
    std::vector<CategoryReport> categories;
    for (auto category : {QueryCategory::Short, QueryCategory::Medium, QueryCategory::Long,
                          QueryCategory::Common, QueryCategory::Rare, QueryCategory::NoMatch}) {
        CategoryReport entry;
        entry.category = category;
        for (const auto& record : records) {
            if (record.category != category || record.limit != limit) {
                continue;
            }
            ++entry.query_count;
            entry.mean_results += static_cast<double>(record.results[0]);
            for (std::size_t which = 0; which < 2; ++which) {
                entry.mean_us[which] += record.latency[which].mean_us;
                entry.mean_nodes_visited[which] += static_cast<double>(record.nodes_visited[which]);
            }
        }
        if (entry.query_count == 0) {
            continue;
        }
        const auto count = static_cast<double>(entry.query_count);
        entry.mean_results /= count;
        for (std::size_t which = 0; which < 2; ++which) {
            entry.mean_us[which] /= count;
            entry.mean_nodes_visited[which] /= count;
        }
        categories.push_back(entry);
    }
    return categories;
}

// Reported so readers can judge how far to trust sub-microsecond samples.
void measureTimer(BenchmarkReport& report) {
    constexpr int kReadPairs = 200000;
    double smallest_step_us = std::numeric_limits<double>::max();
    double total_us = 0;
    for (int i = 0; i < kReadPairs; ++i) {
        const auto first = Clock::now();
        const auto second = Clock::now();
        const double step = elapsedMicroseconds(first, second);
        total_us += step;
        if (step > 0 && step < smallest_step_us) {
            smallest_step_us = step;
        }
    }
    report.timer_tick_ns = smallest_step_us == std::numeric_limits<double>::max() ? 0 : smallest_step_us * 1000.0;
    report.timer_overhead_ns = total_us / kReadPairs * 1000.0;
}

std::string describeMovie(const std::vector<Movie>& movies, MovieId id) {
    if (id >= movies.size()) {
        return "invalid movie ID " + std::to_string(id);
    }
    const auto& movie = movies[id];
    return "#" + std::to_string(id) + " " + movie.primary_title + " [" + movie.tconst + "]";
}

std::string limitLabel(std::size_t limit) {
    return limit == kNoResultCap ? "none" : std::to_string(limit);
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

LatencySummary summarizeLatencies(std::vector<double> samples_us) {
    LatencySummary summary;
    if (samples_us.empty()) {
        return summary;
    }
    summary.mean_us = meanOf(samples_us);
    summary.median_us = medianOf(samples_us);
    std::sort(samples_us.begin(), samples_us.end());
    // Nearest rank: the smallest sample with at least 95% of samples at or below it.
    const auto rank = (samples_us.size() * 95 + 99) / 100;
    summary.p95_us = samples_us[rank - 1];
    return summary;
}

std::optional<ResultMismatch> compareResultLists(const std::string& prefix, std::size_t limit,
                                                 const std::vector<MovieId>& first,
                                                 const std::vector<MovieId>& second) {
    if (first == second) {
        return std::nullopt;
    }
    constexpr std::size_t kContextIds = 10;
    const auto common = std::min(first.size(), second.size());
    const auto difference = static_cast<std::size_t>(
        std::mismatch(first.begin(), first.begin() + static_cast<std::ptrdiff_t>(common),
                      second.begin()).first - first.begin());
    const auto window = [&](const std::vector<MovieId>& ids) {
        const auto begin = ids.begin() + static_cast<std::ptrdiff_t>(difference);
        const auto count = std::min(kContextIds, ids.size() - difference);
        return std::vector<MovieId>(begin, begin + static_cast<std::ptrdiff_t>(count));
    };
    ResultMismatch mismatch;
    mismatch.prefix = prefix;
    mismatch.limit = limit;
    mismatch.first_count = first.size();
    mismatch.second_count = second.size();
    mismatch.first_difference = difference;
    mismatch.first_ids = window(first);
    mismatch.second_ids = window(second);
    return mismatch;
}

bool BenchmarkReport::passed() const {
    return cross_mismatches == 0 && algorithms[0].scan_mismatches == 0
        && algorithms[1].scan_mismatches == 0;
}

BenchmarkReport runIndexBenchmark(const IndexFactory& first, const IndexFactory& second,
                                  const std::vector<Movie>& movies, const BenchmarkConfig& config) {
    if (config.measured_passes == 0 || config.build_repetitions == 0) {
        throw std::runtime_error("Benchmark needs at least one build and one measured pass");
    }
    const SortedScan scan(movies);
    BenchmarkReport report;
    report.movie_count = movies.size();
    report.config = config;
    report.queries = buildWorkload(movies, scan, config.seed);
    if (report.queries.empty()) {
        throw std::runtime_error("Cannot benchmark an empty dataset: no searchable movie titles were loaded");
    }

    measureTimer(report);
    const IndexPair indexes = measureBuilds({&first, &second}, movies, config.build_repetitions, report);

    std::mt19937 order_rng(config.seed);
    warmUp(indexes, report.queries, config.result_limit, config.warmup_passes);
    measureQueries(indexes, report.queries, scan, config.result_limit, config.measured_passes,
                   order_rng, report, report.query_records);
    report.categories = summarizeCategories(report.query_records, config.result_limit);
    if (config.exhaustive_passes > 0) {
        warmUp(indexes, report.queries, kNoResultCap, 1);
        measureQueries(indexes, report.queries, scan, kNoResultCap, config.exhaustive_passes,
                       order_rng, report, report.query_records);
    }
    return report;
}

BenchmarkReport runBenchmark(const std::vector<Movie>& movies, const BenchmarkConfig& config) {
    const IndexFactory trie = [] { return std::make_unique<Trie>(); };
    const IndexFactory tst = [] { return std::make_unique<TernarySearchTree>(); };
    return runIndexBenchmark(trie, tst, movies, config);
}

void printBenchmarkReport(const BenchmarkReport& report, const std::vector<Movie>& movies,
                          std::ostream& out) {
    const auto& left = report.algorithms[0];
    const auto& right = report.algorithms[1];
    const auto& config = report.config;

    constexpr int kLabelWidth = 36;
    constexpr int kValueWidth = 22;
    const auto row = [&](const std::string& label, const std::string& a, const std::string& b) {
        out << std::left << std::setw(kLabelWidth) << label
            << std::right << std::setw(kValueWidth) << a << std::setw(kValueWidth) << b << '\n';
    };
    const auto rule = [&] {
        out << std::string(kLabelWidth + 2 * kValueWidth, '-') << '\n';
    };
    const auto latencyRows = [&](const PassSummary& a, const PassSummary& b) {
        row("Mean query time (us)", formatFixed(a.latency.mean_us, 3), formatFixed(b.latency.mean_us, 3));
        row("Median query time (us)", formatFixed(a.latency.median_us, 3), formatFixed(b.latency.median_us, 3));
        row("95th percentile query time (us)", formatFixed(a.latency.p95_us, 3), formatFixed(b.latency.p95_us, 3));
        row("Mean nodes visited per query", formatFixed(a.mean_nodes_visited, 1), formatFixed(b.mean_nodes_visited, 1));
        row("Mean results returned per query", formatFixed(a.mean_results, 1), formatFixed(b.mean_results, 1));
    };

    out << "\nBENCHMARK RESULTS\n";
    rule();
    out << "Dataset: " << formatCount(report.movie_count) << " movies\n"
        << "Workload: " << report.queries.size() << " unique prefixes, seed " << config.seed << " (";
    for (std::size_t i = 0; i < report.categories.size(); ++i) {
        out << (i == 0 ? "" : ", ") << report.categories[i].query_count << ' '
            << queryCategoryName(report.categories[i].category);
    }
    out << ")\n"
        << "Passes: " << config.warmup_passes << " warm-up (untimed), " << config.measured_passes
        << " measured at limit " << config.result_limit << ", " << config.exhaustive_passes
        << " measured with no limit\n"
        << "Timer: std::chrono::steady_clock, smallest observed step "
        << formatFixed(report.timer_tick_ns, 0) << " ns, read overhead about "
        << formatFixed(report.timer_overhead_ns, 0) << " ns per sample (not subtracted)\n";
    if (report.queries.size() < kMinimumBenchmarkQueries) {
        out << "WARNING: this dataset only produced " << report.queries.size() << " unique prefixes, below the "
            << kMinimumBenchmarkQueries << "-query target. Use the full IMDb dataset for reported results.\n";
    }

    out << '\n';
    row("Metric", left.name, right.name);
    rule();
    row("Construction time, median of " + std::to_string(config.build_repetitions) + " (ms)",
        formatFixed(left.median_build_ms, 1), formatFixed(right.median_build_ms, 1));
    row("Nodes allocated", formatCount(left.stats.node_count), formatCount(right.stats.node_count));
    row("Estimated index memory", formatMiB(left.stats.estimated_bytes), formatMiB(right.stats.estimated_bytes));
    latencyRows(left.limited, right.limited);
    row("Mismatches against sorted scan", formatCount(left.scan_mismatches), formatCount(right.scan_mismatches));
    out << "Memory is an estimate of index storage (1 MiB = 1,048,576 bytes), not process RAM.\n"
        << "Query times pool every sample of every query, so each query counts equally.\n";

    out << "\nBy query category, limit " << config.result_limit << " (means)\n";
    constexpr int kCategoryWidth = 10;
    constexpr int kNumberWidth = 12;
    out << std::left << std::setw(kCategoryWidth) << "Category" << std::right
        << std::setw(kNumberWidth) << "Queries" << std::setw(kNumberWidth) << "Results"
        << std::setw(kNumberWidth) << "Trie us" << std::setw(kNumberWidth) << "TST us"
        << std::setw(kNumberWidth) << "Trie nodes" << std::setw(kNumberWidth) << "TST nodes" << '\n';
    rule();
    for (const auto& entry : report.categories) {
        out << std::left << std::setw(kCategoryWidth) << queryCategoryName(entry.category) << std::right
            << std::setw(kNumberWidth) << entry.query_count
            << std::setw(kNumberWidth) << formatFixed(entry.mean_results, 1)
            << std::setw(kNumberWidth) << formatFixed(entry.mean_us[0], 3)
            << std::setw(kNumberWidth) << formatFixed(entry.mean_us[1], 3)
            << std::setw(kNumberWidth) << formatFixed(entry.mean_nodes_visited[0], 1)
            << std::setw(kNumberWidth) << formatFixed(entry.mean_nodes_visited[1], 1) << '\n';
    }

    if (config.exhaustive_passes > 0) {
        out << "\nSame queries with no result limit (full enumeration)\n";
        row("Metric", left.name, right.name);
        rule();
        latencyRows(left.exhaustive, right.exhaustive);
    }

    out << "\nCorrectness: compared " << formatCount(report.comparisons) << " result lists between "
        << left.name << " and " << right.name << ", found " << formatCount(report.cross_mismatches)
        << " mismatches.\n";
    if (report.passed()) {
        out << "Correctness checks: PASSED\n";
        return;
    }
    out << "Correctness checks: FAILED\n";
    for (const auto& mismatch : report.mismatch_details) {
        out << "  " << mismatch.compared << ", prefix \"" << mismatch.prefix << "\", limit "
            << limitLabel(mismatch.limit) << ": " << mismatch.first_count << " vs "
            << mismatch.second_count << " results, first difference at position "
            << mismatch.first_difference << '\n';
        const auto list = [&](const char* side, const std::vector<MovieId>& ids) {
            out << "    " << side << ":";
            if (ids.empty()) {
                out << " (no more results)";
            }
            out << '\n';
            for (MovieId id : ids) {
                out << "      " << describeMovie(movies, id) << '\n';
            }
        };
        list("first", mismatch.first_ids);
        list("second", mismatch.second_ids);
    }
    if (report.cross_mismatches + left.scan_mismatches + right.scan_mismatches > report.mismatch_details.size()) {
        out << "  (Only the first " << report.mismatch_details.size() << " mismatches are listed.)\n";
    }
}

void writeBenchmarkCsv(const BenchmarkReport& report, const std::string& path) {
    const std::filesystem::path target(path);
    if (target.has_parent_path()) {
        std::error_code error;
        std::filesystem::create_directories(target.parent_path(), error);
        if (error) {
            throw std::runtime_error("Cannot create CSV directory for " + path + ": " + error.message());
        }
    }
    std::ofstream out(target);
    if (!out) {
        throw std::runtime_error("Cannot open CSV output file: " + path);
    }
    // Prefixes are normalized to letters, digits, and spaces, so no field needs quoting.
    out << "prefix,category,limit,total_matches,algorithm,mean_us,median_us,p95_us,"
           "nodes_visited,results,results_identical\n";
    for (const auto& record : report.query_records) {
        for (std::size_t which = 0; which < 2; ++which) {
            const auto& latency = record.latency[which];
            out << record.prefix << ',' << queryCategoryName(record.category) << ','
                << limitLabel(record.limit) << ',' << record.total_matches << ','
                << report.algorithms[which].name << ',' << formatFixed(latency.mean_us, 4) << ','
                << formatFixed(latency.median_us, 4) << ',' << formatFixed(latency.p95_us, 4) << ','
                << record.nodes_visited[which] << ',' << record.results[which] << ','
                << (record.results_identical ? "true" : "false") << '\n';
        }
    }
    if (!out) {
        throw std::runtime_error("Failed while writing CSV output file: " + path);
    }
}
