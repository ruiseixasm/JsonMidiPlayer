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

// #define DEBUGGING true	// UNCOMMENT THIS LINE FOR DEBUGGING METRICS
#define FILE_TYPE "Json Midi Player"
#define FILE_URL  "https://github.com/ruiseixasm/JsonMidiPlayer"
#define VERSION   "8.2.0"
#define DRAG_DURATION_MS (1000.0/((120/60)*24))


// Declare the function in the header file
void disableBackgroundThrottling();
void setRealTimeScheduling();
void highResolutionSleep(long long microseconds);

int play(const char* json_str, int loops = 1, int rec_transport = 0, int mtc_fps = 30, bool verbose = false);


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

// Based on MMC as CC of Arturia KeyStep
const unsigned char mmc_cc_play				= 54;	// Play
const unsigned char mmc_cc_stop				= 51;	// Stop
const unsigned char mmc_cc_rec				= 50;	// Record
// The Arturia KeyStep 37 and KeyStep Pro
const unsigned char mmc_cc_rewind			= 52;	// Rewind
const unsigned char mmc_cc_fast_forward		= 53;	// Fast Forward


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


inline unsigned char messagePriority(unsigned char message_action, unsigned char data_byte_1 = 0) {

	// Where the Priority is set
	switch (message_action) {
		case action_note_off: 			return 0x40;     	// High priority 4 for Off
		case action_note_on: 			return 0x60;       	// Low priority 6 for On
		case action_control_change:
			switch (data_byte_1) {
				case 0:	// 0 -  Bank Select (MSB) / 32 - Bank Select (LSB)
				case 32:				return 0x10;		// Very high priority 1.0 (Equivalent to Program Change)
				case 123:				return 0x90;		// Low priority 9 (123 - All notes off (0x7B))
				default:				return 0x50;		// Average priority 5 (common CC messages) (between note on and off)
			}
		case action_pitch_bend: 		return 0x50;        // Average priority 5
		case action_key_pressure: 		return 0x80;        // Low priority 8 (After Key Pressing, Note On)
		case action_channel_pressure:	return 0x80;       	// Low priority 8
		case action_program_change: 	return 0x11;        // Very high priority 1.1

		case system_clock_start:		return 0x32;		// Start clock message with High Priority 3.2
		case system_timing_clock:		return 0x33;		// Clock message with High Priority 3.3
		case system_clock_stop:			return 0xB2;		// Stop clock message with Lowest priority 11.2
		case system_song_pointer:		return 0xB3;		// Song position message with Lowest priority 11.3

		// Any other message action, lowest priority given
		default: 						return 0xFF;
	}
}



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
        MidiDevice(std::string device_name, unsigned int device_port, bool verbose = false)
                    : name(device_name), port(device_port), verbose(verbose) { }
    
        // Move constructor
        MidiDevice(MidiDevice &&other) noexcept : midiOut(std::move(other.midiOut)),
                name(std::move(other.name)), port(other.port), verbose(other.verbose),
                opened_port(other.opened_port) { }
    
        // Delete the copy constructor and copy assignment operator
        MidiDevice(const MidiDevice &) = delete;
        MidiDevice &operator=(const MidiDevice &) = delete;
    

        bool openPort() {
			if (!opened_port && !unavailable_device) {
				try {
					midiOut.openPort(port);
					opened_port = true;
					if (verbose) std::cout << "\t" << name << std::endl;
				} catch (RtMidiError &error) {
					unavailable_device = true;
					error.printMessage();
				}
			}
			return opened_port;
		}


        void closePort() {
			if (opened_port) {
				midiOut.closePort();
				opened_port = false;
				if (verbose) std::cout << "\t" << name << std::endl;
			}
		}


        const std::string& getName() const {
			return name;
		}


        void sendMessage(const std::vector<unsigned char> *midi_message) {
			if (opened_port) {
				midiOut.sendMessage(midi_message);
			}
		}

	private:

        // Keeps MidiPin pointers by Channel_Pitch (uint16_t) (similar to byte_16)
        std::unordered_map<uint16_t, MidiPin*>		channelpitch_last_pins_note_on;			// For Note On tracking
        // Keeps MidiPin dummy copies, thus NOT pointers of MidiPin
        std::unordered_map<unsigned char, MidiPin*> statusbyte_last_pins;    		// For Pitch Bend and Aftertouch
        std::unordered_map<uint16_t, MidiPin*>      statusdatabyte_last_pins;		// For Control Change and Key Pressure
    
	public:

		void setLastNoteOnPin(uint16_t channel_pitch, MidiPin* pluck_pin) {
			channelpitch_last_pins_note_on[channel_pitch] = pluck_pin;
		}

		MidiPin* getLastNoteOnPin(uint16_t channel_pitch) const {
			auto it = channelpitch_last_pins_note_on.find(channel_pitch);
			
			if (it != channelpitch_last_pins_note_on.end()) {
				return it->second; // Last Note On
			}
			return nullptr; // Not found
		}


		void setLastStatusbyteOnPin(unsigned char status_byte, MidiPin* pluck_pin) {
			statusbyte_last_pins[status_byte] = pluck_pin;
		}

		MidiPin* getLastStatusbytePin(unsigned char status_byte) const {
			auto it = statusbyte_last_pins.find(status_byte);
			
			if (it != statusbyte_last_pins.end()) {
				return it->second; // Last Note On
			}
			return nullptr; // Not found
		}


		void setLastStatusDatabyteOnPin(uint16_t status_data_byte, MidiPin* pluck_pin) {
			statusdatabyte_last_pins[status_data_byte] = pluck_pin;
		}

		MidiPin* getLastStatusDatabytePin(uint16_t status_data_byte) const {
			auto it = statusdatabyte_last_pins.find(status_data_byte);
			
			if (it != statusdatabyte_last_pins.end()) {
				return it->second; // Last Note On
			}
			return nullptr; // Not found
		}
    };



