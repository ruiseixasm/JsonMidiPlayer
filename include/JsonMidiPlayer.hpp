/*
JsonMidiPlayer - Json Midi Player is intended to be used
in conjugation with the Json Midi Creator to Play its composed Elements
Original Copyright (c) 2024 Rui Seixas Monteiro. All right reserved.
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
Lesser General Public License for more details.
https://github.com/ruiseixasm/JsonMidiCreator
https://github.com/ruiseixasm/JsonMidiPlayer
*/
#ifndef MIDI_JSON_PLAYER_HPP
#define MIDI_JSON_PLAYER_HPP

#include <iostream>
#include <string>
#include <array>
#include <vector>
#include <list>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <thread>               // Include for std::this_thread::sleep_for
#include <chrono>               // Include for std::chrono::seconds
#include <nlohmann/json.hpp>    // Include the JSON library
#include "RtMidi.h"             // Includes the necessary MIDI library
#include <unordered_map>
#include <memory>

#ifdef _WIN32
    #define NOMINMAX    // disables the definition of min and max macros.
    #include <Windows.h>
    #include <processthreadsapi.h> // For SetProcessInformation

    // // Disable background throttling
    // void disableBackgroundThrottling() {
    //     PROCESS_POWER_THROTTLING_STATE PowerThrottling;
    //     PowerThrottling.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    //     PowerThrottling.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
    //     PowerThrottling.StateMask = 0;

    //     SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &PowerThrottling, sizeof(PowerThrottling));
    // }
#else
    #include <pthread.h>
    #include <time.h>
#endif

// #define DEBUGGING true
#define FILE_TYPE "Json Midi Player"
#define FILE_URL  "https://github.com/ruiseixasm/JsonMidiPlayer"
#define VERSION   "4.2.0"
#define DRAG_DURATION_MS (1000.0/((120/60)*24))


const unsigned char action_note_off         = 0x80; // Note off
const unsigned char action_note_on          = 0x90; // Note on
const unsigned char action_key_pressure     = 0xA0; // Polyphonic Key Pressure
const unsigned char action_control_change   = 0xB0; // Control Change
const unsigned char action_program_change   = 0xC0; // Program Change
const unsigned char action_channel_pressure = 0xD0; // Channel Pressure
const unsigned char action_pitch_bend       = 0xE0; // Pitch Bend
const unsigned char action_system           = 0xF0; // Device related Messages, System

const unsigned char system_sysex_start      = 0xF0; // Sysex Start
const unsigned char system_time_mtc         = 0xF1; // MIDI Time Code Quarter Frame
const unsigned char system_song_pointer     = 0xF2; // Song Position Pointer
const unsigned char system_song_select      = 0xF3; // Song Select
const unsigned char system_tune_request     = 0xF6; // Tune Request
const unsigned char system_sysex_end        = 0xF7; // Sysex End
const unsigned char system_timing_clock     = 0xF8; // Timing Clock
const unsigned char system_clock_start      = 0xFA; // Start
const unsigned char system_clock_continue   = 0xFB; // Continue
const unsigned char system_clock_stop       = 0xFC; // Stop
const unsigned char system_active_sensing   = 0xFE; // Active Sensing
const unsigned char system_system_reset     = 0xFF; // System Reset



class MidiPin;


class MidiDevice {
private:
    RtMidiOut midiOut;
    const std::string name;
    const unsigned int port;
    const bool verbose;
    bool opened_port = false;
    bool unavailable_device = false;

public:

    std::unordered_map<unsigned char, std::list<MidiPin*>>
                                                last_pin_note_on;   // For Note On tracking
    std::unordered_map<unsigned char, MidiPin*> last_pin_byte_8;    // For Pitch Bend and Aftertouch alike
    std::unordered_map<uint16_t, MidiPin*>      last_pin_byte_16;   // For Control Change and Key Pressure
    MidiPin *last_pin_clock = nullptr;          // Midi clock messages 0xF0
    MidiPin *last_pin_song_pointer = nullptr;   // Midi clock messages 0xF2


public:
    MidiDevice(std::string device_name, unsigned int device_port, bool verbose = false)
                : name(device_name), port(device_port), verbose(verbose) { }
    ~MidiDevice() { closePort(); }

    // Move constructor
    MidiDevice(MidiDevice &&other) noexcept : midiOut(std::move(other.midiOut)),
            name(std::move(other.name)), port(other.port), verbose(other.verbose),
            opened_port(other.opened_port) { }

    // Delete the copy constructor and copy assignment operator
    MidiDevice(const MidiDevice &) = delete;
    MidiDevice &operator=(const MidiDevice &) = delete;

