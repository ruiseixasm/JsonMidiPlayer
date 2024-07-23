# List all available generators command: cmake --help
# Run cmake with a specific generator selected: cmake -g "MinGW Makefiles" ..
cmake_minimum_required(VERSION 3.15)
project(MidiJsonPlayer
        VERSION 0.2.1
        DESCRIPTION "Very simple MIDI Player of JSON files"
        HOMEPAGE_URL "https://github.com/ruiseixasm/MidiJsonPlayer"
        LANGUAGES CXX)

# Include directories
include_directories(include single_include)

# Add main.cpp explicitly
set(STATIC_SOURCES src/MidiJsonPlayer.cpp src/RtMidi.cpp)

# Create the shared library
add_library(MidiJsonPlayer_library STATIC ${STATIC_SOURCES})
# Add the -fPIC option for position-independent code
set_target_properties(MidiJsonPlayer_library PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Specify output directories
set_target_properties(MidiJsonPlayer_library PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# Add main.cpp explicitly
set(CTYPES_SOURCES src/MidiJsonPlayer_ctypes.cpp)

# Create the shared library
add_library(MidiJsonPlayer_ctypes SHARED ${CTYPES_SOURCES})
# Link to MyLibrary
target_link_libraries(MidiJsonPlayer_ctypes PRIVATE MidiJsonPlayer_library)
# Add the -fPIC option for position-independent code
set_target_properties(MidiJsonPlayer_ctypes PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Specify output directories
set_target_properties(MidiJsonPlayer_ctypes PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# Add main.cpp explicitly
set(EXECUTABLE_SOURCES main.cpp src/MidiJsonPlayer.cpp)

# Set properties for Windows DLL export
if(WIN32)
    set(EXECUTABLE_NAME "MidiJsonPlayer")
else()
    set(EXECUTABLE_NAME "MidiJsonPlayer.out")
endif()

# Add the executable target
add_executable(${EXECUTABLE_NAME} ${EXECUTABLE_SOURCES})
# Link to MyLibrary
target_link_libraries(${EXECUTABLE_NAME} PRIVATE MidiJsonPlayer_library)

# # Set the build type to "Debug"
# set(CMAKE_BUILD_TYPE Debug)

# # Add compiler flags for debugging
# target_compile_options(${EXECUTABLE_NAME} PRIVATE -g)

# # Disable optimization for debug builds
# target_compile_options(${EXECUTABLE_NAME} PRIVATE -O0)

# Check if we are on Windows
if (WIN32)  # Try to load ASIO SDK
    add_compile_definitions(__WINDOWS_MM__)
    target_link_libraries(MidiJsonPlayer_library PRIVATE
            winmm.lib
            ws2_32.lib
            wininet.lib
            version.lib
            Shlwapi.dll
        )
else()
    # Find and link ALSA library
    find_package(ALSA REQUIRED)
    if (ALSA_FOUND)
        include_directories(${ALSA_INCLUDE_DIRS})
        target_link_libraries(MidiJsonPlayer_library ${ALSA_LIBRARIES})
        add_definitions(-D__LINUX_ALSA__)
    endif()
endif()

# Print the project directory during configuration
message("CMAKE_CURRENT_BINARY_DIR: ${CMAKE_CURRENT_BINARY_DIR}")
