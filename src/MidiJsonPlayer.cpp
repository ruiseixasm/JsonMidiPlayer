#include "MidiJsonPlayer.hpp"




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

extern "C" {    // Needed for Python ctypes

    int PlayList(const char* json_str) {
        
        std::vector<MidiDevice> midi_devices;
        std::list<MidiPin> midiToProcess;
        std::list<MidiPin> midiProcessed;

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
                    std::cout << "Available output Port #" << i << ": " << portName << '\n';
                    midi_devices.push_back(MidiDevice(portName, i));
                }
            }
            if (midi_devices.size() == 0) {
                std::cout << "No MIDI output ports available.\n";
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

            std::cout << "Processed JSON: " << json_files_data.dump(4) << std::endl;
        } catch (const nlohmann::json::parse_error& e) {
            std::cerr << "JSON parse error: " << e.what() << std::endl;
        }

        // Sort the list by time in ascendent order
        midiToProcess.sort([]( const MidiPin &a, const MidiPin &b ) { return a.getTime() < b.getTime(); });

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

            // auto finish = std::chrono::high_resolution_clock::now();
            // std::cout << std::chrono::duration_cast<std::chrono::microseconds>(finish-start).count() << "us\n";

            // double passed_milliseconds = (double)(std::chrono::duration_cast<std::chrono::microseconds>(finish-start).count()) / 1000;
            // std::cout << passed_milliseconds << "ms\n";
        }
        
        while (midiProcessed.size() > 0) {
            midiProcessed.pop_front();
        }

        return 0;
    }
}
