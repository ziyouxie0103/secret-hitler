add_library(secret_hitler_engine
    src/game.cpp
)

target_include_directories(secret_hitler_engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(SH_BUILD_TESTS)
    include(CTest)
    enable_testing()

    target_compile_definitions(secret_hitler_engine
        PUBLIC
            SH_ENABLE_TEST_HOOKS
    )

    file(GLOB SECRET_HITLER_ENGINE_TEST_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
    )

    add_executable(secret_hitler_engine_tests
        ${SECRET_HITLER_ENGINE_TEST_SOURCES}
    )

    target_link_libraries(secret_hitler_engine_tests
        PRIVATE
            secret_hitler_engine
    )

    add_test(
        NAME secret_hitler_engine_tests_5
        COMMAND secret_hitler_engine_tests 5
    )

    add_test(
        NAME secret_hitler_engine_tests_6
        COMMAND secret_hitler_engine_tests 6
    )

    add_test(
        NAME secret_hitler_engine_tests_7
        COMMAND secret_hitler_engine_tests 7
    )

    add_test(
        NAME secret_hitler_engine_tests_8
        COMMAND secret_hitler_engine_tests 8
    )

    add_test(
        NAME secret_hitler_engine_tests_9
        COMMAND secret_hitler_engine_tests 9
    )

    add_test(
        NAME secret_hitler_engine_tests_10
        COMMAND secret_hitler_engine_tests 10
    )

    add_test(
        NAME secret_hitler_engine_tests_all
        COMMAND secret_hitler_engine_tests all
    )
endif()
