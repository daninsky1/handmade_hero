#include "handmade_hero.h"


void game_output_sound(GameState* game_state, GameSoundOutputBuffer& sound_buffer)
{
    int16_t tone_volume = 8000;
    uint32_t wave_period = sound_buffer.samples_per_second / game_state->tone_hz;

    int16_t* sample_out = sound_buffer.samples;
    for (uint32_t sample_i = 0; sample_i < sound_buffer.sample_count; ++sample_i) {
#if 1
        float sine_value = sinf(game_state->tsine);
        int16_t sample_value = static_cast<int16_t>(sine_value * tone_volume);
#else
        int16_t sample_value = 0;
#endif
        *sample_out++ = sample_value;
        *sample_out++ = sample_value;

        const float two_pi = 2.0f * static_cast<float>(M_PI);
        game_state->tsine += two_pi * 1.0f / static_cast<float>(wave_period);
        if (game_state->tsine > two_pi) game_state->tsine -= two_pi;
    }
}

void render_test_gradient(GameOffscreenBuffer& buffer, int xoff, int yoff)
{
    uint8_t* row = static_cast<uint8_t*>(buffer.memory);
    uint32_t* pixel = static_cast<uint32_t*>(buffer.memory);

    for (int y = 0; y < buffer.height; ++y) {
        for (int x = 0; x < buffer.width; ++x) {

            uint8_t green = static_cast<uint8_t>(y + yoff);
            uint8_t blue = static_cast<uint8_t>(x + xoff);
            *pixel++ = static_cast<uint32_t>((green << 8) | blue);
        }
        row += buffer.pitch;
    }
}

void render_player(GameOffscreenBuffer* buffer, int playerx, int playery)
{
    uint8_t* end_of_buffer = static_cast<uint8_t*>(buffer->memory)
        + buffer->height * buffer->pitch;
    uint32_t color = 0xFFFFFFFF;
    int top = playery;
    int bottom = playery + 10;
    for (int x = playerx; x < playerx+10; ++x) {
        uint8_t* pixel = static_cast<uint8_t*>(buffer->memory)
            + x * buffer->bytes_per_pixel
            + top * buffer->pitch;
        for (int y = top; y < bottom; ++y) {
            if ((pixel >= buffer->memory) && (pixel < end_of_buffer)) {
                *reinterpret_cast<uint32_t*>(pixel) = color;
            }
            pixel += buffer->pitch;
        }
    }
}

extern "C" void game_update_and_render(GameMemory* memory, GameInput* input, GameOffscreenBuffer& buffer)
{
    ASSERT((&input->controllers[0].start - &input->controllers[0].buttons[0]) == (ARRAY_COUNT(input->controllers[0].buttons) - 1));
    ASSERT(sizeof(GameState) <= memory->permanent_storage_size);

    GameState* game_state = reinterpret_cast<GameState*>(memory->permanent_storage);
    if (!memory->is_initialized) {
        char* file_name = __FILE__;

#if DEVELOPER_BUILD
        DEBUGReadFileResult file = memory->DEBUG_platform_read_entire_file(file_name);
        if (file.content) {
            memory->DEBUG_platform_write_entire_file("test.txt", file.content_size, file.content);
            memory->DEBUG_platform_free_file_memory(file.content);
        }
#endif

        game_state->tone_hz = 110;
        game_state->tsine = 0.0f;

        game_state->playerx = 100;
        game_state->playery = 100;

        // TODO(casey): This may be more appropriate to do in the pratform layer
        memory->is_initialized = true;
    }
    
    for (uint32_t controller_index = 0; controller_index < ARRAY_COUNT(input->controllers); ++controller_index) {
        GameControllerInput* controller = get_controller(input, controller_index);
        if (controller->is_analog) {
            // NOTE(casey): Use analog movement tuning
            game_state->blue_offset += static_cast<int>(4.0f * controller->stick_averagex);
            game_state->tone_hz = 110 + static_cast<uint32_t>(128.0f * controller->stick_averagey);
        }
        else {
            // NOTE(casey): Use digital movement tuning
            if (controller->move_left.ended_down) {
                game_state->blue_offset -= 1;
            }
            if (controller->move_left.ended_down) {
                game_state->blue_offset += 1;
            }
        }

        float move_step = 6.0f;
        if (controller->move_up.ended_down) {
            game_state->playery -= static_cast<int>(move_step);
        }
        if (controller->move_down.ended_down) {
            game_state->playery += static_cast<int>(move_step);
        }
        if (controller->move_left.ended_down) {
            game_state->playerx -= static_cast<int>(move_step);
        }
        if (controller->move_right.ended_down) {
            game_state->playerx += static_cast<int>(move_step);
        }

        game_state->playerx += static_cast<int>(move_step * controller->stick_averagex);
        game_state->playery -= static_cast<int>(move_step * controller->stick_averagey);

        if (game_state->tjump > 0) {
            game_state->playery += static_cast<int>(5.0f * sinf(0.5f*(float)M_PI*game_state->tjump));

        }
        if (controller->action_down.ended_down) {
            game_state->tjump = 4.0f;
        }
        game_state->tjump -= 0.033f;
    }
    // TODO(casey): Allow sample offsets here for more robust platform options
    render_test_gradient(buffer, game_state->blue_offset, game_state->green_offset);
    render_player(&buffer, game_state->playerx, game_state->playery);
}

extern "C" void game_get_sound_samples(GameMemory* memory, GameSoundOutputBuffer& sound_buffer)
{
    GameState* game_state = reinterpret_cast<GameState*>(memory->permanent_storage);
    game_output_sound(game_state, sound_buffer);
}

#if HANDMADE_WIN32_BUILD
#include "windows.h"
BOOL WINAPI DllMain(
    HINSTANCE,  // handle to DLL module
    DWORD fdwReason,     // reason for calling function
    LPVOID)  // reserved
{
    // Perform actions based on the reason for calling.
    switch( fdwReason )
    {
        case DLL_PROCESS_ATTACH:
         // Initialize once for each new process.
         // Return FALSE to fail DLL load.
            break;

        case DLL_THREAD_ATTACH:
         // Do thread-specific initialization.
            break;

        case DLL_THREAD_DETACH:
         // Do thread-specific cleanup.
            break;

        case DLL_PROCESS_DETACH:
         // Perform any necessary cleanup.
            break;
    }
    return TRUE;  // Successful DLL_PROCESS_ATTACH.
}

#endif

