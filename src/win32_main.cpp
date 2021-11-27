/*
* TODO(casey): THIS IS NOT A FINAL PLATFORM LAYER!!!
* 
* Saved game location
* Getting a handle to our own executable file
* Asset loading path
* Threading (launch a thread)
* Raw Input (support for multiple keyboards)
* Sleep/timeBeginPrtiod
* ClipCursor() (for multimonitor support)
* Fullscreen support
* WM_SETCURSOR (control cursor visibility)
* QueryCancelAutoplay
* WM_ACTIVATEAPP (for when we are not the active applicatioon)
* Blit speed improvements (BitBlt)
* Hardware acceleration (OpenGL or Direct3D or BOTH??)
* GetKeyboardLayout (for French keyboards, international WASD support)
* 
* just a partial list of stuff!!
*/


#define _USE_MATH_DEFINES
#include <iostream>
#include <string>
#include <sstream>
#include <cstdint>

#include <stdio.h>
#include <malloc.h>

// TODO(casey): Implement sine ourselves
#include <cmath>

#include "win32_main.h"
#include "handmade_hero.h"

// TODO:(casey): This is a global for now
static bool glob_running;
static bool glob_pause;
static Win32OffscreenBuffer glob_backbuffer;
static LPDIRECTSOUNDBUFFER glob_secondary_buffer;
static int64_t glob_perf_count_frequency;

// NOTE(daniel): I got to make globally available for windows procedure get access
// to this
static int xoff = 0;
static int yoff = 0;

// NOTE(casey): XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState)
typedef X_INPUT_GET_STATE(xinput_get_state);
DWORD WINAPI XInputGetStateStub(DWORD, XINPUT_STATE*)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
static xinput_get_state* XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// NOTE(casey): XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_SET_STATE(xinput_set_state);
DWORD WINAPI XInputSetStateStub(DWORD, XINPUT_VIBRATION*)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
static xinput_set_state* XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPGUID lpGuid, LPDIRECTSOUND* ppDS, LPUNKNOWN  pUnkOuter);
typedef DIRECT_SOUND_CREATE(direct_sound_create);

#if HANDMADE_DEVELOPER_BUILD
void DEBUG_print_last_error()
{
    // TODO(daniel); fmt_err handiling
    DWORD err_code = GetLastError();
    CHAR msg[256];

    DWORD fmt_err = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM,
        NULL, err_code,
        LANG_USER_DEFAULT,
        (LPTSTR)msg,
        256,
        NULL
    );

    OutputDebugStringA(msg);
}

DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUG_platform_free_file_memory)
{
    if (memory) {
        VirtualFree(memory, 0, MEM_RELEASE);
    }
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUG_platform_read_entire_file)
{
    DEBUGReadFileResult result = {};

    HANDLE file_handle = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (file_handle != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER file_size;
        if (GetFileSizeEx(file_handle, &file_size)) {
            uint32_t file_size32 = safe_truncate_int64_t(file_size.QuadPart);
            result.content = VirtualAlloc(0, file_size32, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
            if (result.content) {
                DWORD bytes_read;
                if (ReadFile(file_handle, result.content, file_size32, &bytes_read, 0)
                    && file_size32 == bytes_read) {
                    // NOTE(casey): File read successfully
                    result.content_size = file_size32;
                }
                else {
                    // TODO(casey): Logging
                    DEBUG_platform_free_file_memory(result.content);
                    result.content = nullptr;
                }
            }
            else {
                // TODO(casey): Logging
            }
        }
        else {
            // TODO(casey): Logging
        }
        CloseHandle(file_handle);
    }
    else {
        // TODO(casey): Logging
    }
    return result;
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUG_platform_write_entire_file)
{
    bool result = false;

    HANDLE file_handle = CreateFile(filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if (file_handle != INVALID_HANDLE_VALUE) {
        DWORD bytes_written;
        if (WriteFile(file_handle, memory, memory_size, &bytes_written, 0)) {
            // NOTE(casey): File written successfully
            result = bytes_written == memory_size;
        }
        else {
            // TODO(casey): Logging
        }
        CloseHandle(file_handle);
    }
    else {
        // TODO(casey): Logging
    }

    return result;
}
#endif

inline FILETIME win32_get_last_write_time(const char* filename)
{
    FILETIME last_write_time = { };

    WIN32_FIND_DATA find_data;
    HANDLE find_handle = FindFirstFileA(filename, &find_data);
    if (find_handle != INVALID_HANDLE_VALUE) {
        last_write_time = find_data.ftLastWriteTime;
        FindClose(find_handle);
    }

    return last_write_time;
}

// NOTE(daniel): warning 4191, disabled for GetProcAddress to avoid cast madness:
// reinterpret_cast<funcptr>(reinterpret_cast<void*>(GetProcAddress(...))))
static bool win32_load_game_library(const char* dllname, Win32GameLibrary& game_lib)
{
    bool error = false;
    game_lib.dll_last_write_time = win32_get_last_write_time(dllname);
    constexpr char* temp_dll_name = "handmade_hero_temp.dll";

    // IMPORTANT(daniel): BUG:
    // Storing CopyFile() return error status causes the file to get lock in
    // our process and never been released.
    // A fast correction is to not store this return value until we find a
    // better way to do it
    CopyFile(dllname, temp_dll_name, FALSE);

    game_lib.dll = LoadLibraryA(temp_dll_name);

    if (game_lib.dll) {
#pragma warning(disable: 4191)
        game_lib.update_and_render = reinterpret_cast<GameUpdateAndRender*>(
            GetProcAddress(game_lib.dll, "game_update_and_render"));
        game_lib.get_sound_samples = reinterpret_cast<GameGetSoundSamples*>(
            GetProcAddress(game_lib.dll, "game_get_sound_samples"));
#pragma warning(default: 4191)
        game_lib.is_valid = game_lib.update_and_render && game_lib.get_sound_samples;
    }

    if (!game_lib.is_valid) {
        error = true;
        game_lib.update_and_render = game_update_and_render_stub;
        game_lib.get_sound_samples = game_get_sound_samples_stub;
    }

    return error;
}

static void win32_unload_game_library(Win32GameLibrary* game_lib)
{
    if (game_lib) {
        FreeLibrary(game_lib->dll);
        game_lib->dll = 0;
    }
    game_lib->is_valid = false;
    game_lib->update_and_render = game_update_and_render_stub;
    game_lib->get_sound_samples = game_get_sound_samples_stub;
}

static void win32_load_xinput()
{
    HMODULE xinput_library = LoadLibraryA("xinput1_4.dll");
    if (!xinput_library) {
        xinput_library = LoadLibraryA("xinput9_1_0.dll");
    }
    if (!xinput_library) {
        // TODO(casey): Diagnostic
        xinput_library = LoadLibraryA("xinput1_3.dll");
    }
    
#pragma warning(disable: 4191)
    if (xinput_library) {

         XInputGetState = reinterpret_cast<xinput_get_state*>(GetProcAddress(xinput_library, "XInputGetState"));
         if (!XInputGetState) XInputGetState = XInputGetStateStub;
         XInputSetState = reinterpret_cast<xinput_set_state*>(GetProcAddress(xinput_library, "XInputSetState"));
         if (!XInputSetState) XInputSetState = XInputSetStateStub;
         // TODO(casey): Diagnostic
#pragma warning(default: 4191)
    }
        else {
        // TODO(casey): Diagnostic
    }
}

static void win32_init_dsound(HWND window, DWORD samples_per_second, DWORD buffer_size)
{
    // NOTE(casey): Load the library
    HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");

    if (DSoundLibrary) {
        // TODO(daniel): Check warning 4191
#pragma warning(disable: 4191)
        // NOTE(casey): Get a DirectSound object! - cooperative
        direct_sound_create* DirectSoundCreate = reinterpret_cast<direct_sound_create*>(GetProcAddress(DSoundLibrary, "DirectSoundCreate"));
#pragma warning(disable: 4191)
        // TODO(casey): Double-check that this works on XP - DirectSound8 or 7??
        LPDIRECTSOUND DirectSound;
        if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0))) {
            WAVEFORMATEX wave_format = {};
            wave_format.wFormatTag = WAVE_FORMAT_PCM;
            wave_format.nChannels = 2;
            wave_format.nSamplesPerSec = samples_per_second;
            wave_format.wBitsPerSample = 16;
            wave_format.nBlockAlign = static_cast<WORD>(wave_format.nChannels * wave_format.wBitsPerSample / 8);
            wave_format.nAvgBytesPerSec = wave_format.nSamplesPerSec * wave_format.nBlockAlign;
            wave_format.cbSize = 0;
            
            if (SUCCEEDED(DirectSound->SetCooperativeLevel(window, DSSCL_PRIORITY))) {
                DSBUFFERDESC buffer_description = {};
                buffer_description.dwSize = sizeof(buffer_description);
                buffer_description.dwFlags = DSBCAPS_PRIMARYBUFFER;

                // NOTE(casey): "Create" a primary buffer
                // TODO(casey): DSBCAPS_GLOBALFOCUS
                LPDIRECTSOUNDBUFFER primary_buffer;
                if (SUCCEEDED(DirectSound->CreateSoundBuffer(&buffer_description, &primary_buffer, 0))) {

                    if (SUCCEEDED(primary_buffer->SetFormat(&wave_format))) {
                        // TODO(casey): We have finally set the format!
                        OutputDebugStringA("Primary buffer format was set.\n");
                    }
                    else {
                        // TODO(casey): Diagnostic
                    }
                }
                else {
                    // TODO(casey): Diagnostic
                }
            }
            else {
                // TODO(casey): Diagnostic
            }

            // TODO(casey): DSBCAPD_GETCURRENTPOSITION2
            DSBUFFERDESC buffer_description = {};
            buffer_description.dwSize = sizeof(buffer_description);
            buffer_description.dwFlags = 0;
            buffer_description.dwBufferBytes = buffer_size;
            buffer_description.lpwfxFormat = &wave_format;
            if (SUCCEEDED(DirectSound->CreateSoundBuffer(&buffer_description, &glob_secondary_buffer, 0))) {
                OutputDebugStringA("Secondary buffer format was created successfully.\n");
            }
        }
        else {
            // TODO(casey): Diagnostic
        }
    }
    else {
        // TODO(casey): Diagnostic
    }

}

