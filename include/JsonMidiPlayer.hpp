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
#define VERSION   "7.3.1"
#define DRAG_DURATION_MS (1000.0/((120/60)*24))


// Declare the function in the header file
void disableBackgroundThrottling();

void setRealTimeScheduling();
void highResolutionSleep(long long microseconds);
int PlayList(const char* json_str, int loop = 1, bool verbose = false);


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


static constexpr uint32_t TICKS_PER_BEAT 	= 960;	// Internal PPQN
static constexpr uint32_t CLOCKS_PER_BEAT 	= 24;   // MIDI spec
static constexpr uint32_t TICKS_PER_CLOCK 	= TICKS_PER_BEAT / CLOCKS_PER_BEAT;	// = 40


inline uint32_t getTicksFromBeats(uint32_t num, uint32_t den) {
	// Equivalent to Beat((uint32_t)(beats * TICKS_PER_BEAT + 0.5));
	if (den > 0) {
		return (num * TICKS_PER_BEAT + den / 2) / den;
	}
	return 0;
}


class MidiDevice;


class MidiPin {

private:
    double time_ms = 0.0;   // Set afterwards based on the _position_beat
    const uint32_t _ticks = 0;
    const unsigned char priority;
    MidiDevice * const midi_device = nullptr;
    std::vector<unsigned char> midi_message;  // Replaces midi_message[3]
    // Auxiliary variable for the final playing loop!!
    double delay_time_ms = 0.0;

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
        delay_time_ms(0.0),             // Default to 0.0
        note_pressed_times(1)           // Default to 1
    { }

    // Pin constructor from position_beats (num, den)
    MidiPin(uint32_t num, uint32_t den, MidiDevice* midi_device,
        const std::vector<unsigned char>& json_midi_message, const unsigned char priority = 0xFF)
            : _ticks(getTicksFromBeats(num, den)),
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

	void setTime_ms(double time_milliseconds) {
		time_ms = time_milliseconds;
	}

    double getTime_ms() const {
        return time_ms;
    }

    uint32_t getPositionTicks() const {
        return _ticks;
    }

    MidiDevice *getMidiDevice() const {
        return midi_device;
    }

    void pluckTooth();

