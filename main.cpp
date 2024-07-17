#include <iostream>
#include <fstream>
#include <string>
#include <array>
#include <vector>
#include <list>
#include <algorithm>
#include <getopt.h>
#include <cstdlib>
#include <thread>               // Include for std::this_thread::sleep_for
#include <chrono>               // Include for std::chrono::seconds
#include <nlohmann/json.hpp>    // Include the JSON library
#include "RtMidi.h"             // Includes the necessary MIDI library

class MidiDevice {
private:
    RtMidiOut midiOut;
    const std::string name;
    const unsigned int port;
    bool opened_port = false;
    std::array<unsigned char, 256> keyboards = {0};
public:
    MidiDevice(std::string device_name, unsigned int device_port) : name(device_name), port(device_port) { }

    ~MidiDevice() {
        if (opened_port) {

            // Release all still pressed key_notes before disconnecting
            unsigned char midi_message[3] = {0};
            for (size_t byte_position = 0; byte_position < 256; byte_position++) {
                if (keyboards[byte_position] > 0) {
                    unsigned char byte_channel = byte_position / 16;    // 16 bytes for each keyboard
                    midi_message[0] = 0x80 | byte_channel;  // Note Off command
                    unsigned char keyboard_byte = keyboards[byte_position];

                    for (size_t bit_position = 0; bit_position < 8; bit_position++) {

                        if (keyboard_byte & 0b10000000 >> bit_position) {

                            midi_message[1] = byte_position % 16 * 8 + bit_position;    // param_1
                            midiOut.sendMessage(midi_message, 3);
                        }
                    }
                }
            }

            midiOut.closePort();
            opened_port = false;
        }
    }

    // // Delete the move constructor and move assignment operator
    // MidiDevice(MidiDevice &&) = delete;
    // MidiDevice &operator=(MidiDevice &&) = delete;

    // Move constructor
    MidiDevice(MidiDevice &&other) noexcept : midiOut(std::move(other.midiOut)),
            name(std::move(other.name)), port(other.port), opened_port(other.opened_port), keyboards(std::move(other.keyboards)) {
        std::cout << "Move constructed: " << name << std::endl;
    }

    // Delete the copy constructor and copy assignment operator
    MidiDevice(const MidiDevice &) = delete;
    MidiDevice &operator=(const MidiDevice &) = delete;

    // Move assignment operator
    MidiDevice &operator=(MidiDevice &&other) noexcept {
        if (this != &other) {
            // Since name and port are const, they cannot be assigned.
            opened_port = other.opened_port;
            keyboards = std::move(other.keyboards);
            // midiOut can't be assigned using the = assignment operator because has none.
            // midiOut = std::move(other.midiOut);
        }
        std::cout << "Move assigned: " << name << std::endl;
        return *this;
    }

    void openPort() {
        if (!opened_port) {
            midiOut.openPort(port);
            opened_port = true;
        }
    }

    void closePort() {
        if (opened_port) {
            midiOut.closePort();
            opened_port = false;
        }
    }

    unsigned int getOpenedPort() const {
        return opened_port;
    }

    const std::string& getName() const {
        return name;
    }

    bool isKeyPressed(unsigned char channel, unsigned char key_note) const {
        unsigned char keyboards_key = channel * 128 + key_note;
        unsigned char keyboards_byte = keyboards_key / 8;
        unsigned char byte_key = keyboards_key % 8;

        return keyboards[keyboards_byte] & 0b10000000 >> byte_key;
    }

    void pressKey(unsigned char channel, unsigned char key_note) {
        unsigned char keyboards_key = channel * 128 + key_note;
        unsigned char keyboards_byte = keyboards_key / 8;
        unsigned char byte_key = keyboards_key % 8;

        keyboards[keyboards_byte] |= 0b10000000 >> byte_key;
    }

    void releaseKey(unsigned char channel, unsigned char key_note) {
        unsigned char keyboards_key = channel * 128 + key_note;
        unsigned char keyboards_byte = keyboards_key / 8;
        unsigned char byte_key = keyboards_key % 8;

        keyboards[keyboards_byte] &= ~0b10000000 >> byte_key;
    }

    void sendMessage(const unsigned char *midi_message, size_t message_size) {

        if (message_size == 3) {
            bool validated = false;

            if (midi_message[1] & 0xF0 == 0x80) {          // 0x80 - Note Off
                if (isKeyPressed(midi_message[1] & 0x0F, midi_message[2])) {
                    releaseKey(midi_message[1] & 0x0F, midi_message[2]);
                    validated = true;
                }
            } else if (midi_message[1] & 0xF0 == 0x90) {   // 0x90 - Note On
                if (!isKeyPressed(midi_message[1] & 0x0F, midi_message[2])) {
                    pressKey(midi_message[1] & 0x0F, midi_message[2]);
                    validated = true;
                }
            } else {
                validated = true;
            }

            if (validated) {
                // Send the MIDI message
                midiOut.sendMessage(midi_message, message_size);
            }
        }
    }

};

// Define the MidiPin class
class MidiPin {

private:
    const double time_ms;
    MidiDevice * const midi_device = nullptr;
    const unsigned char midi_message[3];

public:
    MidiPin(double time_milliseconds, MidiDevice *midi_device, unsigned char command, unsigned char param_1, unsigned char param_2)
        : time_ms(time_milliseconds), midi_device(midi_device), midi_message{command, param_1, param_2} { }

