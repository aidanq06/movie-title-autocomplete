cmake_minimum_required(VERSION 3.16)
project(movie_title_autocomplete VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(movie_core src/normalize.cpp src/imdb_loader.cpp src/trie.cpp)
target_include_directories(movie_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(MSVC)
    target_compile_options(movie_core PRIVATE /W4)
else()
    target_compile_options(movie_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(movie_autocomplete src/main.cpp)
target_link_libraries(movie_autocomplete PRIVATE movie_core)

include(CTest)
if(BUILD_TESTING)
    add_executable(test_core tests/test_core.cpp)
    target_link_libraries(test_core PRIVATE movie_core)
    target_compile_definitions(test_core PRIVATE
        TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
    add_test(NAME core COMMAND test_core)
    add_test(NAME cli COMMAND ${CMAKE_COMMAND}
        "-DAPP=$<TARGET_FILE:movie_autocomplete>"
        "-DDATA=${CMAKE_CURRENT_SOURCE_DIR}/data/sample_movies.tsv"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cli.cmake)
endif()
