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
#define VERSION   "3.0.0"
#define DRAG_DURATION_MS (1000.0/((120/60)*24))

class MidiDevice {
private:
    RtMidiOut midiOut;
    const std::string name;
    const unsigned int port;
    const bool verbose;
    bool opened_port = false;
    bool unavailable_device = false;
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
    bool isPortOpened() const;
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

    unsigned char getDataByte(int nth_byte = 1) const {
        return this->midi_message[nth_byte];
    }

    unsigned char getChannel() const {
        return this->midi_message[0] & 0x0F;
    }

    unsigned char getAction() const {
        return this->midi_message[0] & 0xF0;
    }

};

// Declare the function in the header file
void disableBackgroundThrottling();

void setRealTimeScheduling();
void highResolutionSleep(long long microseconds);
int PlayList(const char* json_str, bool verbose = false);

#endif // MIDI_JSON_PLAYER_HPP
