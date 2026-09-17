# JsonMidiPlayer
Very simple MIDI Player intended to be used to play JSON files created by [JsonMidiCreator](https://github.com/ruiseixasm/JsonMidiCreator) or directly by the generated dynamic library.
# Binary files
If you don't want to compile the source code yourself, you can download the already compiled files from the [JsonMidiPlayer sourceforge site](https://sourceforge.net/projects/json-midi-player/) with the respective direct links below.
## Executable and Library files (ctypes) for JsonMidiCreator
### Releases
https://github.com/ruiseixasm/JsonMidiPlayer/releases
### For Windows
https://sourceforge.net/projects/json-midi-player/files/Windows/
### For Linux
https://sourceforge.net/projects/json-midi-player/files/Linux/
# Compiling Prerequisites
## On Windows
1. Download the Visual Studio 2017+ from https://visualstudio.microsoft.com/
2. While installing it, enable the C++ packages
3. Download and install the cmake from https://cmake.org/download/
4. Download and install the Git software from https://git-scm.com/
5. Create a new folder like `C:\GitHub`
6. Open a command line in the folder created above by typing `cmd` in the windows folder path
7. Type `git clone https://github.com/ruiseixasm/JsonMidiPlayer.git` in it to clone the repository
## On Linux
1. Instal the developing ALSA files with `sudo apt update && sudo apt install -y libasound2-dev`
1. Install the fluidsynth and its fonts with `sudo apt install fluidsynth fluid-soundfont-gm`
1. Load the font into it `fluidsynth -v -a alsa /usr/share/sounds/sf2/FluidR3_GM.sf2`
1. Create a `GitHub` directory and clone the repository into it with `git clone https://github.com/ruiseixasm/JsonMidiPlayer.git`
# Helper software
## On Windows
1. The [loopMidi](https://www.tobias-erichsen.de/software/loopmidi.html) allows the creation of virtual midi ports that interconnect different midi devices.

## On Linux
1. The **VMPK** is a virtual piano that make is possible seeing which keys are being pressed
```sh
sudo apt update
sudo apt install vmpk qsynth fluid-soundfont-gm
```
# Compiling the repository
## On Windows
Please note that for [`ctypes`](https://docs.python.org/3/library/ctypes.html) only the Visual Studio 2017+ is capable of generating working `.dll` files, the MinGW compiler isn't!
1. Create the folder `build` inside the repository folder
2. Open the command line inside the folder created above by typing `cmd` in the windows folder path
3. While in the `build` folder type these commands, one at a time:
    ```sh
    cmake --no-warn-unused-cli -S .. -B . -T host=x64 -A x64
    ```
    ```sh
    cmake --build . --config Release --target ALL_BUILD --
    ```
## On Linux
1. Create the directory `build` inside the repository directory
2. Go to the directory created above and type the following commands, one at a time:
    ```sh
    cmake -DCMAKE_BUILD_TYPE:STRING=Release --no-warn-unused-cli -S.. -B.
    ```
    ```sh
    cmake --build . --config Release --target all --
    ```
# Testing the build
## On Windows
1. Go to the root project folder and open the command line by typing `cmd` in the windows folder path
2. Type the following commands:
    ```sh
    .\build\Release\JsonMidiPlayer.exe -v .\three_notes.json
    ```
    ```sh
    .\build\Release\JsonMidiPlayer.exe -Version
    ```
## On Linux
1. Go to the root project directory and type the following commands:
    ```sh
    ./build/JsonMidiPlayer.out -v ./three_notes.json
    ```
    ```sh
    ./build/JsonMidiPlayer.out -Version
    ```
# Python library for JsonMidiCreator
It is possible to run this program directly from the [JsonMidiCreator](https://github.com/ruiseixasm/JsonMidiCreator) with the `>> Play()` operation, you just need to do the following.
## On Windows
1. Create the folder `lib` inside the cloned [JsonMidiCreator](https://github.com/ruiseixasm/JsonMidiCreator) repository
2. Copy the file `JsonMidiPlayer_ctypes.dll` inside the folder `.\build\lib\Release\` into the folder created above
## On Linux
1. Create the directory `lib` inside the cloned [JsonMidiCreator](https://github.com/ruiseixasm/JsonMidiCreator) repository
2. Copy the file `libJsonMidiPlayer_ctypes.so` inside the directory `./build/lib/` into the directory created above
# Help command
With the help command it is possible to know the diferent parameters to run the program.
```sh
PS C:\Users\rui\Documents\GitHub\JsonMidiPlayer> .\build\Release\JsonMidiPlayer.exe -h                        
Usage: C:\Users\rui\Documents\GitHub\JsonMidiPlayer\build\Release\JsonMidiPlayer.exe [options] input_file_1.json [input_file_2.json]
Options:
  -h, --help       Show this help message and exit
  -l, --loops      The amount of loops the playlist will be played with 1 loop as the default
  -v, --verbose    Enable verbose mode
  -V, --version    Prints the current version number

More info here: https://github.com/ruiseixasm/JsonMidiPlayer

PS C:\Users\rui\Documents\GitHub\JsonMidiPlayer> 
```
# Midi Drag and Delays
The program is quite simple and light, it just loops a list so no much Drag or Delay shall be expected to happen.
Drag is the amount of time the entire playing got out of sync while Delay is just the amount of time each single midi message played out of tempo.
## Example of reported Drag and Delay
Despite some delay, that didn't mean any drag in the example bellow.
```
PS C:\Users\rui\Documents\GitHub\JsonMidiPlayer> .\build\Release\JsonMidiPlayer.exe -v -l 2 .\three_notes.json
JsonMidiPlayer version: 7.2.0
Available output Midi devices:
        Midi device #0: Microsoft GS Wavetable Synth 0
        Midi device #1: loopMIDI Port 1
        Midi device #2: DAW_Port 2
Devices connected:       loopMIDI Port 1
Data stats reporting:
        Midi Messages processing time (ms):                0
        Midi Ports opening time (ms):                      0
        Single loop length (beats):                       28
        Total generated Midi Messages (included):        674
        Total validated Midi Messages (accepted):         32
        Total incorrect Midi Messages (excluded):          0
        Total redundant Midi Messages (excluded):          0
        Total resultant Midi Messages (included):        706
The playlist will now be played in 2 loops for 0 minutes and 25 seconds...
Devices disconnected:    loopMIDI Port 1
Midi stats reporting:
        Total drag (ms):                                   0.000 \
        Cumulative delay (ms):                            14.742 /
        Maximum delay (ms):                                0.832 \
        Minimum delay (ms):                                0.001 /
        Average delay (ms):                                0.021 \
        Standard deviation of delays (ms):                 0.088 /
PS C:\Users\rui\Documents\GitHub\JsonMidiPlayer> 
```

# Troubleshooting
## On Linux
Some distributions don't run **ALSA** by default, in those cases make sure the **VMPK** is
configured to use **ALSA** as the Midi in port and then check it by running these commands.
```sh
aconnect -l
ps aux | grep -i vmpk
```


