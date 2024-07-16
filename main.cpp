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

// For convenience
using json = nlohmann::json;

// Define the Item class
class Item {
public:
    std::string name;
    double price;

    // Constructor
    Item(std::string n, double p) : name(n), price(p) {}

    // Friend function to print the Item
    friend std::ostream &operator<<(std::ostream &os, const Item &item);
};

// Overload the << operator to print Item objects
std::ostream& operator<<(std::ostream& os, const Item& item) {
    os << "Item(name: " << item.name << ", price: " << item.price << ")";
    return os;
}

// Comparator function to sort items by price
bool compareByPrice(const Item& a, const Item& b) {
    return a.price < b.price;
}

// Callback function to handle incoming MIDI messages
void midiCallback(double deltaTime, std::vector<unsigned char> *message, void *userData) {
    unsigned int nBytes = message->size();
    for (unsigned int i = 0; i < nBytes; i++) {
        std::cout << "Byte " << i << " = " << (int)message->at(i) << ", ";
    }
    if (nBytes > 0)
        std::cout << "stamp = " << deltaTime << std::endl;
}


class MidiDevicePort {
private:
    RtMidiOut midiOut;
    unsigned int opened_port = -1;
    const std::string name;
    std::array<unsigned char, 256> keyboards = {0};
public:
    MidiDevicePort(std::string device_name) : name(device_name) { }
    ~MidiDevicePort() {
        midiOut.closePort();
    }

    void openPort(unsigned int port_number) {
        midiOut.openPort(port_number);
        opened_port = port_number;
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

};

// Define the Item class
class MidiPoint {

private:
    const double time_ms;
    const MidiDevicePort *midi_device;
    const unsigned char midi_message[3];

public:
    MidiPoint(double time_milliseconds, unsigned char command, unsigned char param_1, unsigned char param_2)
        : time_ms(time_milliseconds), midi_message{command, param_1, param_2} { }

    double getTime() const {
        return time_ms;
    }

    const unsigned char* getMidiMessage() const {
        return midi_message;
    }
};


void print_list_example();
void print_json_example();
int json_file_example();
int list_midi_in();
int list_midi_out();

int main() {
    // print_list_example();
    // print_json_example();
    // json_file_example();
    // return list_midi_in();
    // return list_midi_out();

    std::list<MidiPoint> midiToProcess;
    std::list<MidiPoint> midiProcessed;
    std::list<MidiPoint> midiRejected;

    // Open the JSON file
    std::ifstream jsonFile("../midiSimpleNotes.json");
    if (!jsonFile.is_open()) {
        std::cerr << "Could not open the file!" << std::endl;
        return 1;
    }

    // Parse the JSON files
    json jsonData;
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
        catch (json::parse_error& ex)
        {
            std::cerr << "parse error at byte " << ex.byte << std::endl;
            continue;
        }

        // Access and print the JSON data
        std::cout << "Time: " << time_milliseconds << " | ";
        std::cout << "Command: " << (int)command << std::endl;



        if (time_milliseconds >= 0 && command >= 128 && command <= 240
            && param_1 < 128 && param_2 < 128) {

            midiToProcess.push_back(MidiPoint(time_milliseconds, command, param_1, param_2));

        } else {

            midiRejected.push_back(MidiPoint(time_milliseconds, command, param_1, param_2));
        }
    }
 
    // Sort the list by time in ascendent order
    midiToProcess.sort([]( const MidiPoint &a, const MidiPoint &b ) { return a.getTime() < b.getTime(); });


    RtMidiOut midiOut;
    RtMidiOut midiOut2;
    
