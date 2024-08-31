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
    if (!opened_port) {
        try {
            midiOut.openPort(port);
            opened_port = true;
            if (verbose) std::cout << "Midi device connected: " << name << std::endl;
        } catch (RtMidiError &error) {
            static bool first_time_error = true;
            if (first_time_error) {
                // Handle the error if needed
                error.printMessage();
                first_time_error = false;
            }
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

void MidiDevice::sendMessage(const unsigned char *midi_message, size_t message_size) {
    midiOut.sendMessage(midi_message, message_size);
}

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
    
    // Set real-time scheduling
    setRealTimeScheduling();
    
    #ifdef DEBUGGING
    auto debugging_start = std::chrono::high_resolution_clock::now();
    auto debugging_now = debugging_start;
    auto debugging_last = debugging_now;
    long long completion_time_us = 0;
    #endif

    struct PlayReporting {
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

                {
                    // temporary/buffer variables
                    double time_milliseconds;
                    nlohmann::json jsonDeviceNames;
                    size_t midi_message_size;
                    unsigned char status_byte;
                    int data_byte_1;
                    int data_byte_2;
                    MidiDevice *midi_device;
                    
                    for (auto jsonElement : jsonFileContent)
                    {
                        if (jsonElement.contains("midi_message") && jsonElement.contains("time_ms")) {
                            play_reporting.total_excluded++;
                            // Create an API with the default API
                            try
                            {
                                time_milliseconds = jsonElement["time_ms"];
                                if (time_milliseconds >= 0) {
                                    jsonDeviceNames = jsonElement["midi_message"]["device"];
                                    status_byte = jsonElement["midi_message"]["status_byte"];

                                    if (status_byte >= 0x80 && status_byte < 0xF0) {    // Channel messages (most significant bit = 1)
                                        midi_message_size = 3;
                                        data_byte_1 = jsonElement["midi_message"]["data_byte_1"];
                                        data_byte_2 = jsonElement["midi_message"]["data_byte_2"];

                                        if (data_byte_1 < 0 || data_byte_1 > 127 ||
                                            data_byte_2 < 0 || data_byte_2 > 127)   // Makes sure it's inside the processing window
                                            continue;

                                    } else if (status_byte == 0xF8 || status_byte == 0xFA || status_byte == 0xFB ||
                                            status_byte == 0xFC || status_byte == 0xFE || status_byte == 0xFF) { // System real-time messages
                                        midi_message_size = 1;
                                        data_byte_1 = 0;
                                        data_byte_2 = 0;
                                    } else if (status_byte == 0xF1 || status_byte == 0xF3) {    // System common messages
                                        midi_message_size = 2;
                                        data_byte_1 = jsonElement["midi_message"]["data_byte"];
                                        data_byte_2 = 0;

                                        if (data_byte_1 < 0x00 || data_byte_1 > 0xFF) // Makes sure it's inside the processing window
                                            continue;
                                    } else {
                                        if (status_byte == 0xF2) {      // Song Position Pointer
                                            midi_message_size = 3;
                                            data_byte_1 = jsonElement["midi_message"]["data_byte_1"];
                                            data_byte_2 = jsonElement["midi_message"]["data_byte_2"];

                                            if (data_byte_1 < 0x00 || data_byte_1 > 0xFF ||
                                                data_byte_2 < 0x00 || data_byte_2 > 0xFF)  // Makes sure it's inside the processing window
                                                continue;

                                        } else if (status_byte == 0xF6) {   // Tune Request
                                            midi_message_size = 1;
                                            data_byte_1 = 0;
                                            data_byte_2 = 0;
                                        } else {
                                            continue;
                                        }
                                    }
                                } else {
                                    continue;
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

                            for (std::string deviceName : jsonDeviceNames) {
                                for (auto &device : midi_devices) {
                                    if (device.getName().find(deviceName) != std::string::npos) {
                                        if (device.openPort())
                                            midiToProcess.push_back(MidiPin(time_milliseconds, &device, midi_message_size, status_byte, data_byte_1, data_byte_2));
                                            play_reporting.total_excluded--;
                                        goto skip_to;
                                    }
                                }
                            }
                        }

                    skip_to: continue;
                    }
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

        // Sort the list by time in ascendent order. Choosen (<=) to avoid Note Off's before than Note On's
        midiToProcess.sort([]( const MidiPin &a, const MidiPin &b ) {
                if (a.getTime() < b.getTime()) return true;
                if (a.getTime() > b.getTime()) return false;
                // For equal time case and to avoid Notes Off happening before Notes On
                if ((a.getMidiMessage()[0] & 0xF0) == 0x80 && (b.getMidiMessage()[0] & 0xF0) == 0x90)
                    return false;
                // For equal time case and to avoid Program Change happening before Control Change
                if ((a.getMidiMessage()[0] & 0xF0) == 0xC0 && (b.getMidiMessage()[0] & 0xF0) == 0xB0)
                    return false;
                return true;
            });

        #ifdef DEBUGGING
        debugging_now = std::chrono::high_resolution_clock::now();
        completion_time = std::chrono::duration_cast<std::chrono::microseconds>(debugging_now - debugging_last);
        completion_time_us = completion_time.count();
        std::cout << "SORTING FULLY PROCESSED IN: " << completion_time_us << " microseconds" << std::endl;
        debugging_last = std::chrono::high_resolution_clock::now();
        #endif

        // Clean up redundant midi messages
        {
            class MidiLastMessage {
            public:
                MidiDevice * const midi_device;
                const unsigned char status_byte;
                unsigned char data_byte_1;
                unsigned char data_byte_2;
                size_t level = 1;

                MidiLastMessage(MidiDevice * const midi_device, unsigned char status_byte, unsigned char data_byte_1, unsigned char data_byte_2):
                        midi_device(midi_device), status_byte(status_byte), data_byte_1(data_byte_1), data_byte_2(data_byte_2) { }

                bool operator == (const MidiPin &midi_pin) {
                    if (midi_device == midi_pin.getMidiDevice() && (status_byte & 0x0F) == (midi_pin.getMidiMessage()[0] & 0x0F)) {
                        if (status_byte >= 0x80 && status_byte < 0xC0)
                            if (data_byte_1 == midi_pin.getMidiMessage()[1])
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

            std::list<MidiLastMessage> last_midi_note_on_list;
            std::list<MidiLastMessage> last_midi_kp_list;
            std::list<MidiLastMessage> last_midi_cc_list;
            std::list<MidiLastMessage> last_midi_cp_list;
            std::list<MidiLastMessage> last_midi_pb_list;
            MidiPin *last_clock_pin = nullptr;
            const unsigned char type_note_off = 0x80;           // Note off
            const unsigned char type_note_on = 0x90;            // Note on
            const unsigned char type_key_pressure = 0xA0;       // Polyphonic Key Pressure
            const unsigned char type_control_change = 0xB0;     // Control Change
            const unsigned char type_channel_pressure = 0xD0;   // Channel Pressure
            const unsigned char type_pitch_bend = 0xE0;         // Pitch Bend

            // Loop through the list and remove elements
            for (auto pin_it = midiToProcess.begin(); pin_it != midiToProcess.end(); ) {

                auto &midi_pin = *pin_it;
                const unsigned char *pin_midi_message = midi_pin.getMidiMessage();

                if (pin_midi_message[0] >= 0x80 && pin_midi_message[0] < 0xF0) {

                    const unsigned char pin_midi_message_type = pin_midi_message[0] & 0xF0;

                    switch (pin_midi_message_type) {
                    case type_note_off:
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
                        midiRedundant.push_back(midi_pin);
                        pin_it = midiToProcess.erase(pin_it);
                        break;
                    case type_note_on:
                        for (auto &last_midi_note_on : last_midi_note_on_list) {
                            if (last_midi_note_on == midi_pin) {

                                // A special case for Note On with velocity 0!
                                if (last_midi_note_on.data_byte_2 == 0 && pin_midi_message[2] > 0 ||
                                    last_midi_note_on.data_byte_2 > 0 && pin_midi_message[2] == 0) {

                                    last_midi_note_on.data_byte_2 = pin_midi_message[2];
                                    ++pin_it; // Only increment if no removal
                                } else {

                                    ++last_midi_note_on;    // Increments level
                                    midiRedundant.push_back(midi_pin);
                                    pin_it = midiToProcess.erase(pin_it);
                                }
                                goto skip_to_2;
                            }
                        }
                        // First timer Note On
                        last_midi_note_on_list.push_back(
                            MidiLastMessage(midi_pin.getMidiDevice(), pin_midi_message[0], pin_midi_message[1], pin_midi_message[2])
                        );
                        ++pin_it; // Only increment if no removal
                        break;
                    case type_key_pressure:
                        for (auto &last_midi_kp : last_midi_kp_list) {
                            if (last_midi_kp == midi_pin) {

                                if (last_midi_kp.data_byte_2 != pin_midi_message[2]) {

                                    last_midi_kp.data_byte_2 = pin_midi_message[2];
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
                            MidiLastMessage(midi_pin.getMidiDevice(), pin_midi_message[0], pin_midi_message[1], pin_midi_message[2])
                        );
                        ++pin_it; // Only increment if no removal
                        break;
                    case type_control_change:
                        for (auto &last_midi_cc : last_midi_cc_list) {
                            if (last_midi_cc == midi_pin) {

                                if (last_midi_cc.data_byte_2 != pin_midi_message[2]) {

                                    last_midi_cc.data_byte_2 = pin_midi_message[2];
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
                            MidiLastMessage(midi_pin.getMidiDevice(), pin_midi_message[0], pin_midi_message[1], pin_midi_message[2])
                        );
                        ++pin_it; // Only increment if no removal
                        break;
                    case type_channel_pressure:
                        for (auto &last_midi_cp : last_midi_cp_list) {
                            if (last_midi_cp == midi_pin) {

                                if (last_midi_cp.data_byte_1 != pin_midi_message[1]) {

                                    last_midi_cp.data_byte_1 = pin_midi_message[1];
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
                            MidiLastMessage(midi_pin.getMidiDevice(), pin_midi_message[0], pin_midi_message[1], pin_midi_message[2])
                        );
                        ++pin_it; // Only increment if no removal
                        break;
                    case type_pitch_bend:
                        for (auto &last_midi_pb : last_midi_pb_list) {
                            if (last_midi_pb == midi_pin) {

                                if (last_midi_pb.data_byte_1 != pin_midi_message[1] ||
                                    last_midi_pb.data_byte_2 != pin_midi_message[2]) {

                                    last_midi_pb.data_byte_1 = pin_midi_message[1];
                                    last_midi_pb.data_byte_2 = pin_midi_message[2];
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
                            MidiLastMessage(midi_pin.getMidiDevice(), pin_midi_message[0], pin_midi_message[1], pin_midi_message[2])
                        );
                        ++pin_it; // Only increment if no removal
                        break;
                    
                    default:
                        ++pin_it; // Only increment if no removal
                        break;
                    }

                } else if ((pin_midi_message[0] & 0xF0) == 0xF0) {

                    switch (pin_midi_message[0])
                    {
                    case 0xF8:  // Timing Clock
                        if (last_clock_pin != nullptr) {
                            if (last_clock_pin->getTime() == midi_pin.getTime()) {
                                if (last_clock_pin->getMidiMessage()[0] == 0xFC) {      // Clock Stop
                                    last_clock_pin->setStatusByte(0xF8);
                                }
                                midiRedundant.push_back(midi_pin);
                                pin_it = midiToProcess.erase(pin_it);
                                goto skip_to_2;
                            } else if (last_clock_pin->getMidiMessage()[0] == 0xFC) {   // Clock Stop
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
                                if (last_clock_pin->getMidiMessage()[0] == 0xFC) {      // Clock Stop
                                    last_clock_pin->setStatusByte(0xF8);
                                }
                                midiRedundant.push_back(midi_pin);
                                pin_it = midiToProcess.erase(pin_it);
                                goto skip_to_2;
                            } else if (last_clock_pin->getMidiMessage()[0] == 0xFC) {   // Clock Stop
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
                            } else if (last_clock_pin->getMidiMessage()[0] == 0xFA) {   // Clock Start
                                midi_pin.setStatusByte(0xF8);
                            } else if (last_clock_pin->getMidiMessage()[0] == 0xFB) {   // Clock Continue
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
                            } else if (last_clock_pin->getMidiMessage()[0] == 0xFC) {   // Clock Stop
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
                    
                    ++pin_it; // Only increment if no removal
                    break;
                }

            skip_to_2: continue;
            }

            // MIDI NOTES
            // Get time_ms of last message
            auto last_message_time_ms = midiToProcess.back().getTime();
            // Add the needed note off for all those still on at the end!
            for (auto &last_midi_note_on : last_midi_note_on_list) {
                // Transform midi on in midi off
                const unsigned char note_off_status_byte = last_midi_note_on.status_byte & 0x0F | 0x80;
                midiToProcess.push_back(MidiPin(last_message_time_ms, last_midi_note_on.midi_device,
                        3, note_off_status_byte, last_midi_note_on.data_byte_1));
            }

            // MIDI CLOCK
            if (last_clock_pin != nullptr && last_clock_pin->getMidiMessage()[0] == 0xF8)
                last_clock_pin->setStatusByte(0xFC);    // Clock Stop
        }

        #ifdef DEBUGGING
        debugging_now = std::chrono::high_resolution_clock::now();
        completion_time = std::chrono::duration_cast<std::chrono::microseconds>(debugging_now - debugging_last);
        completion_time_us = completion_time.count();
        std::cout << "MIDI MESSAGES CLEANING UP FULLY PROCESSED IN: " << completion_time_us << " microseconds" << std::endl;
        debugging_last = std::chrono::high_resolution_clock::now();
        #endif

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

    System Real-Time Message         Status Byte 
    ------------------------         -----------
    Timing Clock                         F8
    Start Sequence                       FA
    Continue Sequence                    FB
    Stop Sequence                        FC
    Active Sensing                       FE
    System Reset                         FF
*/