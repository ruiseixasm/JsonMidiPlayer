# List all available generators command: cmake --help
# Run cmake with a specific generator selected: cmake -g "MinGW Makefiles" ..
cmake_minimum_required(VERSION 3.15)
project(MidiJsonPlayer
        VERSION 0.0.1
        DESCRIPTION "Very simple MIDI Player of JSON files"
        HOMEPAGE_URL "https://github.com/ruiseixasm/MidiJsonPlayer"
        LANGUAGES CXX)

# Include directories
include_directories(include single_include)

# Add main.cpp explicitly
set(SOURCES main.cpp)

#
# NEED TO REBUILD THE ENTIRE BUILD EACH TIME A NEW .CPP FILE IS ADDED!!!
#

# Add all source files from the src directory
file(GLOB SRC_SOURCES "src/*.cpp")

# Combine all sources
list(APPEND SOURCES ${SRC_SOURCES})

# Specify the executable name
if (WIN32) # LINUX is new for CMake VERSION 3.25
    set(EXECUTABLE_NAME "MidiJsonPlayer")
else()
    set(EXECUTABLE_NAME "MidiJsonPlayer.out")
endif()

# Add the executable target
add_executable(${EXECUTABLE_NAME} ${SOURCES})

# Set the build type to "Debug"
set(CMAKE_BUILD_TYPE Debug)

# Add compiler flags for debugging
target_compile_options(${EXECUTABLE_NAME} PRIVATE -g)

# Disable optimization for debug builds
target_compile_options(${EXECUTABLE_NAME} PRIVATE -O0)
