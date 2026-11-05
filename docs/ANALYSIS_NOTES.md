# Analysis notes

Material for the final report and presentation. Every number here came from running the
code. Interpretations that were not directly tested are marked **Hypothesis**.

- [Short version](#short-version)
- [Why these structures](#why-these-structures)
- [Theoretical behavior](#theoretical-behavior)
- [Method](#method)
- [Measured results](#measured-results)
- [What the results show](#what-the-results-show)
- [Did the benchmark support our expectations?](#did-the-benchmark-support-our-expectations)
- [Threats to validity](#threats-to-validity)
- [Validation](#validation)
- [Changes to Phase 1](#changes-to-phase-1)
- [Test coverage map](#test-coverage-map)

## Short version

- Both structures returned identical results for all 7,150 compared result lists per
  benchmark run, and both matched an independent sorted scan.
- They have the same number of nodes, apart from the Trie's empty root. The memory and
  speed differences come from how each node is laid out, not from how many there are.
- In this implementation the TST built faster, used slightly less estimated memory, and
  answered bounded queries faster, even though it visited more nodes per query.
- Queries with no match were close to a tie, even though the TST visited about three times as many nodes.
- This compares **our** Trie, which uses compact sorted child vectors, with our TST. It
  does not show that TSTs beat Tries in general. A Trie with a fixed child array would
  be a different trade-off, and we did not measure one.

## Why these structures

**Why Tries suit prefix search.** A Trie stores shared prefixes once, so all titles
starting with a prefix sit under one node. Finding that node depends on the prefix length
rather than the number of titles, and walking its subtree in order gives results in sorted
order without a separate sort step.

**Why a TST is a fair alternative.** It offers the same prefix navigation and sorted
traversal, but replaces each node's child collection with a small binary search tree made
of the nodes themselves. A node's cost no longer depends on how many children it has. The
trade is that reaching a character can take several node visits instead of one lookup.

## Theoretical behavior

Let `L` be the key or prefix length, `A` the alphabet size (at most 37 here: a-z, 0-9,
and space), `S` the number of nodes in the walked subtree, and `R` the number of IDs returned.

| Operation | Our Trie | Our TST |
| --- | --- | --- |
| Find a child | Binary search: `O(log A)` comparisons | Walk siblings: up to `O(A)` nodes, depending on insertion order |
| Locate a prefix | `O(L log A)` | `O(L)` middle steps plus sibling passes; worst case `O(L * A)` |
| Enumerate results | `O(S + R)`, stopping early at the limit | `O(S + R)`, stopping early at the limit |
| Insert a title | `O(L log A)`, plus shifting a sorted vector when adding an edge | Same as locating, plus appending new nodes |
| Sort duplicates at terminals | `O(k log k)` per terminal with `k` movies | Same, using the same comparator |

Both search functions also normalize the raw query, and the cost is the same for both.

**Insertion order** changes the TST's shape but not the Trie's. Sibling links form an
unbalanced binary search tree in first-seen order, so sorted input would produce long
right chains. With `A` at most 37 the damage per position is bounded, and results never change
(`testInsertionOrderDoesNotChangeResults` shuffles the input and verifies this). We
inserted in file order for both structures and did not measure sorted input.

**Why results must be identical even when efficiency differs.** Both are indexes over the
same data answering the same question, so any difference in results means a bug, not a
trade-off. Speed comparisons are only meaningful if both are doing the same job. Identical
results follow from three shared rules: one normalization function, lexicographic traversal
where a complete title precedes its extensions, and one comparator for duplicates at a terminal.

## Method

The README's [Benchmark section](../README.md#benchmark) describes the procedure. Decisions
worth defending in the report:

- **Independent correctness check.** Comparing the two structures alone would miss a bug
  they share, so each is also checked against a sorted scan that uses neither. A test proves
  this catches a shared fault that a direct comparison misses.
- **Query timing.** Only the `search()` call is timed. It includes normalizing the prefix
  and allocating the result, which are the same for both. Checks and frees happen afterward.
- **Order effects.** Query order is reshuffled each pass, and the structure measured first
  alternates, so neither always benefits from running second.
- **Two limits.** A limit of ten is what a user sees, but it hides the cost of walking large
  subtrees. The no-limit passes expose that cost.
- **Platform-independent queries.** `std::uniform_int_distribution` and `std::shuffle` differ
  between standard libraries, so we use raw `mt19937` output. Windows and Linux runs produced
  identical prefixes, node visits, and result counts in all 520 CSV rows we compared.

## Measured results

**Dataset.** IMDb `title.basics.tsv.gz`, downloaded 2026-11-04T22:57:51Z, SHA-256
`b21a800d45e56eb07f8f8eb87053c63e828b71532cb1b31948c426acd3562c13`. The loader scanned
270,772 rows, found 0 malformed, and selected exactly 150,000 of the 750,796 eligible
movies, from `tt0000009` to `tt0283092`. A separate Python script that parsed the raw bytes on
its own reproduced the whole selection: all 150,000 IDs, years, and normalized titles, in the same order.

The selection has 136,509 distinct normalized titles, 8,976 of them shared by more than one
movie (`hamlet` has 20). Keys average 17.4 characters and the longest is 137. 86 movies have
no year. Years run 1894 to 2026 with a median of 1968.

**Machine.** Intel Core Ultra 9 185H, 32 GB of RAM, Windows 11 Pro, running on a laptop with
no isolation from other processes.

### Construction, memory, and bounded queries (limit 10)

Each cell gives Trie / TST. Windows and Linux GCC list three runs; the others list one.
Query times are in microseconds.

| Toolchain | Construction (ms) | Mean | Median | 95th percentile |
| --- | --- | --- | --- | --- |
| Windows, GCC 13.1, `-O3` | 366.4 / 229.2 | 3.940 / 2.797 | 3.700 / 2.600 | 8.400 / 6.200 |
| | 380.4 / 226.9 | 4.759 / 2.937 | 4.100 / 2.600 | 10.700 / 6.200 |
| | 391.9 / 230.6 | 3.943 / 2.574 | 3.700 / 2.400 | 8.400 / 5.200 |
| WSL2 Ubuntu, GCC 15.2, `-O2` | 346.1 / 264.9 | 3.501 / 2.547 | 3.063 / 2.242 | 7.639 / 5.455 |
| | 358.6 / 280.0 | 3.389 / 2.850 | 2.915 / 2.082 | 7.046 / 5.017 |
| | 355.5 / 277.4 | 3.528 / 2.563 | 2.904 / 2.079 | 8.112 / 5.217 |
| WSL2 Ubuntu, Clang 21.1, `-O2` | 344.3 / 292.9 | 3.424 / 2.688 | 2.907 / 2.186 | 7.385 / 5.040 |
| Windows, MSVC 19.44, Release | 457.4 / 287.8 | 5.232 / 4.648 | 4.600 / 4.200 | not recorded |

Do not compare times between rows. The optimization flags differ, and the clocks have different
resolutions. Compare the Trie and TST within a row.

Node counts and visits are the same on every platform: 1,395,441 Trie nodes, 1,395,440 TST
nodes, and a mean of 66.9 Trie / 75.8 TST nodes visited per query, returning a mean of 6.0 results.

### By query category (limit 10, Windows GCC, first run)

| Category | Queries | Mean results | Trie us | TST us | Trie nodes | TST nodes |
| --- | --- | --- | --- | --- | --- | --- |
| short | 20 | 10.0 | 6.345 | 4.233 | 127.5 | 134.8 |
| medium | 30 | 7.6 | 4.647 | 3.343 | 74.1 | 83.6 |
| long | 30 | 5.7 | 3.687 | 2.682 | 58.3 | 67.8 |
| common | 15 | 10.0 | 6.298 | 4.104 | 113.1 | 120.6 |
| rare | 15 | 1.7 | 1.874 | 1.514 | 24.8 | 33.3 |
| no-match | 20 | 0.0 | 0.636 | 0.698 | 5.4 | 15.2 |

### Full enumeration (no limit)

Mean of 1,239.7 results per query. Each cell gives Trie / TST, in microseconds.

| Toolchain | Mean | Median | 95th percentile | Mean nodes visited |
| --- | --- | --- | --- | --- |
| Windows, GCC 13.1 | 1145.8 / 417.2 | 13.2 / 7.5 | 6032 / 2066 | 10,332.4 / 10,339.7 |
| | 1199.1 / 408.0 | 14.1 / 7.3 | 6421 / 1990 | |
| | 1129.5 / 411.7 | 13.0 / 7.2 | 6512 / 1940 | |
| WSL2 Ubuntu, GCC 15.2 | 665.5 / 463.6 | 9.4 / 7.7 | 3232 / 2193 | same |
| | 687.9 / 470.0 | 9.6 / 7.4 | 3332 / 2070 | |
| | 688.8 / 455.6 | 9.9 / 7.2 | 3463 / 2138 | |
| WSL2 Ubuntu, Clang 21.1 | 667.1 / 500.4 | 9.3 / 7.7 | 3349 / 2836 | same |
| Windows, MSVC 19.44 | 1149.4 / 584.0 | 15.4 / 10.2 | not recorded | same |

### Memory breakdown

The totals on GCC's libstdc++ are 124,781,272 bytes for the Trie (119.00 MiB) and 118,662,440
bytes for the TST (113.17 MiB). They break down as follows, assuming 48-byte Trie nodes and
56-byte TST nodes on this 64-bit platform:

| Component | Trie | TST |
| --- | --- | --- |
| Node vector, capacity 2,097,152 slots | 100,663,296 | 117,440,512 |
| Child edge vectors (16 bytes per edge, about 2.5% slack) | 22,896,048 | none |
| Terminal ID vectors | 1,221,928 | 1,221,928 |
| **Total** | **124,781,272** | **118,662,440** |

Vector doubling leaves 33.5% of the node slots empty, which is 27% of the Trie's total and 33% of
the TST's. The same data gives 94.68 / 85.25 MiB under MSVC, whose vectors grow by a smaller factor.

Without that slack (arithmetic, not a measured run), the node vectors would hold only what they
need, and the totals would be about 86.9 MiB for the Trie and 75.7 MiB for the TST.

## What the results show

**Node count is not the difference.** Each TST node stands for exactly one distinct nonempty
prefix, as does each non-root Trie node, so the counts always differ by one. Tests assert this
on the fixture, on random data, and on the real data. Claims that Tries allocate more nodes
than TSTs do not apply to this design.

**Why our node representations affect memory.** Per prefix, the Trie pays 48 bytes for the node
plus about 16 for the edge pointing to it, and the TST pays 56 bytes. The margin is small, and
growth slack can hide much of it: 4.9% with doubling and about 13% with exact-fit node vectors.
Three design choices drive this:

- Sorted child vectors keep the Trie compact. A textbook Trie with 37 eight-byte child slots per
  node would need about 320 bytes per node, or about 426 MiB here (arithmetic, not measured).
- We used 64-bit links in both structures so the comparison reflects structure rather than link
  width. With 32-bit links both would shrink, to about 40 bytes for a TST node.
- Each TST node stores an empty 24-byte terminal vector even though only about 10% of nodes hold
  movies, and the Trie has the same cost. Both could save memory by moving terminals elsewhere.

**How prefix length affects visited nodes.** It is not simply "longer means more work". With a limit
of ten, short prefixes visited the *most* nodes (127.5 Trie) and long ones far fewer (58.3), because
a short prefix needs little descent but must walk further into a big subtree to collect ten titles,
each about 17 characters deep. Prefix length mostly determines how the work is split between descent
and enumeration.

**How the number of matches affects enumeration.** The limit decides this. With a limit of ten, cost
is bounded no matter how many titles match: `the` has 22,415 matches and visited 145 Trie / 149 TST
nodes. Without a limit, the mean query visits over 10,000 nodes and costs hundreds of times more. The
mean is pulled up by a few huge prefixes such as `t` (29,061 matches), which is why the unlimited median
stays around 7 to 15 microseconds while the mean reaches 400 to 1,200.

**What the visit counts do and do not mean.** Both structures visit the same subtree when enumerating
everything, so the no-limit difference (10,339.7 vs 10,332.4) is purely descent. With no match, the TST's
15.2 visits against the Trie's 5.4 show the sibling walking. But visits are not equal units of work: the
Trie's binary search over a child vector counts as one visit however many comparisons it takes. So fewer
visits did not translate into less time, and the TST was faster despite visiting more nodes.

**Why the TST was faster here.** **Hypothesis:** memory layout. Each Trie step reads the node, then
follows a pointer to a separately allocated child vector, which is a likely extra cache miss. A TST step
finds its links inside the node it already read. Two observations are consistent with this, but neither
proves it: time per visited node was lower for the TST (about 37 vs 59 ns in the first Windows run), and
the Trie's disadvantage in full enumeration depended heavily on the runtime (2.7x to 2.9x on Windows GCC,
2.0x on MSVC, 1.3x to 1.5x on Linux glibc), which suggests heap placement matters. Construction points the same way, since the Trie makes a separate allocation for each node's
child vector. We did not collect cache counters. Running `perf stat` on Linux, or testing a Trie variant
with children stored contiguously, would test this directly.

**No-match queries were close to a tie.** Across the six GCC runs the Trie's mean ranged from 9% below
the TST's to 17% above it, so the winner changed between runs. These queries are mostly descent, where the Trie's fewer visits and
the TST's cheaper visits roughly cancel out. They are also the fastest queries and so the ones most
affected by clock resolution.

## Did the benchmark support our expectations?

The proposal is not in this repository, so this section compares against the usual textbook expectation:
**replace this with the wording from our proposal.** That expectation is that Tries are faster to query
but use more memory, and that TSTs trade speed for space.

| Expectation | Result here |
| --- | --- |
| The TST uses less memory | Supported, but narrowly (4.9% to 10% by estimate) and sensitive to vector growth |
| The Trie answers queries faster | **Not supported** for this implementation: TST medians were 25 to 37% lower on GCC and Clang, and 9% lower in the one MSVC run |
| The Trie visits fewer nodes | Supported (66.9 vs 75.8), but this did not mean less time |
| Both return identical results | Supported: 0 mismatches on every platform |

The likely reason speed went against expectation is that the textbook argument assumes constant-time
child lookup through an array. Our Trie gave that up for memory, using a binary search over a separate
heap vector. A fair report sentence would be: *"Our sparse-vector Trie did not show the query speed
advantage usually attributed to Tries; we did not test an array-based Trie, which would likely
trade much more memory for faster lookups."*

## Threats to validity

- **One machine, not isolated.** A laptop running other software. Differences between Trie and TST
  repeated across runs and toolchains, but absolute times varied by 20% between runs.
- **Means are noisy.** In one Linux run the `long` and `rare` category means flipped in the Trie's favor,
  while the overall medians held steady across all runs. Prefer medians, and do not build conclusions on
  one category.
- **Clock resolution.** The step is 100 ns on Windows and 31 ns under WSL2, with read overhead of about
  40 to 60 ns not subtracted. That is minor against a 3 us query, but not against a 0.6 us one.
- **The sample is old.** About 96% of the selected movies predate 2000. Results could change on a different
  title distribution, such as one with more non-English or longer titles.
- **The workload is ours.** Real autocomplete traffic is skewed toward popular prefixes and retyped
  keystrokes. We report categories so readers can see the mix instead of relying on one average.
- **Insertion order was not varied.** Sorted input could slow the TST, and we did not measure it.
- **The Trie uses more allocations.** This is a real property of our design, but it makes the results
  depend on the allocator, as the Windows and Linux differences show.
- **Memory is estimated.** It counts reserved capacity and excludes allocator overhead, which would add
  more to the Trie because of its many small allocations. We did not measure process memory.

## Validation

Run on 2026-11-04 from this branch. Check counts are from the final tree.

| Environment | Build | What ran | Result |
| --- | --- | --- | --- |
| Windows, MinGW GCC 13.1, CMake | Release, Debug | Full CTest, including the real 150,000 records | All 6 pass, 0 warnings |
| Windows, MSVC 19.44, CMake | Release, `/W4` | Full CTest, including the real 150,000 records | All 6 pass |
| WSL2 Ubuntu, GCC 15.2, direct compile | `-O2` | 5 test programs, 13 CLI commands, benchmark | All pass, 0 warnings |
| WSL2 Ubuntu, Clang 21.1, direct compile | `-O2` | 5 test programs, 13 CLI commands, benchmark | All pass, 0 warnings |
| WSL2 Ubuntu, GCC and Clang, direct compile | ASan + UBSan | 5 test programs on real data, 13 CLI commands | All pass, no sanitizer reports |

Test program check counts: `core` 450, `tst` 88, `indexes` 3,669, `benchmark` 858, and `full_dataset`
900,407 on the real data.

**Not validated, and worth doing before submission:**

- **macOS has not run Phase 2.** Phase 1 was tested on macOS by Aidan, but this code has not been.
  Please run the Quick start commands there.
- **CMake and `ctest` have not run on Linux.** CMake was not installed in the WSL environment, so
  the tests were compiled directly. As a result, `tests/test_cli.cmake` has only run on Windows.
  The 13 manual commands cover most of its paths on Linux, but not all of them.

We also confirmed the tests can fail. Six deliberate TST bugs were each caught: wrong traversal order,
no terminal sorting, skipping the endpoint's movies, leaking an endpoint sibling, ignoring the limit at
a terminal, and swapping left and right on insertion.

## Changes to Phase 1

The Trie, loader, normalizer, comparator, and shared interfaces are **unchanged**. No correctness
bugs were found in them. The handoff's stated values were all reproduced.

| Change | Reason |
| --- | --- |
| `src/main.cpp`: `--data` with no action now opens the menu instead of printing a hint | The Phase 2 brief makes interactive mode the default. `--search`, `--max-movies`, and `--help` keep their behavior, and default `--search` output is unchanged |
| `tests/test_cli.cmake`: `--max-movies 0` now expects `nothing to search` instead of `Supply --search PREFIX` | Follows from the change above. All other Phase 1 assertions are untouched |
| `CMakeLists.txt`: warning flags now apply to every target | Previously only `movie_core` |
| `.gitignore`: added `/results/` | Benchmark output |

The brief and the handoff differ on some names, and we followed the brief: `include/tst.hpp` and
`src/tst.cpp` rather than `ternary_search_tree.*`, and the branch `feature/krish-tst-integration` rather
than `feature/krish-tst-comparison`. The class name `TernarySearchTree` matches both.

## Test coverage map

| # | Requirement | Where |
| --- | --- | --- |
| 1, 2 | Same results for simple and long prefixes | `testSimpleAndLongPrefixes`, `test_indexes.cpp` |
| 3 | Nonexistent prefixes | `testNonexistentPrefixes` |
| 4 | Empty prefixes | `testEmptyPrefixes` |
| 5, 6 | Uppercase, punctuation, and extra spaces | `testCaseAndPunctuation` |
| 7, 9 | Duplicate titles and identical order | `testDuplicatesAndOrder` |
| 8 | A title that prefixes longer titles | `testTitleThatPrefixesLongerTitles` |
| 10, 16 | Ten-result limit, more than ten matches | `testResultLimit` |
| 11 | Missing years | `testMissingYears` |
| 12 | Small fixture | `testSmallFixtureEveryPrefix` tries every prefix of every title |
| 13 | Repeated queries | `testRepeatedQueries` |
| 14 | Rebuilding | `testRebuilding` |
| 15 | Sensible statistics | `testStatistics` |
| 17 | Benchmark detects wrong results | `testBenchmarkDetectsMismatches`, `test_benchmark.cpp` |
| 18 | Invalid dataset paths | `tests/test_cli.cmake`, in every mode |
| 19 | Real 150,000 records | `test_full_dataset.cpp` |
| 20 | Benchmark queries match across structures | `testWorkloadMatchesAcrossStructures` |
| Extra | TST left, middle, and right branching | `testLeftMiddleRightBranching`, `test_tst.cpp` |
| Extra | Randomized comparison, 150,000 generated records | `testRandomizedAgainstScan`, `testGenerated150000Records` |
