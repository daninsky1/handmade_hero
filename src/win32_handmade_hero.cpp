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
* Just a partial list of stuff!!
*/

#include "handmade_hero_config.h"

#define _USE_MATH_DEFINES
#include <iostream>
#include <string>
#include <sstream>
#include <cstdint>

#include <stdio.h>
#include <malloc.h>

// TODO(casey): Implement sine ourselves
#include <cmath>


#include "handmade_hero.h"
#include "win32_handmade_hero.h"

// TODO:(casey): This is a global for now
static bool glob_running;
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

DEBUGReadFileResult DEBUG_platform_read_entire_file(char* filename)
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

void DEBUG_platform_free_file_memory(void* memory)
{
    if (memory) {
        VirtualFree(memory, 0, MEM_RELEASE);
    }
}

bool DEBUG_platform_write_entire_file(char* filename, uint32_t memory_size, void* memory)
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
    
    // TODO(daniel): Check warning 4191
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
    int byter_per_pixel = 4;
    buffer.bytes_per_pixel = byter_per_pixel;

    buffer.info.bmiHeader.biSize = sizeof(buffer.info.bmiHeader);
    buffer.info.bmiHeader.biWidth = buffer.width;
    buffer.info.bmiHeader.biHeight = -buffer.height;
    buffer.info.bmiHeader.biPlanes = 1;
    buffer.info.bmiHeader.biBitCount = 32;
    buffer.info.bmiHeader.biCompression = BI_RGB;

    size_t bitmap_memory_sz = static_cast<size_t>(buffer.width * buffer.height * byter_per_pixel);
    buffer.memory = VirtualAlloc(0, bitmap_memory_sz, MEM_COMMIT, PAGE_READWRITE);
    buffer.pitch = w * byter_per_pixel;

    // TODO(casey): Probably clear this to black
}

static void win32_display_buffer_in_window(Win32OffscreenBuffer& buffer,
                                           HDC device_context,
                                           int window_width, int window_height)
                                           
