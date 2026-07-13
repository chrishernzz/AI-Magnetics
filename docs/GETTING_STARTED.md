# Getting Started: Build & Run AIMagnetics
This guide walks you through building and running the tool locally.

---
## Prerequisites
### Windows (Recommended)
- **Visual Studio 2019+** or **Visual Studio Build Tools** (for MSVC compiler)
- **CMake 3.16+** ([download](https://cmake.org/download/))
- **Git** (optional, for cloning)
### macOS/Linux
- **GCC 9+** or **Clang 10+**
- **CMake 3.16+**

---
## Build Instructions
### Step 1: Generate Build Files with CMake
```bash
# Navigate to project root
cd c:\Users\chernandez1\OneDrive - AMETEK Inc\Desktop\AIMagnetics
# Create build directory
mkdir build
cd build
# Generate Visual Studio project files (Windows)
cmake -G "Visual Studio 16 2019" ..

# OR: For Unix-like systems
# cmake ..

Step 2: Build the Executable
Windows (Visual Studio):
cmake --build . --config Release
macOS/Linux:
make

Step 3: Run the Server
Navigate to the build output directory and run:
Windows:
magnetics_server.exe
macOS/Linux:
./magnetics_server
You should see:
Server started on port 8080
Listening for requests...
