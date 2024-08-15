#include <iostream>
#include <thread>
#include <chrono>

#ifdef _WIN32
    #include <Windows.h>
    #ifdef max
        #undef max
    #endif
    #ifdef min
        #undef min
    #endif
#else
    #include <pthread.h>
    #include <time.h>
#endif

// Function to set real-time scheduling
void setRealTimeScheduling() {
#ifdef _WIN32
    // Set the thread priority to highest for real-time scheduling on Windows
    // https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreadpriority
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    // THREAD_MODE_BACKGROUND_BEGIN        0x00010000
    // THREAD_MODE_BACKGROUND_END          0x00020000
    // THREAD_PRIORITY_ABOVE_NORMAL        1
    // THREAD_PRIORITY_BELOW_NORMAL        -1
    // THREAD_PRIORITY_HIGHEST             2
    // THREAD_PRIORITY_IDLE                -15
    // THREAD_PRIORITY_LOWEST              -2
    // THREAD_PRIORITY_NORMAL              0
    // THREAD_PRIORITY_TIME_CRITICAL       15
    // https://learn.microsoft.com/en-us/windows/win32/procthread/scheduling-priorities

#else
    // Set real-time scheduling on Linux
    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
#endif
}

// High-resolution sleep function
void highResolutionSleep(long long microseconds) {
#ifdef _WIN32
    // Windows: High-resolution sleep using QueryPerformanceCounter
    LARGE_INTEGER frequency, start, end;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);

    long long sleepInterval = microseconds > 2000 ? 1000 : 0;  // Sleep 1ms if the wait is longer than 2ms
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

int main() {
    // Set real-time scheduling
    setRealTimeScheduling();
    
    // Perform high-resolution sleep
    std::cout << "Sleeping for 1000 microseconds...\n";
    highResolutionSleep(1000);  // Sleep for 1000 microseconds (1 millisecond)
    
    std::cout << "Woke up!\n";
    
    return 0;
}
