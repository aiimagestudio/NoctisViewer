cmake_minimum_required(VERSION 3.10)
project(Noctis_Viewer VERSION 1.3 LANGUAGES CXX)

# C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows-specific settings
if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE -DWIN32 -DNOMINMAX)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
endif()

# Source files
set(SOURCES
    noctis_viewer.cpp
    noctis_viewer.rc
)

# Create executable
add_executable(Noctis_Viewer WIN32 ${SOURCES})

# Link libraries
if(WIN32)
    target_link_libraries(Noctis_Viewer
        gdi32
        user32
        shell32
        ole32
        comctl32
        oleaut32
        uuid
        shlwapi
    )
endif()

# Optimization flags
if(MSVC)
    target_compile_options(Noctis_Viewer PRIVATE /W4 /utf-8)
else()
    target_compile_options(Noctis_Viewer PRIVATE -O2 -Wall)
endif()

# Set output name
set_target_properties(Noctis_Viewer PROPERTIES
    OUTPUT_NAME "Noctis_Viewer"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)
