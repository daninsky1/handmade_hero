#pragma once

/*
* NOTE(daniel):
* HANDMADE_DEVELOPER_BUILD:
*   0 - Build for public release
*   1 - Build for developer only
* 
* DEVELOPER_PERFORMANCE
*   0 - Slow code welcome.
*   1 - Not slow code allowed!
*/

#include <cstdint>
#define _USE_MATH_DEFINES
#include <cmath>

#if HANDMADE_DEVELOPER_BUILD
    #define ASSERT(expression) if(!(expression)) { *(int*)0 = 0; }
#else
    #define ASSERT(expression)
#endif

#define KILOBYTES(value) ((value) * 1024ull)
#define MEGABYTES(value) (KILOBYTES(value) * 1024ull)
#define GIGABYTES(value) (MEGABYTES(value) * 1024ull)
#define TERABYTES(value) (GIGABYTES(value) * 1024ull)
#define ARRAY_COUNT(Array) (sizeof(Array) / sizeof((Array)[0]))
// TODO(casey): swap, min, max .. macros ???
inline uint32_t safe_truncate_int64_t(int64_t value)
{
    ASSERT(value <= 0xFFFFFFFF);
    uint32_t result = static_cast<uint32_t>(value);
    return result;
}
/*
* NOTE(casey): services that the platform layer provides to the game
*/
#if HANDMADE_DEVELOPER_BUILD
/* IMPORTANT(casey): These are NOT for doing anything in the shipping game = they are
*  blocking and the write doesn't protect againt lost data!
*/
struct DEBUGReadFileResult {
    void* content;
    uint32_t content_size;
};
#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(void* memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory);

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) DEBUGReadFileResult name(char* filename)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) bool name(char* filename, uint32_t memory_size, void* memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile);

#endif

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
    uint32_t samples_per_second;
    uint32_t sample_count;
    int16_t* samples;
};

struct GameButtonState{
    int half_transition;
    bool ended_down;
};
struct GameControllerInput{
    bool is_connected;
    bool is_analog;
    float stick_averagex;
    float stick_averagey;

    union {
        GameButtonState buttons[12];
        struct {
            GameButtonState move_up;
            GameButtonState move_down;
            GameButtonState move_left;
            GameButtonState move_right;

            GameButtonState action_up;
            GameButtonState action_down;
            GameButtonState action_left;
            GameButtonState action_right;

            GameButtonState left_shoulder;
            GameButtonState right_shoulder;

            GameButtonState back;
            GameButtonState start;

            // NOTE(casey): All buttons must be added above this line

            GameButtonState terminator;
        };
    };
};

struct GameInput {
    // TODO(casey): Insert clock values here
    GameControllerInput controllers[5];
};

inline GameControllerInput* get_controller(GameInput* input, uint32_t controller_index)
{
    ASSERT(controller_index < ARRAY_COUNT(input->controllers));
    GameControllerInput* result = &input->controllers[controller_index];
    return result;
}

struct GameMemory {
    bool is_initialized;
    uint64_t permanent_storage_size;
    void* permanent_storage;    // NOTE(casey): REQUIRED to be cleared to zero at startup
    uint64_t transient_storage_size;
    void* transient_storage;    // NOTE(casey): REQUIRED to be cleared to zero at startup
#if HANDMADE_DEVELOPER_BUILD
    // NOTE(daniel): Function pointers
    DEBUGPlatformFreeFileMemory* DEBUG_platform_free_file_memory;
    DEBUGPlatformReadEntireFile* DEBUG_platform_read_entire_file;
    DEBUGPlatformWriteEntireFile* DEBUG_platform_write_entire_file;
#endif
};

#pragma warning(disable: 4100)
#define GAME_UPDATE_AND_RENDER(name) void name(GameMemory* memory, GameInput* input, GameOffscreenBuffer& buffer)
typedef GAME_UPDATE_AND_RENDER(GameUpdateAndRender);
GAME_UPDATE_AND_RENDER(game_update_and_render_stub) { }

// NOTE(casey): At the moment, this has to be a bery fast function, it cannot be
// more than a millisecond or so.
// TODO(casey): Reduce the pressure on this function's performance by measuring it
// or asking about it, etc.
#define GAME_GET_SOUND_SAMPLES(name) void name(GameMemory* memory, GameSoundOutputBuffer& sound_buffer)
typedef GAME_GET_SOUND_SAMPLES(GameGetSoundSamples);
GAME_GET_SOUND_SAMPLES(game_get_sound_samples_stub) { }
#pragma warning(default: 4100)

struct GameState {
    uint32_t tone_hz;
    int green_offset;
    int blue_offset;
    float tsine;
};
