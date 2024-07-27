#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <getopt.h>             // Used to process inputed arguments from the command line
#include "JsonMidiPlayer.hpp"

void printUsage(const char *programName) {
    std::cout << "Usage: " << programName << " [options] input_file output_file\n"
              << "Options:\n"
              << "  -h, --help       Show this help message and exit\n"
              << "  -v, --verbose    Enable verbose mode\n";
}

int main(int argc, char *argv[]) {

    for (size_t argi = 0; argi < argc; argi++) {
        std::cout << argv[argi] << std::endl;
    }

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
                return 2;   // avoids the execution of any file
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

    if (optind + 1 > argc) {    // optind points to the first non-option argument (at least 1 file)
        std::cerr << "Error: Missing input or output file\n";
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

    return PlayList(json_files_list.c_str());
}
