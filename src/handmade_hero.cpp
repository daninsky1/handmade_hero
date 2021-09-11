#include "handmade_hero.h"

static void render_test_gradient(GameOffscreenBuffer& buffer, int xoff, int yoff)
{
    uint8_t* row = static_cast<uint8_t*>(buffer.memory);
    uint32_t* pixel = static_cast<uint32_t*>(buffer.memory);

    for (int y = 0; y < buffer.height; ++y) {
        for (int x = 0; x < buffer.width; ++x) {
            uint8_t blue = x + xoff;
            uint8_t green = y + yoff;

            *pixel++ = (green << 8) | blue;
        }
        row += buffer.pitch;
    }
}

static void game_update_and_render(GameOffscreenBuffer& buffer, int blue_offset, int green_offset)
{
    render_test_gradient(buffer, blue_offset, green_offset);
}
