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


// MidiPin methods definition
void MidiPin::pluckTooth() {
    if (midi_device != nullptr)
        midi_device->sendMessage(&midi_message);
}


// MidiDevice methods definition
bool MidiDevice::openPort() {
    if (!opened_port && !unavailable_device) {
        try {
            midiOut.openPort(port);
            opened_port = true;
            if (verbose) std::cout << "   " << name;
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
        if (verbose) std::cout << "   " << name;
    }
}

bool MidiDevice::hasPortOpen() const {
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


int PlayList(const char* json_str, int loops, bool verbose) {
    
	if (loops < 0) {
		if (verbose) std::cout << "\tNegative loops are NOT acceptable.\n";
		return 1;
	}

	Player player;

	int read_error = player.readAvailableDevices(verbose);
	if (read_error) return read_error;
	int load_error = player.loadJsonContent(json_str, verbose);
	if (load_error) return load_error;

	player.processMidiPins();
	player.addClockingPins();
	player.applyTime_ms();
	player.reportProcessing(verbose);
	player.printPlayingTime(loops, verbose);
	player.loopPlaylist(loops);
	player.reportPlaying(verbose);

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