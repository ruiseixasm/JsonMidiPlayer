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


int play(const char* json_str, int loops, int rec_transport, bool verbose) {
    
	#ifdef DEBUGGING
	std::cout << "\t\tLOOPS: 	" << loops << std::endl;
	std::cout << "\t\tRECORD:   " << rec_transport << std::endl;
	std::cout << "\t\tVERBOSE: 	" << verbose << std::endl;
	#endif

    if (verbose) std::cout << "JsonMidiPlayer version: " << VERSION << std::endl;

	if (loops < 0) {
		if (verbose) std::cout << "\tNegative loops are NOT acceptable.\n";
		return 1;
	}

	Player player;
	// Data input
	int read_error = player.readAvailableDevices(verbose);
	if (read_error) return read_error;
	int load_error = player.loadJsonContent(json_str, verbose);
	if (load_error) return load_error;
	// Midi pins processing
	player.prepareMidiPins();
	player.addClockingTransportPins(rec_transport);	// Includes MIDI Machine Control or Transport CC control messages
	player.applyTime_ms();
	player.addMtcPins();	// MTC pins are purely time_ms pins, so, they must come after `time_ms` being applied to tick pins
	player.reportProcessing(verbose);
	// Midi pins playing
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

