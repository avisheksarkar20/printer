cmake_minimum_required(VERSION 3.16)
project(PrintQueueManager LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
    message(FATAL_ERROR "PrintQueueManager targets Windows only (winspool/Win32 API).")
endif()

set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/SpoolerAPI.cpp
    src/resource.rc
)

set(HEADERS
    src/MainWindow.h
    src/SpoolerAPI.h
    src/resource.h
)

add_executable(PrintQueueManager WIN32 ${SOURCES} ${HEADERS})

target_include_directories(PrintQueueManager PRIVATE src)

# UNICODE for TCHAR = wchar_t everywhere
target_compile_definitions(PrintQueueManager PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# Link Win32 libraries: spooler API, common controls, GDI
target_link_libraries(PrintQueueManager PRIVATE
    winspool
    comctl32
    gdi32
    user32
    kernel32
    advapi32
)

# MSVC-specific settings
if(MSVC)
    target_compile_options(PrintQueueManager PRIVATE /W4 /permissive-)
    # Use Common Controls v6 via manifest embedded in resource.rc
    set_target_properties(PrintQueueManager PROPERTIES
        LINK_FLAGS "/MANIFEST:NO"
    )
endif()

# MinGW fallback
if(MINGW)
    target_compile_options(PrintQueueManager PRIVATE -Wall -Wextra -municode)
    target_link_options(PrintQueueManager PRIVATE -municode -mwindows)
endif()
