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


static constexpr uint32_t TICKS_PER_BEAT 	= 960;	// Internal PPQN
static constexpr uint32_t CLOCKS_PER_BEAT 	= 24;   // MIDI spec
static constexpr uint32_t TICKS_PER_CLOCK 	= TICKS_PER_BEAT / CLOCKS_PER_BEAT;	// = 40

class Beat {
    const uint32_t _ticks;

    // private constructor — factories call this
    explicit Beat(uint32_t ticks) : _ticks(ticks) {}

public:

	static uint32_t getTicksFromBeats(uint32_t num, uint32_t den) {
		// Equivalent to Beat((uint32_t)(beats * TICKS_PER_BEAT + 0.5));
		if (num < 0 || den <= 0) {
			return 0;
		}
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
	// For the sorting
    bool operator< (const MidiPin& mp) const {
		if (_ticks != mp._ticks) { return _ticks < mp._ticks; }
		return priority < mp.priority;
	}

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

	 
	double getTimeFromTicks(uint32_t position_ticks) const {
		return (double)position_ticks * 625 / _bpm_10;
	}

	double beatsToMs() const {
		return (double)_ticks * 625 / _bpm_10;
	}

    // ── tempo-aware conversion ──
    uint64_t toMicroseconds(int16_t bpm_10) const {
        // µs = ticks × 62,500 / bpm
        return (uint64_t)_ticks * 625000 / bpm_10;
    }


    int16_t getBPM_10() const {
        return _bpm_10;
    }

    uint32_t getPositionTicks() const {
        return _ticks;
    }

	void setTime(double time_milliseconds) {
		time_ms = time_milliseconds;
	}

    double getTime() const {
        return time_ms;
    }

    bool operator< (const Tempo& t) const { return _ticks <  t._ticks; }
    bool operator==(const Tempo& t) const { return _ticks == t._ticks; }
    bool operator!=(const Tempo& t) const { return _ticks != t._ticks; }
};


class Clocking {
    std::vector<MidiDevice*> _clocked_devices;
	std::list<Tempo> _tempos;

	double getClockTime_ms(uint32_t position_ticks) const {
		if (_tempos.size() > 0) {
			const Tempo& first_tempo = *_tempos.begin();
			return first_tempo.getTimeFromTicks(position_ticks);
		}
		return 0.0;
	}

public:

	void addDevice(MidiDevice* midi_device) {
		_clocked_devices.push_back(midi_device);
	}

	void addTempo(int16_t bpm_10, uint32_t position_ticks) {
		if (bpm_10 > 0) {
			_tempos.emplace_back(bpm_10, position_ticks);
		}
	}

	void addTempo(int16_t bpm_10, uint32_t num, uint32_t den) {
		if (bpm_10 > 0 && den > 0) {
			_tempos.emplace_back(bpm_10, Beat::getTicksFromBeats(num, den));
		}
	}

	bool sortTempos() {
    	if (_tempos.empty()) return false;

		_tempos.sort();	// Gurantees the tempos are sorted by ticks first

		// Makes sure there are a `Tempo` at the origin (ticks == 0)
		auto firsy_tempo_it = _tempos.begin();
		uint32_t first_position_ticks = firsy_tempo_it->getPositionTicks();
		if (first_position_ticks > 0) {
			int16_t origin_bpm_10 = firsy_tempo_it->getBPM_10();
			_tempos.emplace_front(origin_bpm_10, 0);
		}

		double cumulative_time_ms = 0.0;
		for (auto tempo_it = std::next(_tempos.begin()); tempo_it != _tempos.end(); ++tempo_it) {
			auto previous_it = std::prev(tempo_it);
			uint32_t position_ticks = tempo_it->getPositionTicks();
			cumulative_time_ms += interpolateTime_ms(
				*previous_it, *tempo_it, position_ticks
			);
			tempo_it->setTime(cumulative_time_ms);
		}
		return true;
	}

