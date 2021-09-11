#pragma once

#include <cstdint>
/*
* TODO(casey): services that the platform layer provides to the game
* 

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

static void game_update_and_render(GameOffscreenBuffer& buffer, int blue_offset, int green_offset);
