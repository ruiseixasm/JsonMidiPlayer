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
#define VERSION   "7.3.0"
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
	
    inline static uint32_t _length_ticks = 0;
    inline static double _length_time_ms = 0.0;
	inline static std::list<Tempo> _tempos;
    inline static std::vector<MidiDevice*> _clocked_devices;


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
	static double interpolateAbsoluteTime_ms(const Tempo& left, const Tempo& right, uint32_t ticks) {
		// Cursor is shared by all instances each time this method is called, just one clocking (static member variables) !
		static RampCursor ramp_cursor;

		uint32_t left_ticks  = left.getPositionTicks();
		uint32_t right_ticks = right.getPositionTicks();

		if (left_ticks < right_ticks && ticks > left_ticks && ticks <= right_ticks) {
			// Update cursor (`ramp_cursor.tick > ticks` because cursor can't move backwards)
			if (ramp_cursor.position_ticks <= left_ticks || ramp_cursor.position_ticks > ticks) {
				ramp_cursor.updateCursor(left, right);
			}
			// Move cursor
			return ramp_cursor.moveCursor(ticks);
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

		// Makes sure there are a `Tempo` at the origin (ticks == 0)
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
		for (auto pin_it = midiPins_sorted->begin(); pin_it != midiPins_sorted->end(); ) {

			// Pins above the length of the clocking are removed
			uint32_t pin_ticks = pin_it->getPositionTicks();
			// Makes sure no out of clocking length pins are processed
			if (pin_ticks > _length_ticks) {
				pin_it = midiPins_sorted->erase(pin_it);
				continue;
			}

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
			++pin_it;	// Next pin
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

    

// Declare the function in the header file
void disableBackgroundThrottling();

void setRealTimeScheduling();
void highResolutionSleep(long long microseconds);
int PlayList(const char* json_str, int loop = 1, bool verbose = false);


#endif // MIDI_JSON_PLAYER_HPP
