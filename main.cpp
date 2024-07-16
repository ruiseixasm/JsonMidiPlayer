#include <iostream>
#include <fstream>
#include <string>
#include <array>
#include <vector>
#include <list>
#include <algorithm>
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
            // Release all pressed key_notes before disconnecting


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
    MidiPin(double time_milliseconds, unsigned char command, unsigned char param_1, unsigned char param_2)
        : time_ms(time_milliseconds), midi_message{command, param_1, param_2} { }

    double getTime() const {
        return time_ms;
    }

    void pluckTooth() {
        if (midi_device != nullptr) {
            // Send the MIDI message
            midi_device->sendMessage(midi_message, 3);
        }
    }

    const unsigned char* getMidiMessage() const {
        return midi_message;
    }
};


int main() {

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
    unsigned char command;
    unsigned char param_1;
    unsigned char param_2;
    
    for (auto jsonElement : jsonData)
    {
        // Create an API with the default API
        try
        {
            time_milliseconds = jsonElement["time_ms"];
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



        if (time_milliseconds >= 0 && command >= 128 && command <= 240
            && param_1 < 128 && param_2 < 128) {

            midiToProcess.push_back(MidiPin(time_milliseconds, command, param_1, param_2));

        } else {

            midiRejected.push_back(MidiPin(time_milliseconds, command, param_1, param_2));
        }
    }
 
    // Sort the list by time in ascendent order
    midiToProcess.sort([]( const MidiPin &a, const MidiPin &b ) { return a.getTime() < b.getTime(); });


    RtMidiOut midiOut;
    RtMidiOut midiOut2;

    MidiDevice someDevicePort("Dummy midi outport", 0);
    
    try {

        // List available MIDI output ports
        std::vector<MidiDevice> midi_devices;
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

        // Open the first available MIDI output port
        if (midiOut.getPortCount() > 0) {
            midiOut.openPort(0);
            if (midiOut2.getPortCount() > 1)
                midiOut2.openPort(1);
        } else {
            std::cerr << "No MIDI output ports available.\n";
            return EXIT_FAILURE;
        }

    } catch (RtMidiError &error) {
        error.printMessage();
        return EXIT_FAILURE;
    }



    auto start = std::chrono::high_resolution_clock::now();

    while (midiToProcess.size() > 0) {
        auto next_point_us = std::chrono::microseconds(static_cast<long long>(midiToProcess.front().getTime() * 1000));
        auto present = std::chrono::high_resolution_clock::now();
        auto elapsed_time_us = std::chrono::duration_cast<std::chrono::microseconds>(present - start);
        auto sleep_time_us = next_point_us - elapsed_time_us;

        std::this_thread::sleep_for(std::chrono::microseconds(sleep_time_us));

        // Send the MIDI message
        midiOut.sendMessage(midiToProcess.front().getMidiMessage(), 3);

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

