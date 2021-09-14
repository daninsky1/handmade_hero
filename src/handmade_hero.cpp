#include "handmade_hero.h"


static void game_output_sound(GameSoundOutputBuffer & sound_buffer, int hz)
{
    static float tsine;
    int16_t tone_volume = 8000;
    int tone_hz = hz;
    int wave_period = sound_buffer.samples_per_second / tone_hz;

    int16_t* sample_out = sound_buffer.samples;
    for (int sample_i = 0; sample_i < sound_buffer.sample_count; ++sample_i) {
        float sine_value = std::sinf(tsine);
        int16_t sample_value = static_cast<int16_t>(sine_value * tone_volume);
        *sample_out++ = sample_value;
        *sample_out++ = sample_value;

        tsine += 2.0f * M_PI * 1.0f / static_cast<float>(wave_period);
    }
}

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

static void game_update_and_render(GameOffscreenBuffer& buffer, int blue_offset, int green_offset,
    GameSoundOutputBuffer& sound_buffer, int hz)
{
    // TODO(casey): Allow sample offsets here for more robust platform options
    game_output_sound(sound_buffer, hz);
    render_test_gradient(buffer, blue_offset, green_offset);
}
