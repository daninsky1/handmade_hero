#pragma once

#include <windows.h>
#include <Xinput.h>
#include <dsound.h>
#include <joystickapi.h>    // Generic joystick controller

#include "handmade_hero.h"


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

struct Win32GameLibrary {
    HMODULE dll;
    FILETIME dll_last_write_time;
    // NOTE(daniel): Function types
    GameUpdateAndRender* update_and_render;
    GameGetSoundSamples* get_sound_samples;

    bool is_valid;
};


#define WIN32_STATE_FILE_COUNT MAX_PATH
struct Win32State {
    uint64_t total_size;
    void* game_memory_block;

    HANDLE recording_handle;
    int input_recording_index;

    HANDLE playback_handle;
    int input_playing_index;

    char exe_filename[WIN32_STATE_FILE_COUNT];
    char *one_past_last_exe_filename_slash;
};
