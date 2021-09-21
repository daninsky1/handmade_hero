#pragma once

/*
* NOTE(daniel):
* DEVELOPER_BUILD:
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

#if DEVELOPER_BUILD
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
#if DEVELOPER_BUILD
/* IMPORTANT(casey): These are NOT for doing anything in the shipping game = they are
*  blocking and the write doesn't protect againt lost data!
*/
struct DEBUGReadFileResult {
    void* content;
    uint32_t content_size;
};
DEBUGReadFileResult DEBUG_platform_read_entire_file(char* filename);
void DEBUG_platform_free_file_memory(void* memory);
bool DEBUG_platform_write_entire_file(char* filename, uint32_t memory_size, void* memory);
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


struct GameState {
    uint32_t tone_hz;
    int green_offset;
    int blue_offset;
};

struct GameMemory {
    bool is_initialized;
    uint64_t permanent_storage_size;
    void* permanent_storage;    // NOTE(casey): REQUIRED to be cleared to zero at startup
    uint64_t transient_storage_size;
    void* transient_storage;    // NOTE(casey): REQUIRED to be cleared to zero at startup
};

void game_update_and_render(GameMemory* memory, GameInput* input, GameOffscreenBuffer& buffer, GameSoundOutputBuffer& sound_buffer);
