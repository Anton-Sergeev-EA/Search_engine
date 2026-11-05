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