class MidiPin {

private:
	bool mtc_pin = false;	// By default it isn't an MPC pin (without a tick)
    double time_ms = 0.0;   // Set afterwards based on the _position_beat
    uint32_t _ticks = 0;
    const unsigned char priority;
    MidiDevice * const midi_device = nullptr;
    std::vector<unsigned char> midi_message;  // Replaces midi_message[3]
    // Auxiliary variable for the final loops playing!!
    double delay_time_ms = 0.0;

	// Links to the respective Note Off and note status as removed or not
	MidiPin* note_off_pin = nullptr;
	bool removed_note = false;

public:
    // Pin DEFAULT constructor, no arguments,
    // needed for emplace and insert of the std::unordered_map inside MidiDevice class !!
    MidiPin()
        : time_ms(0.0),                 // Default to 0.0
        _ticks(0),                    	// Default to 0
        priority(0),                    // Default to 0
        midi_device(nullptr),           // Default to nullptr
        midi_message(),                 // Default to an empty vector
        delay_time_ms(0.0)             // Default to 0.0
    { }


    // Pin constructor from position_beats (num, den)
    MidiPin(uint32_t num, uint32_t den, MidiDevice* midi_device,
        const std::vector<unsigned char>& json_midi_message, const unsigned char priority = 0xFF)
            : _ticks(getTicksFromBeats(num, den)),
            midi_device(midi_device),
            midi_message(json_midi_message),    // Directly initialize midi_message
            priority(priority)
        { }


    // Pin constructor from ticks
    MidiPin(uint32_t ticks, MidiDevice* midi_device,
        const std::vector<unsigned char>& json_midi_message, const unsigned char priority = 0xFF)
            : _ticks(ticks),
            midi_device(midi_device),
            midi_message(json_midi_message),    // Directly initialize midi_message
            priority(priority)
        { }


    // Pin constructor from time_ms without ticks set
    MidiPin(double time_ms, MidiDevice* midi_device,
        const std::vector<unsigned char>& json_midi_message, const unsigned char priority = 0xFF)
            : mtc_pin(true),	// If set by `time_ms`, then it is an MPC pin
			time_ms(time_ms),
            midi_device(midi_device),
            midi_message(json_midi_message),    // Directly initialize midi_message
            priority(priority)
        { }


    // Pin copy constructor
    MidiPin(const MidiPin& other)
        : mtc_pin(other.mtc_pin),					  // Makes sure mtc_pin flag is preserved
		  time_ms(other.time_ms),                     // Copy the time_ms
          _ticks(other._ticks),       				  // Copy the position ticks
          midi_device(other.midi_device),             // Copy the pointer to the MidiDevice
          midi_message(other.midi_message),           // Copy the midi_message vector
          priority(other.priority),                   // Copy the priority
          delay_time_ms(other.delay_time_ms),         // Copy the delay_time_ms
          note_off_pin(other.note_off_pin),           // Copy the note off pin too
          removed_note(other.removed_note)            // Tags the note pin as removed
    { }


	bool isAnMtcPin() const {
		return mtc_pin;
	}
	

	void setTime_ms(double time_milliseconds) {
		time_ms = time_milliseconds;
	}

    double getTime_ms() const {
        return time_ms;
    }


    void setPositionTicks(uint32_t ticks) {
        _ticks = ticks;
    }

    uint32_t getPositionTicks() const {
        return _ticks;
    }


    MidiDevice *getMidiDevice() const {
        return midi_device;
    }


    void pluckTooth() {
		if (midi_device != nullptr)
			midi_device->sendMessage(&midi_message);
	}


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

	
	void setNoteOffPin(MidiPin* note_off_pin) {
		if (this->note_off_pin == nullptr) {
			this->note_off_pin = note_off_pin;
		}
	}

	MidiPin* getNoteOffPin() {
		return note_off_pin;
	}


	void removeNote() {
		removed_note = true;
		if (note_off_pin != nullptr) {
			note_off_pin->removed_note = true;
		}
	}