    void addDelayTime(double delay_time_ms) {
        this->delay_time_ms += delay_time_ms;
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
	// For the sorting
    bool operator< (const MidiPin& mp) const {
		if (_ticks != mp._ticks) { return _ticks < mp._ticks; }
		return priority < mp.priority;
	}

    // Intended for Automation messages only
    bool operator == (const MidiPin &midi_pin) {
        // mapped by status byte, so, with the same action type for sure
        switch (this->getAction()) {
            case action_control_change:
            case action_key_pressure:
                return this->getDataByte(2) == midi_pin.getDataByte(2);		// Value or Pressure
            case action_pitch_bend:
                return this->getDataByte(1) == midi_pin.getDataByte(1) ||
                        this->getDataByte(2) == midi_pin.getDataByte(2);	// LSB and MSB
            case action_channel_pressure:
                return this->getDataByte(1) == midi_pin.getDataByte(1);		// Pressure
        }
        return false;
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
        std::unordered_map<uint16_t, MidiPin>       statusdatabyte_last_pin_controlchange;	// For Control Change and Key Pressure
    
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
    const int16_t _bpm_10;
    const uint32_t _ticks;
    double time_ms = 0.0;   // associates a time_ms for each Tempo in order to interpolate afterwards

public:

    // ── canonical constructor ──
    Tempo(int16_t bpm_10, uint32_t position_ticks)
        : _bpm_10(bpm_10), _ticks(position_ticks) {}


    int16_t getBPM_10() const {
        return _bpm_10;
    }

    uint32_t getPositionTicks() const {
        return _ticks;
    }

	void setTime_ms(double time_milliseconds) {
		time_ms = time_milliseconds;
	}

    double getTime_ms() const {
        return time_ms;
    }

    bool operator< (const Tempo& t) const { return _ticks <  t._ticks; }
};


// Remembers where the last accumulation stopped, so the next call
// only sums the ticks between here and the requested tick.
struct RampCursor {
    uint32_t position_ticks = 0;     // cursor position: the last tick accumulated
    uint32_t left_ticks 	= 0;     // tick of the segment's left marker
    double   time_ms    	= 0.0;   // absolute time (ms) at that cursor position
    double   bpm        	= 0.0;   // BPM (in bpm_10 units) at the cursor position
    double   bpm_slope  	= 0.0;   // BPM change per tick across the segment


	void updateCursor(const Tempo& left, const Tempo& right) {
		uint32_t right_ticks = right.getPositionTicks();  // tick of the segment's right marker

		left_ticks = left.getPositionTicks();             // tick of the segment's left marker
		position_ticks       = left_ticks;                          // cursor starts at the left marker
		time_ms    = left.getTime_ms();                   // absolute time (ms) at the left marker
		bpm        = (double)left.getBPM_10();            // BPM at the left marker, in bpm_10 units

		uint32_t N = right_ticks - left_ticks;            // N = number of ticks in the segment
		bpm_slope  = ((double)right.getBPM_10() - bpm) / (double)N;  // BPM change per tick
	}


    // Advance the cursor up to `target_ticks`, summing 625/BPM per tick.
    // Each step adds the duration of one tick and moves the BPM one step along the ramp.
    //   625.0 = 60000 ms/min ÷ 960 ticks/beat ÷ 10 (the bpm_10 scale)
    // Returns the absolute time (ms) at `target_ticks`.
    double moveCursor(uint32_t target_ticks) {
        while (position_ticks < target_ticks) {
            bpm     += bpm_slope;            // BPM at the tick being added
            time_ms += 625.0 / bpm;          // duration of that tick, in ms
            ++position_ticks;                          // advance the cursor
        }
        return time_ms;
    }
};


class Clocking {
	
    uint32_t _length_ticks = 0;
    double _length_time_ms = 0.0;
	std::list<Tempo> _tempos;
    std::vector<MidiDevice*> _clocked_devices;
	mutable RampCursor _ramp_cursor;


	static double extrapolateAbsoluteTime_ms(const Tempo& tempo, uint32_t ticks) {
		uint32_t tempo_ticks = tempo.getPositionTicks();
		double left_time_ms = tempo.getTime_ms();
		if (ticks > tempo_ticks) {
			int16_t tempo_bpm_10 = tempo.getBPM_10();
			return left_time_ms + (double)(ticks - tempo_ticks) * 625.0 / (double)tempo_bpm_10;
		}
		return left_time_ms;
	}


	// Most compatible method for varying BPMs
	double interpolateAbsoluteTime_ms(const Tempo& left, const Tempo& right, uint32_t ticks) const {

		uint32_t left_ticks  = left.getPositionTicks();
		uint32_t right_ticks = right.getPositionTicks();

		if (left_ticks < right_ticks && ticks > left_ticks && ticks <= right_ticks) {
			// Update cursor (`_ramp_cursor.tick > ticks` because cursor can't move backwards)
			if (_ramp_cursor.position_ticks <= left_ticks || _ramp_cursor.position_ticks > ticks) {
				_ramp_cursor.updateCursor(left, right);
			}
			// Move cursor
			return _ramp_cursor.moveCursor(ticks);
		}
		return left.getTime_ms();
	}


	std::list<Tempo>::const_iterator pickLeftTempo_it(
			std::list<Tempo>::const_iterator left_tempo_it,
			uint32_t at_position_ticks
		) const {

		// Picks the left tempo iterator
		for (auto tempo_it = std::next(left_tempo_it); ; ++tempo_it) {
			if (tempo_it == _tempos.end() || tempo_it->getPositionTicks() > at_position_ticks) {	// It's the pin that one needs to keep up
				return std::prev(tempo_it);
			}
		}
		return left_tempo_it;
	}

public:

	// First to run
	void setLengthTicks(uint32_t num, uint32_t den) {
		if (den == 1) {	// Measures have an integer amount of Beats
			_length_ticks = getTicksFromBeats(num, den);
		}
	}

	uint32_t getLengthTicks() const {
		return _length_ticks;
	}

	double getLengthTime_ms() const {
		return _length_time_ms;
	}

	void addTempo(int16_t bpm_10, uint32_t num, uint32_t den) {
		if (bpm_10 > 0 && den > 0) {
			_tempos.emplace_back(bpm_10, getTicksFromBeats(num, den));
		}
	}

	void addDevice(MidiDevice* midi_device) {
		_clocked_devices.push_back(midi_device);
	}


	size_t addClockMessagesToPlay(std::list<MidiPin> *midiPins) const {
		size_t added_messages = 0;
		// _length_ticks is a multiple of TICKS_PER_CLOCK, beats multiples
		size_t total_clock_pins = _length_ticks / TICKS_PER_CLOCK;
		
		for (const auto& device : _clocked_devices) {
			// New Start clock message with High Priority 3.0 (Let's messages like Program Change go first)
			midiPins->push_back( MidiPin(TICKS_PER_CLOCK * 0, device, { system_clock_start }, 0x30) );
			for (size_t pin_i = 1; pin_i < total_clock_pins; pin_i++) {
				// New clock message with High Priority 3.1 (Let's messages like Program Change go first)
				midiPins->push_back( MidiPin((uint32_t)(TICKS_PER_CLOCK * pin_i), device, { system_timing_clock }, 0x31) );
				added_messages++;
			}
			// New Stop clock message with Lowest priority 11.0
			midiPins->push_back( MidiPin((uint32_t)(TICKS_PER_CLOCK * total_clock_pins), device, { system_clock_stop }, 0xB0) );
			// New Stop clock message with Lowest priority 11.1
			midiPins->push_back( MidiPin((uint32_t)(TICKS_PER_CLOCK * total_clock_pins), device, { system_song_pointer, 0, 0 }, 0xB1) );
			added_messages += 3;	// for Start, Stop and Pointer messages
		}
		return added_messages;
	}


	void applyTime_ms(std::list<MidiPin> *midiPins_sorted) {
    	if (_tempos.empty()) {
			// In this scenario all the pins ned to be removed or they will be triggered at the same time at 0 ms !
    		midiPins_sorted->clear();
			return;	// Failsafe
		}

		_tempos.sort();	// Guarantees the tempos are sorted by ticks first

		// Makes sure there is a `Tempo` at the origin (ticks == 0)
		auto first_tempo_it = _tempos.begin();
		uint32_t first_position_ticks = first_tempo_it->getPositionTicks();
		if (first_position_ticks > 0) {
			int16_t origin_bpm_10 = first_tempo_it->getBPM_10();
			_tempos.emplace_front(origin_bpm_10, 0);
		}

		for (auto tempo_it = std::next(_tempos.begin()); tempo_it != _tempos.end(); ++tempo_it) {
			auto previous_it = std::prev(tempo_it);
			uint32_t previous_ticks = previous_it->getPositionTicks();
			uint32_t tempo_ticks = tempo_it->getPositionTicks();
			if (tempo_ticks > previous_ticks) {
				if (tempo_it->getBPM_10() == previous_it->getBPM_10()) {
					tempo_it->setTime_ms(
						extrapolateAbsoluteTime_ms(*previous_it, tempo_ticks)
					);
				} else {
					tempo_it->setTime_ms(
						interpolateAbsoluteTime_ms(*previous_it, *tempo_it, tempo_ticks)
					);
				}
			} else {
				tempo_it->setTime_ms(
					previous_it->getTime_ms()
				);
			}
		}

		// To be compatible with the `pickLeftTempo_it` method
		std::list<Tempo>::const_iterator left_tempo_it = _tempos.begin();
		// Adds the cumulative Time
		uint32_t previous_pin_position_ticks = 0;
		double pin_time_ms = 0.0;	// The tick 0 one is by definition at 0.0
		for (auto pin_it = midiPins_sorted->begin(); pin_it != midiPins_sorted->end(); ++pin_it) {

			// Pins above the length of the clocking are removed
			uint32_t pin_ticks = pin_it->getPositionTicks();

			// Updates the pin_time_ms if needed
			if (pin_ticks > previous_pin_position_ticks) {
				// Picks the right left tempo
				left_tempo_it = pickLeftTempo_it(left_tempo_it, pin_ticks);
				if (std::next(left_tempo_it) == _tempos.end() || left_tempo_it->getBPM_10() == std::next(left_tempo_it)->getBPM_10()) {
					pin_time_ms = extrapolateAbsoluteTime_ms(*left_tempo_it, pin_ticks);
				} else {
					pin_time_ms = interpolateAbsoluteTime_ms(*left_tempo_it, *std::next(left_tempo_it), pin_ticks);
				}
				previous_pin_position_ticks = pin_ticks;
			}
			pin_it->setTime_ms(pin_time_ms);
		}
		// Sets the Clocking length time_ms
		if (_length_ticks == previous_pin_position_ticks) {
			_length_time_ms = pin_time_ms;
		} else {
			// Picks the right left tempo
			left_tempo_it = pickLeftTempo_it(left_tempo_it, _length_ticks);
			if (std::next(left_tempo_it) == _tempos.end() || left_tempo_it->getBPM_10() == std::next(left_tempo_it)->getBPM_10()) {
				_length_time_ms = extrapolateAbsoluteTime_ms(*left_tempo_it, _length_ticks);
			} else {
				_length_time_ms = interpolateAbsoluteTime_ms(*left_tempo_it, *std::next(left_tempo_it), _length_ticks);
			}
		}
	}
	
	
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
};



struct PlayReporting {
	size_t json_processing  = 0;    // milliseconds
	size_t ports_opening  	= 0;    // milliseconds
	size_t total_generated  = 0;
	size_t total_validated  = 0;
	size_t total_incorrect  = 0;
	size_t total_redundant  = 0;
	double total_drag       = 0.0;
	double total_delay      = 0.0;
	double maximum_delay    = 0.0;
	double minimum_delay    = 0.0;
	double average_delay    = 0.0;
	double sd_delay         = 0.0;
};


class Player {

	std::chrono::high_resolution_clock::time_point data_processing_start;
	PlayReporting play_reporting;

	Clocking clocking;
    std::vector<MidiDevice> available_midi_devices;
    std::list<MidiPin> midiPins;

    #ifdef DEBUGGING
    std::chrono::high_resolution_clock::time_point debugging_start;
    std::chrono::high_resolution_clock::time_point debugging_now;
    std::chrono::high_resolution_clock::time_point debugging_last;
    long long completion_time_us = 0;
    #endif

public:

	Player() {
		
		disableBackgroundThrottling();
		// Set real-time scheduling
		setRealTimeScheduling();
    
		#ifdef DEBUGGING
		debugging_start = std::chrono::high_resolution_clock::now();
		debugging_now = debugging_start;
		debugging_last = debugging_start;
		#endif

	}

	int readAvailableDevices(bool verbose) {

        try {
            RtMidiOut midiOut;  // Temporary MidiOut manipulator
            unsigned int nPorts = midiOut.getPortCount();
            if (nPorts == 0) {
                if (verbose) std::cout << "No output Midi devices available.\n";
                return 1;
            }
            if (verbose) std::cout << "Available output Midi devices:\n";
            for (unsigned int i = 0; i < nPorts; i++) {
                std::string portName = midiOut.getPortName(i);
                if (verbose) std::cout << "\tMidi device #" << i << ": " << portName << std::endl;
                available_midi_devices.push_back(MidiDevice(portName, i, verbose));   // The object is copied
            }
            if (available_midi_devices.empty()) {
                if (verbose) std::cout << "\tNo output Midi devices available.\n";
                return 1;
            }
        } catch (RtMidiError &error) {
            error.printMessage();
            return EXIT_FAILURE;
        }
		return 0;
	}


	int loadJsonContent(const char* json_str, bool verbose) {

        if (verbose) std::cout << "Devices connected:    ";

        auto data_processing_start = std::chrono::high_resolution_clock::now();

        try {

			nlohmann::json root = nlohmann::json::parse(json_str);

			// index the first element, the only one
			const auto& jsonData = root.at(0);

			nlohmann::json jsonFileType;
			nlohmann::json jsonFileUrl;
			nlohmann::json jsonFileClocking;
			nlohmann::json jsonFilePlaylist;

			try
			{
				jsonFileType = jsonData["filetype"];
				jsonFileUrl = jsonData["url"];
				jsonFileClocking = jsonData["clocking"];
				jsonFilePlaylist = jsonData["playlist"];
			}
			catch (nlohmann::json::parse_error& ex)
			{
				if (verbose) std::cerr << "Unable to extract json data: " << ex.byte << std::endl;
				goto skip_reading_items;
			}
			
			if (jsonFileType != FILE_TYPE || jsonFileUrl != FILE_URL) {
				if (verbose) std::cerr << "Wrong type of file!" << std::endl;
				goto skip_reading_items;
			}

			// Set Length
			const auto& lb = jsonFileClocking.at("length_beats");
			uint32_t length_beats_num = lb.at(0).get<uint32_t>();
			uint32_t length_beats_den = lb.at(1).get<uint32_t>();
			clocking.setLengthTicks(length_beats_num, length_beats_den);

			if (clocking.getLengthTicks() > 0) {

				// Load the Tempos
				nlohmann::json jsonFileClocking_tempos = jsonFileClocking.at("tempos");
				if (jsonFileClocking_tempos.is_array() && !jsonFileClocking_tempos.empty()) {
					try {
						for (auto jsonClockingTempo : jsonFileClocking_tempos) {

							int16_t bpm_10 = jsonClockingTempo["bpm_10"];
							const auto& pb = jsonClockingTempo.at("position_beats");
							uint32_t position_beats_num = pb.at(0).get<uint32_t>();
							uint32_t position_beats_den = pb.at(1).get<uint32_t>();
							clocking.addTempo(
								bpm_10, position_beats_num, position_beats_den
							);
						}
					} catch (const nlohmann::json::exception& e) {
						if (verbose) std::cerr << "JSON error: " << e.what() << std::endl;
						goto skip_reading_items;
					} catch (const std::exception& e) {
						if (verbose) std::cerr << "Error: " << e.what() << std::endl;
						goto skip_reading_items;
					} catch (...) {
						if (verbose) std::cerr << "Unknown error occurred." << std::endl;
						goto skip_reading_items;
					}
				}

				// Dictionary where the key is a JSON list
				std::unordered_map<std::string, MidiDevice*> connected_devices_by_name;
				std::unordered_set<std::string> unavailable_devices;
				
				// Load the Devices
				nlohmann::json jsonFileClocking_devices = jsonFileClocking.at("devices");
				if (jsonFileClocking_devices.is_array() && !jsonFileClocking_devices.empty()) {

					try {
						// Keeps the last called device in the JsonMidiPlayer file
						MidiDevice *last_called_midi_device = nullptr;

						for (std::string jsonClockingDevice_name : jsonFileClocking_devices) {

							if (connected_devices_by_name.find(jsonClockingDevice_name) != connected_devices_by_name.end()) {
								last_called_midi_device = connected_devices_by_name[jsonClockingDevice_name];
								goto skip_to_next_device;
							}
					
							if (unavailable_devices.find(jsonClockingDevice_name) != unavailable_devices.end()) {
								continue;
							}
					
							for (auto &available_device : available_midi_devices) {
								if (available_device.getName().find(jsonClockingDevice_name) != std::string::npos) {
									//
									// Where the Device Port is connected/opened (Main reason for errors)
									//
									auto port_opening_start = std::chrono::high_resolution_clock::now();

									bool device_available = available_device.openPort();

									auto port_opening_finish = std::chrono::high_resolution_clock::now();
									auto port_processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(port_opening_finish - port_opening_start);
									play_reporting.ports_opening += port_processing_time.count();

									if (device_available) {	// Where the connection happens
										connected_devices_by_name[jsonClockingDevice_name] = &available_device; 
										last_called_midi_device = &available_device;

										clocking.addDevice(&available_device);

										goto skip_to_next_device; // For Message devices only the first one found is connected and NOT all of them

									} else {
										connected_devices_by_name[jsonClockingDevice_name] = nullptr; 
									}
								} else {
									unavailable_devices.insert(jsonClockingDevice_name);
								}
							}
							skip_to_next_device: ;	// Does nothing, just jumps to next device
						}
					} catch (const nlohmann::json::exception& e) {
						if (verbose) std::cerr << "JSON error: " << e.what() << std::endl;
						goto skip_reading_items;
					} catch (const std::exception& e) {
						if (verbose) std::cerr << "Error: " << e.what() << std::endl;
						goto skip_reading_items;
					} catch (...) {
						if (verbose) std::cerr << "Unknown error occurred." << std::endl;
						goto skip_reading_items;
					}
				}

				// Check if jsonFilePlaylist is a non-empty array
				if (jsonFilePlaylist.is_array() && !jsonFilePlaylist.empty()) {

					// Keeps the last called device in the JsonMidiPlayer file
					MidiDevice *last_called_midi_device = nullptr;
					// Just the declarations, no need to set them
					unsigned char data_byte_1;
					unsigned char data_byte_2;
					unsigned char priority;

					for (auto jsonPlaylistItem : jsonFilePlaylist)
					{
						// Most of the time it's a midi_message being processed, so it makes sense to be the first to check
						if (jsonPlaylistItem.contains("midi_message")) {

							if (last_called_midi_device != nullptr) {

								play_reporting.total_incorrect++;

								// Create an API with the default API
								try
								{
									const auto& pb = jsonPlaylistItem.at("position_beats");
									uint32_t position_beats_num = pb.at(0).get<uint32_t>();
									uint32_t position_beats_den = pb.at(1).get<uint32_t>();
									if (position_beats_num < 0 || position_beats_den <= 0) {

										continue;
										
									} else {

										unsigned char status_byte = jsonPlaylistItem["midi_message"]["status_byte"];
										std::vector<unsigned char> json_midi_message = { status_byte }; // Starts the json_midi_message to a new Status Byte
										
										unsigned char message_action = status_byte & 0xF0;

										// Where the Midi message is set
										switch (message_action) {
											case action_note_off:
											case action_note_on:
											case action_control_change:
											case action_pitch_bend:
											case action_key_pressure:
											{
												// This is already a try catch situation
												data_byte_1 = jsonPlaylistItem["midi_message"]["data_byte_1"];
												data_byte_2 = jsonPlaylistItem["midi_message"]["data_byte_2"];
												if (data_byte_1 & 128 | data_byte_2 & 128)
													continue;
												json_midi_message.push_back(data_byte_1);
												json_midi_message.push_back(data_byte_2);
												break;
											}
											case action_program_change:
											case action_channel_pressure:
											{
												data_byte_1 = jsonPlaylistItem["midi_message"]["data_byte"];
												if (data_byte_1 & 128)
													continue;
												json_midi_message.push_back(data_byte_1);
												break;
											}
											default:
												break;
										}

										// Where the Priority is set
										switch (message_action) {
											case action_note_off:
												priority = 0x40 | status_byte & 0x0F;       // Normal priority 4 for Off
												break;
											case action_note_on:
												priority = 0x50 | status_byte & 0x0F;       // Normal priority 5 for On
												break;
											case action_control_change:
												if (data_byte_1 == 1) {             // Modulation
													priority = 0x60 | status_byte & 0x0F;       // Low priority 6
												} else if (data_byte_1 == 0 || data_byte_1 == 32) {
													// 0 -  Bank Select (MSB)
													// 32 - Bank Select (LSB)
													priority = 0x10;                            // High priority 1.0	(Equivalent to Program Change)
												} else if (data_byte_1 == 123) {
													// 123 - All notes off (0x7B)
													// shall come after Notes On and Off
													priority = 0x90 | status_byte & 0x0F;       // Low priority 9
												} else {
													priority = 0x20 | status_byte & 0x0F;       // High priority 2
												}
												break;
											case action_pitch_bend:
												priority = 0x70 | status_byte & 0x0F;           // Low priority 7
												break;
											case action_key_pressure:
												priority = 0x80 | status_byte & 0x0F;           // Low priority 8
												break;
											case action_program_change:
												priority = 0x11;                            // High priority 1.1
												break;
											case action_channel_pressure:
												priority = 0x80 | status_byte & 0x0F;       // Low priority 8
												break;
											default:
												continue;   // Not a valid message, no priority given, jumps to the next one
										}

										midiPins.push_back(
											MidiPin(position_beats_num, position_beats_den, last_called_midi_device, json_midi_message, priority)
										);
										play_reporting.total_incorrect--;    // Cancels out the initial ++ increase at the beginning of the loop
										play_reporting.total_validated++;
									}
								}
								catch (const nlohmann::json::exception& e) {
									if (verbose) std::cerr << "JSON error: " << e.what() << std::endl;
									continue;
								} catch (const std::exception& e) {
									if (verbose) std::cerr << "Error: " << e.what() << std::endl;
									continue;
								} catch (...) {
									if (verbose) std::cerr << "Unknown error occurred." << std::endl;
									continue;
								}
							}

						// Where the last device is set based on the json "device" input
						} else if (jsonPlaylistItem.contains("devices")) {

							// The devices JSON list key
							nlohmann::json json_device_names = jsonPlaylistItem["devices"];

							last_called_midi_device = nullptr; // No available device found at start
							// It's a list of Devices that is given as Device
							for (std::string device_name : json_device_names) {
								
								if (connected_devices_by_name.find(device_name) != connected_devices_by_name.end()) {
									last_called_midi_device = connected_devices_by_name[device_name];
									goto skip_to_next_item;
								}
						
								if (unavailable_devices.find(device_name) != unavailable_devices.end()) {
									continue;
								}
						
								for (auto &available_device : available_midi_devices) {
									if (available_device.getName().find(device_name) != std::string::npos) {
										//
										// Where the Device Port is connected/opened (Main reason for errors)
										//
										auto port_opening_start = std::chrono::high_resolution_clock::now();

										bool device_available = available_device.openPort();

										auto port_opening_finish = std::chrono::high_resolution_clock::now();
										auto port_processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(port_opening_finish - port_opening_start);
										play_reporting.ports_opening += port_processing_time.count();

										if (device_available) {	// Where the connection happens
											connected_devices_by_name[device_name] = &available_device; 
											last_called_midi_device = &available_device;

											goto skip_to_next_item; // For Message devices only the first one found is connected and NOT all of them

										} else {
											connected_devices_by_name[device_name] = nullptr; 
										}
									} else {
										unavailable_devices.insert(device_name);
									}
								}
							}
						}
					skip_to_next_item: ;    // Does nothing, just jumps to next item
					}

				} else {
					if (verbose) std::cout << "JSON file is empty." << std::endl;
				}
			} else {
				if (verbose) std::cout << "Clocking Length is 0." << std::endl;
			}
        } catch (const nlohmann::json::parse_error& e) {
            if (verbose) std::cerr << "JSON parse error: " << e.what() << std::endl;
        }
		
		skip_reading_items: ;	// Does nothing, just stops reading items
        if (verbose) std::cout << std::endl;
		return 0;
	}


	void processMidiPins() {

		#ifdef DEBUGGING
		debugging_now = std::chrono::high_resolution_clock::now();
		completion_time = std::chrono::duration_cast<std::chrono::microseconds>(debugging_now - debugging_last);
		completion_time_us = completion_time.count();
		std::cout << "SORTING FULLY PROCESSED IN: " << completion_time_us << " microseconds" << std::endl;
		debugging_last = std::chrono::high_resolution_clock::now();
		#endif

		midiPins.sort();	// Makes sure pins are sorted first

		// remove redundant pins
		for (auto pin_it = midiPins.begin(); pin_it != midiPins.end(); ) {

			// Auxiliary variables
			MidiPin &pluck_pin = *pin_it;	// Just an handy conversion
			MidiDevice &pluck_device = *pluck_pin.getDevice();
			// Position beats and ticks
			const uint32_t pin_actual_position_ticks = pluck_pin.getPositionTicks();

			// Starts by removing any pin out of the clocking length
			if (pin_actual_position_ticks > clocking.getLengthTicks()) {
				pin_it = midiPins.erase(pin_it);
				continue;
			}

			const auto midi_action = pluck_pin.getAction();

			switch (midi_action) {
				case action_note_off:
				{
					auto& dict_last_on = pluck_device.channelpitch_last_pins_note_on;
					uint16_t channel_pitch = pluck_pin.getChannel() << 8 | pluck_pin.getDataByte();
					
					if (dict_last_on.find(channel_pitch) != dict_last_on.end()) { // Note On in the dict found

						auto &last_note_on_pin = dict_last_on[channel_pitch];	// It's a MidiPin*&

						last_note_on_pin->decreaseNotePressedTimes();
						if (last_note_on_pin->getNotePressedTimes() != 0) {	// The Only configuration to release Note is 1
							pin_it = midiPins.erase(pin_it);
							++(play_reporting.total_redundant);  // Note Off as no Note On pair (STATS)
							// By erasing a pin above, there is no need to increase the pin iterator
							goto skip_to_next_pin;
						}
					}
					++pin_it; // Only increments if no removal
				}
				break;
				case action_note_on:
				{
					auto& dict_last_on = pluck_device.channelpitch_last_pins_note_on;
					uint16_t channel_pitch = pluck_pin.getChannel() << 8 | pluck_pin.getDataByte();

					if (dict_last_on.find(channel_pitch) != dict_last_on.end()) {	// Note On in the dict found

						auto &last_note_on_pin = dict_last_on[channel_pitch];	// It's a MidiPin*&

						if (last_note_on_pin->getNotePressedTimes() > 0) {

							// Position beats and ticks
							const uint32_t last_note_position_ticks = last_note_on_pin->getPositionTicks();

							last_note_on_pin->increaseNotePressedTimes();	// Because the remaining EXTRA note off
							if (pin_actual_position_ticks == last_note_position_ticks) {
								
								pin_it = midiPins.erase(pin_it);	// Can't trigger the same note twice at the same time
								++(play_reporting.total_redundant);	// STATS
								// By erasing a pin above, there is no need to increase the pin iterator

							} else {	// It's still triggerable
								
								// New note off message
								std::vector<unsigned char> midi_pin_message = {
									static_cast<unsigned char>(pluck_pin.getChannel() | action_note_off),
									pluck_pin.getDataByte(1),
									0	// Note off has velocity 0 (Data Byte 2)
								};
								pin_it = midiPins.insert(pin_it,   // Makes a copy to the place given by pin_it
									MidiPin(
											pin_actual_position_ticks,
											pluck_pin.getMidiDevice(),
											midi_pin_message
										)
									);
								play_reporting.total_generated++;
								// THIS IS RIGHT, NEW PIN ADDED, IT'S INTENDED TO BE TWO CONSECUTIVE SKIPS !!
								// Skips the previously inserted Note Off MidiPin
								++pin_it;  // Move the iterator to the next element
								// The usual increment given that it jumps the steps bellow
								++pin_it; // Only increments if no removal
							}
							goto skip_to_next_pin;
						}
					}
					// First timer Note On
					// It's safe to use a direct reference given that the Note On midi_pin note parameters are never changed
					dict_last_on[channel_pitch] = &pluck_pin;
					++pin_it; // Only increments if no removal
				}
				break;
				case action_key_pressure:
				{
					auto& dict_last = pluck_device.statusdatabyte_last_pin_controlchange;
					uint16_t status_byte = pluck_pin.getStatusByte();
					uint16_t data_byte = pluck_pin.getDataByte(1);
					uint16_t status_data_byte =  status_byte << 8 | data_byte;

					if (dict_last.find(status_data_byte) != dict_last.end()) {  // Key found
						auto &last_pin_16 = dict_last[status_data_byte];

						if (last_pin_16 == pluck_pin) {
							pin_it = midiPins.erase(pin_it);
							++(play_reporting.total_redundant);
						} else {
							last_pin_16.setDataByte(2, pluck_pin.getDataByte(2));
							++pin_it; // Only increment if no removal
						}
					} else {
						// Needs to use a pin dummy copy given that their midi parameters may be changed
						dict_last.emplace(status_data_byte, MidiPin(pluck_pin));    // Just a dummy copy
						++pin_it; // Only increment if no removal
					}
				}
				break;
				case action_pitch_bend:
				{
					unsigned char status_byte = pluck_pin.getStatusByte();
					auto& dict_last = pluck_device.statusbyte_last_pins_pitchbend;

					if (dict_last.find(status_byte) != dict_last.end()) {  // Key found
						auto &last_pin_8 = dict_last[status_byte];

						if (last_pin_8 == pluck_pin) {
							pin_it = midiPins.erase(pin_it);
							++(play_reporting.total_redundant);
						} else {
							last_pin_8.setDataByte(1, pluck_pin.getDataByte(1));
							last_pin_8.setDataByte(2, pluck_pin.getDataByte(2));
							++pin_it; // Only increment if no removal
						}
					} else {
						// Needs to use a pin dummy copy given that their midi parameters may be changed
						dict_last.emplace(status_byte, MidiPin(pluck_pin));    // Just a dummy copy
						++pin_it; // Only increment if no removal
					}
				}
				break;
				case action_channel_pressure:
				{
					unsigned char dict_key = pluck_pin.getStatusByte();
					auto& dict_last = pluck_device.statusbyte_last_pins_pitchbend;

					if (dict_last.find(dict_key) != dict_last.end()) {  // Key found
						auto &last_pin_8 = dict_last[dict_key];

						if (last_pin_8 == pluck_pin) {
							pin_it = midiPins.erase(pin_it);
							++(play_reporting.total_redundant);
						} else {
							last_pin_8.setDataByte(1, pluck_pin.getDataByte(1));
							++pin_it; // Only increment if no removal
						}
					} else {
						// Needs to use a pin dummy copy given that their midi parameters may be changed
						dict_last.emplace(dict_key, MidiPin(pluck_pin));    // Just a dummy copy
						++pin_it; // Only increment if no removal
					}
				}
				break;

				default:    // Includes Controle Change and Program Change 0xC0 (Never considered redundant!)
					++pin_it; // Only increment if no removal
				break;
			}

			skip_to_next_pin: ;	// Does nothing, just processes next pin
		}

		// Adds missing note off midi messages for unreleased notes
		for (auto &device : available_midi_devices) {
			
			if (device.hasPortOpen()) {
				
				// MIDI NOTES SHALL NOT BE LEFT PRESSED !!
				// Add the needed note off for all those still on at the end!
				// Iterate over all keys and values
				for (const auto& pair : device.channelpitch_last_pins_note_on) {
					// uint16_t channel_pitch = pair.first;
					auto& last_pin_note_on = pair.second;

					if (last_pin_note_on->getNotePressedTimes() > 0) {
						// Transform midi on in midi off
						std::vector<unsigned char> midi_pin_note_off_message = {
							static_cast<unsigned char>(last_pin_note_on->getChannel() | action_note_off),    // note_off_status_byte
							last_pin_note_on->getDataByte(1),
							0	// Note off has velocity 0 (Data Byte 2)
						};
						// Adds a new MidiPin as a copy to the list of pins to be processed
						uint32_t clocking_length_ticks = clocking.getLengthTicks();
						midiPins.push_back( MidiPin(clocking_length_ticks, &device, midi_pin_note_off_message) );
						play_reporting.total_generated++;
					}
				}
			}
		}
	}


	void addClockingPins() {
		size_t total_clock_messages = clocking.addClockMessagesToPlay(&midiPins);
		if (total_clock_messages > 0) {
			play_reporting.total_generated += total_clock_messages;
			midiPins.sort();
		}
	}


	void applyTime_ms() {

		clocking.applyTime_ms(&midiPins);

		#ifdef DEBUGGING
		debugging_now = std::chrono::high_resolution_clock::now();
		completion_time = std::chrono::duration_cast<std::chrono::microseconds>(debugging_now - debugging_last);
		completion_time_us = completion_time.count();
		std::cout << "SORTING FULLY PROCESSED IN: " << completion_time_us << " microseconds" << std::endl;
		debugging_last = std::chrono::high_resolution_clock::now();
		#endif
	}


	void reportProcessing(bool verbose) {
		if (verbose) {
			auto data_processing_finish = std::chrono::high_resolution_clock::now();
			auto data_processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(data_processing_finish - data_processing_start);
			play_reporting.json_processing = data_processing_time.count();
			size_t real_processing_json_time = 0;
			if (play_reporting.json_processing > play_reporting.ports_opening) {
				real_processing_json_time = play_reporting.json_processing - play_reporting.ports_opening;
			}

			// Where the reporting is finally done
			std::cout << "Data stats reporting:" << std::endl;
			std::cout << "\tMidi Messages processing time (ms):       " << std::setw(10) << real_processing_json_time << std::endl;
			std::cout << "\tMidi Ports opening time (ms):             " << std::setw(10) << play_reporting.ports_opening << std::endl;
			std::cout << "\tSingle loop length (beats):               " << std::setw(10) << clocking.getLengthTicks() / TICKS_PER_BEAT << std::endl;
			std::cout << "\tTotal generated Midi Messages (included): " << std::setw(10) << play_reporting.total_generated << std::endl;
			std::cout << "\tTotal validated Midi Messages (accepted): " << std::setw(10) << play_reporting.total_validated << std::endl;
			std::cout << "\tTotal incorrect Midi Messages (excluded): " << std::setw(10) << play_reporting.total_incorrect << std::endl;
			std::cout << "\tTotal redundant Midi Messages (excluded): " << std::setw(10) << play_reporting.total_redundant << std::endl;
			std::cout << "\tTotal resultant Midi Messages (included): " << std::setw(10) << midiPins.size() << std::endl;
		}
	}


	void printPlayingTime(int loop, bool verbose) {
		if (verbose) {
			size_t duration_time_sec = std::round(clocking.getLengthTime_ms() * loop / 1000);
			if (loop == 1) {
				std::cout << "The playlist will now be played in 1 loop for "
				<< duration_time_sec / 60 << " minutes and " << duration_time_sec % 60 << " seconds..." << std::endl;
			} else {
				std::cout << "The playlist will now be played in " << loop << " loops for "
				<< duration_time_sec / 60 << " minutes and " << duration_time_sec % 60 << " seconds..." << std::endl;
			}
		}
	}


	void loopPlaylist(int loop) {

		const uint32_t lengthTicks = clocking.getLengthTicks();
		const double lengthTime_ms = clocking.getLengthTime_ms();
		const auto playing_start = std::chrono::high_resolution_clock::now();

		for (int loop_i = 0; loop_i < loop; ++loop_i) {

			const double loopTime_ms = lengthTime_ms * loop_i;
			uint32_t position_ticks = 0;

			// Loop through the list and remove elements
			for (auto pin_it = midiPins.begin(); pin_it != midiPins.end(); ++pin_it) {

				// Auxiliary variables
				uint32_t pin_ticks = pin_it->getPositionTicks();

				// Pin position time
				long long next_pin_time_us = std::round((loopTime_ms + pin_it->getTime_ms() + play_reporting.total_drag) * 1000);
				if (pin_ticks > position_ticks) {

					auto playing_now = std::chrono::high_resolution_clock::now();
					auto elapsed_time = std::chrono::duration_cast<std::chrono::microseconds>(playing_now - playing_start);
					long long elapsed_time_us = elapsed_time.count();
					long long sleep_time_us = next_pin_time_us > elapsed_time_us ? next_pin_time_us - elapsed_time_us : 0;

					if (sleep_time_us > 0) highResolutionSleep(sleep_time_us);  // Sleep for x microseconds
					position_ticks = pin_ticks;
				}

				auto pluck_time = std::chrono::high_resolution_clock::now() - playing_start;
				pin_it->pluckTooth();  // as soon as possible! <----- Midi Send

				auto pluck_time_us = static_cast<double>(
					std::chrono::duration_cast<std::chrono::microseconds>(pluck_time).count()
				);
				double delay_time_ms = (pluck_time_us - next_pin_time_us) / 1000;
				pin_it->addDelayTime(delay_time_ms);

				// Process drag if existent
				if (delay_time_ms > DRAG_DURATION_MS) {
					play_reporting.total_drag += delay_time_ms - DRAG_DURATION_MS;  // Drag isn't Delay
				}
			}

			if (lengthTicks > position_ticks) {

				// Finish position time
				long long finish_time_us = std::round((loopTime_ms + lengthTime_ms + play_reporting.total_drag) * 1000);
				
				auto playing_now = std::chrono::high_resolution_clock::now();
				auto elapsed_time = std::chrono::duration_cast<std::chrono::microseconds>(playing_now - playing_start);
				long long elapsed_time_us = elapsed_time.count();
				long long sleep_time_us = finish_time_us > elapsed_time_us ? finish_time_us - elapsed_time_us : 0;

				if (sleep_time_us > 0) highResolutionSleep(sleep_time_us);  // Sleep for x microseconds
			}
		}
		
		#ifdef DEBUGGING
		debugging_now = std::chrono::high_resolution_clock::now();
		completion_time = std::chrono::duration_cast<std::chrono::microseconds>(debugging_now - debugging_last);
		completion_time_us = completion_time.count();
		std::cout << "PLAYING FULLY PROCESSED IN: " << completion_time_us << " microseconds" << std::endl;
		debugging_last = std::chrono::high_resolution_clock::now();
		#endif
	}


	void reportPlaying(bool verbose) {

		if (verbose) {

			if (!midiPins.empty()) {	// Avoids division by 0 with `midiPins.size() == 0`

				for (auto &midi_pin : midiPins) {
					auto delay_time_ms = midi_pin.getDelayTime();
					play_reporting.total_delay += delay_time_ms;
					if (delay_time_ms > play_reporting.maximum_delay) {
						play_reporting.maximum_delay = delay_time_ms;
					}
				}

				play_reporting.minimum_delay = play_reporting.maximum_delay;
				play_reporting.average_delay = play_reporting.total_delay / midiPins.size();

				for (auto &midi_pin : midiPins) {
					auto delay_time_ms = midi_pin.getDelayTime();
					if (delay_time_ms < play_reporting.minimum_delay) {
						play_reporting.minimum_delay = delay_time_ms;
					}
					play_reporting.sd_delay += std::pow(delay_time_ms - play_reporting.average_delay, 2);
				}

				play_reporting.sd_delay /= midiPins.size();
				play_reporting.sd_delay = std::sqrt(play_reporting.sd_delay);
			}

			std::cout << "Devices disconnected: ";
			// Exiting devices scope automatically disconnects them

			// Where the reporting is finally done
			std::cout << std::endl << "Midi stats reporting:" << std::endl;
			// Set fixed floating-point notation and precision
			std::cout << std::fixed << std::setprecision(3);
			std::cout << "\tTotal drag (ms):      " << std::setw(34) << play_reporting.total_drag << " \\" << std::endl;
			std::cout << "\tCumulative delay (ms):" << std::setw(34) << play_reporting.total_delay << " /" << std::endl;
			std::cout << "\tMaximum delay (ms): " << std::setw(36) << play_reporting.maximum_delay << " \\" << std::endl;
			std::cout << "\tMinimum delay (ms): " << std::setw(36) << play_reporting.minimum_delay << " /" << std::endl;
			std::cout << "\tAverage delay (ms): " << std::setw(36) << play_reporting.average_delay << " \\" << std::endl;
			std::cout << "\tStandard deviation of delays (ms):" << std::setw(36 - 14) << play_reporting.sd_delay << " /"  << std::endl;
		}
	}

};

    

#endif // MIDI_JSON_PLAYER_HPP
