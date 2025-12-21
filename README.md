## Introduction

## Dependencies
1. raylib
2. enet
3. flatbuffers

## Instructions

1. Install CMake and G++ compiler
```
sudo apt update
sudo apt install -y cmake g++
```

2. Install dependencies for raylib
```
sudo apt install -y libx11-dev libxcursor-dev libxinerama-dev libxrandr-dev libxi-dev libgl1-mesa-dev
```

3. Build
```
mkdir build && cd build
cmake ..
cmake --build .
```