    try {

        // List available MIDI output ports
        std::vector<MidiDevicePort> midi_port_names;
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
                //midi_port_names.push_back(MidiDevicePort(portName));
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





// ChatGPT generated functions used as ressource

int list_midi_out()
{
    try {
        RtMidiOut midiOut;

        // List available MIDI output ports
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
            } catch (RtMidiError &error) {
                error.printMessage();
            }
        }

        // Open the first available MIDI output port
        if (midiOut.getPortCount() > 0) {
            midiOut.openPort(0);
        } else {
            std::cerr << "No MIDI output ports available.\n";
            return EXIT_FAILURE;
        }

        // Create a vector to hold the MIDI message (note on)
        std::vector<unsigned char> message;
        message.push_back(144); // Note on message (144 = 0x90)
        message.push_back(60);  // Note number (60 = Middle C)
        message.push_back(90);  // Velocity

        // Send the MIDI message
        midiOut.sendMessage(&message);

        // Wait for a bit (1 second)
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Create a vector to hold the MIDI message (note off)
        message[2] = 0; // Velocity = 0 for note off
        midiOut.sendMessage(&message);

        std::cout << "MIDI message sent successfully.\n";

    } catch (RtMidiError &error) {
        error.printMessage();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int list_midi_in()
{
    RtMidiIn *midiin = nullptr;

    // Create an API with the default API
    try
    {
        midiin = new RtMidiIn();
    }
    catch (RtMidiError &error)
    {
        error.printMessage();
        exit(EXIT_FAILURE);
    }

    // Check available ports
    unsigned int nPorts = midiin->getPortCount();
    if (nPorts == 0)
    {
        std::cout << "No MIDI input ports available!" << std::endl;
        delete midiin;
        return 0;
    }

    // List available ports
    std::cout << "Available MIDI input ports:" << std::endl;
    for (unsigned int i = 0; i < nPorts; i++)
    {
        try
        {
            std::string portName = midiin->getPortName(i);
            std::cout << i << ": " << portName << std::endl;
        }
        catch (RtMidiError &error)
        {
            error.printMessage();
        }
    }

    // Open the first available port (for demonstration purposes)
    try
    {
        midiin->openPort(0);
    }
    catch (RtMidiError &error)
    {
        error.printMessage();
        delete midiin;
        exit(EXIT_FAILURE);
    }

    // Set the callback function to handle incoming MIDI messages
    midiin->setCallback(&midiCallback);

    // Don't ignore sysex, timing, or active sensing messages
    midiin->ignoreTypes(false, false, false);

    // Wait for user input to quit
    std::cout << "\nReading MIDI input ... press <enter> to quit." << std::endl;
    char input;
    std::cin.get(input);

    // Clean up
    delete midiin;
    
    return 0;
}

void print_list_example()
{
    // Create a list of Item objects
    std::list<Item> itemList = {
        Item("Apple", 1.99),
        Item("Banana", 0.99),
        Item("Orange", 2.49),
        Item("Mango", 1.49)};

    // Sort the list by price using the compareByPrice function
    itemList.sort(compareByPrice);

    // Print the sorted list
    for (const auto &item : itemList)
    {
        std::cout << item << std::endl;
    }
}

void print_json_example()
{
    // Create a JSON object
    json j;
    j["name"] = "John";
    j["age"] = 30;
    j["city"] = "New York";

    // Output the JSON object
    std::cout << j.dump(4) << std::endl; // Pretty print with 4 spaces indent

    // Parse a JSON string
    std::string jsonString = R"({"name": "Jane", "age": 25, "city": "San Francisco"})";
    json parsedJson = json::parse(jsonString);

    // Output parsed JSON object
    std::cout << "Name: " << parsedJson["name"] << std::endl;
    std::cout << "Age: " << parsedJson["age"] << std::endl;
    std::cout << "City: " << parsedJson["city"] << std::endl;

    // Access and modify JSON values
    parsedJson["age"] = 26;
    std::cout << parsedJson.dump(4) << std::endl;
}

int json_file_example()
{
    // Open the JSON file
    std::ifstream jsonFile("../john.json");
    if (!jsonFile.is_open()) {
        std::cerr << "Could not open the file!" << std::endl;
        return 1;
    }

    // Parse the JSON file
    json jsonData;
    jsonFile >> jsonData;

    // Close the JSON file
    jsonFile.close();

    // Access and print the JSON data
    std::cout << "Name: " << jsonData["name"] << std::endl;
    std::cout << "Age: " << jsonData["age"] << std::endl;
    std::cout << "City: " << jsonData["city"] << std::endl;

    return 0;
}

