cmake_minimum_required(VERSION 3.16)
project(movie_title_autocomplete VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

function(enable_project_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(movie_core src/normalize.cpp src/imdb_loader.cpp src/trie.cpp src/tst.cpp)
target_include_directories(movie_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
enable_project_warnings(movie_core)

add_executable(movie_autocomplete src/main.cpp)
target_link_libraries(movie_autocomplete PRIVATE movie_core)
enable_project_warnings(movie_autocomplete)

include(CTest)
if(BUILD_TESTING)
    function(add_project_test test_name source)
        add_executable(test_${test_name} ${source})
        target_link_libraries(test_${test_name} PRIVATE movie_core)
        target_compile_definitions(test_${test_name} PRIVATE
            TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
        enable_project_warnings(test_${test_name})
        add_test(NAME ${test_name} COMMAND test_${test_name})
    endfunction()

    add_project_test(core tests/test_core.cpp)
    add_project_test(tst tests/test_tst.cpp)
    add_project_test(indexes tests/test_indexes.cpp)

    add_test(NAME cli COMMAND ${CMAKE_COMMAND}
        "-DAPP=$<TARGET_FILE:movie_autocomplete>"
        "-DDATA=${CMAKE_CURRENT_SOURCE_DIR}/data/sample_movies.tsv"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cli.cmake)
endif()
