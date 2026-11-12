cmake_minimum_required(VERSION 3.20)
project(mai_ir_search LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
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

add_library(linguistics_pipeline
            src/linguistics/language/pipeline.cpp
            src/linguistics/language/tokenizer.cpp
            src/linguistics/language/lemmatizer.cpp)
target_include_directories(linguistics_pipeline PUBLIC src)
target_compile_options(linguistics_pipeline PRIVATE
                       -Wall -Wextra -Werror -pedantic)

add_library(linguistics_application
            src/linguistics/app/application.cpp
            src/linguistics/app/config.cpp
            src/linguistics/processing/document_processor.cpp
            src/linguistics/processing/html_extractor.cpp
            src/linguistics/storage/mongo_storage.cpp
            src/linguistics/reporting/report.cpp
            src/linguistics/reporting/report_writer.cpp
            src/linguistics/reporting/statistics.cpp)
target_include_directories(linguistics_application
                           PUBLIC src
                           PRIVATE ${MONGOC_INCLUDE_DIR} ${BSON_INCLUDE_DIR})
target_link_libraries(linguistics_application
                      PUBLIC linguistics_pipeline ${MONGOC_LIBRARY}
                             ${BSON_LIBRARY} LibXml2::LibXml2)
target_compile_options(linguistics_application PRIVATE
                       -Wall -Wextra -Werror -pedantic)

add_executable(linguistics src/linguistics/app/main.cpp)
target_link_libraries(linguistics PRIVATE linguistics_application)
target_compile_options(linguistics PRIVATE -Wall -Wextra -Werror -pedantic)

add_library(index_compression
            src/boolean_index/compression/variable_byte.cpp)
target_include_directories(index_compression PUBLIC src)
target_compile_options(index_compression PRIVATE
                       -Wall -Wextra -Werror -pedantic)

add_library(tfidf_weights
            src/tfidf_search/ranking/weights.cpp)
target_include_directories(tfidf_weights PUBLIC src)
target_compile_options(tfidf_weights PRIVATE
                       -Wall -Wextra -Werror -pedantic)

add_library(boolean_index_core
            src/boolean_index/app/config.cpp
            src/boolean_index/building/blocks.cpp
            src/boolean_index/building/builder.cpp
            src/boolean_index/storage/files.cpp
            src/boolean_index/storage/index_writer.cpp)
target_include_directories(boolean_index_core
                           PUBLIC src
                                  ${MONGOC_INCLUDE_DIR} ${BSON_INCLUDE_DIR})
target_link_libraries(boolean_index_core
                      PUBLIC index_compression tfidf_weights
                      PRIVATE ${MONGOC_LIBRARY} ${BSON_LIBRARY})
target_compile_options(boolean_index_core PRIVATE
                       -Wall -Wextra -Werror -pedantic)

add_executable(boolean_index src/boolean_index/app/main.cpp)
target_link_libraries(boolean_index PRIVATE boolean_index_core)
target_compile_options(boolean_index PRIVATE -Wall -Wextra -Werror -pedantic)

add_library(boolean_search_core
            src/boolean_search/index/index_reader.cpp
            src/boolean_search/query/posting_operations.cpp
            src/boolean_search/query/query_parser.cpp)
target_include_directories(boolean_search_core PUBLIC src)
target_link_libraries(boolean_search_core PUBLIC linguistics_pipeline
                                                 index_compression)
target_compile_options(boolean_search_core PRIVATE -Wall -Wextra -Werror -pedantic)

add_executable(boolean_search
               src/boolean_search/app/main.cpp
               src/boolean_search/app/config.cpp
               src/boolean_search/app/console.cpp)
target_include_directories(boolean_search PRIVATE ${MONGOC_INCLUDE_DIR}
                                                  ${BSON_INCLUDE_DIR})
target_link_libraries(boolean_search PRIVATE boolean_search_core
                                             ${MONGOC_LIBRARY} ${BSON_LIBRARY})
target_compile_options(boolean_search PRIVATE -Wall -Wextra -Werror -pedantic)

add_library(tfidf_search_core
            src/tfidf_search/index/ranking_index.cpp
            src/tfidf_search/ranking/search.cpp)
target_include_directories(tfidf_search_core PUBLIC src)
target_link_libraries(tfidf_search_core PUBLIC boolean_search_core
                                               tfidf_weights)
target_compile_options(tfidf_search_core PRIVATE
                       -Wall -Wextra -Werror -pedantic)

add_executable(tfidf_search
               src/tfidf_search/app/main.cpp
               src/tfidf_search/app/console.cpp
               src/boolean_search/app/config.cpp)
target_include_directories(tfidf_search PRIVATE ${MONGOC_INCLUDE_DIR}
                                                ${BSON_INCLUDE_DIR})
target_link_libraries(tfidf_search PRIVATE tfidf_search_core
                                           ${MONGOC_LIBRARY} ${BSON_LIBRARY})
target_compile_options(tfidf_search PRIVATE -Wall -Wextra -Werror -pedantic)

include(CTest)
if(BUILD_TESTING)
    find_package(GTest QUIET)
    if(NOT GTest_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.18.0.tar.gz
            URL_HASH SHA256=6e3191c1455468b3fc35a417fb565c1c5071aee1b7e7f85e30cf48a98d37d8b5
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()
    include(GoogleTest)

    add_executable(tokenizer_unit_tests tests/unit/test_tokenizer.cpp)
    target_link_libraries(tokenizer_unit_tests PRIVATE linguistics_pipeline
                                                       GTest::gtest_main)
    target_compile_options(tokenizer_unit_tests PRIVATE -Wall -Wextra -Werror
                                                        -pedantic)
    gtest_discover_tests(tokenizer_unit_tests
                         TEST_PREFIX "unit.tokenizer."
                         PROPERTIES LABELS "gtest\;unit")

    add_executable(lemmatization_unit_tests
                   tests/unit/test_lemmatization.cpp)
    target_link_libraries(lemmatization_unit_tests PRIVATE linguistics_pipeline
                                                           GTest::gtest_main)
    target_compile_definitions(lemmatization_unit_tests PRIVATE
                               MAI_IR_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_compile_options(lemmatization_unit_tests PRIVATE -Wall -Wextra -Werror
                                                            -pedantic)
    gtest_discover_tests(lemmatization_unit_tests
                         TEST_PREFIX "unit.lemmatization."
                         PROPERTIES LABELS "gtest\;unit")

    add_executable(variable_byte_unit_tests
                   tests/unit/test_variable_byte.cpp)
    target_link_libraries(variable_byte_unit_tests PRIVATE index_compression
                                                           GTest::gtest_main)
    target_compile_options(variable_byte_unit_tests PRIVATE
                           -Wall -Wextra -Werror -pedantic)
    gtest_discover_tests(variable_byte_unit_tests
                         TEST_PREFIX "unit.variable_byte."
                         PROPERTIES LABELS "gtest\;unit")

    add_executable(html_extraction_unit_tests
                   tests/unit/test_html_extraction.cpp)
    target_include_directories(html_extraction_unit_tests PRIVATE
                               ${MONGOC_INCLUDE_DIR} ${BSON_INCLUDE_DIR})
    target_link_libraries(html_extraction_unit_tests PRIVATE
                          linguistics_application GTest::gtest_main)
    target_compile_definitions(html_extraction_unit_tests PRIVATE
                               MAI_IR_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_compile_options(html_extraction_unit_tests PRIVATE
                           -Wall -Wextra -Werror -pedantic
                           -Wno-unused-function)
    gtest_discover_tests(html_extraction_unit_tests
                         TEST_PREFIX "unit.html_extraction."
                         PROPERTIES LABELS "gtest\;unit")

    add_executable(config_unit_tests tests/unit/test_config.cpp)
    target_include_directories(config_unit_tests PRIVATE
                               ${MONGOC_INCLUDE_DIR} ${BSON_INCLUDE_DIR})
    target_link_libraries(config_unit_tests PRIVATE linguistics_application
                                                    GTest::gtest_main)
    target_compile_definitions(config_unit_tests PRIVATE
                               MAI_IR_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_compile_options(config_unit_tests PRIVATE
                           -Wall -Wextra -Werror -pedantic
                           -Wno-unused-function)
    gtest_discover_tests(config_unit_tests
                         TEST_PREFIX "unit.config."
                         PROPERTIES LABELS "gtest\;unit")

    add_executable(boolean_index_unit_tests
                   tests/unit/test_boolean_index.cpp)
    target_link_libraries(boolean_index_unit_tests PRIVATE
                          boolean_index_core GTest::gtest_main)
    target_compile_options(boolean_index_unit_tests PRIVATE -Wall -Wextra -Werror
                                                           -pedantic)
    gtest_discover_tests(boolean_index_unit_tests
                         TEST_PREFIX "unit.boolean_index."
                         PROPERTIES LABELS "gtest\;unit")

    add_executable(boolean_search_unit_tests
                   tests/unit/test_boolean_search.cpp)
    target_link_libraries(boolean_search_unit_tests PRIVATE boolean_search_core
                                                           GTest::gtest_main)
    target_compile_definitions(boolean_search_unit_tests PRIVATE
                               MAI_IR_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_compile_options(boolean_search_unit_tests PRIVATE -Wall -Wextra -Werror
                                                            -pedantic)
    gtest_discover_tests(boolean_search_unit_tests
                         TEST_PREFIX "unit.boolean_search."
                         PROPERTIES LABELS "gtest\;unit")

    add_executable(tfidf_search_unit_tests
                   tests/unit/test_tfidf_search.cpp)
    target_link_libraries(tfidf_search_unit_tests PRIVATE tfidf_search_core
                                                         GTest::gtest_main)
    target_compile_definitions(tfidf_search_unit_tests PRIVATE
                               MAI_IR_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_compile_options(tfidf_search_unit_tests PRIVATE
                           -Wall -Wextra -Werror -pedantic)
    gtest_discover_tests(tfidf_search_unit_tests
                         TEST_PREFIX "unit.tfidf_search."
                         PROPERTIES LABELS "gtest\;unit")

    add_executable(document_processing_integration_tests
                   tests/integration/test_document_processing.cpp)
    target_include_directories(document_processing_integration_tests PRIVATE
                               ${MONGOC_INCLUDE_DIR} ${BSON_INCLUDE_DIR})
    target_link_libraries(document_processing_integration_tests PRIVATE
                          linguistics_application GTest::gtest_main)
    target_compile_definitions(document_processing_integration_tests PRIVATE
                               MAI_IR_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_compile_options(document_processing_integration_tests PRIVATE
                           -Wall -Wextra -Werror -pedantic
                           -Wno-unused-function)
    gtest_discover_tests(document_processing_integration_tests
                         TEST_PREFIX "integration.document_processing."
                         PROPERTIES LABELS "gtest\;integration")

    add_executable(recovery_integration_tests
                   tests/integration/test_recovery.cpp)
    target_link_libraries(recovery_integration_tests PRIVATE GTest::gtest_main)
    target_compile_definitions(recovery_integration_tests PRIVATE
                               MAI_IR_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_compile_options(recovery_integration_tests PRIVATE
                           -Wall -Wextra -Werror -pedantic)
    gtest_discover_tests(recovery_integration_tests
                         TEST_PREFIX "integration.recovery."
                         PROPERTIES LABELS "gtest\;integration")

endif()
