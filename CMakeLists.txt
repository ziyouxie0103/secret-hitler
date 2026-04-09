cmake_minimum_required(VERSION 3.20)
project(secret_hitler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SH_BUILD_SERVER "Build the HTTP/WebSocket server" ON)
option(SH_BUILD_TESTS "Build engine tests" ON)
option(SH_USE_FETCHCONTENT "Allow CMake to download missing dependencies" ON)

if(SH_BUILD_TESTS)
    include(CTest)
    enable_testing()
endif()

add_subdirectory(engine)

if(SH_BUILD_SERVER)
    add_subdirectory(server)
endif()
