# JsonMidiPlayer
Very simple MIDI Player intended to be used to play JSON files created by JsonMidiCreator or directly by the generated dynamic library.

# How to compile it
## Prerequisites
### On Windows
1. Download the Visual Studio 2017+ from https://visualstudio.microsoft.com/
2. While installing it, enable the C++ packages
3. Download and install the cmake from https://cmake.org/download/
4. Download and install the Git software from https://git-scm.com/
5. Create a new folder like `C:\GitHub`
6. Open a command line and go to the folder created above
7. Type `git clone https://github.com/ruiseixasm/JsonMidiPlayer.git` in it to clone the repository
### On Linux
1. Install the fluidsynth and their fonts with `sudo apt install fluidsynth fluid-soundfont-gm`
2. Load the font into it `fluidsynth -v -a alsa /usr/share/sounds/sf2/FluidR3_GM.sf2`
3. Create a `GitHub` directory and clone the repository in it with `git clone https://github.com/ruiseixasm/JsonMidiPlayer.git`

## Compiling the repository
### On Windows
Please note that for [`ctypes`](https://docs.python.org/3/library/ctypes.html) only the Visual Studio 2017+ is capable of generating working `.dll` files, the MinGW compiler isn't!
1. Create the folder `build` inside the repository folder
2. Open the command line inside the folder created above
3. Inside the `build` folder type these commands:
    ```
    cmake -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE --no-warn-unused-cli -S .. -B . -T host=x64 -A x64
    cmake --build . --config Release --target ALL_BUILD -j 6 --
    ```
### On Linux
1. Create the folder `build` inside the repository folder
2. 
