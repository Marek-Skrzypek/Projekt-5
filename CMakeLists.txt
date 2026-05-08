cmake_minimum_required(VERSION 3.16)
project(BreakLockProject)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(Projekt)

target_sources(Projekt
    PRIVATE
        src/main.cpp
)

find_path(ASIO_INCLUDE_DIR
    NAMES
        asio.hpp
)

if(NOT ASIO_INCLUDE_DIR)
    message(FATAL_ERROR "Failed to fetch: ASIO.")
endif()


target_include_directories(Projekt
    PUBLIC
        ${ASIO_INCLUDE_DIR}
)

if(WIN32)
    target_compile_definitions(Projekt
        PUBLIC
            _WIN32_WINNT=0x0A00
            ASIO_STANDALONE
    )
    target_link_libraries(Projekt
        PRIVATE
            ws2_32
    )
endif()

if(MINGW)
    target_link_options(Projekt
        PRIVATE
            -static
            -static-libgcc
            -static-libstdc++
    )
endif()