static Win32WindowDimension win32_get_window_dimension(HWND window)
{
    Win32WindowDimension result;
    RECT client_rect;
    GetClientRect(window, &client_rect);
    result.width = client_rect.right - client_rect.left;
    result.height = client_rect.bottom - client_rect.top;

    return result;
}

static void win32_resize_dib_section(Win32OffscreenBuffer& buffer, int w, int h)
{
    // TODO(casey): Bulletproof this.
    // Maybe don't free first, free after, then free first if that fails.
    if (buffer.memory) {
        VirtualFree(buffer.memory, 0, MEM_RELEASE);
    }

    buffer.width = w;
    buffer.height = h;
    int bytes_per_pixel = 4;
    buffer.bytes_per_pixel = bytes_per_pixel;

    buffer.info.bmiHeader.biSize = sizeof(buffer.info.bmiHeader);
    buffer.info.bmiHeader.biWidth = buffer.width;
    buffer.info.bmiHeader.biHeight = -buffer.height;
    buffer.info.bmiHeader.biPlanes = 1;
    buffer.info.bmiHeader.biBitCount = 32;
    buffer.info.bmiHeader.biCompression = BI_RGB;

    size_t bitmap_memory_sz = static_cast<size_t>(buffer.width * buffer.height * bytes_per_pixel);
    buffer.memory = VirtualAlloc(0, bitmap_memory_sz, MEM_COMMIT, PAGE_READWRITE);
    buffer.pitch = w * bytes_per_pixel;

    // TODO(casey): Probably clear this to black
}

static void win32_display_buffer_in_window(Win32OffscreenBuffer& buffer,
                                           HDC device_context,
                                           int window_width, int window_height)
                                           
{
    // TODO:(casey): For prototyping purposes, we're going to always blit 1-to-1
    // pixels to make sure we don't introduce artifactis with stretching while
    // we are learning to code the renderer!
    StretchDIBits(device_context,
                  0, 0, window_width, window_height,
                  0, 0, buffer.width, buffer.height,
                  buffer.memory,
                  &buffer.info,
                  DIB_RGB_COLORS, SRCCOPY);
}


LRESULT CALLBACK win32_main_window_proc_cb(HWND   window,
                                           UINT   message,
                                           WPARAM w_parameter,
                                           LPARAM l_parameter)
{

    LRESULT result = 0;

    switch (message) {
    case WM_SIZE:
    {
        break;
    }
    case WM_CLOSE:
        // TODO:(casey): Handle this with a message to the user?
        glob_running = false;
        OutputDebugString("WM_CLOSE\n");
        break;
    case WM_ACTIVATEAPP:
#if HANDMADE_DEVELOPER_BUILD
        if (w_parameter)
            SetLayeredWindowAttributes(window, RGB(0, 0, 0), 255, LWA_ALPHA);
        else
            SetLayeredWindowAttributes(window, RGB(0, 0, 0), 64, LWA_ALPHA);
#endif
        break;
    case WM_DESTROY:
        // TODO(casey): Handle this as an error - recreate window?
        glob_running = false;
        break;
    case WM_SYSKEYDOWN: case WM_SYSKEYUP:
    case WM_KEYDOWN: case WM_KEYUP:
        //ASSERT(!"Keyboart input bleeded to windows message callback!");
        break;
    case WM_PAINT:
    {
        PAINTSTRUCT paint;
        HDC device_context = BeginPaint(window, &paint);
        /*
        LONG x = paint.rcPaint.left;
        LONG y = paint.rcPaint.top;
        LONG width = paint.rcPaint.right - paint.rcPaint.left;
        LONG height = paint.rcPaint.bottom - paint.rcPaint.top;
        */

        Win32WindowDimension dimension = win32_get_window_dimension(window);
        win32_display_buffer_in_window(glob_backbuffer, device_context, dimension.width, dimension.height);
        static DWORD color = WHITENESS;

        EndPaint(window, &paint);
        break;
    }
    default:
        //OutputDebugString("default\n");
        result = DefWindowProcA(window, message, w_parameter, l_parameter);
        break;
    }

    return result;
}

