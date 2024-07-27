// Compile as: g++ -shared -fPIC -o JsonMidiPlayer.so src/JsonMidiPlayer.cpp -I/single_include/nlohmann -I/include -I/src
#ifndef MIDI_JSON_PLAYER_CTYPES_HPP
#define MIDI_JSON_PLAYER_CTYPES_HPP

#include "JsonMidiPlayer.hpp"

#ifdef _WIN32
    #define DLL_EXPORT __declspec(dllexport)
#else
    #define DLL_EXPORT
#endif

extern "C" {    // Needed for Python ctypes
    DLL_EXPORT int PlayList_ctypes(const char* json_str);
    DLL_EXPORT int add_ctypes(int a, int b);
}

#endif // MIDI_JSON_PLAYER_CTYPES_HPP