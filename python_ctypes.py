"""
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
"""
import platform
import os
import ctypes

# Determine the directory of the current Python file
script_dir = os.path.dirname(os.path.abspath(__file__))

# Determine the operating system
current_os = platform.system()

# Define the name of the shared library based on the operating system
if current_os == "Windows":
    lib_name = 'JsonMidiPlayer_ctypes.dll'
elif current_os == "Darwin":  # macOS
    lib_name = 'libJsonMidiPlayer_ctypes.dylib'
else:  # Assume Linux/Unix
    lib_name = 'libJsonMidiPlayer_ctypes.so'

# Construct the full path to the library
lib_path = os.path.join(script_dir, 'lib', lib_name)

# Check if the library file exists
if not os.path.isfile(lib_path):
    raise FileNotFoundError(f"COULD NOT FIND THE LIBRARY FILE: {lib_path}")
else:
    # Print the library path for debugging
    print(f"Library FOUND in: {lib_path}")
    try:
        # Load the shared library
        lib = ctypes.CDLL(lib_path)
        # Define the argument and return types for the C function
        lib.add_ctypes.argtypes = [ctypes.c_int, ctypes.c_int]
        lib.add_ctypes.restype = ctypes.c_int
        # Call the C++ function from Python
        result = lib.add_ctypes(3, 4)
        print(f"3 + 4 = {result}")

    except FileNotFoundError:
        print(f"Could not find the library file: {lib_path}")
    except OSError as e:
        print(f"An error occurred while loading the library: {e}")
    except AttributeError as e:
        print(f"An error occurred while accessing the function: {e}")
    except Exception as e:
        print(f"An unexpected error occurred: {e}")

