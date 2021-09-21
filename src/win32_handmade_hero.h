#pragma once

#include <windows.h>
#include <Xinput.h>         // XBox joystick controller
#include <dsound.h>
#include <joystickapi.h>    // Generic joystick controller

struct Win32SoundOutput {
    DWORD sample_per_second;
    uint32_t running_sample_index;
    DWORD bytes_per_sample;
    DWORD secondary_buffer_size;
    float tsine;
    DWORD latency_sample_count;
};

struct Win32DEBUGTimeMarker {
    DWORD play_cursor;
    DWORD write_cursor;
};

struct Win32OffscreenBuffer {
    // NOTE(casey): Pixels are always 32-bits wide, memory order BB GG RR XX
    BITMAPINFO info;
    void* memory;
    int width;
    int height;
    int pitch;
    int bytes_per_pixel;
};

struct Win32WindowDimension {
    int width;
    int height;
};