void win32_clear_sound_buffer(Win32SoundOutput& sound_output)
{
    // TODO(casey): More strenuous test!
    // TODO(casey): Switch to sine wave
    LPVOID region1;
    DWORD region1sz;
    LPVOID region2;
    DWORD region2sz;

    if (SUCCEEDED(glob_secondary_buffer->Lock(0, sound_output.secondary_buffer_size,
        &region1, &region1sz,
        &region2, &region2sz, 0))) {
        // TODO(casey): assert that region1sz/region2sz is valid
        // TODO(casey): collapse these two loops
        
        uint8_t* dest_sample = reinterpret_cast<uint8_t*>(region1);
        
        for (DWORD bytes_i = 0; bytes_i < region1sz; ++bytes_i) {
            *dest_sample++ = 0;
        }

        if (region2) {
            dest_sample = reinterpret_cast<uint8_t*>(region2);
            for (DWORD bytes_i = 0; bytes_i < region1sz; ++bytes_i) {
                *dest_sample++ = 0;
            }
        }

        glob_secondary_buffer->Unlock(region1, region1sz, region2, region2sz);
    }
}

void win32_fill_sound_buffer(Win32SoundOutput& sound_output, DWORD byte_to_lock, DWORD bytes_to_write,
    GameSoundOutputBuffer& source_buffer)
{
    // TODO(casey): More strenuous test!
    // TODO(casey): Switch to sine wave
    LPVOID region1;
    DWORD region1sz;
    LPVOID region2;
    DWORD region2sz;

    if (SUCCEEDED(glob_secondary_buffer->Lock(byte_to_lock, bytes_to_write,
        &region1, &region1sz,
        &region2, &region2sz, 0))) {
        // TODO(casey): assert that region1sz/region2sz is valid
        // TODO(casey): collapse these two loops
        DWORD region1_sample_count = region1sz / sound_output.bytes_per_sample;
        int16_t* dest_sample = reinterpret_cast<int16_t*>(region1);
        int16_t* source_sample = source_buffer.samples;
        for (DWORD sample_i = 0; sample_i < region1_sample_count; ++sample_i) {
            *dest_sample++ = *source_sample++;
            *dest_sample++ = *source_sample++;

            ++sound_output.running_sample_index;
        }

        DWORD region2_sample_count = region2sz / sound_output.bytes_per_sample;
        dest_sample = reinterpret_cast<int16_t*>(region2);
        for (DWORD sample_i = 0; sample_i < region2_sample_count; ++sample_i) {
            *dest_sample++ = *source_sample++;
            *dest_sample++ = *source_sample++;

            ++sound_output.running_sample_index;
        }
        glob_secondary_buffer->Unlock(region1, region1sz, region2, region2sz);
    }
}

static void win32_process_keyboard_message(GameButtonState& new_state, bool is_down)
{
    //ASSERT(new_state.ended_down != is_down);
    //char msg[128];
    //sprintf(msg, "%d\n", is_down);
    //OutputDebugString(msg);
    new_state.ended_down = is_down;
    ++new_state.half_transition;
}

