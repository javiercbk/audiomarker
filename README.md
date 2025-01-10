# Audiomarker

Tool to annotate audio files.

## Building

```sh
# Create and enter build directory
mkdir build
cd build

# Configure with CMake release
cmake ..
# or debug
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Build
make -j$(nproc)
```