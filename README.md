# Movie Title Autocomplete

COP3530 Project 2 — Movie Search, Group 123

[Video demonstration](https://youtu.be/XtHltlB4EbI)

[Project report](Report_123.pdf)

- Aidan Quach ([aidanq06](https://github.com/aidanq06))
- Hana Elbatouty ([hana4324](https://github.com/hana4324))
- Krish Gupta ([KrishGupta25](https://github.com/KrishGupta25))

This terminal program searches movie titles by prefix and compares a Trie and a
Ternary Search Tree (TST), both implemented from scratch. It returns up to ten
movies with their release years and IMDb IDs. The benchmark compares build time,
query time, nodes visited, and estimated index memory.

## Build

Requirements: a C++17 compiler and CMake 3.20 or newer.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The full-dataset test is skipped when the IMDb file is not present. The other
tests use the included synthetic fixture.

On Windows with Visual Studio, use `cmake --build build --config Release` and
`ctest --test-dir build -C Release --output-on-failure`. The executable is
`build\Release\movie_autocomplete.exe`.

## Run with the sample data

```sh
./build/movie_autocomplete --data data/sample_movies.tsv
```

The menu lets you search with either structure, compare their results, run the
benchmark, and display dataset and index statistics.

To search both structures directly:

```sh
./build/movie_autocomplete --data data/sample_movies.tsv --search "star" --algorithm both
```

Use `--algorithm trie` or `--algorithm tst` to search one structure.

## Run with IMDb data

Download and decompress IMDb's [title basics dataset](https://datasets.imdbws.com/title.basics.tsv.gz):

```sh
mkdir -p data
curl -L --fail https://datasets.imdbws.com/title.basics.tsv.gz -o data/title.basics.tsv.gz
gzip -dc data/title.basics.tsv.gz > data/title.basics.tsv
./build/movie_autocomplete --data data/title.basics.tsv
```

The loader selects the first 150,000 eligible non-adult movies in file order by
default. Duplicate titles are kept. Missing or invalid release years are shown as
`Unknown year`. Use `--max-movies N` to select a different number.

To run the full benchmark and save per-query results:

```sh
./build/movie_autocomplete --data data/title.basics.tsv --benchmark --csv results/benchmark.csv
```

To include the IMDb data in the tests:

```sh
MOVIE_DATASET="$PWD/data/title.basics.tsv" ctest --test-dir build --output-on-failure
```

The dataset is updated daily. Record its download date and SHA-256 digest when
reporting results. Downloaded data and generated benchmark results are ignored by
Git.

## Results and limitations

The benchmark checks that both structures return the same results and compares
them with a sorted scan. For the recorded 150,000-movie experiment, all 7,150
comparisons matched. Measurements and analysis are in
[docs/ANALYSIS_NOTES.md](docs/ANALYSIS_NOTES.md).

Matching uses ASCII letters and digits; it does not perform Unicode-aware
matching. The first 150,000 eligible records favor older movies. Memory figures
estimate index storage and do not represent total process memory.

## Data source

IMDb provides the data for personal, non-commercial use under its applicable
[dataset terms](https://developer.imdb.com/non-commercial-datasets/). The
original IMDb dataset is not included in this repository.

Information courtesy of [IMDb](https://www.imdb.com). Used with permission.
