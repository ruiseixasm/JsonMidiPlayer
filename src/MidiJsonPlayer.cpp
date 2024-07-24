#include "MidiJsonPlayer.hpp"

MidiDevice::~MidiDevice() {
    closePort();
}

bool canOpenMidiPort(RtMidiOut& midiOut, unsigned int portNumber) {
    try {
        midiOut.openPort(portNumber);
        if (midiOut.isPortOpen()) {
            midiOut.closePort();
            return true;
        }
    } catch (RtMidiError &error) {
        // Handle the error if needed
        // error.printMessage();
    }
    return false;
}

int PlayList(const char* json_str) {
    
    std::vector<MidiDevice> midi_devices;
    std::list<MidiPin> midiToProcess;
    std::list<MidiPin> midiProcessed;
    std::list<MidiPin> midiRedundant;

    try {

        RtMidiOut midiOut;  // Temporary MidiOut manipulator
        unsigned int nPorts = midiOut.getPortCount();
        if (nPorts == 0) {
            std::cout << "No MIDI output ports available.\n";
            return 1;
        }
        std::cout << "Available MIDI output ports:\n";
        for (unsigned int i = 0; i < nPorts; i++) {
            if (canOpenMidiPort(midiOut, i)) {
                std::string portName = midiOut.getPortName(i);
                std::cout << "\tOutput Port #" << i << ": " << portName << '\n';
                midi_devices.push_back(MidiDevice(portName, i));
            }
        }
        if (midi_devices.size() == 0) {
            std::cout << "\tNo MIDI output ports available.\n";
            return 1;
        }

    } catch (RtMidiError &error) {
        error.printMessage();
        return EXIT_FAILURE;
    }

    try {

        nlohmann::json json_files_data = nlohmann::json::parse(json_str);
        for (nlohmann::json jsonData : json_files_data) {

            nlohmann::json jsonFileType;
            nlohmann::json jsonFileContent;

            try
            {
                jsonFileType = jsonData["filetype"];
                jsonFileContent = jsonData["content"];
            }
            catch (nlohmann::json::parse_error& ex)
            {
                std::cerr << "Unable to extract json data: " << ex.byte << std::endl;
                continue;
            }
            
            if (jsonFileType != FILE_TYPE)
                continue;

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
                        std::cerr << "JSON error: " << e.what() << std::endl;
                        continue;
                    } catch (const std::exception& e) {
                        std::cerr << "Error: " << e.what() << std::endl;
                        continue;
                    } catch (...) {
                        std::cerr << "Unknown error occurred." << std::endl;
                        continue;
                    }

                    midi_device = nullptr;
                    for (std::string deviceName : jsonDeviceNames) {
                        for (auto &device : midi_devices) {
                            if (device.getName().find(deviceName) != std::string::npos) {
                                midi_device = &device;
                                goto skip_to;
                            }
                        }
                    }

                skip_to:

                    if (midi_device != nullptr) {
                        midi_device->openPort();
                        midiToProcess.push_back(MidiPin(time_milliseconds, midi_device, midi_message_size, status_byte, data_byte_1, data_byte_2));
                    }
                }
            }
        }
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
    }

    // Sort the list by time in ascendent order
    midiToProcess.sort([]( const MidiPin &a, const MidiPin &b ) { return a.getTime() < b.getTime(); });

    // Clean up redundant midi messages
    {
        class MidiLastMessage {
        public:
            MidiDevice * const midi_device;
            const unsigned char status_byte;
            const unsigned char data_byte_1;
            unsigned char data_byte_2;  // To accept Note On 0 as off (pass through)

            MidiLastMessage(MidiDevice * const midi_device, unsigned char status_byte, unsigned char data_byte_1, unsigned char data_byte_2):
                    midi_device(midi_device), status_byte(status_byte), data_byte_1(data_byte_1), data_byte_2(data_byte_2) { }

            bool operator == (const MidiPin &midi_pin) {
                if (midi_device == midi_pin.getMidiDevice() &&
                        (status_byte & 0x0F) == (midi_pin.getMidiMessage()[0] & 0x0F) && data_byte_1 == midi_pin.getMidiMessage()[1]) 
                    return true;
                return false;
            }
        };

        std::list<MidiLastMessage> midi_note_on_list;
        std::list<MidiLastMessage> midi_cc_list;
        const unsigned char type_note_on = 0x90;
        const unsigned char type_note_off = 0x80;
        const unsigned char type_cc = 0xB0;

        // Loop through the list and remove elements
        for (auto it = midiToProcess.begin(); it != midiToProcess.end(); ) {

            auto &midi_pin = *it;
            const unsigned char *pin_midi_message = midi_pin.getMidiMessage();

            if (pin_midi_message[0] >= 0x80 && pin_midi_message[0] < 0xF0) {

                const unsigned char pin_midi_message_type = pin_midi_message[0] & 0xF0;

                if (pin_midi_message_type == type_note_on) {

                    for (auto &midi_note_on : midi_note_on_list) {
                        if (midi_note_on == midi_pin) {

                            if (midi_note_on.data_byte_2 == 0 && pin_midi_message[2] > 0 ||
                                midi_note_on.data_byte_2 > 0 && pin_midi_message[2] == 0) {

                                midi_note_on.data_byte_2 = pin_midi_message[2];
                                ++it; // Only increment if no removal
                            } else {

                                midiRedundant.push_back(midi_pin);
                                it = midiToProcess.erase(it);
                            }
                            goto skip_to_2;
                        }
                    }

                    // First timer Note On
                    midi_note_on_list.push_back(
                        MidiLastMessage(midi_pin.getMidiDevice(), pin_midi_message[0], pin_midi_message[1], pin_midi_message[2])
                    );
                    ++it; // Only increment if no removal

                } else if (pin_midi_message_type == type_note_off) {

                    // Loop through the list and remove elements
                    for (auto mn = midi_note_on_list.begin(); mn != midi_note_on_list.end(); ++mn) {

                        auto &midi_note_on = *mn;
                        if (midi_note_on == midi_pin) {

                            mn = midi_note_on_list.erase(mn);
                            ++it; // Only increment if no removal
                            goto skip_to_2;
                        }
                    }
                    midiRedundant.push_back(midi_pin);
                    it = midiToProcess.erase(it);

                } else if (pin_midi_message_type == type_cc) {

                    for (auto &midi_cc : midi_cc_list) {
                        if (midi_cc == midi_pin) {

                            if (midi_cc.data_byte_2 != pin_midi_message[2]) {

                                midi_cc.data_byte_2 = pin_midi_message[2];
                                ++it; // Only increment if no removal
                            } else {

                                midiRedundant.push_back(midi_pin);
                                it = midiToProcess.erase(it);
                            }
                            goto skip_to_2;
                        }
                    }

                    // First timer Note On
                    midi_cc_list.push_back(
                        MidiLastMessage(midi_pin.getMidiDevice(), pin_midi_message[0], pin_midi_message[1], pin_midi_message[2])
                    );
                    ++it; // Only increment if no removal

                } else {
                    ++it; // Only increment if no removal
                }
            } else {
                ++it; // Only increment if no removal
            }

        skip_to_2: continue;

        }

        // Get time_ms of last message
        auto last_message_time_ms = midiToProcess.back().getTime();
        // Add the needed note off for all those still on at the end!
        for (auto &midi_note_on : midi_note_on_list) {
            // Transform midi on in midi off
            const unsigned char note_off_status_byte = midi_note_on.status_byte & 0x0F | 0x80;
            midiToProcess.push_back(MidiPin(last_message_time_ms, midi_note_on.midi_device, 3, note_off_status_byte, midi_note_on.data_byte_1));
        }
    }

    auto start = std::chrono::high_resolution_clock::now();

    while (midiToProcess.size() > 0) {
        auto next_point_us = std::chrono::microseconds(static_cast<long long>(midiToProcess.front().getTime() * 1000));
        auto present = std::chrono::high_resolution_clock::now();
        auto elapsed_time_us = std::chrono::duration_cast<std::chrono::microseconds>(present - start);
        auto sleep_time_us = next_point_us - elapsed_time_us;

        std::this_thread::sleep_for(std::chrono::microseconds(sleep_time_us));

        // Send the MIDI message
        midiToProcess.front().pluckTooth();
        midiProcessed.push_back(midiToProcess.front());
        midiToProcess.pop_front();
    }
    
    while (midiProcessed.size() > 0) {
        midiProcessed.pop_front();
    }

    return 0;
}