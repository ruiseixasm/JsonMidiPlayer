# List all available generators command: cmake --help
# Run cmake with a specific generated selected: cmake -g "MinGW Makefiles" ..
cmake_minimum_required(VERSION 3.25)
project(MidiJsonPlayer)

# Add all source files
file(GLOB SOURCES "*.cpp")

# Specify the executable name
set(EXECUTABLE_NAME "MidiJsonPlayer.out")

# Add the executable target
add_executable(${EXECUTABLE_NAME} ${SOURCES})

# Set the build type to "Debug"
set(CMAKE_BUILD_TYPE Debug)

# Add compiler flags for debugging
target_compile_options(${EXECUTABLE_NAME} PRIVATE -g)

# Disable optimization for debug builds
target_compile_options(${EXECUTABLE_NAME} PRIVATE -O0)
