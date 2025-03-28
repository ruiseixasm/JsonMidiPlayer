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
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

// Testing program in the project folder
//   Windows: .\build\Release\JsonMidiPlayer.exe -v .\windows_exported_lead_sheet_melody_jmp.json
//            .\build\Release\JsonMidiPlayer.exe -Version
//   Linux: ./build/Release/JsonMidiPlayer.out -v ./linux_exported_lead_sheet_melody_jmp.json
//          ./build/Release/JsonMidiPlayer.out -Version

// #ifdef _WIN32    // Check if it's a Windows machine
#ifdef _MSC_VER     // Check if using Microsoft compiler (Visual Studio 2019 or later) (#if _MSC_VER >= 1920)
    #include <third_party/getopt.h> // Used to process inputed arguments from the command line
#else
    #include <getopt.h>             // Used to process inputed arguments from the command line
#endif

#include "JsonMidiPlayer.hpp"

void printUsage(const char *programName) {
    std::cout << "Usage: " << programName << " [options] input_file_1.json [input_file_2.json]\n"
              << "Options:\n"
              << "  -h, --help       Show this help message and exit\n"
              << "  -v, --verbose    Enable verbose mode\n"
              << "  -V, --version    Prints the current version number\n\n"
              << "More info here: https://github.com/ruiseixasm/JsonMidiPlayer\n\n";
}

int main(int argc, char *argv[]) {

    int verbose = 0;
    int option_index = 0;

    struct option long_options[] = {
        {"help",    no_argument,       nullptr, 'h'},
        {"verbose", no_argument,       nullptr, 'v'},
        {"version", no_argument,       nullptr, 'V'}, // New option for version
        {nullptr,   0,                 nullptr,  0 }
    };

    while (true) {
        int c = getopt_long(argc, argv, "hvV", long_options, &option_index);
        if (c == -1) break;

        switch (c) {
            case 'h':
                printUsage(argv[0]);
                return 2;   // avoids the execution of any file
            case 'v':
                verbose = 1;
                break;
            case 'V': // Handle the --version option
                std::cout << "JsonMidiPlayer " << VERSION << std::endl;
                return 0;   // Exit after printing the version
            case '?':
                // getopt_long already printed an error message.
                return 1;
            default:
                abort();
        }
    }

    if (optind + 1 > argc) {    // optind points to the first non-option argument (at least 1 file)
        std::cerr << "Error: Missing input file(s)\n";
        printUsage(argv[0]);
        return 1;
    }

    int read_files = 0;
    std::stringstream json_files_buffer;
    json_files_buffer << "[";
    for (size_t filename_position = optind; filename_position < argc; filename_position++) {

        const char* filename = argv[filename_position];
        std::ifstream json_file(filename);
        if (!json_file.is_open()) {
            std::cerr << "Could not open the file: " << filename << std::endl;
            continue;
        }
        read_files++;
        json_files_buffer << json_file.rdbuf() << ",";
        json_file.close();
    }
    if (read_files == 0)
        return 1;
    
    std::string json_files_list = json_files_buffer.str();
    // Replace last "," with a "]"
    json_files_list.back() = ']';

    return PlayList(json_files_list.c_str(), verbose);
}
