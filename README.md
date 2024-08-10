# JsonMidiPlayer
Very simple MIDI Player intended to be used to play JSON files created by JsonMidiCreator or directly by the generated dynamic library.

# How to compile it
## Prerequisites:
### On Windows
    1. Download the Visual Studio 2017+ from https://visualstudio.microsoft.com/
    2. While installing it, enable the C++ configuration
    3. Download and install the cmake from https://cmake.org/download/
    4. Download and install the Git software from https://git-scm.com/
    5. Create a new folder like C:\GitHub
    6. Open a command line and go to the folder created above
    7. Type "git clone https://github.com/ruiseixasm/JsonMidiPlayer.git"
### On Linux
    1. Install the fluidsynth and their fonts
        ```sudo apt install fluidsynth fluid-soundfont-gm```
    2. Load the font into it
        ```fluidsynth -v -a alsa /usr/share/sounds/sf2/FluidR3_GM.sf2```
    3. Create a "GitHub" directory and clone the repository with
        ```git clone https://github.com/ruiseixasm/JsonMidiPlayer.git```


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

    