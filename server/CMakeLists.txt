if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

find_package(Boost 1.70 REQUIRED CONFIG)
find_package(Threads REQUIRED)
find_package(Crow CONFIG QUIET)
find_package(nlohmann_json CONFIG QUIET)

if(NOT Crow_FOUND OR NOT nlohmann_json_FOUND)
    if(NOT SH_USE_FETCHCONTENT)
        message(FATAL_ERROR
            "Server dependencies were not found. Install Crow and nlohmann_json "
            "or configure with -DSH_USE_FETCHCONTENT=ON to let CMake download them."
        )
    endif()

    include(FetchContent)

    if(NOT Crow_FOUND)
        set(CROW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(CROW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(CROW_BUILD_DOCS OFF CACHE BOOL "" FORCE)

        FetchContent_Declare(
            crow
            GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
            GIT_TAG v1.3.2
        )
    endif()

    if(NOT nlohmann_json_FOUND)
        FetchContent_Declare(
            nlohmann_json
            GIT_REPOSITORY https://github.com/nlohmann/json.git
            GIT_TAG v3.11.3
        )
    endif()

    if(NOT Crow_FOUND)
        FetchContent_MakeAvailable(crow)
    endif()

    if(NOT nlohmann_json_FOUND)
        FetchContent_MakeAvailable(nlohmann_json)
    endif()
endif()

add_executable(secret_hitler_server
    src/main.cpp
)

target_include_directories(secret_hitler_server
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(secret_hitler_server
    PRIVATE
        Boost::headers
        Threads::Threads
        secret_hitler_engine
        Crow::Crow
        nlohmann_json::nlohmann_json
)

target_compile_definitions(secret_hitler_server
    PRIVATE
        CROW_USE_BOOST
)
