#pragma once

#include <cstdint>
#define _USE_MATH_DEFINES
#include <cmath>

#define ARRAY_COUNT(Array) (sizeof(Array) / sizeof((Array)[0]))
// TODO(casey): swap, min, max .. macros ???

/*
* TODO(casey): services that the platform layer provides to the game
*/

/*
* NOTE(casey): Services that the game provides to the platforma layer
* (this may expand in the future - sound on separate thread, etc.)
*/

// TODO(casey): In the future, rendering _specifically_ will become a three-tiered abstraction!!!

struct GameOffscreenBuffer {
    // NOTE(casey): Pixels are always 32-bits wide, memory order BB GG RR XX
    void* memory;
    int width;
    int height;
    int pitch;
};

struct GameSoundOutputBuffer {
    int samples_per_second;
    int sample_count;
    int16_t* samples;
};

struct GameButtonState{
    int half_transition;
    bool ended_down;
};
struct GameControllerInput{
    bool is_analog;

    float startx;
    float starty;

    float minx;
    float miny;

    float maxx;
    float maxy;

    float endx;
    float endy;

    union {
        GameButtonState buttons[6];
        struct {
            GameButtonState up;
            GameButtonState down;
            GameButtonState left;
            GameButtonState right;
            GameButtonState left_shoulder;
            GameButtonState right_shoulder;
        };
    };
};
struct GameInput {
    GameControllerInput controllers[4];
};

static void game_update_and_render(GameInput* input, GameOffscreenBuffer& buffer, GameSoundOutputBuffer& sound_buffer);