	bool noteRemoved() const {
		return removed_note;
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
    std::vector<MidiDevice*> _transport_devices;
    std::vector<MidiDevice*> _mmc_devices;
    std::vector<MidiDevice*> _mtc_devices;
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


	void addClockedDevice(MidiDevice* midi_device) {
		_clocked_devices.push_back(midi_device);
		#ifdef DEBUGGING
        std::cout << "\n\t\tADDED CLOCKED DEVICE" << std::endl;
		#endif
	}


	void addTransportDevice(MidiDevice* midi_device) {
		_transport_devices.push_back(midi_device);
		#ifdef DEBUGGING
        std::cout << "\n\t\tADDED TRANSPORT DEVICE" << std::endl;
		#endif
	}


	void addMMCDevice(MidiDevice* midi_device) {
		_mmc_devices.push_back(midi_device);
		#ifdef DEBUGGING
        std::cout << "\n\t\tADDED MMC DEVICE" << std::endl;
		#endif
	}


	void addMTCDevice(MidiDevice* midi_device) {
		_mtc_devices.push_back(midi_device);
		#ifdef DEBUGGING
        std::cout << "\n\t\tADDED MTC DEVICE" << std::endl;
		#endif
	}


	const std::vector<MidiDevice*>& getMTCDevices() const {
		return _mtc_devices;
	}


	size_t addClockAndTransportMessagesToPlay(std::list<MidiPin> *midiPins, bool rec_transport = false) const {
		size_t added_messages = 0;
		// _length_ticks is a multiple of TICKS_PER_CLOCK, beats multiples
		const size_t total_clock_pins = _length_ticks / TICKS_PER_CLOCK;
		
		const unsigned char clock_start_priority 	= messagePriority(system_clock_start);
		const unsigned char clock_timing_priority 	= messagePriority(system_timing_clock);
		const unsigned char clock_stop_priority 	= messagePriority(system_clock_stop);
		const unsigned char song_pointer_priority 	= messagePriority(system_song_pointer);
		// Clocked Devices
		for (const auto& device : _clocked_devices) {
			// New Start clock message with High Priority 3.2 (Let's messages like Program Change go first)
			midiPins->emplace_back((uint32_t)(TICKS_PER_CLOCK * 0), device,
				std::vector<uint8_t>{ system_clock_start }, clock_start_priority);

			for (size_t pin_i = 1; pin_i < total_clock_pins; pin_i++) {
				// New clock message with High Priority 3.3 (Let's messages like Program Change go first)
				midiPins->emplace_back((uint32_t)(TICKS_PER_CLOCK * pin_i), device,
					std::vector<uint8_t>{ system_timing_clock }, clock_timing_priority);
				added_messages++;
			}
			// New Stop clock message with Lowest priority 11.2
			midiPins->emplace_back((uint32_t)(TICKS_PER_CLOCK * total_clock_pins), device,
				std::vector<uint8_t>{ system_clock_stop }, clock_stop_priority);
			// New Stop clock message with Lowest priority 11.3
			midiPins->emplace_back((uint32_t)(TICKS_PER_CLOCK * total_clock_pins), device,
				std::vector<uint8_t>{ system_song_pointer, 0, 0 }, song_pointer_priority);
			added_messages += 3;	// for Start, Stop and Pointer messages
		}

		// Transport CC Messages
		const std::vector<unsigned char> cc_play_button_down 		= { action_control_change, mmc_cc_play, 127 };
		const std::vector<unsigned char> cc_play_button_up 			= { action_control_change, mmc_cc_play, 0 };
		const std::vector<unsigned char> cc_stop_button_down 		= { action_control_change, mmc_cc_stop, 127 };
		const std::vector<unsigned char> cc_stop_button_up 			= { action_control_change, mmc_cc_stop, 0 };
		const std::vector<unsigned char> cc_rec_button_down 		= { action_control_change, mmc_cc_rec, 127 };
		const std::vector<unsigned char> cc_rec_button_up 			= { action_control_change, mmc_cc_rec, 0 };
		const std::vector<unsigned char> cc_rewind_button_down 		= { action_control_change, mmc_cc_rewind, 127 };
		const std::vector<unsigned char> cc_rewind_button_up 		= { action_control_change, mmc_cc_rewind, 0 };
		for (const auto& device : _transport_devices) {
			if (rec_transport) {
				// MMC - Rec - New Start MMC message with High Priority 3.0 (Let's messages like Program Change go first)
				midiPins->emplace_back((uint32_t)(TICKS_PER_CLOCK * 0), device, cc_rec_button_down, clock_start_priority - 1);
				midiPins->emplace_back((uint32_t)(TICKS_PER_CLOCK * 0), device, cc_rec_button_up, clock_start_priority - 1);
			} else {
				// MMC - Play - New Start MMC message with High Priority 3.0 (Let's messages like Program Change go first)
				midiPins->emplace_back((uint32_t)(TICKS_PER_CLOCK * 0), device, cc_play_button_down, clock_start_priority - 1);
				midiPins->emplace_back((uint32_t)(TICKS_PER_CLOCK * 0), device, cc_play_button_up, clock_start_priority - 1);
			}
			// MMC - Stop - New Stop MMC message with Lowest priority 11.4
			midiPins->emplace_back((uint32_t)(TICKS_PER_CLOCK * total_clock_pins), device, cc_stop_button_down);
			midiPins->emplace_back((uint32_t)(TICKS_PER_CLOCK * total_clock_pins), device, cc_stop_button_up);
			// MMC - Rewind - New Reposition MMC message with Lowest priority 11.5
			midiPins->emplace_back((uint32_t)(TICKS_PER_CLOCK * total_clock_pins), device, cc_rewind_button_down);
			midiPins->emplace_back((uint32_t)(TICKS_PER_CLOCK * total_clock_pins), device, cc_rewind_button_up);
			added_messages += 3 * 2;
		}
		
		// Transport MMC Messages
		for (const auto& device : _mmc_devices) {
			if (rec_transport) {
				// MMC - Rec - New Start MMC message with High Priority 3.0 (Let's messages like Program Change go first)
				midiPins->emplace_back((uint32_t)(TICKS_PER_CLOCK * 0), device,
					std::vector<uint8_t>{ system_sysex_start, 0x7F, 0x7F, 0x06, 0x06, system_sysex_end },
					clock_start_priority - 1);
			} else {
				// MMC - Play - New Start MMC message with High Priority 3.0 (Let's messages like Program Change go first)
				midiPins->emplace_back((uint32_t)(TICKS_PER_CLOCK * 0), device,
					std::vector<uint8_t>{ system_sysex_start, 0x7F, 0x7F, 0x06, 0x02, system_sysex_end },
					clock_start_priority - 1);
			}
			// MMC - Stop - New Stop MMC message with Lowest priority 11.4
			midiPins->emplace_back((uint32_t)(TICKS_PER_CLOCK * total_clock_pins), device,
				std::vector<uint8_t>{ system_sysex_start, 0x7F, 0x7F, 0x06, 0x01, system_sysex_end });
			// MMC - Rewind - New Reposition MMC message with Lowest priority 11.5
			midiPins->emplace_back((uint32_t)(TICKS_PER_CLOCK * total_clock_pins), device,
				std::vector<uint8_t>{ system_sysex_start, 0x7F, 0x7F, 0x06, 0x05, system_sysex_end });
			added_messages += 3;
		}

		return added_messages;
	}



	void applyTime_ms(std::list<MidiPin> *midiPins) {
    	if (_tempos.empty()) {
			// In this scenario all the pins ned to be removed or they will be triggered at the same time at 0 ms !
    		midiPins->clear();
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


		midiPins->sort();	// Makes sure the pins are sorted first

		// To be compatible with the `pickLeftTempo_it` method
		std::list<Tempo>::const_iterator left_tempo_it = _tempos.begin();
		// Adds the cumulative Time
		uint32_t previous_pin_position_ticks = 0;
		double pin_time_ms = 0.0;	// The tick 0 one is by definition at 0.0
		for (auto pin_it = midiPins->begin(); pin_it != midiPins->end(); ++pin_it) {

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


	int readExistingDevices(bool verbose) {

        try {
            RtMidiOut midiOut;  // Temporary MidiOut manipulator
            unsigned int nPorts = midiOut.getPortCount();
            if (nPorts == 0) {
                if (verbose) std::cout << "No output Midi devices available.\n";
                return 1;
            }
            if (verbose) std::cout << "Existing output Midi devices:\n";
            for (unsigned int i = 0; i < nPorts; i++) {
                std::string portName = midiOut.getPortName(i);
                if (verbose) std::cout << "\t" << portName << std::endl;
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
		
        #ifdef DEBUGGING
        debugging_now = std::chrono::high_resolution_clock::now();
        auto completion_time = std::chrono::duration_cast<std::chrono::microseconds>(debugging_now - debugging_last);
        long long completion_time_us = completion_time.count();
        std::cout << "\t\tMIDI DEVICES FULLY PROCESSED IN: " << completion_time_us << " microseconds" << std::endl;
        debugging_last = std::chrono::high_resolution_clock::now();
        #endif

		return 0;
	}


	int loadJsonContent(const char* json_str, bool verbose) {

        if (verbose) std::cout << "Devices connected:" << std::endl;;

        data_processing_start = std::chrono::high_resolution_clock::now();

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
				std::cerr << "Unable to extract json data: " << ex.byte << std::endl;
				return 1;
			}
			
			if (jsonFileType != FILE_TYPE || jsonFileUrl != FILE_URL) {
				std::cerr << "Wrong type of file!" << std::endl;
				return 1;
			}

			// Set Length
			const auto& lb = jsonFileClocking.at("length_beats");
			uint32_t length_beats_num = lb.at(0).get<uint32_t>();
			uint32_t length_beats_den = lb.at(1).get<uint32_t>();
			clocking.setLengthTicks(length_beats_num, length_beats_den);

			// Load remaining Clocking data
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
						std::cerr << "Loading Tempos, JSON error: " << e.what() << std::endl;
						return 1;
					} catch (const std::exception& e) {
						std::cerr << "Loading Tempos, Error: " << e.what() << std::endl;
						return 1;
					} catch (...) {
						std::cerr << "Loading Tempos, Unknown error occurred." << std::endl;
						return 1;
					}
				}

				// Dictionary where the key is a JSON list
				std::unordered_map<std::string, MidiDevice*> devices_by_name;
				
				// [&] means: "This lambda may use variables from the surrounding function, and capture them by reference."
				auto load_clocking_devices = [&](const char* json_key, auto add_device) -> int {

					nlohmann::json json_devices = jsonFileClocking.at(json_key);

					if (json_devices.is_array() && !json_devices.empty()) {

						try {

							for (std::string json_device_name : json_devices) {

								for (auto& available_device : available_midi_devices) {

									if (available_device.getName().find(json_device_name) != std::string::npos) {

										devices_by_name[json_device_name] = &available_device;

										//
										// Where the Device Port is connected/opened (Main reason for errors)
										//
										auto port_opening_start = std::chrono::high_resolution_clock::now();
										bool connected_device = available_device.openPort();
										auto port_opening_finish = std::chrono::high_resolution_clock::now();
										auto port_processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(
											port_opening_finish - port_opening_start
										);
										play_reporting.ports_opening += port_processing_time.count();

										add_device(&available_device);
										break;	// Clocking connects ALL named devices BUT matches ONLY one per name
									}
								}
							}

						} catch (const nlohmann::json::exception& e) {
							std::cerr << "Loading: '" << json_key << "', JSON error: " << e.what() << std::endl;
							return 1;
						} catch (const std::exception& e) {
							std::cerr << "Loading: '" << json_key << "', Error: " << e.what() << std::endl;
							return 1;
						} catch (...) {
							std::cerr << "Loading: '" << json_key << "', Unknown error occurred." << std::endl;
							return 1;
						}
					}
					return 0;
				};

				if (load_clocking_devices(
					"clocked_devices",
					[&](MidiDevice* device) { clocking.addClockedDevice(device); }
				)) {
					return 1;
				}

				if (load_clocking_devices(
					"transport_devices",
					[&](MidiDevice* device) { clocking.addTransportDevice(device); }
				)) {
					return 1;
				}

				if (load_clocking_devices(
					"mmc_devices",
					[&](MidiDevice* device) { clocking.addMMCDevice(device); }
				)) {
					return 1;
				}

				if (load_clocking_devices(
					"mtc_devices",
					[&](MidiDevice* device) { clocking.addMTCDevice(device); }
				)) {
					return 1;
				}


				// Check if jsonFilePlaylist is a non-empty array
				if (jsonFilePlaylist.is_array() && !jsonFilePlaylist.empty()) {

					// Keeps the last called device in the JsonMidiPlayer file
					MidiDevice* last_called_midi_device = nullptr;
					// Keeps track of the last Midi Note On pin
					MidiPin* last_note_on = nullptr;
					// Just the declarations, no need to set them
					unsigned char data_byte_1;
					unsigned char data_byte_2;

					for (auto jsonPlaylistItem : jsonFilePlaylist)
					{
						// Most of the time it's a midi_message being processed, so it makes sense to be the first to check
						if (jsonPlaylistItem.contains("midi_message")) {

							play_reporting.total_incorrect++;

							// Create an API with the default API
							try
							{
								const auto& pb = jsonPlaylistItem.at("position_beats");
								uint32_t position_beats_num = pb.at(0).get<uint32_t>();
								uint32_t position_beats_den = pb.at(1).get<uint32_t>();
								if (position_beats_num < 0 || position_beats_den <= 0) {

									continue;
									
								} else {	// Where the Midi Messages are loaded

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
									
									unsigned char priority = messagePriority(message_action, data_byte_1);
									// `emplace_back` is faster than `push_back` because avoids an extra copy
									midiPins.emplace_back(position_beats_num, position_beats_den, last_called_midi_device, json_midi_message, priority);
									if (message_action == action_note_on) {
										last_note_on = &(midiPins.back());
									} else if (message_action == action_note_off && last_note_on != nullptr) {
										// Note Off always comes next to Note On in the playlist (in sequence)
										last_note_on->setNoteOffPin( &(midiPins.back()) );
									}

									play_reporting.total_incorrect--;    // Cancels out the initial ++ increase at the beginning of the for loop
									play_reporting.total_validated++;
								}
							}
							catch (const nlohmann::json::exception& e) {
								std::cerr << "Loading Midi Message JSON error: " << e.what() << std::endl;
								return 1;
							} catch (const std::exception& e) {
								std::cerr << "Loading Midi Message Error: " << e.what() << std::endl;
								return 1;
							} catch (...) {
								std::cerr << "Loading Midi Message Unknown error occurred." << std::endl;
								return 1;
							}

						// Where the last device is updated based on the json "device" input (repeated ones are skip)
						} else if (jsonPlaylistItem.contains("devices")) {

							// The devices JSON list key
							nlohmann::json json_device_names = jsonPlaylistItem["devices"];

							last_called_midi_device = nullptr; // No available device found at start
							// It's a list of Devices that is given as Device
							for (std::string device_name : json_device_names) {
								
								if (devices_by_name.find(device_name) != devices_by_name.end()) {
									last_called_midi_device = devices_by_name[device_name];	// Device already picked up (repeated ones are skip)
									goto skip_to_next_item;
								}
						
								for (auto &available_device : available_midi_devices) {
									if (available_device.getName().find(device_name) != std::string::npos) {
										//
										// Where the Device Port is connected/opened (Main reason for errors)
										//
										auto port_opening_start = std::chrono::high_resolution_clock::now();

										available_device.openPort();

										auto port_opening_finish = std::chrono::high_resolution_clock::now();
										auto port_processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(port_opening_finish - port_opening_start);
										play_reporting.ports_opening += port_processing_time.count();

										devices_by_name[device_name] = &available_device; 
										last_called_midi_device = &available_device;

										goto skip_to_next_item; // For Message devices only the first one found is connected and NOT all of them
									}
								}
							}
						}
					skip_to_next_item: ;    // Does nothing, just jumps to next item
					}

				} else {
					if (verbose) std::cout << "JSON file is empty." << std::endl;
					return 1;
				}
			} else {
				if (verbose) std::cout << "Clocking Length is 0." << std::endl;
				return 1;
			}
        } catch (const nlohmann::json::parse_error& e) {
            std::cerr << "JSON parse error: " << e.what() << std::endl;
			return 1;
        }
		
        #ifdef DEBUGGING
        debugging_now = std::chrono::high_resolution_clock::now();
        auto completion_time = std::chrono::duration_cast<std::chrono::microseconds>(debugging_now - debugging_last);
        long long completion_time_us = completion_time.count();
        std::cout << "\t\tJSON DATA FULLY PROCESSED IN: " << completion_time_us << " microseconds" << std::endl;
        debugging_last = std::chrono::high_resolution_clock::now();
        #endif

		return 0;
	}


	void prepareMidiPins() {

		midiPins.sort();	// Makes sure pins are sorted first

		#ifdef DEBUGGING
		debugging_now = std::chrono::high_resolution_clock::now();
		auto completion_time = std::chrono::duration_cast<std::chrono::microseconds>(debugging_now - debugging_last);
		long long completion_time_us = completion_time.count();
		std::cout << "\t\tSORTING FULLY PROCESSED IN: " << completion_time_us << " microseconds" << std::endl;
		debugging_last = std::chrono::high_resolution_clock::now();
		#endif

		// remove redundant pins
		for (auto pin_it = midiPins.begin(); pin_it != midiPins.end(); ) {

			// Auxiliary variables
			MidiPin &pluck_pin = *pin_it;	// Just an handy conversion
			MidiDevice* pluck_device = pluck_pin.getDevice();

			// Starts by removing any pin WITHOUT a Pluck Device (Safe Code)
			if (pluck_device == nullptr) {
				pin_it = midiPins.erase(pin_it);
				++(play_reporting.total_redundant);
				continue;	// Applies to the `for` loop (`switch` has no `continue`)
			}

			// Position beats and ticks
			const uint32_t pin_actual_position_ticks = pluck_pin.getPositionTicks();
			const auto midi_action = pluck_pin.getAction();

			// Also removes any pin out of the clocking length
			if (pin_actual_position_ticks > clocking.getLengthTicks()) {
				if (midi_action == action_note_off) {
					pluck_pin.setPositionTicks(
						clocking.getLengthTicks()
					);
				} else {
					pluck_pin.removeNote();
					pin_it = midiPins.erase(pin_it);
					++(play_reporting.total_redundant);
					continue;	// Applies to the `for` loop (`switch` has no `continue`)
				}
			}

			switch (midi_action) {
				case action_note_off:
				{
					if (pluck_pin.noteRemoved()) {
						pin_it = midiPins.erase(pin_it);
						++(play_reporting.total_redundant);  // Note Off as no Note On pair (STATS)
						// By erasing a pin above, there is no need to increase the pin iterator
						continue;
					}
					++pin_it; // Only increments if no removal
				}
				break;
				case action_note_on:
				{
					MidiPin* pluck_note_off_pin = pluck_pin.getNoteOffPin();
					if (pluck_note_off_pin == nullptr) {	// Unclosed notes shouldn't be played at all
						
						pin_it = midiPins.erase(pin_it);	// Can't trigger the same note twice at the same time
						++(play_reporting.total_redundant);	// STATS
						// By erasing a pin above, there is no need to increase the pin iterator
						continue;
					} else {

						uint16_t channel_pitch = pluck_pin.getChannel() << 8 | pluck_pin.getDataByte();
						auto last_note_on_pin = pluck_device->getLastNoteOnPin(channel_pitch);

						if (last_note_on_pin != nullptr) { // Note On in the dict found, if found then pressed times > 0!

							// Position beats and ticks
							const uint32_t last_note_position_ticks = last_note_on_pin->getPositionTicks();

							if (pin_actual_position_ticks == last_note_position_ticks) {
								// Transports the new overlapping finish to the previously pressed Note
								MidiPin* last_note_pin_note_off = last_note_on_pin->getNoteOffPin();
								if (last_note_pin_note_off != nullptr) {
									last_note_pin_note_off->setPositionTicks(
										pluck_note_off_pin->getPositionTicks()
									);
								}
								// Removes the overlapping note completely
								pluck_pin.removeNote();	// Sets as removed the respective Note Off too
								pin_it = midiPins.erase(pin_it);	// Can't trigger the same note twice at the same time
								++(play_reporting.total_redundant);	// STATS
								// By erasing a pin above, there is no need to increase the pin iterator
								continue;

							} else {	
								// Overlapping note, previous Note On Note Off need to be updated (No need for removal)
								MidiPin* last_note_note_off_pin = last_note_on_pin->getNoteOffPin();
								if (last_note_note_off_pin != nullptr) {	// Safe code
									// Only if overlapping is the Note Off position updated
									if (last_note_note_off_pin->getPositionTicks() > pin_actual_position_ticks) {
										// It's still triggerable, but bring forward the previous note note off
										last_note_note_off_pin->setPositionTicks(pin_actual_position_ticks);
									}
								}
							}
						}
						// First timer Note On
						// It's safe to use a direct reference given that the Note On midi_pin note parameters are never changed
						pluck_device->setLastNoteOnPin(channel_pitch, &pluck_pin);
						++pin_it; // Only increments if no removal
					}
				}
				break;
				case action_key_pressure:
				{
					uint16_t status_byte = pluck_pin.getStatusByte();
					uint16_t data_byte = pluck_pin.getDataByte(1);
					uint16_t status_data_byte =  status_byte << 8 | data_byte;
					auto last_status_data_byte_pin = pluck_device->getLastStatusDatabytePin(status_data_byte);

					if (last_status_data_byte_pin != nullptr) {  // Key found

						if (*last_status_data_byte_pin == pluck_pin) {
							pin_it = midiPins.erase(pin_it);
							++(play_reporting.total_redundant);
						} else {
							pluck_device->setLastStatusDatabyteOnPin(status_data_byte, &pluck_pin);
							++pin_it; // Only increment if no removal
						}
					} else {
						pluck_device->setLastStatusDatabyteOnPin(status_data_byte, &pluck_pin);
						++pin_it; // Only increment if no removal
					}
				}
				break;
				case action_pitch_bend:
				{
					unsigned char status_byte = pluck_pin.getStatusByte();
					auto last_status_byte_pin = pluck_device->getLastStatusbytePin(status_byte);

					if (last_status_byte_pin != nullptr) {  // Key found

						if (*last_status_byte_pin == pluck_pin) {
							pin_it = midiPins.erase(pin_it);
							++(play_reporting.total_redundant);
						} else {
							pluck_device->setLastStatusbyteOnPin(status_byte, &pluck_pin);
							++pin_it; // Only increment if no removal
						}
					} else {
						pluck_device->setLastStatusbyteOnPin(status_byte, &pluck_pin);
						++pin_it; // Only increment if no removal
					}
				}
				break;
				case action_channel_pressure:
				{
					unsigned char status_byte = pluck_pin.getStatusByte();
					auto last_status_byte_pin = pluck_device->getLastStatusbytePin(status_byte);

					if (last_status_byte_pin != nullptr) {  // Key found

						if (*last_status_byte_pin == pluck_pin) {
							pin_it = midiPins.erase(pin_it);
							++(play_reporting.total_redundant);
						} else {
							pluck_device->setLastStatusbyteOnPin(status_byte, &pluck_pin);
							++pin_it; // Only increment if no removal
						}
					} else {
						pluck_device->setLastStatusbyteOnPin(status_byte, &pluck_pin);
						++pin_it; // Only increment if no removal
					}
				}
				break;

				default:    // Includes Controle Change and Program Change 0xC0 (Never considered redundant!)
					++pin_it; // Only increment if no removal
				break;
			}
		}
	}


	void addClockingTransportPins(bool rec_transport = false) {
		play_reporting.total_generated += clocking.addClockAndTransportMessagesToPlay(&midiPins, rec_transport);
	}


	void applyTime_ms() {

		clocking.applyTime_ms(&midiPins);

		#ifdef DEBUGGING
		debugging_now = std::chrono::high_resolution_clock::now();
		auto completion_time = std::chrono::duration_cast<std::chrono::microseconds>(debugging_now - debugging_last);
		long long completion_time_us = completion_time.count();
		std::cout << "\t\tSETTING TIME MS IN: " << completion_time_us << " microseconds" << std::endl;
		debugging_last = std::chrono::high_resolution_clock::now();
		#endif
	}

	
	void addMtcPins(int mtc_fps = 30) {
		size_t added_messages = 0;

		double ms_per_quarter_frame = 1000.0 / 120.0;
		int fps_type = 3;
		int fps_value = 30;

		switch (mtc_fps) {
			case 24:  fps_type = 0; fps_value = 24; ms_per_quarter_frame = 1000.0 / (24.0 * 4.0); break;
			case 25:  fps_type = 1; fps_value = 25; ms_per_quarter_frame = 1000.0 / (25.0 * 4.0); break;
			case 29:  fps_type = 2; fps_value = 30; ms_per_quarter_frame = 1000.0 / (29.97 * 4.0); break;
			case 30:  
			default:  fps_type = 3; fps_value = 30; ms_per_quarter_frame = 1000.0 / (30.0 * 4.0); break;
		}

		uint8_t fullFrameHourByte = 0x00 | (fps_type << 5);

		const double totalDurationMs = clocking.getLengthTime_ms();
		// const by reference (&)
		const std::vector<MidiDevice*>& mtc_devices = clocking.getMTCDevices();

		const unsigned char clock_start_priority 	= messagePriority(system_clock_start);
		const unsigned char clock_timing_priority 	= messagePriority(system_timing_clock);

		// MTC Devices
		for (const auto& device : mtc_devices) {

			// =========================================================================
			// 1. SEND BIG MESSAGE ONLY ONCE (At tick zero, before the loop)
			// =========================================================================

			// Needs to be explicit concerning the `std::vector<uint8_t>`
			midiPins.emplace_back(0.0, device,
				std::vector<uint8_t>{ 0xF0, 0x7F, 0x7F, 0x01, 0x01, fullFrameHourByte, 0x00, 0x00, 0x00, 0xF7 },
				clock_start_priority
			);
			added_messages++;

			// =========================================================================
			// 2. AFTERWARDS, KEEPS SENDING ONLY THE SHORT MESSAGES (120 per second)
			// =========================================================================
			int totalQuarterFrames = static_cast<int>(std::floor(totalDurationMs / ms_per_quarter_frame));

			for (int qfCount = 0; qfCount <= totalQuarterFrames; ++qfCount) {
				int index = qfCount % 8;
				int totalFramesInTrack = qfCount / 4;

				// =========================================================================
				// *** CHANGED: 29.97 DROP-FRAME TIME CODE CALCULATION ***
				// =========================================================================
				int frame;
				int totalSeconds;
				int second;
				int totalMinutes;
				int minute;
				int hour;

				if (mtc_fps == 29) {
					// *** 29.97 DF: two frame numbers are skipped at the start of
					// *** every minute except minutes 00, 10, 20, 30, 40 and 50.
					int tenMinuteBlocks = totalFramesInTrack / 17982;
					int remainingFrames = totalFramesInTrack % 17982;

					int droppedFrames = tenMinuteBlocks * 18;

					if (remainingFrames >= 1800) {
						droppedFrames += 2 * ((remainingFrames - 1800) / 1798 + 1);
					}

					int timecodeFrames = totalFramesInTrack + droppedFrames;

					frame = timecodeFrames % 30;
					totalSeconds = timecodeFrames / 30;
					second = totalSeconds % 60;
					totalMinutes = totalSeconds / 60;
					minute = totalMinutes % 60;
					hour = totalMinutes / 60;
				}
				else {
					// *** UNCHANGED: NORMAL NON-DROP-FRAME CALCULATION ***
					frame  = totalFramesInTrack % fps_value;
					totalSeconds = totalFramesInTrack / fps_value;
					second = totalSeconds % 60;
					totalMinutes = totalSeconds / 60;
					minute = totalMinutes % 60;
					hour   = totalMinutes / 60;
				}
				// =========================================================================
				// *** END OF CHANGED SECTION ***
				// =========================================================================

				uint8_t dataNibble = 0;
				switch (index) {
					case 0: dataNibble = frame & 0x0F; break;
					case 1: dataNibble = (frame >> 4) & 0x0F; break;
					case 2: dataNibble = second & 0x0F; break;
					case 3: dataNibble = (second >> 4) & 0x0F; break;
					case 4: dataNibble = minute & 0x0F; break;
					case 5: dataNibble = (minute >> 4) & 0x0F; break;
					case 6: dataNibble = hour & 0x0F; break;
					case 7: dataNibble = ((hour >> 4) & 0x01) | ((fps_type & 0x03) << 1); break;
				}
				uint8_t dataByte = (index << 4) | (dataNibble & 0x0F);

				double triggerTimeMs = qfCount * ms_per_quarter_frame;

				// HERE: Only 2 bytes are sent with an interval of 8.33ms
				midiPins.emplace_back(triggerTimeMs, device, std::vector<uint8_t>{0xF1, dataByte}, clock_timing_priority);
				added_messages++;
			}
		}
		if (added_messages > 0) {
			play_reporting.total_generated += added_messages;
			// MTC message have NO ticks, must be sorted by time_ms
			midiPins.sort([](const MidiPin& a, const MidiPin& b) {
				// For messages without tick set, like the MTC ones
				if (a.getTime_ms() != b.getTime_ms()) {
					return a.getTime_ms() < b.getTime_ms();
				}
				return a.getPriority() < b.getPriority();
			});
		}
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
			std::cout << "\tTotal validated Midi Messages (accepted): " << std::setw(10) << play_reporting.total_validated << std::endl;
			std::cout << "\tTotal incorrect Midi Messages (excluded): " << std::setw(10) << play_reporting.total_incorrect << std::endl;
			std::cout << "\tTotal redundant Midi Messages (excluded): " << std::setw(10) << play_reporting.total_redundant << std::endl;
			std::cout << "\tTotal generated Midi Messages (included): " << std::setw(10) << play_reporting.total_generated << std::endl;
			std::cout << "\tTotal resultant Midi Messages (included): " << std::setw(10) << midiPins.size() << std::endl;
		}
	}


