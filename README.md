# Basic MQTT Dashboard

This repository contains an ESP-IDF project for a basic MQTT dashboard application.

## Project layout
- `main/` - application source code
- `CMakeLists.txt` - CMake build configuration
- `sdkconfig` - project configuration
- `partitions.csv` - partition layout

## Build

```bash
idf.py build
```

## Flash

```bash
idf.py -p <PORT> flash monitor
```
