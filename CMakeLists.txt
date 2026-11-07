cmake_minimum_required(VERSION 3.20)
project(SearchEngine LANGUAGES CXX)

find_package(ICU REQUIRED COMPONENTS uc)

add_library(search_text src/text.cpp)
target_compile_features(search_text PUBLIC cxx_std_20)
set_target_properties(search_text PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(search_text PUBLIC src)
target_link_libraries(search_text PRIVATE ICU::uc)

include(CTest)
if(BUILD_TESTING)
    add_executable(search_text_test tests/text_test.cpp)
    target_link_libraries(search_text_test PRIVATE search_text)
    add_test(NAME search_text COMMAND search_text_test)
endif()

add_library(search_config src/config.cpp src/documents.cpp)
target_compile_features(search_config PUBLIC cxx_std_20)
set_target_properties(search_config PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(search_config PUBLIC src)

add_executable(search_scan src/scan.cpp)
target_link_libraries(search_scan PRIVATE search_config)

if(BUILD_TESTING)
    add_executable(search_config_test tests/config_test.cpp)
    target_link_libraries(search_config_test PRIVATE search_config)
    add_test(NAME search_config COMMAND search_config_test)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED IMPORTED_TARGET libpqxx>=7.0)
add_library(search_database src/database.cpp)
target_compile_features(search_database PUBLIC cxx_std_20)
set_target_properties(search_database PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(search_database PUBLIC src)
target_link_libraries(search_database PUBLIC PkgConfig::PQXX search_config search_text)

find_package(Threads REQUIRED)
add_executable(search_indexer src/indexer.cpp)
target_link_libraries(search_indexer PRIVATE search_database Threads::Threads)

option(SEARCH_POSTGRES_TESTS "Run integration tests with an isolated PostgreSQL cluster" OFF)
if(BUILD_TESTING AND SEARCH_POSTGRES_TESTS)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    find_program(POSTGRES_INITDB initdb
        HINTS /usr/lib/postgresql/18/bin /usr/lib/postgresql/17/bin /usr/lib/postgresql/16/bin
        REQUIRED)
    get_filename_component(POSTGRES_BIN "${POSTGRES_INITDB}" DIRECTORY)
    add_executable(search_database_test tests/database_test.cpp)
    target_link_libraries(search_database_test PRIVATE search_database)
    add_test(NAME search_postgres COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/postgres_test.py"
        "${POSTGRES_BIN}" "$<TARGET_FILE:search_database_test>" "$<TARGET_FILE:search_indexer>")
    set_tests_properties(search_postgres PROPERTIES TIMEOUT 90)
endif()
