cmake_minimum_required(VERSION 3.20)
project(FIFOEngine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

add_library(fifo_engine SHARED
    third_party/sqlite3.c
    src/database.cpp
    src/scanner.cpp
    src/forecast.cpp
    src/cleanup.cpp
    src/datagen.cpp
    src/scheduler.cpp
    src/fifo_api.cpp
)

target_include_directories(fifo_engine PRIVATE
    include
    third_party
    src
)

target_compile_definitions(fifo_engine PRIVATE
    FIFO_ENGINE_EXPORTS
    SQLITE_THREADSAFE=1
    SQLITE_ENABLE_WAL=1
)

if(MSVC)
    target_compile_options(fifo_engine PRIVATE /O2 /W3 /utf-8)
    target_compile_definitions(fifo_engine PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(fifo_engine PRIVATE -O2 -Wall)
endif()

# Post-build: copy DLL to WPF output
set(WPF_OUTPUT "${CMAKE_SOURCE_DIR}/../FIFOManagement/bin/Release/net10.0-windows")
add_custom_command(TARGET fifo_engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${WPF_OUTPUT}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:fifo_engine>
        "${WPF_OUTPUT}/fifo_engine.dll"
    COMMENT "Copying fifo_engine.dll to WPF output"
)
