cmake_minimum_required(VERSION 3.20)
project(mai_ir_search LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(LibXml2 REQUIRED)
find_path(MONGOC_INCLUDE_DIR mongoc/mongoc.h
          PATH_SUFFIXES mongoc-2.2.2 libmongoc-1.0 REQUIRED)
find_path(BSON_INCLUDE_DIR bson/bson.h
          PATH_SUFFIXES bson-2.2.2 libbson-1.0 REQUIRED)
find_library(MONGOC_LIBRARY NAMES mongoc2 mongoc-1.0 REQUIRED)
find_library(BSON_LIBRARY NAMES bson2 bson-1.0 REQUIRED)

add_library(linguistics_core src/linguistics/core.cpp)
target_include_directories(linguistics_core PUBLIC src/linguistics)
target_compile_options(linguistics_core PRIVATE -Wall -Wextra -Werror -pedantic)

add_executable(linguistics src/linguistics/main.cpp)
target_include_directories(linguistics PRIVATE ${MONGOC_INCLUDE_DIR} ${BSON_INCLUDE_DIR})
target_link_libraries(linguistics PRIVATE linguistics_core ${MONGOC_LIBRARY}
                                             ${BSON_LIBRARY} LibXml2::LibXml2)
target_compile_options(linguistics PRIVATE -Wall -Wextra -Werror -pedantic)

add_executable(boolean_index src/boolean_index/main.cpp)
target_include_directories(boolean_index PRIVATE ${MONGOC_INCLUDE_DIR}
                                                 ${BSON_INCLUDE_DIR})
target_link_libraries(boolean_index PRIVATE ${MONGOC_LIBRARY} ${BSON_LIBRARY})
target_compile_options(boolean_index PRIVATE -Wall -Wextra -Werror -pedantic)

add_library(boolean_search_core src/boolean_search/engine.cpp)
target_include_directories(boolean_search_core PUBLIC src/boolean_search)
target_link_libraries(boolean_search_core PUBLIC linguistics_core)
target_compile_options(boolean_search_core PRIVATE -Wall -Wextra -Werror -pedantic)

add_executable(boolean_search src/boolean_search/main.cpp)
target_include_directories(boolean_search PRIVATE ${MONGOC_INCLUDE_DIR}
                                                  ${BSON_INCLUDE_DIR})
target_link_libraries(boolean_search PRIVATE boolean_search_core
                                             ${MONGOC_LIBRARY} ${BSON_LIBRARY})
target_compile_options(boolean_search PRIVATE -Wall -Wextra -Werror -pedantic)

include(CTest)
if(BUILD_TESTING)
    add_executable(linguistics_tests tests/linguistics/test_core.cpp)
    target_include_directories(linguistics_tests PRIVATE ${MONGOC_INCLUDE_DIR}
                                                         ${BSON_INCLUDE_DIR})
    target_link_libraries(linguistics_tests PRIVATE linguistics_core
                                                   ${MONGOC_LIBRARY}
                                                   ${BSON_LIBRARY}
                                                   LibXml2::LibXml2)
    target_compile_options(linguistics_tests PRIVATE -Wall -Wextra -Werror
                                                     -pedantic
                                                     -Wno-unused-function)
    foreach(TEST_CASE tokenization morphology extraction config mandelbrot no_stl
                      recovery)
        add_test(NAME linguistics_${TEST_CASE}
                 COMMAND linguistics_tests ${TEST_CASE})
        set_tests_properties(linguistics_${TEST_CASE} PROPERTIES
                             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    endforeach()

    add_executable(boolean_search_tests tests/boolean_search/test_engine.cpp)
    target_link_libraries(boolean_search_tests PRIVATE boolean_search_core)
    target_compile_options(boolean_search_tests PRIVATE -Wall -Wextra -Werror
                                                        -pedantic)
    add_test(NAME boolean_search COMMAND boolean_search_tests)
    set_tests_properties(boolean_search PROPERTIES
                         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()
