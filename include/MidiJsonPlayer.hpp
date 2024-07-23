// Compile as: g++ -shared -fPIC -o MidiJsonPlayer.so src/MidiJsonPlayer.cpp -I/single_include/nlohmann -I/include -I/src

#ifndef MIDI_JSON_PLAYER_HPP
#define MIDI_JSON_PLAYER_HPP

#include <iostream>
#include <string>
#include <array>
#include <vector>
#include <list>
#include <algorithm>
#include <cstdlib>
#include <thread>               // Include for std::this_thread::sleep_for
#include <chrono>               // Include for std::chrono::seconds
#include <nlohmann/json.hpp>    // Include the JSON library
#include "RtMidi.h"             // Includes the necessary MIDI library

#define FILE_TYPE "Midi Json Player"

class MidiDevice {
private:
    RtMidiOut midiOut;
    const std::string name;
    const unsigned int port;
    bool opened_port = false;
    std::array<unsigned char, 256> keyboards = {0};
public:
    MidiDevice(std::string device_name, unsigned int device_port) : name(device_name), port(device_port) { }

    ~MidiDevice();

    // Move constructor
    MidiDevice(MidiDevice &&other) noexcept : midiOut(std::move(other.midiOut)),
            name(std::move(other.name)), port(other.port), opened_port(other.opened_port), keyboards(std::move(other.keyboards)) { }

    // Delete the copy constructor and copy assignment operator
    MidiDevice(const MidiDevice &) = delete;
    MidiDevice &operator=(const MidiDevice &) = delete;

    // Move assignment operator
    MidiDevice &operator=(MidiDevice &&other) noexcept {
        if (this != &other) {
            // Since name and port are const, they cannot be assigned.
            opened_port = other.opened_port;
            keyboards = std::move(other.keyboards);
            // midiOut can't be assigned using the = assignment operator because has none.
            // midiOut = std::move(other.midiOut);
        }
        std::cout << "Move assigned: " << name << std::endl;
        return *this;
    }

    void openPort() {
        if (!opened_port) {
            midiOut.openPort(port);
            opened_port = true;
            std::cout << "Midi device connected: " << name << std::endl;
        }
    }

    void closePort() {
        if (opened_port) {
            midiOut.closePort();
            opened_port = false;
            std::cout << "Midi device disconnected: " << name << std::endl;
        }
    }

    unsigned int getOpenedPort() const {
        return opened_port;
    }

    const std::string& getName() const {
        return name;
    }

    bool isKeyPressed(unsigned char channel, unsigned char key_note) const {
        unsigned char keyboards_key = channel * 128 + key_note;
        unsigned char keyboards_byte = keyboards_key / 8;
        unsigned char byte_key = keyboards_key % 8;

        return keyboards[keyboards_byte] & 0b10000000 >> byte_key;
    }

    void pressKey(unsigned char channel, unsigned char key_note) {
        unsigned char keyboards_key = channel * 128 + key_note;
        unsigned char keyboards_byte = keyboards_key / 8;
        unsigned char byte_key = keyboards_key % 8;

        keyboards[keyboards_byte] |= 0b10000000 >> byte_key;
    }

    void releaseKey(unsigned char channel, unsigned char key_note) {
        unsigned char keyboards_key = channel * 128 + key_note;
        unsigned char keyboards_byte = keyboards_key / 8;
        unsigned char byte_key = keyboards_key % 8;

        keyboards[keyboards_byte] &= ~0b10000000 >> byte_key;
    }

    void sendMessage(const unsigned char *midi_message, size_t message_size) {

        if (message_size <= 3) {
            bool validated = false;

            if (message_size == 3) {
                if (midi_message[1] & 0xF0 == 0x80) {          // 0x80 - Note Off
                    if (isKeyPressed(midi_message[1] & 0x0F, midi_message[2])) {
                        releaseKey(midi_message[1] & 0x0F, midi_message[2]);
                        validated = true;
                    }
                } else if (midi_message[1] & 0xF0 == 0x90) {   // 0x90 - Note On
                    if (!isKeyPressed(midi_message[1] & 0x0F, midi_message[2])) {
                        pressKey(midi_message[1] & 0x0F, midi_message[2]);
                        validated = true;
                    }
                } else {
                    validated = true;
                }
            } else {
                validated = true;
            }

            if (validated) {
                // Send the MIDI message
                midiOut.sendMessage(midi_message, message_size);
            }
        }
    }
};

class MidiPin {

private:
    const double time_ms;
    MidiDevice * const midi_device = nullptr;
    const size_t message_size;
    const unsigned char midi_message[3];    // Status byte and 2 Data bytes
    // https://users.cs.cf.ac.uk/Dave.Marshall/Multimedia/node158.html
public:
    MidiPin(double time_milliseconds, MidiDevice *midi_device, size_t message_size, unsigned char status_byte, unsigned char data_byte_1, unsigned char data_byte_2)
        : time_ms(time_milliseconds), midi_device(midi_device), message_size(message_size), midi_message{status_byte, data_byte_1, data_byte_2} { }

    double getTime() const {
        return time_ms;
    }

    void pluckTooth() {
        if (midi_device != nullptr)
            midi_device->sendMessage(midi_message, message_size);
    }
};

bool canOpenMidiPort(RtMidiOut& midiOut, unsigned int portNumber);

int PlayList(const char* json_str);

#endif // MIDI_JSON_PLAYER_HPP