#pragma once

#include <windows.h>
#include <Xinput.h>         // XBox joystick controller
#include <dsound.h>
#include <joystickapi.h>    // Generic joystick controller

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

struct Win32SoundOutput {
    DWORD sample_per_second;
    uint32_t running_sample_index;
    DWORD bytes_per_sample;
    DWORD secondary_buffer_size;
    DWORD safety_bytes;
    float tsine;
    DWORD latency_sample_count;
    // TODO(casey): Should running sample index be in bytes as well?
    // TODO(casey): Math gets simpler if we add a "bytes_per_second" field?
};


struct Win32DEBUGTimeMarker {
    DWORD output_play_cursor;
    DWORD output_write_cursor;
    DWORD output_location;
    DWORD output_byte_count;
    DWORD expected_flip_play_cursor;

    DWORD flip_play_cursor;
    DWORD flip_write_cursor;
};

DEBUGReadFileResult DEBUG_platform_read_entire_file(char* filename);
