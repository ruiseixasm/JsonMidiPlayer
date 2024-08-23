# JsonMidiPlayer
Very simple MIDI Player intended to be used to play JSON files created by [JsonMidiCreator](https://github.com/ruiseixasm/JsonMidiCreator) or directly by the generated dynamic library.
# Prerequisites
## On Windows
1. Download the Visual Studio 2017+ from https://visualstudio.microsoft.com/
2. While installing it, enable the C++ packages
3. Download and install the cmake from https://cmake.org/download/
4. Download and install the Git software from https://git-scm.com/
5. Create a new folder like `C:\GitHub`
6. Open a command line in the folder created above by typing `cmd` in the windows folder path
7. Type `git clone https://github.com/ruiseixasm/JsonMidiPlayer.git` in it to clone the repository
## On Linux
1. Install the fluidsynth and its fonts with `sudo apt install fluidsynth fluid-soundfont-gm`
2. Load the font into it `fluidsynth -v -a alsa /usr/share/sounds/sf2/FluidR3_GM.sf2`
3. Create a `GitHub` directory and clone the repository into it with `git clone https://github.com/ruiseixasm/JsonMidiPlayer.git`
# Compiling the repository
## On Windows
Please note that for [`ctypes`](https://docs.python.org/3/library/ctypes.html) only the Visual Studio 2017+ is capable of generating working `.dll` files, the MinGW compiler isn't!
1. Create the folder `build` inside the repository folder
2. Open the command line inside the folder created above by typing `cmd` in the windows folder path
3. While in the `build` folder type these commands, one at a time:
    ```
    cmake --no-warn-unused-cli -S .. -B . -T host=x64 -A x64
    ```
    ```
    cmake --build . --config Release --target ALL_BUILD --
    ```
## On Linux
1. Create the directory `build` inside the repository directory
2. Go to the directory created above and type the following commands, one at a time:
    ```
    cmake -DCMAKE_BUILD_TYPE:STRING=Release --no-warn-unused-cli -S.. -B.
    ```
    ```
    cmake --build . --config Release --target all --
    ```
# Testing the build
## On Windows
1. Go to the repository folder `.\build\Release\` and open the command line by typing `cmd` in the windows folder path
2. Type the following command:
    ```
    JsonMidiPlayer.exe ..\..\midiSimpleNotes.json --verbose
    ```
## On Linux
1. go to the repository directory `./build/` and type the following command:
    ```
    ./JsonMidiPlayer.out ../midiSimpleNotes.json --verbose
    ```
# Python library for JsonMidiCreator
It is possible to run this program directly from the [JsonMidiCreator](https://github.com/ruiseixasm/JsonMidiCreator) with the `>> Play()` operation, you just need to do the following.
## On Windows
1. Create the folder `lib` inside the cloned [JsonMidiCreator](https://github.com/ruiseixasm/JsonMidiCreator) repository
2. Copy the file `JsonMidiPlayer_ctypes.dll` inside the folder `.\build\lib\Release\` into the folder created above
## On Linux
1. Create the directory `lib` inside the cloned [JsonMidiCreator](https://github.com/ruiseixasm/JsonMidiCreator) repository
2. Copy the file `libJsonMidiPlayer_ctypes.so` inside the directory `./build/lib/` into the directory created above
# Midi Drag and Delays
The program is quite simple and light, it just loops a list so no much Drag or Delay shall be expected to happen.
Drag is the amount of time the entire playing got out of sync while Delay is just the amount of time each single midi message played out of tempo.
## Example of reported Drag and Delay
Despite some delay, that didn't mean any drag in the example bellow.
```
Midi stats reporting:
        Total processed Midi Messages (sent):            611
        Total redundant Midi Messages (not sent):          0
        Total excluded Midi Messages (not sent):           0
        Total drag (ms):                                   0.000
        Total delay (ms):                                  4.901
        Maximum delay (ms):                                0.282
        Minimum delay (ms):                                0.000
        Average delay (ms):                                0.008
        Standard deviation of delays (ms):                 0.033
```
