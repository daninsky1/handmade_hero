#pragma once

#include <windows.h>
#include <Xinput.h>         // XBox joystick controller
#include <dsound.h>
#include <joystickapi.h>    // Generic joystick controller

struct Win32SoundOutput {
    int sample_per_second;
    uint32_t running_sample_index;
    int bytes_per_sample;
    int secondary_buffer_size;
    float tsine;
    int latency_sample_count;
};

struct Win32OffscreenBuffer {
    // NOTE(casey): Pixels are always 32-bits wide, memory order BB GG RR XX
    BITMAPINFO info;
    void* memory;
    int width;
    int height;
    int pitch;
};

struct Win32WindowDimension {
    int width;
    int height;
};
