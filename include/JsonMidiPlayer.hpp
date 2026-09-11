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
#include <cmath>                // For std::round
#include <cstdlib>
#include <thread>               // Include for std::this_thread::sleep_for
#include <chrono>               // Include for std::chrono::seconds
#include <nlohmann/json.hpp>    // Include the JSON library
#include "RtMidi.h"             // Includes the necessary MIDI library
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <iomanip>              // For std::fixed and std::setprecision

#ifdef _WIN32
    #define NOMINMAX    // disables the definition of min and max macros.
    #include <Windows.h>
    #include <processthreadsapi.h> // For SetProcessInformation
#else
    #include <pthread.h>
    #include <time.h>
#endif

// #define DEBUGGING true
#define FILE_TYPE "Json Midi Player"
#define FILE_URL  "https://github.com/ruiseixasm/JsonMidiPlayer"
#define VERSION   "7.0.0"
#define DRAG_DURATION_MS (1000.0/((120/60)*24))


// Taken from: https://users.cs.cf.ac.uk/Dave.Marshall/Multimedia/node158.html

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



class Beat {
    const uint32_t _ticks;

    // private constructor — factories call this
    explicit Beat(uint32_t ticks) : _ticks(ticks) {}

public:
    static constexpr uint32_t TICKS_PER_BEAT = 960;

	static uint32_t getTicksFromBeats(uint32_t num, uint32_t den) {
		// Equivalent to Beat((uint32_t)(beats * TICKS_PER_BEAT + 0.5));
		return (num * TICKS_PER_BEAT + den / 2) / den;
	}

    Beat() : _ticks(0) {}
    
    static Beat fromTicks(uint32_t ticks) {
        return Beat(ticks);          // ✅ constructs directly
    }

    static Beat fromFraction(uint32_t num, uint32_t den) {
        return Beat(getTicksFromBeats(num, den));
    }

    static Beat fromDouble(double beats) {
        return Beat((uint32_t)(beats * TICKS_PER_BEAT + 0.5));
    }

    uint32_t getTicks() const { return _ticks; }
    double getBeats() const { return (double)_ticks / TICKS_PER_BEAT; }

    bool operator< (const Beat& o) const { return _ticks <  o._ticks; }
    bool operator==(const Beat& o) const { return _ticks == o._ticks; }
    bool operator!=(const Beat& o) const { return _ticks != o._ticks; }
    Beat operator+(const Beat& o) const { return fromTicks(_ticks + o._ticks); }
};



class MidiDevice;


class MidiPin {

private:
    double time_ms = 0.0;   // Set afterwards based on the _position_beat
    const uint32_t _ticks;
    const unsigned char priority;
    MidiDevice * const midi_device = nullptr;
    std::vector<unsigned char> midi_message;  // Replaces midi_message[3]
    // Auxiliary variable for the final playing loop!!
    double delay_time_ms = -1;

	// needed to recognize and already released Note !!
    size_t note_pressed_times = 1;   // BY DEFAULT THE NOTE ON IS 1 TIME PRESSED

public:
    // Pin DEFAULT constructor, no arguments,
    // needed for emplace and insert of the std::unordered_map inside MidiDevice class !!
    MidiPin()
        : time_ms(0.0),                 // Default to 0.0
        _ticks(0),                    	// Default to 0
        priority(0),                    // Default to 0
        midi_device(nullptr),           // Default to nullptr
        midi_message(),                 // Default to an empty vector
        delay_time_ms(-1),              // Default to -1
        note_pressed_times(1)           // Default to 1
    { }

    // Pin constructor from miliseconds
    MidiPin(double time_milliseconds, MidiDevice* midi_device,
        const std::vector<unsigned char>& json_midi_message, const unsigned char priority = 0xFF)
            : time_ms(time_milliseconds),
        	_ticks(0),                    	// Default to 0
            midi_device(midi_device),
            midi_message(json_midi_message),    // Directly initialize midi_message
            priority(priority)
        { }

    // Pin constructor from position_beats (num, den)
    MidiPin(uint32_t num, uint32_t den, MidiDevice* midi_device,
        const std::vector<unsigned char>& json_midi_message, const unsigned char priority = 0xFF)
            : _ticks(Beat::getTicksFromBeats(num, den)),
            midi_device(midi_device),
            midi_message(json_midi_message),    // Directly initialize midi_message
            priority(priority)
        { }

    // Pin constructor from position_beats (num, den)
    MidiPin(uint32_t ticks, MidiDevice* midi_device,
        const std::vector<unsigned char>& json_midi_message, const unsigned char priority = 0xFF)
            : _ticks(ticks),
            midi_device(midi_device),
            midi_message(json_midi_message),    // Directly initialize midi_message
            priority(priority)
        { }

    // Pin constructor from time_ms and position_beats (num, den)
    MidiPin(double time_milliseconds, uint32_t num, uint32_t den, MidiDevice* midi_device,
        const std::vector<unsigned char>& json_midi_message, const unsigned char priority = 0xFF)
            : time_ms(time_milliseconds),
            _ticks(Beat::getTicksFromBeats(num, den)),
            midi_device(midi_device),
            midi_message(json_midi_message),    // Directly initialize midi_message
            priority(priority)
        { }

