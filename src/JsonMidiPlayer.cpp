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
#include "JsonMidiPlayer.hpp"

bool MidiDevice::openPort() {
    if (!opened_port && !unavailable_device) {
        try {
            midiOut.openPort(port);
            opened_port = true;
            if (verbose) std::cout << "Midi device connected: " << name << std::endl;
        } catch (RtMidiError &error) {
            unavailable_device = true;
            error.printMessage();
        }
    }
    return opened_port;
}

void MidiDevice::closePort() {
    if (opened_port) {
        midiOut.closePort();
        opened_port = false;
        if (verbose) std::cout << "Midi device disconnected: " << name << std::endl;
    }
}

bool MidiDevice::isPortOpened() const {
    return opened_port;
}

const std::string& MidiDevice::getName() const {
    return name;
}

unsigned int MidiDevice::getDevicePort() const {
    return port;
}

void MidiDevice::sendMessage(const std::vector<unsigned char> *midi_message) {
    midiOut.sendMessage(midi_message);
}



// #include <iostream>
// #ifdef _WIN32
// #include <windows.h>
// #include <mmsystem.h> // For timeBeginPeriod and timeEndPeriod
// #pragma comment(lib, "winmm.lib")
// #else
// #include <pthread.h>
// #include <sched.h>
// #include <errno.h>
// #endif

// // Function to set real-time scheduling
// void setRealTimeScheduling() {
// #ifdef _WIN32
//     // Increase system timer resolution
//     if (timeBeginPeriod(1) != TIMERR_NOERROR) {
//         std::cerr << "Failed to increase timer resolution." << std::endl;
//     }

//     // Set the process priority to REALTIME_PRIORITY_CLASS
//     if (!SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS)) {
//         std::cerr << "Failed to set process priority class to REALTIME_PRIORITY_CLASS. Error: " 
//                   << GetLastError() << std::endl;
//     }

//     // Set the thread priority to THREAD_PRIORITY_TIME_CRITICAL
//     if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL)) {
//         std::cerr << "Failed to set thread priority to THREAD_PRIORITY_TIME_CRITICAL. Error: " 
//                   << GetLastError() << std::endl;
//     } else {
//         std::cout << "Real-time scheduling successfully set on Windows." << std::endl;
//     }
// #else
//     // Set real-time scheduling on Linux
//     struct sched_param param;
//     param.sched_priority = sched_get_priority_max(SCHED_FIFO);

//     int result = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
//     if (result != 0) {
//         if (result == EPERM) {
//             std::cerr << "Failed to set real-time scheduling: Insufficient privileges. Run as root." 
//                       << std::endl;
//         } else {
//             std::cerr << "Failed to set real-time scheduling. Error: " << strerror(result) << std::endl;
//         }
//     } else {
//         std::cout << "Real-time scheduling successfully set on Linux." << std::endl;
//     }
// #endif
// }



// Function to set real-time scheduling
void setRealTimeScheduling() {
#ifdef _WIN32
    // Set the thread priority to highest for real-time scheduling on Windows
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
#else
    // Set real-time scheduling on Linux
    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
#endif
}

