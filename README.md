# MidiJsonPlayer
Very simple MIDI Player of JSON files

# Prerequisites on Linux
sudo apt install libasound2-dev

# Prerequisites on Windows

# For Winmm.dll Is Missing or Not Found Error in Windows 10 FIX
Open command line sa Administrator and run the commands:
    1. sfc /scannow
    2. DISM.exe /Online /Cleanup-image /Restorehealth

Include the following line in CMakeLists.txt:
    1. add_compile_definitions(__WINDOWS_MM__)

    