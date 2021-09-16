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

void game_update_and_render(GameMemory* memory, GameInput* input, GameOffscreenBuffer& buffer, GameSoundOutputBuffer& sound_buffer)
{
    ASSERT(sizeof(GameState) <= memory->permanent_storage_size);

    GameState* game_state = reinterpret_cast<GameState*>(memory->permanent_storage);
    if (!memory->is_initialized) {
        char* file_name = __FILE__;

        DEBUGReadFileResult file = DEBUG_platform_read_entire_file(file_name);
        if (file.content) {
            DEBUG_platform_write_entire_file("test.txt", file.content_size, file.content);
            DEBUG_platform_free_file_memory(file.content);
        }
        game_state->tone_hz = 110;

        // TODO(casey): This may be more appropriate to do in the pratform layer
        memory->is_initialized = true;
    }
    int tone_volume = 8000;

    GameControllerInput* input0 = &input->controllers[0];
    if (input0->is_analog) {
        // NOTE(casey): Use analog movement tuning
        game_state->blue_offset += static_cast<int>(4.0f * input0->endx);
        game_state->tone_hz = 110 + static_cast<int>(128.0f * input0->endy);
    }
    else {
        // NOTE(casey): Use digital movement tuning
    }

    if (input0->down.ended_down) {
        game_state->green_offset += 1;
    }
    // TODO(casey): Allow sample offsets here for more robust platform options
    game_output_sound(sound_buffer, game_state->tone_hz);
    render_test_gradient(buffer, game_state->blue_offset, game_state->green_offset);
}