    double getTime() const {
        return time_ms;
    }

    void pluckTooth() {
        if (midi_device != nullptr)
            midi_device->sendMessage(midi_message, 3);
    }
};

void printUsage(const char *programName) {
    std::cout << "Usage: " << programName << " [options] input_file output_file\n"
              << "Options:\n"
              << "  -h, --help       Show this help message and exit\n"
              << "  -v, --verbose    Enable verbose mode\n";
}

int main(int argc, char *argv[]) {

    int verbose = 0;
    int option_index = 0;

    struct option long_options[] = {
        {"help",    no_argument,       nullptr, 'h'},
        {"verbose", no_argument,       nullptr, 'v'},
        {nullptr,   0,                 nullptr,  0 }
    };

    while (true) {
        int c = getopt_long(argc, argv, "hv", long_options, &option_index);
        if (c == -1) break;

        switch (c) {
            case 'h':
                printUsage(argv[0]);
                return 0;
            case 'v':
                verbose = 1;
                break;
            case '?':
                // getopt_long already printed an error message.
                return 1;
            default:
                abort();
        }
    }

    if (optind + 2 > argc) {
        std::cerr << "Error: Missing input or output file\n";
        printUsage(argv[0]);
        return 1;
    }

    const char* inputFileName = argv[optind];
    const char* outputFileName = argv[optind + 1];

    if (verbose) {
        std::cout << "Verbose mode enabled\n";
        std::cout << "Input file: " << inputFileName << "\n";
        std::cout << "Output file: " << outputFileName << "\n";
    }

    std::ifstream inputFile(inputFileName);
    std::ofstream outputFile(outputFileName);

    if (!inputFile.is_open()) {
        std::cerr << "Error: Could not open input file " << inputFileName << "\n";
        return 1;
    }

    if (!outputFile.is_open()) {
        std::cerr << "Error: Could not open output file " << outputFileName << "\n";
        return 1;
    }

    std::string line;
    while (std::getline(inputFile, line)) {
        outputFile << line << "\n";
        if (verbose) {
            std::cout << "Processing line: " << line << "\n";
        }
    }

    inputFile.close();
    outputFile.close();

    if (verbose) {
        std::cout << "File processing completed\n";
    }

    // Vector of available MIDI output devices
    std::vector<MidiDevice> midi_devices;

    try {

        RtMidiOut midiOut;  // Temporary MidiOut manipulator
        unsigned int nPorts = midiOut.getPortCount();
        if (nPorts == 0) {
            std::cout << "No MIDI output ports available.\n";
            return 0;
        }
        std::cout << "Available MIDI output ports:\n";
        for (unsigned int i = 0; i < nPorts; i++) {
            try {
                std::string portName = midiOut.getPortName(i);
                std::cout << "  Output Port #" << i << ": " << portName << '\n';
                midi_devices.push_back(MidiDevice(portName, i));
            } catch (RtMidiError &error) {
                error.printMessage();
            }
        }

    } catch (RtMidiError &error) {
        error.printMessage();
        return EXIT_FAILURE;
    }

    std::list<MidiPin> midiToProcess;
    std::list<MidiPin> midiProcessed;
    std::list<MidiPin> midiRejected;

    // Open the JSON file
    std::ifstream jsonFile("../midiSimpleNotes.json");
    if (!jsonFile.is_open()) {
        std::cerr << "Could not open the file!" << std::endl;
        return 1;
    }

    // Parse the JSON files
    nlohmann::json jsonData;
    jsonFile >> jsonData;
    // Close the JSON file
    jsonFile.close();

    double time_milliseconds;
    nlohmann::json jsonDeviceNames;
    unsigned char command;
    unsigned char param_1;
    unsigned char param_2;
    MidiDevice *midi_device;
    
    for (auto jsonElement : jsonData)
    {
        // Create an API with the default API
        try
        {
            time_milliseconds = jsonElement["time_ms"];
            jsonDeviceNames = jsonElement["midi_message"]["device"];
            command = jsonElement["midi_message"]["command"];
            param_1 = jsonElement["midi_message"]["param_1"];
            param_2 = jsonElement["midi_message"]["param_2"];
        }
        catch (nlohmann::json::parse_error& ex)
        {
            std::cerr << "parse error at byte " << ex.byte << std::endl;
            continue;
        }

        // Access and print the JSON data
        std::cout << "Time: " << time_milliseconds << " | ";
        std::cout << "Command: " << (int)command << std::endl;

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

        if (midi_device != nullptr && time_milliseconds >= 0 && command >= 128 && command <= 240
            && param_1 < 128 && param_2 < 128) {

            midi_device->openPort();
            midiToProcess.push_back(MidiPin(time_milliseconds, midi_device, command, param_1, param_2));
        } else {

            midiRejected.push_back(MidiPin(time_milliseconds, nullptr, command, param_1, param_2));
        }
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

        auto finish = std::chrono::high_resolution_clock::now();
        std::cout << std::chrono::duration_cast<std::chrono::microseconds>(finish-start).count() << "us\n";

        double passed_milliseconds = (double)(std::chrono::duration_cast<std::chrono::microseconds>(finish-start).count()) / 1000;
        std::cout << passed_milliseconds << "ms\n";
    }
    


    
    while (midiProcessed.size() > 0) {
        midiProcessed.pop_front();
    }

    while (midiRejected.size() > 0) {
        midiRejected.pop_front();
    }

    return 0;
}

