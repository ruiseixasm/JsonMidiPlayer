#include "JsonMidiPlayer_ctypes.hpp"

int PlayList_ctypes(const char* json_str) {
    return PlayList(json_str);
}

int add_ctypes(int a, int b) {
    return a + b;
}