	size_t addClockMessagesToPlay(std::list<MidiPin> *midiToProcess) const {
		size_t added_mesages = 0;
		if (_clocked_devices.size() > 0 && midiToProcess->size() > 0) {
			const MidiPin& last_message = midiToProcess->back();
			uint32_t last_tick = last_message.getPositionTicks();
			size_t total_clock_ticks = (last_tick + TICKS_PER_CLOCK - 1) / TICKS_PER_CLOCK;	// Wraps outside messages
			
			for (const auto& device : _clocked_devices) {
				// New Start clock message with High Priority 3.0 (Let's messages like Program Change go first)
				midiToProcess->push_back( MidiPin(TICKS_PER_CLOCK * 0, device, { system_clock_start }, 0x30) );
				added_mesages++;
				for (size_t tick_i = 1; tick_i < total_clock_ticks; tick_i++) {
					// New clock message with High Priority 3.1 (Let's messages like Program Change go first)
					midiToProcess->push_back( MidiPin((uint32_t)(TICKS_PER_CLOCK * tick_i), device, { system_timing_clock }, 0x31) );
					added_mesages++;
				}
				// New Stop clock message with Lowest priority 11.0
				midiToProcess->push_back( MidiPin((uint32_t)(TICKS_PER_CLOCK * total_clock_ticks), device, { system_clock_stop }, 0xB0) );
				added_mesages++;
				// New Stop clock message with Lowest priority 11.1
				midiToProcess->push_back( MidiPin((uint32_t)(TICKS_PER_CLOCK * total_clock_ticks), device, { system_song_pointer, 0, 0 }, 0xB1) );
				added_mesages++;
			}
			midiToProcess->sort();	// Does the final sorting given the new pins
		}
		return added_mesages;
	}

	static double interpolateTime_ms(const Tempo& left, const Tempo& right, uint32_t ticks) {
		uint32_t left_ticks = left.getPositionTicks();
		uint32_t right_ticks = right.getPositionTicks();
		// Trapezoid (exclusion of `left_ticks == right_ticks`)
		if (left_ticks < right_ticks && ticks >= left_ticks && ticks <= right_ticks) {
			int16_t left_bpm_10 = left.getBPM_10();
			int16_t right_bpm_10 = right.getBPM_10();
			double slope = (double)(right_bpm_10 - left_bpm_10) / (double)(right_ticks - left_ticks);
			double delta_ticks_at_t = (double)(ticks - left_ticks);
			double bpm_10_at_t = (double)left_bpm_10 + slope * delta_ticks_at_t;
			return delta_ticks_at_t * 625.0 * 2.0 / ((double)left_bpm_10 + bpm_10_at_t);
		}
		return 0.0;
	}

	static double extrapolateTime_ms(const Tempo& tempo, uint32_t ticks) {
		uint32_t tempo_ticks = tempo.getPositionTicks();
		if (ticks >= tempo_ticks) {
			int16_t tempo_bpm_10 = tempo.getBPM_10();
			return (double)(ticks - tempo_ticks) * 625.0 / (double)tempo_bpm_10;
		}
		return 0.0;
	}

	bool applyTime_ms(std::list<MidiPin> *midiToProcess) const {
    	if (_tempos.empty()) return false;
		// `const_iterator` because this is a `const` method
		std::list<Tempo>::const_iterator left_tempo = _tempos.begin();
		std::list<Tempo>::const_iterator right_tempo = std::next(left_tempo);
		// Adds the cumulative Time
		double tempo_time_ms = 0.0;	// The first one is always 0.0
		for (auto pin_it = midiToProcess->begin(); pin_it != midiToProcess->end(); ++pin_it) {

			uint32_t pin_ticks = pin_it->getPositionTicks();
			double pin_time_ms = 0.0;
			if (right_tempo == _tempos.end()) {
				pin_time_ms = extrapolateTime_ms(*left_tempo, pin_ticks);
			} else {
				// Pich the right left tempo
				for (auto tempo_it = right_tempo; tempo_it != _tempos.end(); ++tempo_it) {
					
					uint32_t tempo_ticks = tempo_it->getPositionTicks();
					if (pin_ticks <= tempo_ticks) {	// It's the pin that one needs to keep up
						right_tempo = tempo_it;
						left_tempo = std::prev(tempo_it);
						break;
					}
					// Only if can't be found it updates the left_tempo
					left_tempo = tempo_it;
					right_tempo = std::next(left_tempo);
				}		
				tempo_time_ms = left_tempo->getTime();
				pin_time_ms = interpolateTime_ms(*left_tempo, *right_tempo, pin_ticks);
			}
			pin_it->setTime(tempo_time_ms + pin_time_ms);
		}
		return true;
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

    

// Declare the function in the header file
void disableBackgroundThrottling();

void setRealTimeScheduling();
void highResolutionSleep(long long microseconds);
int PlayList(const char* json_str, bool verbose = false);


#endif // MIDI_JSON_PLAYER_HPP