int PlayList(const char* json_str, bool verbose) {
    
    disableBackgroundThrottling();

    // Set real-time scheduling
    setRealTimeScheduling();
    
    #ifdef DEBUGGING
    auto debugging_start = std::chrono::high_resolution_clock::now();
    auto debugging_now = debugging_start;
    auto debugging_last = debugging_now;
    long long completion_time_us = 0;
    #endif

    struct PlayReporting {
        size_t pre_processing   = 0;    // milliseconds
        size_t total_processed  = 0;
        size_t total_redundant  = 0;
        size_t total_excluded   = 0;
        double total_drag       = 0.0;
        double total_delay      = 0.0;
        double maximum_delay    = 0.0;
        double minimum_delay    = 0.0;
        double average_delay    = 0.0;
        double sd_delay         = 0.0;
    };
    PlayReporting play_reporting;

    // Where the playing happens
    {
        std::vector<MidiDevice> midi_devices;
        std::list<MidiPin> midiToProcess;
        std::list<MidiPin> midiProcessed;
        std::list<MidiPin> midiRedundant;

        //
        // Where each Available Device is collected BUT NOT connected
        //

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
                midi_devices.push_back(MidiDevice(portName, i, verbose));
            }
            if (midi_devices.size() == 0) {
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
        completion_time_us = completion_time.count();
        std::cout << "MIDI DEVICES FULLY PROCESSED IN: " << completion_time_us << " microseconds" << std::endl;
        debugging_last = std::chrono::high_resolution_clock::now();
        #endif

        //
        // Where the JSON content is processed and added up the Pluck midi messages
        //

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
        const unsigned char system_timing_start     = 0xFA; // Start
        const unsigned char system_timing_continue  = 0xFB; // Continue
        const unsigned char system_timing_stop      = 0xFC; // Stop
        const unsigned char system_active_sensing   = 0xFE; // Active Sensing
        const unsigned char system_system_reset     = 0xFF; // System Reset


        auto data_processing_start = std::chrono::high_resolution_clock::now();

        try {

            nlohmann::json json_files_data = nlohmann::json::parse(json_str);
            for (nlohmann::json jsonData : json_files_data) {

                nlohmann::json jsonFileType;
                nlohmann::json jsonFileUrl;
                nlohmann::json jsonFileContent;

                try
                {
                    jsonFileType = jsonData["filetype"];
                    jsonFileUrl = jsonData["url"];
                    jsonFileContent = jsonData["content"];
                }
                catch (nlohmann::json::parse_error& ex)
                {
                    if (verbose) std::cerr << "Unable to extract json data: " << ex.byte << std::endl;
                    continue;
                }
                
                if (jsonFileType != FILE_TYPE || jsonFileUrl != FILE_URL) {
                    if (verbose) std::cerr << "Wrong type of file!" << std::endl;
                    continue;
                }

                MidiDevice *clip_midi_device = nullptr;

                for (auto jsonElement : jsonFileContent)
                {
                    if (jsonElement.contains("devices")) {

                        nlohmann::json jsonDeviceNames = jsonElement["devices"];
                        // It's a list of Devices that is given as Device
                        for (std::string deviceName : jsonDeviceNames) {
                            for (auto &device : midi_devices) {
                                if (device.getName().find(deviceName) != std::string::npos) {
                                    //
                                    // Where the Device Port is connected/opened (Main reason for errors)
                                    //
                                    try {
                                        if (device.openPort()) {
                                            clip_midi_device = &device;
                                            goto skip_to;
                                        }
                                    } catch (const std::exception& e) {
                                        if (verbose) std::cerr << "Error: " << e.what() << std::endl;
                                        clip_midi_device = nullptr;
                                        goto skip_to;
                                    }
                                }
                            }
                        }
                        clip_midi_device = nullptr; // No available device found

                    } else if (clip_midi_device != nullptr && jsonElement.contains("midi_message")) {
                        
                        play_reporting.total_excluded++;
                        double time_milliseconds = jsonElement["time_ms"];

                        // Create an API with the default API
                        try
                        {
                            if (time_milliseconds < 0) {

                                continue;
                                
                            } else {

                                unsigned char status_byte = jsonElement["midi_message"]["status_byte"];
                                std::vector<unsigned char> json_midi_message = { status_byte }; // Starts the json_midi_message to a new Status Byte
                                unsigned char priority = 0xFF;  // Lowest priority 16 by default
                                
                                unsigned char message_action = status_byte & 0xF0;
                                switch (message_action) {
                                    case action_system:
                                        switch (status_byte) {
                                            case system_timing_clock:
                                            case system_timing_start:
                                            case system_timing_stop:
                                            case system_timing_continue:
                                                // Any clock message falls here
                                                priority = 0x30 | status_byte & 0x0F;       // High priority 3
                                                break;
                                            case system_song_pointer:
                                            {
                                                // This is already a try catch situation
                                                unsigned char data_byte_1 = jsonElement["midi_message"]["data_byte_1"];
                                                unsigned char data_byte_2 = jsonElement["midi_message"]["data_byte_2"];
                                                if (data_byte_1 & 128 | data_byte_2 & 128)  // Makes sure it's inside the processing window
                                                    continue;

                                                json_midi_message.push_back(data_byte_1);
                                                json_midi_message.push_back(data_byte_2);
                                                priority = 0xB0 | status_byte & 0x0F;       // Low priority 12
                                                break;
                                            }
                                            case system_sysex_start:
                                            {
                                                // sysex_data_bytes = jsonElement["midi_message"]["data_bytes"].get<std::vector<unsigned char>>();
                                                
                                                nlohmann::json data_bytes = jsonElement["midi_message"]["data_bytes"];
                                                for (unsigned char sysex_data_byte : data_bytes) {
                                                    // Makes sure it's SysEx valid data
                                                    if (sysex_data_byte != 0xF0 && sysex_data_byte != 0xF7) {
                                                        json_midi_message.push_back(sysex_data_byte);
                                                    } else {
                                                        continue;
                                                    }
                                                }
                                                if (json_midi_message.size() < 2)
                                                    continue;
                                                
                                                json_midi_message.push_back(0xF7);  // End SysEx Data Byte
                                                priority = 0xF0 | status_byte & 0x0F;       // Lowest priority 16
                                                break;
                                            }
                                            default:
                                                // All other messages get a low priority
                                                priority = 0xD0 | status_byte & 0x0F;       // Low priority 14
                                                break;
                                        }
                                        break;
                                    case action_note_off:
                                    case action_note_on:
                                    case action_control_change:
                                    case action_pitch_bend:
                                    case action_key_pressure:
                                    {
                                        // This is already a try catch situation
                                        unsigned char data_byte_1 = jsonElement["midi_message"]["data_byte_1"];
                                        unsigned char data_byte_2 = jsonElement["midi_message"]["data_byte_2"];
                                        if (data_byte_1 & 128 | data_byte_2 & 128)
                                            continue;

                                        json_midi_message.push_back(data_byte_1);
                                        json_midi_message.push_back(data_byte_2);

                                        // Set the respective priorities
                                        switch (message_action) {

                                            case action_note_off:
                                                priority = 0x40 | status_byte & 0x0F;       // Normal priority 4
                                                break;
                                            case action_note_on:
                                                priority = 0x50 | status_byte & 0x0F;       // Normal priority 5
                                                break;
                                            case action_control_change:
                                                if (data_byte_1 == 1) {             // Modulation
                                                    priority = 0x60 | status_byte & 0x0F;       // Low priority 6
                                                } else if (data_byte_1 == 0 || data_byte_1 == 32) {
                                                    // 0 -  Bank Select (MSB)
                                                    // 32 - Bank Select (LSB)
                                                    priority = 0x00 | status_byte & 0x0F;       // Top priority 0
                                                } else {
                                                    priority = 0x20 | status_byte & 0x0F;       // High priority 2
                                                }
                                                break;
                                            case action_pitch_bend:
                                                priority = 0x70 | status_byte & 0x0F;       // Low priority 7
                                                break;
                                            case action_key_pressure:
                                                priority = 0x80 | status_byte & 0x0F;       // Low priority 8
                                                break;
                                        }
                                        break;
                                    }
                                    case action_program_change:
                                    case action_channel_pressure:
                                    {
                                        unsigned char data_byte = jsonElement["midi_message"]["data_byte"];
                                        if (data_byte & 128)
                                            continue;
                                        
                                        json_midi_message.push_back(data_byte);
                                        // Set the respective priorities
                                        switch (message_action) {

                                            case action_program_change:
                                                priority = 0x10 | status_byte & 0x0F;       // High priority 1
                                                break;
                                            case action_channel_pressure:
                                                priority = 0x80 | status_byte & 0x0F;       // Low priority 8
                                                break;
                                        }
                                        break;
                                    }

                                    default:
                                        continue;
                                }

                                midiToProcess.push_back( MidiPin(time_milliseconds, clip_midi_device, json_midi_message, priority) );
                                play_reporting.total_excluded--;    // Cancels out the initial ++ increase at the beginning of the loop
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




                        // nlohmann::json jsonDeviceNames = jsonElement["midi_message"]["device"];
                        // // It's a list of Devices that is given as Device
                        // for (std::string deviceName : jsonDeviceNames) {
                        //     for (auto &device : midi_devices) {
                        //         if (device.getName().find(deviceName) != std::string::npos) {
                        //             //
                        //             // Where the Device Port is connected/opened (Main reason for errors)
                        //             //
                        //             if (device.openPort())
                        //                 //
                        //                 // Where each Midi Pin or Pins are added to the midi processing list
                        //                 //
                        //                 if (status_byte == 0xF0) {  // SysEx message
                        //                     if (json_midi_message.size() > 2) {  // Avoids sending empty payload SysEx messages
                        //                         midiToProcess.push_back(MidiPin(time_milliseconds, &device, json_midi_message, priority));
                        //                     } else {
                        //                         play_reporting.total_excluded++;    // Marks it as excluded
                        //                     }
                        //                 } else {    // Processes NON SysEx messages
                        //                     midiToProcess.push_back(MidiPin(time_milliseconds, &device, json_midi_message, priority));
                        //                 }
                        //                 play_reporting.total_excluded--;    // Cancels out the initial ++ increase at the beginning of the loop
                        //             goto skip_to;
                        //         }
                        //     }
                        // }
                    }

                skip_to: continue;
                }
            }
        } catch (const nlohmann::json::parse_error& e) {
            if (verbose) std::cerr << "JSON parse error: " << e.what() << std::endl;
        }

        #ifdef DEBUGGING
        debugging_now = std::chrono::high_resolution_clock::now();
        completion_time = std::chrono::duration_cast<std::chrono::microseconds>(debugging_now - debugging_last);
        completion_time_us = completion_time.count();
        std::cout << "JSON DATA FULLY PROCESSED IN: " << completion_time_us << " microseconds" << std::endl;
        debugging_last = std::chrono::high_resolution_clock::now();
        #endif

        //
        // Where the existing Midi messages are sorted by time and other parameters
        //

        // Two levels sorting criteria
        midiToProcess.sort([]( const MidiPin &a, const MidiPin &b ) {
            
            // Time is the primary sorting criteria
            if (a.getTime() != b.getTime())  
                return a.getTime() < b.getTime();           // Primary: Sort by time (ascending)
        
            // Then sort by Priority (Ascendent)
            return a.getPriority() <= b.getPriority();      // Secondary: Sort by priority (ascending)
            
        });

        #ifdef DEBUGGING
        debugging_now = std::chrono::high_resolution_clock::now();
        completion_time = std::chrono::duration_cast<std::chrono::microseconds>(debugging_now - debugging_last);
        completion_time_us = completion_time.count();
        std::cout << "SORTING FULLY PROCESSED IN: " << completion_time_us << " microseconds" << std::endl;
        debugging_last = std::chrono::high_resolution_clock::now();
        #endif

        //
        // Where the existing Midi messages lists are Cleaned up and processed
        //

        // Clean up redundant midi messages
        {
            class MidiLastMessage {
            public:
                MidiDevice * const midi_device;
                const unsigned char status_byte;
                unsigned char data_byte_1;
                unsigned char data_byte_2;
                size_t level = 1;   // VERY IMPORTANT TO AVOID EARLIER NOTE OFF

                MidiLastMessage(MidiDevice * const midi_device, unsigned char status_byte, unsigned char data_byte_1, unsigned char data_byte_2):
                        midi_device(midi_device), status_byte(status_byte), data_byte_1(data_byte_1), data_byte_2(data_byte_2) { }

                bool operator == (const MidiPin &midi_pin) {
                    if (midi_device == midi_pin.getMidiDevice() && (status_byte & 0x0F) == (midi_pin.getStatusByte() & 0x0F)) {
                        if (status_byte >= 0x80 && status_byte < 0xC0)
                            if (data_byte_1 == midi_pin.getDataByte(1))
                                return true;
                        if (status_byte >= 0xC0 && status_byte < 0xF0)
                            return true;
                    }
                    return false;
                }

                // Prefix increment
                MidiLastMessage& operator++() {
                    ++level;
                    return *this;
                }
                // Postfix increment
                MidiLastMessage operator++(int) {
                    MidiLastMessage temp = *this;
                    ++level;
                    return temp;
                }
                // Prefix decrement
                MidiLastMessage& operator--() {
                    --level;
                    return *this;
                }
                // Postfix decrement
                MidiLastMessage operator--(int) {
                    MidiLastMessage temp = *this;
                    --level;
                    return temp;
                }
            };

            std::list<MidiLastMessage> last_midi_note_on_list;  // Midi Notes 0x80 and 0x90
            std::list<MidiLastMessage> last_midi_kp_list;       // Midi Key Aftertouch 0xA0
            std::list<MidiLastMessage> last_midi_cc_list;       // Midi Control Change 0xB0
            std::list<MidiLastMessage> last_midi_cp_list;       // Midi Channel Aftertouch 0xD0
            std::list<MidiLastMessage> last_midi_pb_list;       // Midi Pitch Bend 0xE0
            MidiPin *last_clock_pin = nullptr;                  // Midi clock messages 0xF0

            // Loop through the list and remove elements
            for (auto pin_it = midiToProcess.begin(); pin_it != midiToProcess.end(); ) {

                auto &midi_pin = *pin_it;

                if (midi_pin.getStatusByte() >= 0x80 && midi_pin.getStatusByte() < 0xF0) {

                    switch (midi_pin.getAction()) {
                    case action_note_off:
                        // Loop through the list and remove elements
                        for (auto note_on = last_midi_note_on_list.begin(); note_on != last_midi_note_on_list.end(); ++note_on) {

                            auto &last_midi_note_on = *note_on;
                            if (last_midi_note_on == midi_pin) {

                                if (last_midi_note_on.level == 1) {
                                    note_on = last_midi_note_on_list.erase(note_on);
                                    ++pin_it; // Only increment if no removal
                                } else {
                                    --last_midi_note_on;    // Decrements level
                                    midiRedundant.push_back(midi_pin);
                                    pin_it = midiToProcess.erase(pin_it);
                                }
                                goto skip_to_2;
                            }
                        }
                        midiRedundant.push_back(midi_pin);  /// Note Off as no Note On pair
                        pin_it = midiToProcess.erase(pin_it);
                        break;
                    case action_note_on:
                        for (auto &last_midi_note_on : last_midi_note_on_list) {
                            if (last_midi_note_on == midi_pin) {

                                // A special case for Note On with velocity 0!
                                if (last_midi_note_on.data_byte_2 == 0 && midi_pin.getDataByte(2) > 0 ||
                                    last_midi_note_on.data_byte_2 > 0 && midi_pin.getDataByte(2) == 0) {

                                    last_midi_note_on.data_byte_2 = midi_pin.getDataByte(2);
                                    ++pin_it; // Only increment if no removal
                                } else {

                                    ++last_midi_note_on;    // Increments level

                                    std::vector<unsigned char> midi_message = {
                                        static_cast<unsigned char>(midi_pin.getStatusByte() & 0x0F | 0x80),
                                        midi_pin.getDataByte(1),
                                        0
                                    };
                                    pin_it = midiToProcess.insert(pin_it,
                                        MidiPin(
                                                midi_pin.getTime(),
                                                midi_pin.getMidiDevice(),
                                                midi_message
                                            )
                                        );
                                    // Skips the previously inserted Note Off MidiPin
                                    ++pin_it;  // Move the iterator to the next element
                                    // Skips the Note On MidiPin
                                    ++pin_it;  // Move the iterator to the next element
                                }
                                goto skip_to_2;
                            }
                        }
                        
                        // First timer Note On
                        last_midi_note_on_list.push_back(
                            MidiLastMessage(midi_pin.getMidiDevice(), midi_pin.getStatusByte(), midi_pin.getDataByte(1), midi_pin.getDataByte(2))
                        );
                        ++pin_it; // Only increment if no removal
                        break;
                    case action_key_pressure:
                        for (auto &last_midi_kp : last_midi_kp_list) {
                            if (last_midi_kp == midi_pin) {

                                if (last_midi_kp.data_byte_2 != midi_pin.getDataByte(2)) {

                                    last_midi_kp.data_byte_2 = midi_pin.getDataByte(2);
                                    ++pin_it; // Only increment if no removal
                                } else {

                                    midiRedundant.push_back(midi_pin);
                                    pin_it = midiToProcess.erase(pin_it);
                                }
                                goto skip_to_2;
                            }
                        }
                        // First timer Note On
                        last_midi_kp_list.push_back(
                            MidiLastMessage(midi_pin.getMidiDevice(), midi_pin.getStatusByte(), midi_pin.getDataByte(1), midi_pin.getDataByte(2))
                        );
                        ++pin_it; // Only increment if no removal
                        break;
                    case action_control_change:
                        for (auto &last_midi_cc : last_midi_cc_list) {
                            if (last_midi_cc == midi_pin) {

                                if (last_midi_cc.data_byte_2 != midi_pin.getDataByte(2)) {

                                    last_midi_cc.data_byte_2 = midi_pin.getDataByte(2);
                                    ++pin_it; // Only increment if no removal
                                } else {

                                    midiRedundant.push_back(midi_pin);
                                    pin_it = midiToProcess.erase(pin_it);
                                }
                                goto skip_to_2;
                            }
                        }
                        // First timer Note On
                        last_midi_cc_list.push_back(
                            MidiLastMessage(midi_pin.getMidiDevice(), midi_pin.getStatusByte(), midi_pin.getDataByte(1), midi_pin.getDataByte(2))
                        );
                        ++pin_it; // Only increment if no removal
                        break;
                    case action_channel_pressure:
                        for (auto &last_midi_cp : last_midi_cp_list) {
                            if (last_midi_cp == midi_pin) {

                                if (last_midi_cp.data_byte_1 != midi_pin.getDataByte(1)) {

                                    last_midi_cp.data_byte_1 = midi_pin.getDataByte(1);
                                    ++pin_it; // Only increment if no removal
                                } else {

                                    midiRedundant.push_back(midi_pin);
                                    pin_it = midiToProcess.erase(pin_it);
                                }
                                goto skip_to_2;
                            }
                        }
                        // First timer Note On
                        last_midi_cp_list.push_back(
                            MidiLastMessage(midi_pin.getMidiDevice(), midi_pin.getStatusByte(), midi_pin.getDataByte(1), midi_pin.getDataByte(2))
                        );
                        ++pin_it; // Only increment if no removal
                        break;
                    case action_pitch_bend:
                        for (auto &last_midi_pb : last_midi_pb_list) {
                            if (last_midi_pb == midi_pin) {

                                if (last_midi_pb.data_byte_1 != midi_pin.getDataByte(1) ||
                                    last_midi_pb.data_byte_2 != midi_pin.getDataByte(2)) {

                                    last_midi_pb.data_byte_1 = midi_pin.getDataByte(1);
                                    last_midi_pb.data_byte_2 = midi_pin.getDataByte(2);
                                    ++pin_it; // Only increment if no removal
                                } else {

                                    midiRedundant.push_back(midi_pin);
                                    pin_it = midiToProcess.erase(pin_it);
                                }
                                goto skip_to_2;
                            }
                        }
                        // First timer Note On
                        last_midi_pb_list.push_back(
                            MidiLastMessage(midi_pin.getMidiDevice(), midi_pin.getStatusByte(), midi_pin.getDataByte(1), midi_pin.getDataByte(2))
                        );
                        ++pin_it; // Only increment if no removal
                        break;
                    
                    default:    // Includes Program Change 0xC0 (Never considered redundant!)
                        ++pin_it; // Only increment if no removal
                        break;
                    }

                } else if (midi_pin.getAction() == action_system) {

                    switch (midi_pin.getStatusByte())
                    {
                    case 0xF8:  // Timing Clock
                        if (last_clock_pin != nullptr) {
                            if (last_clock_pin->getTime() == midi_pin.getTime()) {
                                if (last_clock_pin->getStatusByte() == 0xFC) {      // Clock Stop
                                    last_clock_pin->setStatusByte(0xF8);
                                }
                                midiRedundant.push_back(midi_pin);
                                pin_it = midiToProcess.erase(pin_it);
                                goto skip_to_2;
                            } else if (last_clock_pin->getStatusByte() == 0xFC) {   // Clock Stop
                                midi_pin.setStatusByte(0xFB);
                            }
                        } else {
                            midi_pin.setStatusByte(0xFA);
                        }
                        last_clock_pin = &midi_pin;
                        ++pin_it; // Only increment if no removal
                        break;
                    case 0xFA:  // Start Clock
                        if (last_clock_pin != nullptr) {
                            if (last_clock_pin->getTime() == midi_pin.getTime()) {
                                if (last_clock_pin->getStatusByte() == 0xFC) {      // Clock Stop
                                    last_clock_pin->setStatusByte(0xF8);
                                }
                                midiRedundant.push_back(midi_pin);
                                pin_it = midiToProcess.erase(pin_it);
                                goto skip_to_2;
                            } else if (last_clock_pin->getStatusByte() == 0xFC) {   // Clock Stop
                                midi_pin.setStatusByte(0xFB);
                            } else {
                                midi_pin.setStatusByte(0xF8);
                            }
                        }
                        last_clock_pin = &midi_pin;
                        ++pin_it; // Only increment if no removal
                        break;
                    case 0xFB:  // Continue Clock
                        if (last_clock_pin != nullptr) {
                            if (last_clock_pin->getTime() == midi_pin.getTime()) {
                                last_clock_pin->setStatusByte(0xF8);
                                midiRedundant.push_back(midi_pin);
                                pin_it = midiToProcess.erase(pin_it);
                                goto skip_to_2;
                            } else if (last_clock_pin->getStatusByte() == 0xFA) {   // Clock Start
                                midi_pin.setStatusByte(0xF8);
                            } else if (last_clock_pin->getStatusByte() == 0xFB) {   // Clock Continue
                                midi_pin.setStatusByte(0xF8);
                            } else {                                                    // NOT Clock Start or Continue
                                last_clock_pin->setStatusByte(0xFC);
                            }
                        } else {
                            midi_pin.setStatusByte(0xFA);
                        }
                        last_clock_pin = &midi_pin;
                        ++pin_it; // Only increment if no removal
                        break;
                    case 0xFC:  // Stop Clock
                        if (last_clock_pin != nullptr) {
                            if (last_clock_pin->getTime() == midi_pin.getTime()) {
                                last_clock_pin->setStatusByte(0xFC);
                                midiRedundant.push_back(midi_pin);
                                pin_it = midiToProcess.erase(pin_it);
                                goto skip_to_2;
                            } else if (last_clock_pin->getStatusByte() == 0xFC) {   // Clock Stop
                                midiRedundant.push_back(midi_pin);
                                pin_it = midiToProcess.erase(pin_it);
                                goto skip_to_2;
                            }
                        }
                        last_clock_pin = &midi_pin;
                        ++pin_it; // Only increment if no removal
                        break;
                    case system_song_pointer:  // Song Pointer
                        if (last_clock_pin != nullptr) {
                            if (last_clock_pin->getTime() == midi_pin.getTime()
                                    && last_clock_pin->getStatusByte() == system_song_pointer
                                    && last_clock_pin->getDataByte(1) == midi_pin.getDataByte(1)
                                    && last_clock_pin->getDataByte(2) == midi_pin.getDataByte(2)) {
                                midiRedundant.push_back(midi_pin);
                                pin_it = midiToProcess.erase(pin_it);
                                goto skip_to_2;
                            }
                        }
                        last_clock_pin = &midi_pin;
                        ++pin_it; // Only increment if no removal
                        break;
                    
                    default:
                        ++pin_it; // Only increment if no removal
                        break;
                    }

                } else {
                    // Whatever it is (maybe Rest), let it pass!
                    ++pin_it; // Only increment if no removal
                }

            skip_to_2: continue;
            }

            // MIDI NOTES
            // Get time_ms of last message
            auto last_message_time_ms = midiToProcess.back().getTime();
            // Add the needed note off for all those still on at the end!
            for (auto &last_midi_note_on : last_midi_note_on_list) {
                // Transform midi on in midi off
                std::vector<unsigned char> midi_message = {
                    static_cast<unsigned char>(last_midi_note_on.status_byte & 0x0F | 0x80),    // note_off_status_byte
                    last_midi_note_on.data_byte_1,
                    last_midi_note_on.data_byte_2
                };
                midiToProcess.push_back(MidiPin(last_message_time_ms, last_midi_note_on.midi_device, midi_message));
            }

            // MIDI CLOCK
            if (last_clock_pin != nullptr && last_clock_pin->getStatusByte() == 0xF8)
                last_clock_pin->setStatusByte(0xFC);    // Clock Stop
        }

        #ifdef DEBUGGING
        debugging_now = std::chrono::high_resolution_clock::now();
        completion_time = std::chrono::duration_cast<std::chrono::microseconds>(debugging_now - debugging_last);
        completion_time_us = completion_time.count();
        std::cout << "MIDI MESSAGES CLEANING UP FULLY PROCESSED IN: " << completion_time_us << " microseconds" << std::endl;
        debugging_last = std::chrono::high_resolution_clock::now();
        #endif

        auto data_processing_finish = std::chrono::high_resolution_clock::now();

        auto pre_processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(data_processing_finish - data_processing_start);
        play_reporting.pre_processing = pre_processing_time.count();

        //
        // Where the Midi messages are sent to each Device
        //

        auto playing_start = std::chrono::high_resolution_clock::now();

        while (midiToProcess.size() > 0) {
            
            MidiPin &midi_pin = midiToProcess.front();  // Pin MIDI message

            long long next_pin_time_us = std::round((midi_pin.getTime() + play_reporting.total_drag) * 1000);
            auto playing_now = std::chrono::high_resolution_clock::now();
            auto elapsed_time = std::chrono::duration_cast<std::chrono::microseconds>(playing_now - playing_start);
            long long elapsed_time_us = elapsed_time.count();
            long long sleep_time_us = next_pin_time_us > elapsed_time_us ? next_pin_time_us - elapsed_time_us : 0;

            highResolutionSleep(sleep_time_us);  // Sleep for x microseconds

            auto pluck_time = std::chrono::high_resolution_clock::now() - playing_start;
            midi_pin.pluckTooth();  // as soon as possible! <----- Midi Send

            auto pluck_time_us = static_cast<double>(
                std::chrono::duration_cast<std::chrono::microseconds>(pluck_time).count()
            );
            double delay_time_ms = (pluck_time_us - next_pin_time_us) / 1000;
            midi_pin.setDelayTime(delay_time_ms);
            midiProcessed.push_back(midi_pin);
            midiToProcess.pop_front();

            // Process drag if existent
            if (delay_time_ms > DRAG_DURATION_MS)
                play_reporting.total_drag += delay_time_ms - DRAG_DURATION_MS;  // Drag isn't Delay
        }

        #ifdef DEBUGGING
        debugging_now = std::chrono::high_resolution_clock::now();
        completion_time = std::chrono::duration_cast<std::chrono::microseconds>(debugging_now - debugging_last);
        completion_time_us = completion_time.count();
        std::cout << "PLAYING FULLY PROCESSED IN: " << completion_time_us << " microseconds" << std::endl;
        debugging_last = std::chrono::high_resolution_clock::now();
        #endif

        //
        // Where the final Statistics are calculated
        //

        play_reporting.total_processed  = midiProcessed.size();
        play_reporting.total_redundant  = midiRedundant.size();

        if (play_reporting.total_processed > 0) {

            for (auto &midi_pin : midiProcessed) {
                auto delay_time_ms = midi_pin.getDelayTime();
                play_reporting.total_delay += delay_time_ms;
                play_reporting.maximum_delay = std::max(play_reporting.maximum_delay, delay_time_ms);
            }

            play_reporting.minimum_delay = play_reporting.maximum_delay;
            play_reporting.average_delay = play_reporting.total_delay / play_reporting.total_processed;

            for (auto &midi_pin : midiProcessed) {
                auto delay_time_ms = midi_pin.getDelayTime();
                play_reporting.minimum_delay = std::min(play_reporting.minimum_delay, delay_time_ms);
                play_reporting.sd_delay += std::pow(delay_time_ms - play_reporting.average_delay, 2);
            }

            play_reporting.sd_delay /= play_reporting.total_processed;
            play_reporting.sd_delay = std::sqrt(play_reporting.sd_delay);
        }
    }

    // Where the reporting is finally done
    if (verbose) std::cout << "Midi stats reporting:" << std::endl;
    if (verbose) std::cout << "\tData pre-processing time (ms):            " << std::setw(10) << play_reporting.pre_processing << std::endl;
    if (verbose) std::cout << "\tTotal processed Midi Messages (sent):     " << std::setw(10) << play_reporting.total_processed << std::endl;
    if (verbose) std::cout << "\tTotal redundant Midi Messages (not sent): " << std::setw(10) << play_reporting.total_redundant << std::endl;
    if (verbose) std::cout << "\tTotal excluded Midi Messages (not sent):  " << std::setw(10) << play_reporting.total_excluded << std::endl;
        
    // Set fixed floating-point notation and precision
    if (verbose) std::cout << std::fixed << std::setprecision(3);
    if (verbose) std::cout << "\tTotal drag (ms):    " << std::setw(36) << play_reporting.total_drag << " \\" << std::endl;
    if (verbose) std::cout << "\tTotal delay (ms):   " << std::setw(36) << play_reporting.total_delay << " /" << std::endl;
    if (verbose) std::cout << "\tMaximum delay (ms): " << std::setw(36) << play_reporting.maximum_delay << " \\" << std::endl;
    if (verbose) std::cout << "\tMinimum delay (ms): " << std::setw(36) << play_reporting.minimum_delay << " /" << std::endl;
    if (verbose) std::cout << "\tAverage delay (ms): " << std::setw(36) << play_reporting.average_delay << " \\" << std::endl;
    if (verbose) std::cout << "\tStandard deviation of delays (ms):" << std::setw(36 - 14) << play_reporting.sd_delay << " /"  << std::endl;


    return 0;
}

void disableBackgroundThrottling() {
#ifdef _WIN32
    // Windows-specific code to disable background throttling
    PROCESS_POWER_THROTTLING_STATE PowerThrottling;
    PowerThrottling.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    PowerThrottling.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
    PowerThrottling.StateMask = 0;

    SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &PowerThrottling, sizeof(PowerThrottling));
#else
    // Linux: No equivalent functionality, so do nothing
#endif
}

// High-resolution sleep function
void highResolutionSleep(long long microseconds) {
#ifdef _WIN32
    // Windows: High-resolution sleep using QueryPerformanceCounter
    LARGE_INTEGER frequency, start, end;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);

    long long sleepInterval = microseconds > 100*1000 ? microseconds - 100*1000 : 0;  // Sleep 1ms if the wait is longer than 100ms
    if (sleepInterval > 0) {
        // Sleep for most of the time to save CPU, then busy wait for the remaining time
        std::this_thread::sleep_for(std::chrono::microseconds(sleepInterval));
    }

    double elapsedMicroseconds = 0;
    do {
        QueryPerformanceCounter(&end);
        elapsedMicroseconds = static_cast<double>(end.QuadPart - start.QuadPart) * 1e6 / frequency.QuadPart;
    } while (elapsedMicroseconds < microseconds);
    
#else
    // Linux: High-resolution sleep using clock_nanosleep
    struct timespec ts;
    ts.tv_sec = microseconds / 1e6;
    ts.tv_nsec = (microseconds % static_cast<long long>(1e6)) * 1000;
    clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, nullptr);
#endif
}

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


    SysEx Message                    Status Byte 
    ------------------------         -----------
    0xF0: SysEx Start
    <Data Bytes>: Manufacturer ID + Command + Data
    0xF7: SysEx End

    self_playlist.append(
        {
            "time_ms": self.get_time_ms(single_pulse_duration_ms * total_clock_pulses),
            "midi_message": {
                "status_byte": 0xF0,    # Start of SysEx
                "data_bytes": [0x7F, 0x7F, 0x06, 0x01],  # Universal Stop command
                "end_byte": 0xF7,       # End of SysEx
                "device": devices
            }
        }
    )


*/