    // Move assignment operator
    MidiDevice &operator=(MidiDevice &&other) noexcept {
        if (this != &other) {
            // Since name and port are const, they cannot be assigned.
            opened_port = other.opened_port;
            // midiOut can't be assigned using the = assignment operator because has none.
            // midiOut = std::move(other.midiOut);
        }
        std::cout << "Move assigned: " << name << std::endl;
        return *this;
    }

    bool openPort();
    void closePort();
    bool hasPortOpen() const;
    const std::string& getName() const;
    unsigned int getDevicePort() const;
    void sendMessage(const std::vector<unsigned char> *midi_message);
};



class MidiPin {

private:
    const double time_ms;
    const unsigned char priority;
    MidiDevice * const midi_device = nullptr;
    std::vector<unsigned char> midi_message;  // Replaces midi_message[3]
    // https://users.cs.cf.ac.uk/Dave.Marshall/Multimedia/node158.html
    double delay_time_ms = -1;

public:
    MidiPin(double time_milliseconds, MidiDevice* midi_device,
        const std::vector<unsigned char>& json_midi_message, const unsigned char priority = 0xFF)
            : time_ms(time_milliseconds),
            midi_device(midi_device),
            midi_message(json_midi_message),    // Directly initialize midi_message
            priority(priority)
        { }

    double getTime() const {
        return time_ms;
    }

    MidiDevice *getMidiDevice() const {
        return midi_device;
    }

    unsigned int getDevicePort() const {
        return midi_device->getDevicePort();
    }

    void pluckTooth() {
        if (midi_device != nullptr)
            midi_device->sendMessage(&midi_message);
    }

    void setDelayTime(double delay_time_ms) {
        this->delay_time_ms = delay_time_ms;
    }

    double getDelayTime() const {
        return this->delay_time_ms;
    }

    void setStatusByte(unsigned char status_byte) {
        this->midi_message[0] = status_byte;
    }

    unsigned char getStatusByte() const {
        return this->midi_message[0];
    }

    void setDataByte(int nth_byte, unsigned char data_byte) {
        this->midi_message[nth_byte] = data_byte;
    }

    unsigned char getDataByte(int nth_byte = 1) const {
        return this->midi_message[nth_byte];
    }

    unsigned char getChannel() const {
        return this->midi_message[0] & 0x0F;
    }

    unsigned char getAction() const {
        return this->midi_message[0] & 0xF0;
    }

    unsigned char getPriority() const {
        return this->priority;
    }

    MidiDevice * const getDevice() const {
        return this->midi_device;
    }


public:

    // If this is a Note On pin, then, by definition, is already at level 1
    size_t level = 1;   // VERY IMPORTANT TO AVOID EARLIER NOTE OFF

    // Intended for Note On only
    bool operator == (const MidiPin &midi_pin) {
        return this->getDataByte(1) == midi_pin.getDataByte(1); // Key number
    }

    // Intended for Automation messages only
    bool operator != (const MidiPin &midi_pin) {
        if (this->getStatusByte() == midi_pin.getStatusByte()) {
            switch (midi_pin.getAction()) {
                case action_control_change:
                case action_key_pressure:
                    return this->getDataByte(2) != midi_pin.getDataByte(2);
                case action_pitch_bend:
                    return this->getDataByte(1) != midi_pin.getDataByte(1) ||
                           this->getDataByte(2) != midi_pin.getDataByte(2);
                case action_channel_pressure:
                    return this->getDataByte(1) != midi_pin.getDataByte(1);
            }
        }
        return true;
    }

    // Prefix increment
    MidiPin& operator++() {
        ++level;
        return *this;
    }
    // Postfix increment
    MidiPin operator++(int) {
        MidiPin temp = *this;
        ++level;
        return temp;
    }
    // Prefix decrement
    MidiPin& operator--() {
        --level;
        return *this;
    }
    // Postfix decrement
    MidiPin operator--(int) {
        MidiPin temp = *this;
        --level;
        return temp;
    }

};



// Declare the function in the header file
void disableBackgroundThrottling();

void setRealTimeScheduling();
void highResolutionSleep(long long microseconds);
int PlayList(const char* json_str, bool verbose = false);


// Custom hash function for nlohmann::json (required for unordered_map keys)
struct JsonHash {
    std::size_t operator()(const nlohmann::json& jsonKey) const {
        return std::hash<std::string>{}(jsonKey.dump()); // Hash the JSON as a string
    }
};

// Custom equality function for JSON keys
struct JsonEqual {
    bool operator()(const nlohmann::json& lhs, const nlohmann::json& rhs) const {
        return lhs == rhs; // Compare JSON objects directly
    }
};



#endif // MIDI_JSON_PLAYER_HPP
