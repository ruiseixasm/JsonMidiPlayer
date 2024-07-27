# JsonMidiPlayer
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


# Start the fluidsynth on Linux
    1. sudo apt install fluidsynth fluid-soundfont-gm
    2. fluidsynth -v -a alsa /usr/share/sounds/sf2/FluidR3_GM.sf2

# Extra commands to check midi connection
    1. aconnect -o
    2. aconnect 14:0 128:0
    3. aconnect -i -o
    4. aconnect 14:0 128:0
    5. aconnect -l

# How to add Debugger configuration .json file on VSCode
    1. Run->Add Configuration...
    2. Press the corner button "Add Configuration..." on the launch.json file
    3. Choose the "launch" version of the c++ debugger

    