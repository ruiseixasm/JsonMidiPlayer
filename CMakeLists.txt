# List all available generators command: cmake --help
# Run cmake with a specific generator selected: cmake -g "MinGW Makefiles" ..
cmake_minimum_required(VERSION 3.15)
project(MidiJsonPlayer
        VERSION 0.0.1
        DESCRIPTION "Very simple MIDI Player of JSON files"
        HOMEPAGE_URL "https://github.com/ruiseixasm/MidiJsonPlayer"
        LANGUAGES CXX)

# # API Options
# option(RTMIDI_API_JACK "Compile with JACK support." ${HAVE_JACK})
# option(RTMIDI_API_WINMM "Compile with WINMM support." ${WIN32})
# option(RTMIDI_API_CORE "Compile with CoreMIDI support." ${APPLE})
# option(RTMIDI_API_ALSA "Compile with ALSA support." ${ALSA})
# option(RTMIDI_API_AMIDI "Compile with Android support." ${ANDROID})

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
    add_compile_definitions(__WINDOWS_MM__)
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

# Check if we are on Windows
if (WIN32)  # Try to load ASIO SDK
    # find_package(ASIO)
    # if(TARGET ASIO::SDK)
    #     target_link_libraries(openshot-audio PRIVATE ASIO::SDK)
    #     set(NEED_ASIO TRUE)
    # endif()
    # Order here can be important!
    # For example, winmm.lib must come before kernel32.lib (if linked)
    # or older 32-bit windows versions will have linking issues for
    # certain entry points
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
        target_link_libraries(${EXECUTABLE_NAME} ${ALSA_LIBRARIES})
        add_definitions(-D__LINUX_ALSA__)
    endif()

    # # Find and link RtMidi library
    # add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/path/to/rtaudio)
    # include_directories(${CMAKE_CURRENT_SOURCE_DIR}/path/to/rtaudio/include)
    # target_link_libraries(${EXECUTABLE_NAME} rtmidi)
endif()