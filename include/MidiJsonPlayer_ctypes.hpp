#ifndef MIDI_JSON_PLAYER_CTYPES_HPP
#define MIDI_JSON_PLAYER_CTYPES_HPP

#include "MidiJsonPlayer.hpp"

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