    // Pin constructor from time_ms and position_beats (num, den)
    MidiPin(double time_milliseconds, uint32_t ticks, MidiDevice* midi_device,
        const std::vector<unsigned char>& json_midi_message, const unsigned char priority = 0xFF)
            : time_ms(time_milliseconds),
            _ticks(ticks),
            midi_device(midi_device),
            midi_message(json_midi_message),    // Directly initialize midi_message
            priority(priority)
        { }

    // Pin copy constructor
    MidiPin(const MidiPin& other)
        : time_ms(other.time_ms),                     // Copy the time_ms
          _ticks(other._ticks),       				  // Copy the position ticks
          midi_device(other.midi_device),             // Copy the pointer to the MidiDevice
          midi_message(other.midi_message),           // Copy the midi_message vector
          priority(other.priority),                   // Copy the priority
          delay_time_ms(other.delay_time_ms),         // Copy the delay_time_ms
          note_pressed_times(other.note_pressed_times)          // Copy the note_released
    { }

	void setTime(double time_milliseconds) {
		time_ms = time_milliseconds;
	}

    double getTime() const {
        return time_ms;
    }

    uint32_t getPositionTicks() const {
        return _ticks;
    }

    MidiDevice *getMidiDevice() const {
        return midi_device;
    }

    void pluckTooth();

    void setDelayTime(double delay_time_ms) {
        this->delay_time_ms = delay_time_ms;
    }

    double getDelayTime() const {
        return this->delay_time_ms;
    }

    std::vector<unsigned char> getMessage() const {
        return this->midi_message; // Returns a copy
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

	size_t getNotePressedTimes() const {
		return this->note_pressed_times;
	}

	void increaseNotePressedTimes() {
		this->note_pressed_times++;
	}

	void decreaseNotePressedTimes() {
		this->note_pressed_times--;
	}

public:

    // Intended for Automation messages only
    bool operator != (const MidiPin &midi_pin) {
        // mapped by status byte, so, with the same action type for sure
        switch (this->getAction()) {
            case action_control_change:
            case action_key_pressure:
                return this->getDataByte(2) != midi_pin.getDataByte(2);		// Value or Pressure
            case action_pitch_bend:
                return this->getDataByte(1) != midi_pin.getDataByte(1) ||
                        this->getDataByte(2) != midi_pin.getDataByte(2);	// LSB and MSB
            case action_channel_pressure:
                return this->getDataByte(1) != midi_pin.getDataByte(1);		// Pressure
        }
        return true;
    }

};


class MidiDevice {
    private:
        RtMidiOut midiOut;
        const std::string name;
        const unsigned int port;
        const bool verbose;
        bool opened_port = false;
        bool unavailable_device = false;
    
    public:
    
        // Keeps MidiPin pointers by Channel_Pitch (uint16_t) (similar to byte_16)
        std::unordered_map<uint16_t, MidiPin*>		channelpitch_last_pins_note_on;			// For Note On tracking
        
        // Keeps MidiPin dummy copies, thus NOT pointers of MidiPin
        std::unordered_map<unsigned char, MidiPin>  statusbyte_last_pins_pitchbend;    		// For Pitch Bend and Aftertouch
        std::unordered_map<uint16_t, MidiPin>       statusdatabyte_last_pin_controlchange;	// For Control Changeand Key Pressure

        // Keeps MidiPin pointers
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
    

	
class Tempo {
    const uint16_t _bpm_10;
    const uint32_t _ticks;

public:

    // ── canonical constructor ──
    Tempo(uint16_t bpm_10, uint32_t position_ticks)
        : _bpm_10(bpm_10), _ticks(position_ticks) {}

	// beats_per_second	= (1 / 60) * BPM
	// seconds_per_ms   = 1,000
	// ticks_per_beat   = 960
	//
	// beats_per_ms = beats_per_second / seconds_per_ms = (1 / 60) * BPM / 1,000
	// ticks_per_ms = ticks_per_beat * beats_per_ms
	//		= 960 * (1 / 60) * BPM / 1,000
	//		= 960 / (1,000 * 60) * BPM
	//		= 0.016 * BPM
	//
	// ms_per_tick = 1 / ticks_per_ms
	//		= 1 / (960 * (1 / 60) * BPM / 1,000)
	//		= 60 * 1,000 / (960 * BPM)
	//		= 62.5 / BPM

	double beatsToMs() const {
		return (double)_ticks * 625 / _bpm_10;
	}

    // ── tempo-aware conversion ──
    uint64_t toMicroseconds(uint16_t bpm_10) const {
        // µs = ticks × 62,500 / bpm
        return (uint64_t)_ticks * 625000 / bpm_10;
    }


    uint16_t getBPM_10() const {
        return _bpm_10;
    }

    uint32_t getPositionTicks() const {
        return _ticks;
    }

    bool operator< (const Tempo& o) const { return _ticks <  o._ticks; }
    bool operator==(const Tempo& o) const { return _ticks == o._ticks; }
    bool operator!=(const Tempo& o) const { return _ticks != o._ticks; }
};


class Clocking {
	std::list<Tempo> _tempos;

public:

	void addTempo(uint16_t bpm_10, uint32_t position_ticks) {
		_tempos.emplace_back(bpm_10, position_ticks);
	}

	void sortTempos() {
		_tempos.sort();
	}

	void setPinTime();
};

    

// Declare the function in the header file
void disableBackgroundThrottling();

void setRealTimeScheduling();
void highResolutionSleep(long long microseconds);
int PlayList(const char* json_str, bool verbose = false);


#endif // MIDI_JSON_PLAYER_HPP