	void printPlayingTime(int loops, bool verbose) {
		if (verbose) {
			size_t duration_time_sec = std::round(clocking.getLengthTime_ms() * loops / 1000);
			if (loops == 1) {
				std::cout << "The playlist will now be played in 1 loop for "
				<< duration_time_sec / 60 << " minutes and " << duration_time_sec % 60 << " seconds..." << std::endl;
			} else {
				std::cout << "The playlist will now be played in " << loops << " loops for "
				<< duration_time_sec / 60 << " minutes and " << duration_time_sec % 60 << " seconds..." << std::endl;
			}
		}
	}


	void loopPlaylist(int loops) {

		const uint32_t lengthTicks = clocking.getLengthTicks();
		const double lengthTime_ms = clocking.getLengthTime_ms();
		const auto playing_start = std::chrono::high_resolution_clock::now();

		for (int loop_i = 0; loop_i < loops; ++loop_i) {

			const double loopTime_ms = lengthTime_ms * loop_i;
			uint32_t position_ticks = 0;

			// Loop through the list and remove elements
			for (auto pin_it = midiPins.begin(); pin_it != midiPins.end(); ++pin_it) {

				// Auxiliary variables
				uint32_t pin_ticks = pin_it->getPositionTicks();

				// Pin position time
				long long next_pin_time_us = std::round((loopTime_ms + pin_it->getTime_ms() + play_reporting.total_drag) * 1000);
				if (pin_ticks > position_ticks || pin_it->isAnMtcPin()) {	// MTC messages always at 0

					auto playing_now = std::chrono::high_resolution_clock::now();
					auto elapsed_time = std::chrono::duration_cast<std::chrono::microseconds>(playing_now - playing_start);
					long long elapsed_time_us = elapsed_time.count();
					long long sleep_time_us = next_pin_time_us > elapsed_time_us ? next_pin_time_us - elapsed_time_us : 0;

					if (sleep_time_us > 0) highResolutionSleep(sleep_time_us);  // Sleep for x microseconds

					#ifdef DEBUGGING
					if (next_pin_time_us - elapsed_time_us < 0) {
						std::cout << "\n\t\tNEGATIVE SLEEP TIME OF: " << next_pin_time_us - elapsed_time_us;
						std::cout << "\t\tIS MTC: " << pin_it->isAnMtcPin() << std::endl;
					}
					#endif

					if (!pin_it->isAnMtcPin()) {	// Because a MTC pin is always at 0 tick
						position_ticks = pin_ticks;
					}
				}

				auto pluck_time = std::chrono::high_resolution_clock::now() - playing_start;
				pin_it->pluckTooth();  // as soon as possible! <----- Midi Send

				auto pluck_time_us = static_cast<double>(
					std::chrono::duration_cast<std::chrono::microseconds>(pluck_time).count()
				);
				double delay_time_ms = (pluck_time_us - next_pin_time_us) / 1000;
				pin_it->addDelayTime(delay_time_ms);

				#ifdef DEBUGGING
				if (delay_time_ms < 0) {
					std::cout << "\t\tNEGATIVE DELAY OF: " << delay_time_ms;
					std::cout << "\t\tIS MTC: " << pin_it->isAnMtcPin() << std::endl;
				}
				#endif

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
		auto completion_time = std::chrono::duration_cast<std::chrono::microseconds>(debugging_now - debugging_last);
		long long completion_time_us = completion_time.count();
		std::cout << "\t\tPLAYING FULLY PROCESSED IN: " << completion_time_us << " microseconds" << std::endl;
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

			std::cout << "Devices disconnected:" << std::endl;
			// Disconnect midi devices
			for (auto &available_device : available_midi_devices) {
				available_device.closePort();
			}

			// Where the reporting is finally done
			std::cout << "Midi stats reporting:" << std::endl;
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


/*
    Voice Message           Status Byte      Data Byte1          Data Byte2
    -------------           -----------   -----------------   -----------------
    Note off                      8x      Key number          Note Off velocity
    Note on                       9x      Key number          Note on velocity
    Polyphonic Key Pressure       Ax      Key number          Amount of pressure
    Control Change                Bx      Controller number   Controller value
    Program Change                Cx      Program number      None
    Channel Pressure              Dx      Pressure value      None            
    Pitch Bend                    Ex      MSB                 LSB
    Song Position Ptr             F2      0                   0

    System Real-Time Message         Status Byte 
    ------------------------         -----------
    Timing Clock                         F8
    Start Sequence                       FA
    Continue Sequence                    FB
    Stop Sequence                        FC
    Active Sensing                       FE
    System Reset                         FF


	Action			MMC	SysEx
	Stop			F0 7F 7F 06 01 F7
	Play			F0 7F 7F 06 02 F7
	Deferred Play	F0 7F 7F 06 03 F7
	Fast Forward	F0 7F 7F 06 04 F7
	Rewind			F0 7F 7F 06 05 F7
	Record Strobe	F0 7F 7F 06 06 F7
	Record Exit		F0 7F 7F 06 07 F7
	Pause			F0 7F 7F 06 09 F7
	Locate			F0 7F 7F 06 44 … F7


    SysEx Message                    Status Byte 
    ------------------------         -----------
    0xF0: SysEx Start
    <Data Bytes>: Manufacturer ID + Command + Data
    0xF7: SysEx End

    self_playlist.append(
        {
            ...,
            "midi_message": {
                "status_byte": 0xF0,    # Start of SysEx
                "data_bytes": [0x7F, 0x7F, 0x06, 0x01],  # Universal Stop command
                "end_byte": 0xF7,       # End of SysEx
                "device": devices
            }
        }
    )


*/