static void win32_process_digital_button(DWORD button_state, GameButtonState& old_state,
    DWORD button_bit, GameButtonState& new_state)
{
    new_state.ended_down = (button_state & button_bit) != 0;
    new_state.half_transition = (old_state.ended_down != new_state.ended_down) ? 1 : 0;
}
//
// RECORD INPUT
//
void win32_begin_recording_input(Win32State* win32_state, int input_recording_index) {
    win32_state->input_recording_index = input_recording_index;

    char* filename = "input.hmi";
    win32_state->recording_handle = 
        CreateFile(filename, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

    DWORD bytes_to_write = static_cast<DWORD>(win32_state->total_size);
    ASSERT(win32_state->total_size == bytes_to_write);
    DWORD bytes_written;
    WriteFile(win32_state->recording_handle, win32_state->game_memory_block, bytes_to_write, &bytes_written, 0);
}

void win32_end_recording_input(Win32State* win32_state) {
    CloseHandle(win32_state->recording_handle);
    win32_state->input_recording_index = 0;
}

void win32_begin_input_playback(Win32State* win32_state, int input_playing_index)
{
    win32_state->input_playing_index = input_playing_index;

    char* filename = "input.hmi";
    win32_state->playback_handle = 
        CreateFile(filename, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    DEBUG_print_last_error();

    DWORD bytes_to_read = static_cast<DWORD>(win32_state->total_size);
    ASSERT(win32_state->total_size == bytes_to_read);
    DWORD bytes_read;
    ReadFile(win32_state->playback_handle, win32_state->game_memory_block, bytes_to_read, &bytes_read, 0);

    DEBUG_print_last_error();
}

void win32_end_input_playback(Win32State* win32_state)
{
    CloseHandle(win32_state->playback_handle);
    win32_state->input_playing_index = 0;
}

void win32_record_input(Win32State* win32_state, GameInput* new_game_input)
{
    DWORD bytes_written;
    WriteFile(win32_state->recording_handle, new_game_input, sizeof(*new_game_input), &bytes_written, 0);
}

void win32_playback_input(Win32State* win32_state, GameInput* new_game_input)
{
    DWORD bytes_read = 0;
    if (ReadFile(win32_state->playback_handle, new_game_input, sizeof(*new_game_input), &bytes_read, 0)) {
        if (bytes_read == 0) {
            int playing_index = win32_state->input_playing_index;
            win32_end_input_playback(win32_state);
            win32_begin_input_playback(win32_state, playing_index);
            ReadFile(win32_state->playback_handle, new_game_input, sizeof(*new_game_input), &bytes_read, 0);
        }
    };
    DEBUG_print_last_error();
}

void win32_process_pending_messages(Win32State* win32_state, GameControllerInput* keyboard_controller)
{
    MSG message;
    while (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
        switch (message.message) {
        case WM_QUIT:
            glob_running = false;
            break;
        case WM_SYSKEYDOWN: case WM_SYSKEYUP:
        case WM_KEYDOWN: case WM_KEYUP:
        {
            uint32_t vk_code = static_cast<uint32_t>(message.wParam);
            bool was_down = (message.lParam & (1 << 30)) != 0;
            bool is_down = (message.lParam & (1L << 31L)) == 0;
            if (was_down != is_down) {
                if (vk_code == 'W') {
                    win32_process_keyboard_message(keyboard_controller->move_up, is_down);
                }
                else if (vk_code == 'A') {
                    win32_process_keyboard_message(keyboard_controller->move_left, is_down);
                }
                else if (vk_code == 'S') {
                    win32_process_keyboard_message(keyboard_controller->move_down, is_down);
                }
                else if (vk_code == 'D') {
                    win32_process_keyboard_message(keyboard_controller->move_right, is_down);
                }
                else if (vk_code == 'Q') {
                    win32_process_keyboard_message(keyboard_controller->left_shoulder, is_down);
                }
                else if (vk_code == 'E') {
                    win32_process_keyboard_message(keyboard_controller->right_shoulder, is_down);
                }
                else if (vk_code == VK_UP) {
                    win32_process_keyboard_message(keyboard_controller->action_up, is_down);
                }
                else if (vk_code == VK_DOWN) {
                    ASSERT(was_down != is_down);
                    win32_process_keyboard_message(keyboard_controller->action_down, is_down);
                }
                else if (vk_code == VK_LEFT) {
                    win32_process_keyboard_message(keyboard_controller->action_left, is_down);
                }
                else if (vk_code == VK_RIGHT) {
                    win32_process_keyboard_message(keyboard_controller->action_right, is_down);
                }
                else if (vk_code == VK_SPACE) {
                    OutputDebugString("SPACE\n");
                }
#if HANDMADE_DEVELOPER_BUILD
                else if (vk_code == 'P') {
                    if (is_down)
                        glob_pause = !glob_pause;
                }
                else if (vk_code == 'L') {
                    if (is_down) {
                        if (win32_state->input_recording_index == 0) {
                            win32_begin_recording_input(win32_state, 1);
                        }
                        else {
                            win32_end_recording_input(win32_state);
                            win32_begin_input_playback(win32_state, 1);
                        }
                    }
                }
#endif

                bool altkey_is_down = (message.lParam & (1 << 29)) != 0;
                if ((vk_code == VK_F4) && altkey_is_down)
                    glob_running = false;
            }
            break;
        }
        default:
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
        TranslateMessage(&message);
        DispatchMessage(&message);
    }
}

static float win32_process_stick_value(SHORT value, SHORT deadzone_threshold)
{
    float result = 0;
    if (value < -deadzone_threshold)
        result = static_cast<float>(value + deadzone_threshold) / (32768.0f - static_cast<float>(deadzone_threshold));
    else if (value > deadzone_threshold)
        result = static_cast<float>(value + deadzone_threshold) / (32768.0f - static_cast<float>(deadzone_threshold));
    return result;
}

inline LARGE_INTEGER win32_get_wall_clock()
{
    LARGE_INTEGER result;
    QueryPerformanceCounter(&result);
    return result;
}
inline float win32_get_seconds_elapsed(LARGE_INTEGER start, LARGE_INTEGER end)
{
    float result = static_cast<float>(end.QuadPart - start.QuadPart)
        / static_cast<float>(glob_perf_count_frequency);
    return result;
}

static void win32_DEBUG_draw_vertical(Win32OffscreenBuffer* backbuffer, int x, int top, int bottom, uint32_t color)
{
    if (top <= 0) {
        top = 0;
    }
    if (bottom > backbuffer->height) {
        bottom = backbuffer->height;
    }
    if ((x >= 0) && (x < backbuffer->width)) {
    uint8_t* pixel = static_cast<uint8_t*>(backbuffer->memory)
        + x * backbuffer->bytes_per_pixel
        + top * backbuffer->pitch;
        for (int y = top; y < bottom; ++y) {
            *reinterpret_cast<uint32_t*>(pixel) = color;
            pixel += backbuffer->pitch;
        }
    }
}

static void win32_draw_sound_buffer_marker(Win32OffscreenBuffer* backbuffer, Win32SoundOutput* /*sound_buffer*/,
    float C, int padx, int top, int bottom, DWORD value, uint32_t color)
{
    float xf = C * static_cast<float>(value);
    int x = padx + static_cast<int>(xf);
    win32_DEBUG_draw_vertical(backbuffer, x, top, bottom, color);
}

static void win32_DEBUG_sync_display(Win32OffscreenBuffer* backbuffer, int marker_count,
    Win32DEBUGTimeMarker* markers, int current_marker_index, Win32SoundOutput* sound_output,
    float /*target_seconds_per_frame*/)
{
    int padx = 16;
    int pady = 16;

    int line_height = 64;
    float C = static_cast<float>(backbuffer->width) / static_cast<float>(sound_output->secondary_buffer_size);
    backbuffer->width;
    for (int marker_index = 0; marker_index < marker_count; ++marker_index) {
        Win32DEBUGTimeMarker* this_marker = &markers[marker_index];
        ASSERT(this_marker->output_play_cursor < sound_output->secondary_buffer_size);
        ASSERT(this_marker->output_write_cursor < sound_output->secondary_buffer_size);
        ASSERT(this_marker->output_location < sound_output->secondary_buffer_size);
        ASSERT(this_marker->output_byte_count < sound_output->secondary_buffer_size);
        ASSERT(this_marker->flip_play_cursor < sound_output->secondary_buffer_size);
        ASSERT(this_marker->flip_write_cursor < sound_output->secondary_buffer_size);

        DWORD play_color = 0xFFFFFFFF;
        DWORD write_color = 0xFFFF0000;
        DWORD expected_flip_color = 0xFFFFFF00;
        /*DWORD play_win_color = 0xFFFF00FF;*/

        int top = pady;
        int bottom = pady + line_height;
        int first_top = bottom;
        if (marker_index == current_marker_index) {
            top += pady + line_height;
            bottom += pady + line_height;

            win32_draw_sound_buffer_marker(backbuffer, sound_output, C,
                padx, top, bottom, this_marker->output_play_cursor, play_color);
            win32_draw_sound_buffer_marker(backbuffer, sound_output, C,
                padx, top, bottom, this_marker->output_write_cursor, write_color);

            top += pady + line_height;
            bottom += pady + line_height;

            win32_draw_sound_buffer_marker(backbuffer, sound_output, C,
                padx, top, bottom, this_marker->output_location, play_color);
            win32_draw_sound_buffer_marker(backbuffer, sound_output, C,
                padx, top, bottom, this_marker->output_location + this_marker->output_byte_count, write_color);

            top += pady + line_height;
            bottom += pady + line_height;

            win32_draw_sound_buffer_marker(backbuffer, sound_output, C,
                padx, first_top, bottom, this_marker->expected_flip_play_cursor, expected_flip_color);
        }
        win32_draw_sound_buffer_marker(backbuffer, sound_output, C,
            padx, top, bottom, this_marker->flip_play_cursor, play_color);
        win32_draw_sound_buffer_marker(backbuffer, sound_output, C,
            padx, top, bottom, this_marker->flip_play_cursor, play_color);
        win32_draw_sound_buffer_marker(backbuffer, sound_output, C,
            padx, top, bottom, this_marker->flip_write_cursor, write_color);
    }
}


int CALLBACK WinMain(HINSTANCE instance,
                     HINSTANCE, LPSTR, int)
{
    LARGE_INTEGER perf_count_frequency_result;
    QueryPerformanceFrequency(&perf_count_frequency_result);
    glob_perf_count_frequency = perf_count_frequency_result.QuadPart;

    // NOTE(casey): Set the Windows scheduler granularity to 1ms
    // so that our Sleep() can be more granular.
    UINT desired_scheduler_ms = 1;
    bool sleep_granular = (timeBeginPeriod(desired_scheduler_ms) == TIMERR_NOERROR);

    win32_load_xinput();
    // NOTE(daniel): Resolution
    int w = 1280, h = 720;
    win32_resize_dib_section(glob_backbuffer, w, h);
    WNDCLASSA window_class = {};
    
    // TODO:(casey): Check if HREDRAW/VREDRAW/OWNDC still matter
    window_class.style = CS_HREDRAW|CS_VREDRAW;
    window_class.lpfnWndProc = win32_main_window_proc_cb;    // handle windows messages
    window_class.hInstance = instance;
    window_class.lpszClassName = "HandmadeHeroWindowClass";

    // TODO(casey): How do we reliably query on this on Windows?
    constexpr int monitor_refresh_hz = 60;
    constexpr int game_update_hz = monitor_refresh_hz / 2;
    constexpr float target_seconds_per_frame = 1.0f / static_cast<float>(game_update_hz);
    // TODO(casey): Let's think about running non-frame-quantized for audio latency...
    // TODO(casey): Let's use the write cursor delta from the play cursor to adjust
    // the target audio latency.
    
    if (RegisterClass(&window_class)) {
        HWND window = CreateWindowExA(
            /* WS_EX_TOPMOST | */ WS_EX_LAYERED,
            window_class.lpszClassName,
            "Handmade Hero",
            WS_OVERLAPPEDWINDOW|WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            0,
            0,
            instance,
            nullptr
        );
        if (window) {

            Win32SoundOutput sound_output = {};

            sound_output.sample_per_second = 48000;
            sound_output.bytes_per_sample = sizeof(int16_t) * 2;
            sound_output.secondary_buffer_size = static_cast<DWORD>(sound_output.sample_per_second * sound_output.bytes_per_sample);
            // TODO(casey): Get rid of latency_sample_count
            sound_output.latency_sample_count = 3 * (sound_output.sample_per_second / game_update_hz);      // BUG
            // TODO(casey): Actually comput this variance and se what the lowest value is.
            sound_output.safety_bytes = sound_output.sample_per_second * sound_output.bytes_per_sample / game_update_hz / 3;
            win32_init_dsound(window, sound_output.sample_per_second, sound_output.secondary_buffer_size);
            win32_clear_sound_buffer(sound_output);
            glob_secondary_buffer->Play(0, 0, DSBPLAY_LOOPING);

            Win32State win32_state = {};
            glob_running = true;

#if 0
            while (glob_running) {
                DWORD play_cursor;
                DWORD write_cursor;
                glob_secondary_buffer->GetCurrentPosition(&play_cursor, &write_cursor);

                char text_buffer[256];
                sprintf_s(text_buffer, sizeof(text_buffer), "pc: %u wc: %u\n", play_cursor, write_cursor);
                OutputDebugStringA(text_buffer);
            }
#endif

            // TODO(casey): Pool with bitmap VirtualAlloc
            int16_t* samples = reinterpret_cast<int16_t*>(
                VirtualAlloc(0, sound_output.secondary_buffer_size, MEM_COMMIT, PAGE_READWRITE));

#if HANDMADE_DEVELOPER_BUILD
            LPVOID base_address = reinterpret_cast<LPVOID>(TERABYTES(2ull));
#else
            LPVOID base_address = 0;
#endif
            GameMemory game_memory = {};
            game_memory.permanent_storage_size = MEGABYTES(64);
            game_memory.transient_storage_size = GIGABYTES(1ull);
            
#if HANDMADE_DEVELOPER_BUILD
            game_memory.DEBUG_platform_free_file_memory = &DEBUG_platform_free_file_memory;
            game_memory.DEBUG_platform_read_entire_file = &DEBUG_platform_read_entire_file;
            game_memory.DEBUG_platform_write_entire_file = &DEBUG_platform_write_entire_file;
#endif
            win32_state.total_size = game_memory.permanent_storage_size + game_memory.transient_storage_size;
            win32_state.game_memory_block = VirtualAlloc(base_address, win32_state.total_size,
                MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            game_memory.permanent_storage = win32_state.game_memory_block;
            game_memory.transient_storage = static_cast<uint8_t*>(game_memory.permanent_storage)
                + game_memory.permanent_storage_size;

            if (samples && game_memory.permanent_storage && game_memory.transient_storage) {
                GameInput game_inputs[2] = {};
                GameInput* new_game_input = &game_inputs[0];
                GameInput* old_game_input = &game_inputs[1];
#if HANDMADE_DEVELOPER_BUILD
                int DEBUG_time_markers_index = 0;
                Win32DEBUGTimeMarker DEBUG_time_markers[game_update_hz / 2] = {};
#endif
                
                DWORD audio_latency_bytes = 0;
                float audio_latency_seconds = 0;
                bool sound_is_valid = false;

                LARGE_INTEGER last_counter = win32_get_wall_clock();
                LARGE_INTEGER flip_wall_clock = win32_get_wall_clock();

                uint64_t last_cycle_count = __rdtsc();

                // NOTE(daniel): GENERIC JOYSTICK CONTROLLER
                // NOTE(daniel): eventually replace joystick input with DirectInput
                JOYINFOEX joyinfoex;
                UINT joystick_id = JOYSTICKID1;
                JOYCAPS joy_capabilities;
                /*BOOL dev_attached;*/
                // NOTE(daniel): Number of joystick supported by the system drive,
                // however doesn't indicate the number of joystick attached to the system
                if (joyGetNumDevs()) { }    // TODO(daniel): Success joystrick supported
                else { }                    // TODO(daniel): LOG no joystick driver supported

                if (joyGetPosEx(JOYSTICKID1, &joyinfoex) != JOYERR_UNPLUGGED) {
                    joystick_id = JOYSTICKID1;
                    joyGetDevCaps(joystick_id, &joy_capabilities, sizeof(joy_capabilities));
                }
                else { }    // LOG no joystick available;

                joyinfoex.dwSize = sizeof(joyinfoex);
                joyinfoex.dwFlags = JOY_RETURNBUTTONS | JOY_RETURNX | JOY_RETURNY | JOY_RETURNPOV;

                constexpr char* handmade_hero_dllname = "handmade_hero.dll";
                Win32GameLibrary game_library = { };
                bool game_load_err = win32_load_game_library(handmade_hero_dllname, game_library);

                while (glob_running) {
                    FILETIME newdll_write_time = win32_get_last_write_time(handmade_hero_dllname);
                    if (CompareFileTime(&newdll_write_time, &game_library.dll_last_write_time) != 0) {
                        win32_unload_game_library(&game_library);
                        win32_load_game_library(handmade_hero_dllname, game_library);
                    }

                    // TODO(casey): Zeroing macro
                    // TODO(casey): We can't zero everything because the up/down state will be wrong!!!
                    GameControllerInput* old_keyboard_controller = get_controller(old_game_input, 0);
                    GameControllerInput* new_keyboard_controller = get_controller(new_game_input, 0);
                    *new_keyboard_controller = {};
                    new_keyboard_controller->is_connected = true;
                    for (int button_index = 0; button_index < ARRAY_COUNT(new_keyboard_controller->buttons); ++button_index) {
                        new_keyboard_controller->buttons[button_index].ended_down =
                            old_keyboard_controller->buttons[button_index].ended_down;
                    }

                    win32_process_pending_messages(&win32_state, new_keyboard_controller);
                    if (!glob_pause) {
                        // TODO(casey): Should we poll this more frequently?
                        DWORD max_controller_count = 1 + XUSER_MAX_COUNT;
                        if (max_controller_count > (ARRAY_COUNT(new_game_input->controllers) - 1)) {
                            max_controller_count = ARRAY_COUNT(new_game_input->controllers) - 1;
                        }

                        for (DWORD controller_index = 1; controller_index < max_controller_count; ++controller_index) {
                            /*DWORD our_controller_index = 1 + max_controller_count;*/
                            GameControllerInput* old_xbox_controller = get_controller(old_game_input, controller_index);
                            GameControllerInput* new_xbox_controller = get_controller(new_game_input, controller_index);

                            XINPUT_STATE controller_state;
                            if (XInputGetState(controller_index, &controller_state) == ERROR_SUCCESS) {
                                new_xbox_controller->is_connected = true;
                                new_xbox_controller->is_analog = old_xbox_controller->is_analog;
                                // NOTE(casey): This controller is plugged in
                                // TODO(casey): See if controller_state.dwPacketNumber increments to rapidly
                                XINPUT_GAMEPAD* pad = &controller_state.Gamepad;
                                // TODO(casey): DPad
                                //
                                bool gpad_up = pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
                                bool gpad_down = pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
                                bool gpad_right = pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
                                bool gpad_left = pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;

                                new_xbox_controller->is_analog = true;
                                new_xbox_controller->stick_averagex = win32_process_stick_value(pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                                new_xbox_controller->stick_averagey = win32_process_stick_value(pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

                                if ((new_xbox_controller->stick_averagex != 0.0f) || (new_xbox_controller->stick_averagey != 0.0f)) {
                                    new_xbox_controller->is_analog = true;
                                }
                                if (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP) {
                                    new_xbox_controller->stick_averagey = 1.0f;
                                    new_xbox_controller->is_analog = false;
                                }
                                if (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN) {
                                    new_xbox_controller->stick_averagey = -1.0f;
                                    new_xbox_controller->is_analog = false;
                                }
                                if (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT) {
                                    new_xbox_controller->stick_averagex = -1.0f;
                                    new_xbox_controller->is_analog = false;
                                }
                                if (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) {
                                    new_xbox_controller->stick_averagex = 1.0f;
                                    new_xbox_controller->is_analog = false;
                                }


                                float threshold = 0.5f;
                                win32_process_digital_button((new_xbox_controller->stick_averagex < -threshold) ? 1ul : 0ul, old_xbox_controller->move_down,
                                    1ul, new_xbox_controller->move_down);
                                win32_process_digital_button((new_xbox_controller->stick_averagex < -threshold) ? 1ul : 0ul, old_xbox_controller->move_up,
                                    1ul, new_xbox_controller->move_up);
                                win32_process_digital_button((new_xbox_controller->stick_averagex < -threshold) ? 1ul : 0ul, old_xbox_controller->move_left,
                                    1ul, new_xbox_controller->move_left);
                                win32_process_digital_button((new_xbox_controller->stick_averagex < -threshold) ? 1ul : 0ul, old_xbox_controller->move_right,
                                    1ul, new_xbox_controller->move_right);

                                win32_process_digital_button(pad->wButtons, old_xbox_controller->action_down,
                                    XINPUT_GAMEPAD_A, new_xbox_controller->action_down);
                                win32_process_digital_button(pad->wButtons, old_xbox_controller->action_right,
                                    XINPUT_GAMEPAD_B, new_xbox_controller->action_right);
                                win32_process_digital_button(pad->wButtons, old_xbox_controller->action_left,
                                    XINPUT_GAMEPAD_X, new_xbox_controller->action_left);
                                win32_process_digital_button(pad->wButtons, old_xbox_controller->action_up,
                                    XINPUT_GAMEPAD_Y, new_xbox_controller->action_up);
                                win32_process_digital_button(pad->wButtons, old_xbox_controller->left_shoulder,
                                    XINPUT_GAMEPAD_LEFT_SHOULDER, new_xbox_controller->left_shoulder);
                                win32_process_digital_button(pad->wButtons, old_xbox_controller->right_shoulder,
                                    XINPUT_GAMEPAD_RIGHT_SHOULDER, new_xbox_controller->right_shoulder);

                                win32_process_digital_button(pad->wButtons, old_xbox_controller->back,
                                    XINPUT_GAMEPAD_BACK, new_xbox_controller->back);
                                win32_process_digital_button(pad->wButtons, old_xbox_controller->start,
                                    XINPUT_GAMEPAD_START, new_xbox_controller->start);

                                // bool gpad_start      = pad->wButtons & XINPUT_GAMEPAD_START;
                                // bool gpad_back       = pad->wButtons & XINPUT_GAMEPAD_BACK;
                            }
                            else {
                                // NOTE(casey): The controller is not available 
                                new_xbox_controller->is_connected = false;
                            }
                        }
                        // Test vibration
#if 0
                        XINPUT_VIBRATION vibration;
                        vibration.wLeftMotorSpeed = INT16_MAX;
                        vibration.wRightMotorSpeed = INT16_MAX;
                        XInputSetState(0, &vibration);
#endif
                        // NOTE(daniel): my generic controller input
                        // NOTE(daniel): This is playstation 2 nomenclatures
                        // TODO(daniel): Check this controller array possition
                        GameControllerInput* old_joypad_controller = get_controller(old_game_input, 5);
                        GameControllerInput* new_joypad_controller = get_controller(old_game_input, 5);

                        // NOTE(daniel): Get joypad state
                        if (joyGetPosEx(joystick_id, &joyinfoex) != JOYERR_UNPLUGGED) {
                            new_joypad_controller->is_analog = true;
                            // NOTE(daniel): Using a guess deadzone value to a generic joypad, maybe
                            // this can be an game option to address unpredictable joypad deadzones
                            // NOTE(daniel): joypad analog normalisation to signed short values like the xinput
                            SHORT s_joypad_lanalogx = static_cast<SHORT>(UINT16_MAX / 2 - joyinfoex.dwXpos) * -1 - 1;   // signed values
                            SHORT s_joypad_lanalogy = static_cast<SHORT>(UINT16_MAX / 2 - joyinfoex.dwYpos);
                            new_joypad_controller->stick_averagex = win32_process_stick_value(s_joypad_lanalogx, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE/2);
                            new_joypad_controller->stick_averagey = win32_process_stick_value(s_joypad_lanalogy, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE/2);

                            // Joystick dpad
                            // TODO(daniel): BUG: limited dpad directions
                            // BUG: how do the xinput process digital directional buttons?
                            float threshold = 0.5f;
                            win32_process_digital_button((new_joypad_controller->stick_averagex < -threshold) ? 1ul : 0ul, old_joypad_controller->move_down,
                                1ul, new_joypad_controller->move_down);
                            bool joypad_down = joyinfoex.dwPOV == JOY_POVBACKWARD;
                            bool joypad_up = joyinfoex.dwPOV == JOY_POVFORWARD;
                            bool joypad_left = joyinfoex.dwPOV == JOY_POVLEFT;
                            bool joypad_right = joyinfoex.dwPOV == JOY_POVRIGHT;

                            // Joystick buttons
                            // Triangle
                            win32_process_digital_button(joyinfoex.dwButtons, old_joypad_controller->action_up,
                                JOY_BUTTON1, new_joypad_controller->action_up);
                            // Circle
                            win32_process_digital_button(joyinfoex.dwButtons, old_joypad_controller->action_right,
                                JOY_BUTTON2, new_joypad_controller->action_right);
                            // X
                            win32_process_digital_button(joyinfoex.dwButtons, old_joypad_controller->action_down,
                                JOY_BUTTON3, new_joypad_controller->action_down);
                            // Square
                            win32_process_digital_button(joyinfoex.dwButtons, old_joypad_controller->action_left,
                                JOY_BUTTON4, new_joypad_controller->action_left);
                            // L1
                            win32_process_digital_button(joyinfoex.dwButtons, old_joypad_controller->left_shoulder,
                                JOY_BUTTON5, new_joypad_controller->left_shoulder);
                            // L2
                            win32_process_digital_button(joyinfoex.dwButtons, old_joypad_controller->right_shoulder,
                                JOY_BUTTON6, new_joypad_controller->right_shoulder);
                            
                            // TODO(daniel): Process start and select

                        }

                        GameOffscreenBuffer buffer = {};
                        buffer.memory = glob_backbuffer.memory;
                        buffer.width = glob_backbuffer.width;
                        buffer.height = glob_backbuffer.height;
                        buffer.pitch = glob_backbuffer.pitch;
                        buffer.bytes_per_pixel = glob_backbuffer.bytes_per_pixel;

                        if (win32_state.input_recording_index) {
                            win32_record_input(&win32_state, new_game_input);
                        }
                        if (win32_state.input_playing_index) {
                            win32_playback_input(&win32_state, new_game_input);
                        }

                        game_library.update_and_render(&game_memory, new_game_input, buffer);

                        LARGE_INTEGER audio_wall_clock = win32_get_wall_clock();
                        float from_begin_to_audio_sec = 1000.0f
                            * win32_get_seconds_elapsed(flip_wall_clock, audio_wall_clock);

                        DWORD play_cursor;
                        DWORD write_cursor;
                        if (glob_secondary_buffer->GetCurrentPosition(&play_cursor, &write_cursor) == DS_OK) {
                            /* NOTE(casey):
                             *
                             * Here is how sound output computation works.
                             *
                             * We define a safety value that is the numer of sample we
                             * think our game update loop may vary by (let's say up to
                             * 2ms)
                             *
                             * When we wake up to write audio, we will look and see what
                             * the play cursor position is and we will forecast ahead
                             * where we think the play cursor will be on the next frame
                             * boundary.
                             *
                             * We will then look to see if the write cursor is before
                             * that by at leat our safety value. If it is, the target
                             * fill position is that frame boundary plus one frame. This
                             * gives us perfect audio sync in the case of a card that
                             * has low enough latency.
                             *
                             * If the write cursor is _after_ that safery margin, then
                             * we assume we can never sync the audio perfectly, so we
                             * will write one frame's worth of audio plus the safety
                             * margin's worth of guard samples.
                             * */
#if HANDMADE_DEVELOPER_BUILD

#endif
                            if (!sound_is_valid) {
                                sound_output.running_sample_index = write_cursor / sound_output.bytes_per_sample;
                                sound_is_valid = true;
                            }
                            DWORD byte_to_lock = sound_output.running_sample_index
                                * sound_output.bytes_per_sample
                                % sound_output.secondary_buffer_size;

                            DWORD expected_sound_bytes_per_frame = sound_output.sample_per_second
                                * sound_output.bytes_per_sample
                                / game_update_hz;

                            float sec_left_until_flip = target_seconds_per_frame - from_begin_to_audio_sec;
                            DWORD expected_bytes_until_flip =
                                static_cast<DWORD>(sec_left_until_flip / target_seconds_per_frame)
                                / expected_sound_bytes_per_frame
                                * expected_sound_bytes_per_frame;

                            DWORD expected_frame_boundary_byte = play_cursor + expected_bytes_until_flip;

                            DWORD safe_write_cursor = write_cursor;
                            if (safe_write_cursor < play_cursor) {
                                safe_write_cursor += sound_output.secondary_buffer_size;
                            }
                            ASSERT(safe_write_cursor >= play_cursor);
                            safe_write_cursor += sound_output.safety_bytes;

                            bool audio_card_is_low_latency = safe_write_cursor < expected_frame_boundary_byte;

                            DWORD target_cursor = 0;
                            if (audio_card_is_low_latency) {
                                target_cursor = expected_frame_boundary_byte + expected_sound_bytes_per_frame;
                            }
                            else {
                                target_cursor = (write_cursor + expected_sound_bytes_per_frame + sound_output.safety_bytes);
                            }
                            target_cursor %= sound_output.secondary_buffer_size;;

                            DWORD bytes_to_write = 0;
                            if (byte_to_lock > target_cursor) {
                                bytes_to_write = sound_output.secondary_buffer_size - byte_to_lock;
                                bytes_to_write += target_cursor;
                            }
                            else {
                                bytes_to_write = target_cursor - byte_to_lock;
                            }
                            GameSoundOutputBuffer sound_buffer = {};
                            sound_buffer.samples_per_second = sound_output.sample_per_second;
                            sound_buffer.sample_count = static_cast<DWORD>(bytes_to_write) / sound_output.bytes_per_sample;
                            sound_buffer.samples = samples;
                            game_library.get_sound_samples(&game_memory, sound_buffer);

#if HANDMADE_DEVELOPER_BUILD
                            Win32DEBUGTimeMarker* marker = &DEBUG_time_markers[DEBUG_time_markers_index];
                            marker->output_play_cursor = play_cursor;
                            marker->output_write_cursor = write_cursor;
                            marker->output_location = byte_to_lock;
                            marker->output_byte_count = bytes_to_write;
                            marker->expected_flip_play_cursor = expected_frame_boundary_byte;



                            DWORD unwrapped_write_cursor = write_cursor;
                            if (unwrapped_write_cursor < play_cursor) {
                                unwrapped_write_cursor += sound_output.secondary_buffer_size;
                            }
                            audio_latency_bytes = unwrapped_write_cursor - play_cursor;
                            audio_latency_seconds = static_cast<float>(audio_latency_bytes)
                                / static_cast<float>(sound_output.bytes_per_sample)
                                / static_cast<float>(sound_output.sample_per_second);
#if 0
#pragma warning(disable: 4777)
                            char text_buffer[256];
                            sprintf_s(text_buffer, sizeof(text_buffer), "btl: %u tc: %u btw: %u - pc: %u wc: %u delta: %u (%fs)\n",
                                byte_to_lock, target_cursor, bytes_to_write, play_cursor, write_cursor,
                                audio_latency_bytes, audio_latency_seconds);
#pragma warning(default: 4777)
                            OutputDebugStringA(text_buffer);
#endif
#endif
                            win32_fill_sound_buffer(sound_output, byte_to_lock, bytes_to_write, sound_buffer);
                        }
                        else {
                            sound_is_valid = false;
                        }


                        LARGE_INTEGER work_counter = win32_get_wall_clock();
                        float work_seconds_elapsed = win32_get_seconds_elapsed(last_counter, work_counter);

                        // TODO(casey): NOT TESTED YET! PROBABLY BUGGY!!!
                        float seconds_elapsed_for_frame = work_seconds_elapsed;
                        if (seconds_elapsed_for_frame < target_seconds_per_frame) {
                            if (sleep_granular) {
                                DWORD sleep_ms = static_cast<DWORD>(1000.f * (target_seconds_per_frame - seconds_elapsed_for_frame));
                                if (sleep_ms)
                                    Sleep(sleep_ms);
                            }
                            float test_seconds_elapsed_for_frame = win32_get_seconds_elapsed(last_counter, win32_get_wall_clock());

                            if (test_seconds_elapsed_for_frame < target_seconds_per_frame) {
                                // TODO(casey): LOG MISS SLEEP HERE
                            }
                            //ASSERT(test_seconds_elapsed_for_frame < target_seconds_per_frame);
                            while (seconds_elapsed_for_frame < target_seconds_per_frame) {
                                seconds_elapsed_for_frame = win32_get_seconds_elapsed(last_counter, win32_get_wall_clock());
                            }
                        }
                        else {
                            // TODO(casey): MISSED FRAME RATE!
                            // TODO(casey): Logging
                        }

                        LARGE_INTEGER end_counter = win32_get_wall_clock();
                        double msec_per_framef = 1000.0 * win32_get_seconds_elapsed(last_counter, end_counter);
                        last_counter = end_counter;

                        Win32WindowDimension dimension = win32_get_window_dimension(window);
#if HANDMADE_DEVELOPER_BUILD
                        // NOTE(casey): Note, current is wrong on the zero'th index
                        win32_DEBUG_sync_display(&glob_backbuffer, ARRAY_COUNT(DEBUG_time_markers), DEBUG_time_markers, DEBUG_time_markers_index - 1, &sound_output, target_seconds_per_frame);
#endif
                        HDC device_context = GetDC(window);
                        win32_display_buffer_in_window(glob_backbuffer, device_context, dimension.width, dimension.height);
                        ReleaseDC(window, device_context);

                        flip_wall_clock = win32_get_wall_clock();

#if HANDMADE_DEVELOPER_BUILD
                        // NOTE(casey): This is debug code
                        {
                            DWORD debug_play_cursor;
                            DWORD debug_write_cursor;
                            if (glob_secondary_buffer->GetCurrentPosition(&debug_play_cursor, &debug_write_cursor) == DS_OK) {
                                ASSERT(DEBUG_time_markers_index < ARRAY_COUNT(DEBUG_time_markers));

                                Win32DEBUGTimeMarker* marker = &DEBUG_time_markers[DEBUG_time_markers_index];
                                marker->flip_play_cursor = debug_play_cursor;
                                marker->flip_write_cursor = debug_write_cursor;
                            }
                        }
#endif
                        GameInput* temp = new_game_input;
                        new_game_input = old_game_input;
                        old_game_input = temp;
                        // TODO(casey): Should I clear these here?


                        uint64_t end_cycle_count = __rdtsc();
                        uint64_t cycles_elapsed = end_cycle_count - last_cycle_count;
                        last_cycle_count = end_cycle_count;

                        double fpsf = 0.0f;
                        double mcpff = static_cast<double>(cycles_elapsed) / (1000.0 * 1000.0);        // Mega Cycles Per Frame
#if 0
                        char fps_buffer[256];
                        sprintf_s(fps_buffer, sizeof(fps_buffer), "%.02fmsec/f, %.02ffps, %.02fmcpf - \n", msec_per_framef, fpsf, mcpff);
                        OutputDebugStringA(fps_buffer);
#endif
#if HANDMADE_DEVELOPER_BUILD
                        ++DEBUG_time_markers_index;
                        if (DEBUG_time_markers_index == ARRAY_COUNT(DEBUG_time_markers)) {
                            DEBUG_time_markers_index = 0;
                        }
#endif
                    }
                }
            }
            else {
                return 0;
                // TODO(casey): Logging
            }
        }
        else {
            return 0;
            // TODO(casey): Logging
        }
    }
    else {
        return 0;
        // TODO(casey): Logging
    }
    return 0;
}