{
    // TODO:(casey): Aspect ration correction
    StretchDIBits(device_context,
               // x, y, width, height,
               // x, y, width, 
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
    new_state.ended_down = is_down;
    ++new_state.half_transition;
}

static void win32_process_xinput_digital_button(DWORD xinput_button_state, GameButtonState& old_state,
    DWORD button_bit, GameButtonState& new_state)
{
    new_state.ended_down = (xinput_button_state & button_bit) != 0;
    new_state.half_transition = (old_state.ended_down != new_state.ended_down) ? 1 : 0;
}

void win32_process_pending_messages(GameControllerInput* keyboard_controller)
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

static float win32_process_xinput_stick_value(SHORT value, SHORT deadzone_threshold)
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

static void win32_DEBUG_draw_vertical(Win32OffscreenBuffer* glob_backbuffer, int x, int top, int bottom, uint32_t color)
{
    uint8_t* pixel = static_cast<uint8_t*>(glob_backbuffer->memory)
        + x * glob_backbuffer->bytes_per_pixel
        + top * glob_backbuffer->pitch;
    for (int y = top; y < bottom; ++y) {
        *reinterpret_cast<uint32_t*>(pixel) = color;
         pixel += glob_backbuffer->pitch;
    }
}

static void win32_draw_sound_buffer_marker(Win32OffscreenBuffer* backbuffer, Win32SoundOutput* sound_output,
    float target_seconds_per_frame, float C, int padx, int top, int bottom, DWORD value, uint32_t color)
{
    ASSERT(value < sound_output->secondary_buffer_size);
    float xf = C * static_cast<float>(value);
    int x = padx + static_cast<int>(xf);
    win32_DEBUG_draw_vertical(backbuffer, x, top, bottom, color);
}

static void win32_DEBUG_sync_display(Win32OffscreenBuffer* backbuffer, int marker_count,
    Win32DEBUGTimeMarker* markers, Win32SoundOutput* sound_output, float target_seconds_per_frame)
{
    // TODO(casey): Draw where we're writing out sound
    int padx = 16;
    int pady = 16;

    int top = pady;
    int bottom = backbuffer->height - pady;
    float C = static_cast<float>(backbuffer->width) / static_cast<float>(sound_output->secondary_buffer_size);
    backbuffer->width;
    for (int marker_index = 0; marker_index < marker_count; ++marker_index) {
        Win32DEBUGTimeMarker* this_marker = &markers[marker_index];
        win32_draw_sound_buffer_marker(backbuffer, sound_output, target_seconds_per_frame, C,
            padx, top, bottom, this_marker->play_cursor, 0xFFFFFFFF);
        win32_draw_sound_buffer_marker(backbuffer, sound_output, target_seconds_per_frame, C,
            padx, top, bottom, this_marker->write_cursor, 0xFFFF0000);
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
    constexpr int frames_of_audio_latency = 3;
    constexpr int monitor_refresh_hz = 60;
    constexpr int game_update_hz = monitor_refresh_hz / 2;
    constexpr float target_seconds_per_frame = 1.0f / static_cast<float>(game_update_hz);
    
    if (RegisterClass(&window_class)) {
        HWND window = CreateWindowExA(
            0,
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
            // NOTE(casey): Since we specified CS_OWNDC, we can just
            // get one device context and use it forever because we
            // are not sharing it with anyone
            HDC device_context = GetDC(window);

            Win32SoundOutput sound_output = {};

            sound_output.sample_per_second = 48000;
            sound_output.bytes_per_sample = sizeof(int16_t) * 2;
            sound_output.secondary_buffer_size = static_cast<DWORD>(sound_output.sample_per_second * sound_output.bytes_per_sample);
            sound_output.latency_sample_count = frames_of_audio_latency * (sound_output.sample_per_second / game_update_hz);      // BUG

            win32_init_dsound(window, sound_output.sample_per_second, sound_output.secondary_buffer_size);
            win32_clear_sound_buffer(sound_output);
            glob_secondary_buffer->Play(0, 0, DSBPLAY_LOOPING);

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


#if DEVELOPER_BUILD
            LPVOID base_address = reinterpret_cast<LPVOID>(TERABYTES(2ull));
#else
            LPVOID base_address = 0;
#endif
            GameMemory game_memory = {};
            game_memory.permanent_storage_size = MEGABYTES(64);
            game_memory.transient_storage_size = GIGABYTES(4ull);

            uint64_t total_size = game_memory.permanent_storage_size + game_memory.transient_storage_size;
            game_memory.permanent_storage = VirtualAlloc(base_address, total_size,
                MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            game_memory.transient_storage = static_cast<uint8_t*>(game_memory.permanent_storage)
                + game_memory.permanent_storage_size;

            if (samples && game_memory.permanent_storage && game_memory.transient_storage) {
                GameInput xbox_input[2] = {};
                GameInput* new_xbox_input = &xbox_input[0];
                GameInput* old_xbox_input = &xbox_input[1];

                int DEBUG_time_markers_index = 0;
                Win32DEBUGTimeMarker DEBUG_time_markers[game_update_hz / 2] = {};

                DWORD last_play_cursor = 0;
                bool sound_is_valid = false;

                LARGE_INTEGER last_counter = win32_get_wall_clock();
                
                uint64_t last_cycle_count = __rdtsc();

                // NOTE(daniel): GENERIC JOYSTICK CONTROLLER
                // NOTE(daniel): eventually replace joystick input with DirectInput
                JOYINFOEX joyinfoex;
                UINT joystick_id = JOYSTICKID1;
                JOYCAPS joy_capabilities;
                BOOL dev_attached;
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

                GameInput generic_input[2] = {};
                GameInput* new_generic_input = &generic_input[0];
                GameInput* old_generic_input = &generic_input[1];

                while (glob_running) {
                    // TODO(casey): Zeroing macro
                    // TODO(casey): We can't zero everything because the up/down state will be wrong!!!
                    GameControllerInput* old_keyboard_controller = &new_xbox_input->controllers[0];
                    GameControllerInput* new_keyboard_controller = &new_xbox_input->controllers[0];
                    *new_keyboard_controller = {};
                    new_keyboard_controller->is_connected = true;
                    for (int button_index = 0; button_index < ARRAY_COUNT(new_keyboard_controller->buttons); ++button_index) {
                        new_keyboard_controller->buttons[button_index].ended_down =
                            old_keyboard_controller->buttons[button_index].ended_down;
                    }

                    win32_process_pending_messages(new_keyboard_controller);

                    // TODO(casey): Should we poll this more frequently?
                    DWORD max_controller_count = 1 + XUSER_MAX_COUNT;
                    if (max_controller_count > (ARRAY_COUNT(new_xbox_input->controllers) - 1)) {
                        max_controller_count = ARRAY_COUNT(new_xbox_input->controllers) - 1;
                    }

                    for (DWORD controller_index = 0; controller_index < max_controller_count; ++controller_index) {
                        DWORD our_controller_index = 1 + max_controller_count;
                        GameControllerInput* old_xbox_controller = get_controller(old_xbox_input, controller_index);
                        GameControllerInput* new_xbox_controller = get_controller(new_xbox_input, controller_index);

                        XINPUT_STATE controller_state;
                        if (XInputGetState(controller_index, &controller_state) == ERROR_SUCCESS) {
                            new_xbox_controller->is_connected = true;
                            // NOTE(casey): This controller is plugged in
                            // TODO(casey): See if controller_state.dwPacketNumber increments to rapidly
                            XINPUT_GAMEPAD* pad = &controller_state.Gamepad;
                            // TODO(casey): DPad
#pragma warning(disable:4189)
                            bool gpad_up = pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
                            bool gpad_down = pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
                            bool gpad_right = pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
                            bool gpad_left = pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
#pragma warning(default:4189)
                            new_xbox_controller->is_analog = true;
                            new_xbox_controller->stick_averagex= win32_process_xinput_stick_value(pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                            new_xbox_controller->stick_averagey= win32_process_xinput_stick_value(pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

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
                            win32_process_xinput_digital_button((new_xbox_controller->stick_averagex < -threshold) ? 1 : 0, old_xbox_controller->move_down,
                                1, new_xbox_controller->move_down);
                            win32_process_xinput_digital_button((new_xbox_controller->stick_averagex < -threshold) ? 1 : 0, old_xbox_controller->move_up,
                                1, new_xbox_controller->move_up);
                            win32_process_xinput_digital_button((new_xbox_controller->stick_averagex < -threshold) ? 1 : 0, old_xbox_controller->move_left,
                                1, new_xbox_controller->move_left);
                            win32_process_xinput_digital_button((new_xbox_controller->stick_averagex < -threshold) ? 1 : 0, old_xbox_controller->move_right,
                                1, new_xbox_controller->move_right);

                            win32_process_xinput_digital_button(pad->wButtons, old_xbox_controller->action_down,
                                XINPUT_GAMEPAD_A, new_xbox_controller->action_down);
                            win32_process_xinput_digital_button(pad->wButtons, old_xbox_controller->action_right,
                                XINPUT_GAMEPAD_B, new_xbox_controller->action_right);
                            win32_process_xinput_digital_button(pad->wButtons, old_xbox_controller->action_left,
                                XINPUT_GAMEPAD_X, new_xbox_controller->action_left);
                            win32_process_xinput_digital_button(pad->wButtons, old_xbox_controller->action_up,
                                XINPUT_GAMEPAD_Y, new_xbox_controller->action_up);
                            win32_process_xinput_digital_button(pad->wButtons, old_xbox_controller->left_shoulder,
                                XINPUT_GAMEPAD_LEFT_SHOULDER, new_xbox_controller->left_shoulder);
                            win32_process_xinput_digital_button(pad->wButtons, old_xbox_controller->right_shoulder,
                                XINPUT_GAMEPAD_RIGHT_SHOULDER, new_xbox_controller->right_shoulder);

                            win32_process_xinput_digital_button(pad->wButtons, old_xbox_controller->back,
                                XINPUT_GAMEPAD_BACK, new_xbox_controller->back);
                            win32_process_xinput_digital_button(pad->wButtons, old_xbox_controller->start,
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
                    
                    if (joyGetPosEx(joystick_id, &joyinfoex) != JOYERR_UNPLUGGED) {
                        GameControllerInput* old_generic_controller = &old_generic_input->controllers[0];
                        GameControllerInput* new_generic_controller = &new_generic_input->controllers[0];
                        // TODO(daniel): BUG: limited dpad directions
                        // Joystick dpad
#pragma warning(disable:4189)
                        bool generic_gpad_down = joyinfoex.dwPOV == JOY_POVBACKWARD;
                        bool generic_gpad_up = joyinfoex.dwPOV == JOY_POVFORWARD;
                        bool generic_gpad_left = joyinfoex.dwPOV == JOY_POVLEFT;
                        bool generic_gpad_right = joyinfoex.dwPOV == JOY_POVRIGHT;
                        // Joystick buttons
                        bool generic_gpad_tri = joyinfoex.dwButtons & JOY_BUTTON1;
                        bool generic_gpad_cir = joyinfoex.dwButtons & JOY_BUTTON2;
                        bool generic_gpad_x = joyinfoex.dwButtons & JOY_BUTTON3;
                        bool generic_gpad_sq = joyinfoex.dwButtons & JOY_BUTTON4;
                        bool generic_gpad_l2 = joyinfoex.dwButtons & JOY_BUTTON5;
                        bool generic_gpad_r2 = joyinfoex.dwButtons & JOY_BUTTON6;
                        bool generic_gpad_l1 = joyinfoex.dwButtons & JOY_BUTTON7;
                        bool generic_gpad_r1 = joyinfoex.dwButtons & JOY_BUTTON8;
                        // Joystick left analog x and y
                        uint64_t u_generic_gpad_lanalogx = joyinfoex.dwXpos;    // unsigned values
                        uint64_t u_generic_gpad_lanalogy = joyinfoex.dwYpos;
                        int64_t s_generic_gpad_lanalogx = static_cast<int64_t>(UINT16_MAX / 2 - u_generic_gpad_lanalogx);   // signed values
                        int64_t s_generic_gpad_lanalogy = static_cast<int64_t>(UINT16_MAX / 2 - u_generic_gpad_lanalogy);
#pragma warning(default:4189)
                    }
                    // NOTE(casey): Compute how much sound to write and where
                    DWORD byte_to_lock = 0;
                    DWORD target_cursor = 0;
                    DWORD bytes_to_write = 0;
                    if (sound_is_valid) {
                        byte_to_lock = sound_output.running_sample_index
                            * sound_output.bytes_per_sample
                            % sound_output.secondary_buffer_size;
                        target_cursor = (last_play_cursor
                            + sound_output.latency_sample_count
                            * sound_output.bytes_per_sample)
                            % sound_output.secondary_buffer_size;
                        
                        if (byte_to_lock > target_cursor) {
                            bytes_to_write = sound_output.secondary_buffer_size - byte_to_lock;
                            bytes_to_write += target_cursor;
                        }
                        else {
                            bytes_to_write = target_cursor - byte_to_lock;
                        }
                    }
                    
                    GameSoundOutputBuffer sound_buffer = {};
                    sound_buffer.samples_per_second = sound_output.sample_per_second;
                    sound_buffer.sample_count = static_cast<DWORD>(bytes_to_write) / sound_output.bytes_per_sample;
                    sound_buffer.samples = samples;

                    GameOffscreenBuffer buffer = {};
                    buffer.memory = glob_backbuffer.memory;
                    buffer.width = glob_backbuffer.width;
                    buffer.height = glob_backbuffer.height;
                    buffer.pitch = glob_backbuffer.pitch;

                    game_update_and_render(&game_memory, new_xbox_input, buffer, sound_buffer);


                    if (sound_is_valid) {
#if DEVELOPER_BUILD
                        DWORD play_cursor;
                        DWORD write_cursor;
                        glob_secondary_buffer->GetCurrentPosition(&play_cursor, &write_cursor);
                        char text_buffer[256];
                        sprintf_s(text_buffer, sizeof(text_buffer), "lpc: %u btl: %u tc: %u btw: %u - pc: %u wc: %u\n",
                            last_play_cursor, byte_to_lock, target_cursor, bytes_to_write, play_cursor, write_cursor);
                        OutputDebugStringA(text_buffer);
#endif
                        win32_fill_sound_buffer(sound_output, byte_to_lock, bytes_to_write, sound_buffer);
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
#if DEVELOPER_BUILD
                        win32_DEBUG_sync_display(&glob_backbuffer, ARRAY_COUNT(DEBUG_time_markers), DEBUG_time_markers, &sound_output, target_seconds_per_frame);
#endif
                        win32_display_buffer_in_window(glob_backbuffer, device_context, dimension.width, dimension.height);

                        DWORD play_cursor;
                        DWORD write_cursor;
                        if (glob_secondary_buffer->GetCurrentPosition(&play_cursor, &write_cursor) == DS_OK) {
                            last_play_cursor = play_cursor;
                            if (!sound_is_valid) {
                                sound_output.running_sample_index = write_cursor / sound_output.bytes_per_sample;
                                sound_is_valid = true;
                            }
                        }
                        else {
                            sound_is_valid = false;
                        }

#if DEVELOPER_BUILD
                    // NOTE(casey): This is debug code
                    {
                        ASSERT(DEBUG_time_markers_index < ARRAY_COUNT(DEBUG_time_markers));
                        Win32DEBUGTimeMarker* marker = &DEBUG_time_markers[DEBUG_time_markers_index++];
                        if (DEBUG_time_markers_index == ARRAY_COUNT(DEBUG_time_markers))
                            DEBUG_time_markers_index = 0;
                        marker->play_cursor = play_cursor;
                        marker->write_cursor = write_cursor;
                    }
#endif
                    GameInput* temp = new_xbox_input;
                    new_xbox_input = old_xbox_input;
                    old_xbox_input = temp;
                    // TODO(casey): Should I clear these here?


                    uint64_t end_cycle_count = __rdtsc();
                    uint64_t cycles_elapsed = end_cycle_count - last_cycle_count;
                    last_cycle_count = end_cycle_count;

                    double fpsf = 0.0f;
                    double mcpff = static_cast<double>(cycles_elapsed) / (1000.0 * 1000.0);        // Mega Cycles Per Frame

                    char fps_buffer[256];
                    sprintf_s(fps_buffer, sizeof(fps_buffer), "%.02fmsec/f, %.02ffps, %.02fmcpf - \n", msec_per_framef, fpsf, mcpff);
                     OutputDebugStringA(fps_buffer);
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
