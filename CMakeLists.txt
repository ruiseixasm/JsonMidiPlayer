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

# Set properties for Windows DLL export
if(WIN32)
    add_compile_definitions(__WINDOWS_MM__)
endif()

# Add all source files from the src directory
file(GLOB SRC_SOURCES "src/*.cpp")

set(LIBRARY_NAME "MidiJsonPlayer_ctypes")
# Create the shared library
add_library(${LIBRARY_NAME} SHARED ${SRC_SOURCES})

# Specify the executable name
if (WIN32) # LINUX is new for CMake VERSION 3.25
    target_compile_definitions(${LIBRARY_NAME} PRIVATE -DBUILDING_DLL)
endif()

# Add main.cpp explicitly
set(SOURCES main.cpp)

# Combine all sources
list(APPEND SOURCES ${SRC_SOURCES})

# Set properties for Windows DLL export
if(WIN32)
    set(EXECUTABLE_NAME "MidiJsonPlayer")
else()
    set(EXECUTABLE_NAME "MidiJsonPlayer.out")
endif()

# Add the executable target
add_executable(${EXECUTABLE_NAME} ${SOURCES})

# # Set the build type to "Debug"
# set(CMAKE_BUILD_TYPE Debug)

# # Add compiler flags for debugging
# target_compile_options(${EXECUTABLE_NAME} PRIVATE -g)

# # Disable optimization for debug builds
# target_compile_options(${EXECUTABLE_NAME} PRIVATE -O0)

# Check if we are on Windows
if (WIN32)  # Try to load ASIO SDK
    target_link_libraries(${LIBRARY_NAME} PRIVATE
            winmm.lib
            ws2_32.lib
            wininet.lib
            version.lib
            Shlwapi.dll
        )
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE
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
        target_link_libraries(MidiJsonPlayer_ctypes ${ALSA_LIBRARIES})
        target_link_libraries(${EXECUTABLE_NAME} ${ALSA_LIBRARIES})
        add_definitions(-D__LINUX_ALSA__)
    endif()
endif()

# Print the project directory during configuration
message("CMAKE_CURRENT_BINARY_DIR: ${CMAKE_CURRENT_BINARY_DIR}")
