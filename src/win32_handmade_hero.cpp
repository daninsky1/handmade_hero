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


#define _USE_MATH_DEFINES
#include <iostream>
#include <string>
#include <sstream>
#include <cstdint>

#include <stdio.h>
#include <malloc.h>

// TODO(casey): Implement sine ourselves
#include <cmath>

#include "handmade_hero.h"[]
#include "handmade_hero.cpp"

#include <windows.h>
#include <Xinput.h>         // XBox joystick controller
#include <dsound.h>
#include <joystickapi.h>    // Generic joystick controller

#include "handmade_hero_config.h"

struct Win32OffscreenBuffer {
    // NOTE(casey): Pixels are always 32-bits wide, memory order BB GG RR XX
    BITMAPINFO info;
    void* memory;
    int width;
    int height;
    int pitch;
};

struct Win32WindowDimension {
    int width;
    int height;
};

// TODO:(casey): This is a global for now
static bool glob_running;
static Win32OffscreenBuffer glob_backbuffer;
static LPDIRECTSOUNDBUFFER glob_secondary_buffer;

// NOTE(daniel): I got to make globally available for windows procedure get access
// to this
static int xoff = 0;
static int yoff = 0;

// NOTE(casey): XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
static x_input_get_state* XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// NOTE(casey): XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
static x_input_set_state* XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPGUID lpGuid, LPDIRECTSOUND* ppDS, LPUNKNOWN  pUnkOuter);
typedef DIRECT_SOUND_CREATE(direct_sound_create);

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
    
    if (xinput_library) {
         XInputGetState = reinterpret_cast<x_input_get_state*>(GetProcAddress(xinput_library, "XInputGetState"));
         if (!XInputGetState) XInputGetState = XInputGetStateStub;
         XInputSetState = reinterpret_cast<x_input_set_state*>(GetProcAddress(xinput_library, "XInputSetState"));
         if (!XInputSetState) XInputSetState = XInputSetStateStub;
         // TODO(casey): Diagnostic
    }
    else {
        // TODO(casey): Diagnostic
    }
}

static void win32_init_dsound(HWND window, int32_t samples_per_second, int32_t buffer_size)
{
    // NOTE(casey): Load the library
    HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");

    if (DSoundLibrary) {
        // NOTE(casey): Get a DirectSound object! - cooperative
        direct_sound_create* DirectSoundCreate = reinterpret_cast<direct_sound_create*>(GetProcAddress(DSoundLibrary, "DirectSoundCreate"));
        // TODO(casey): Double-check that this works on XP - DirectSound8 or 7??
        LPDIRECTSOUND DirectSound;
        if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0))) {
            WAVEFORMATEX wave_format = {};
            wave_format.wFormatTag = WAVE_FORMAT_PCM;
            wave_format.nChannels = 2;
            wave_format.nSamplesPerSec = samples_per_second;
            wave_format.wBitsPerSample = 16;
            wave_format.nBlockAlign = wave_format.nChannels * wave_format.wBitsPerSample / 8;
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
    buffer.pitch = 4;

    buffer.info.bmiHeader.biSize = sizeof(buffer.info.bmiHeader);
    buffer.info.bmiHeader.biWidth = buffer.width;
    buffer.info.bmiHeader.biHeight = -buffer.height;
    buffer.info.bmiHeader.biPlanes = 1;
    buffer.info.bmiHeader.biBitCount = 32;
    buffer.info.bmiHeader.biCompression = BI_RGB;

    int bitmap_memory_sz = buffer.width * buffer.height * buffer.pitch;
    buffer.memory = VirtualAlloc(0, bitmap_memory_sz, MEM_COMMIT, PAGE_READWRITE);

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
    case WM_KEYDOWN:    case WM_KEYUP:
    {
        uint32_t vk_code = w_parameter;
        bool was_down = (l_parameter & (1 << 30)) != 0;
        bool is_down = (l_parameter & (1L << 31L)) == 0;
        if (was_down != is_down) {
            if (vk_code == 'W') {
                OutputDebugString("W\n");
            }
            else if (vk_code == 'A') {
                OutputDebugString("A\n");
            }
            else if (vk_code == 'S') {
                OutputDebugString("S\n");
            }
            else if (vk_code == 'D') {
                OutputDebugString("D\n");
            }
            else if (vk_code == 'Q') {
                OutputDebugString("Q\n");
            }
            else if (vk_code == 'E') {
                OutputDebugString("E\n");
            }
            else if (vk_code == VK_UP) {
                OutputDebugString("UP\n");
            }
            else if (vk_code == VK_DOWN) {
                OutputDebugString("DOWN\n");
            }
            else if (vk_code == VK_LEFT) {
                OutputDebugString("LEFT\n");
            }
            else if (vk_code == VK_RIGHT) {
                OutputDebugString("RIGHT\n");
            }
            else if (vk_code == VK_ESCAPE) {
                OutputDebugString("ESCAPE: ");
                if (is_down) {
                    OutputDebugString("is_down ");
                }
                if (was_down) {
                    OutputDebugString("was_down");
                }
                OutputDebugString("\n");
            }
            else if (vk_code == VK_SPACE) {
                OutputDebugString("SPACE\n");
            }
            
            bool altkey_is_down = (l_parameter & (1 << 29)) != 0;
            if ((vk_code == VK_F4) && altkey_is_down)
                glob_running = false;
        }
        break;
    }
    case WM_PAINT:
    {
        PAINTSTRUCT paint;
        HDC device_context = BeginPaint(window, &paint);
        LONG x = paint.rcPaint.left;
        LONG y = paint.rcPaint.top;
        LONG width = paint.rcPaint.right - paint.rcPaint.left;
        LONG height = paint.rcPaint.bottom - paint.rcPaint.top;

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

struct Win32SoundOutput {
    int sample_per_second;
    int tone_hz;               // NOTE(daniel): Note A2
    int16_t tone_volume;
    uint32_t running_sample_index;
    int wave_period;
    int bytes_per_sample;
    int secondary_buffer_size;
    float tsine;
    int latency_sample_count;
};

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
            float sine_value = std::sinf(sound_output.tsine);
            int16_t sample_value = static_cast<int16_t>(sine_value * sound_output.tone_volume);
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

int CALLBACK WinMain(HINSTANCE instance,
                     HINSTANCE, LPSTR lpCmdLine,
                     int nCmdShow)
{
    LARGE_INTEGER perf_count_frequency_result;
    QueryPerformanceFrequency(&perf_count_frequency_result);
    int64_t perf_count_frequency = perf_count_frequency_result.QuadPart;

    win32_load_xinput();
    // NOTE(daniel): Resolution
    int w = 1280;
    int h = 720;
    win32_resize_dib_section(glob_backbuffer, w, h);
    WNDCLASSA window_class = {};
    
    // TODO:(casey): Check if HREDRAW/VREDRAW/OWNDC still matter
    window_class.style = CS_HREDRAW|CS_VREDRAW;
    window_class.lpfnWndProc = win32_main_window_proc_cb;    // handle windows messages
    window_class.hInstance = instance;

    // TODO(daniel): Check for hCursor for inkbreaker
    // window_class.hCursor;
    // window_class.lpszMenuName;
    window_class.lpszClassName = "HandmadeHeroWindowClass";
    
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
            sound_output.tone_hz = 110;      // A2
            sound_output.tone_volume = 8000;
            sound_output.wave_period = sound_output.sample_per_second / sound_output.tone_hz;
            sound_output.bytes_per_sample = sizeof(int16_t) * 2;
            sound_output.secondary_buffer_size = sound_output.sample_per_second * sound_output.bytes_per_sample;
            sound_output.latency_sample_count = sound_output.sample_per_second / 16;      // BUG
            
            win32_init_dsound(window, sound_output.sample_per_second, sound_output.secondary_buffer_size);
            win32_clear_sound_buffer(sound_output);
            glob_secondary_buffer->Play(0, 0, DSBPLAY_LOOPING);


            glob_running = true;

            bool sound_is_playing = true;


            // NOTE(daniel): GENERIC JOYSTICK CONTROLLER
            // NOTE(daniel): eventually replace joystick input with DirectInput
            JOYINFOEX joyinfoex;
            UINT joystick_id = JOYSTICKID1;
            JOYCAPS joy_capabilities;
            BOOL dev_attached;
            // NOTE(daniel): Number of joystick supported by the system drive,
            // however doesn't indicate the number of joystick attached to the system
            UINT num_dev = joyGetNumDevs();
            if (num_dev) {
                std::string num_dev_msg;
                num_dev_msg.append(std::to_string(num_dev));
                OutputDebugString("16 Joysticks supported");
            }
            else {
                // TODO(daniel): LOG no joystick driver supported
            }
            
            if (joyGetPosEx(JOYSTICKID1, &joyinfoex) != JOYERR_UNPLUGGED) {
                joystick_id = JOYSTICKID1;
                joyGetDevCaps(joystick_id, &joy_capabilities, sizeof(joy_capabilities));
            }
            else {
                return JOYERR_UNPLUGGED;
            }

            

            LARGE_INTEGER last_count;
            QueryPerformanceCounter(&last_count);

            uint64_t last_cycle_count = __rdtsc();

            while (glob_running) {
                MSG message;
                // NOTE(daniel): Flush queue
                while (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
                    if (message.message == WM_QUIT) glob_running = false;
                    TranslateMessage(&message);
                    DispatchMessage(&message);
                }

                // TODO(casey): Should we poll this more frequently?
                for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i) {
                    XINPUT_STATE controller_state;
                    if (XInputGetState(i, &controller_state) == ERROR_SUCCESS) {
                        // NOTE(casey): This controller is plugged in
                        // TODO(casey): See if controller_state.dwPacketNumber increments to rapidly
                        XINPUT_GAMEPAD* pad = &controller_state.Gamepad;
                        bool gpad_up         = pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
                        bool gpad_down       = pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
                        bool gpad_right      = pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
                        bool gpad_left       = pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
                        bool gpad_start      = pad->wButtons & XINPUT_GAMEPAD_START;
                        bool gpad_back       = pad->wButtons & XINPUT_GAMEPAD_BACK;
                        bool gpad_lthumb     = pad->wButtons & XINPUT_GAMEPAD_LEFT_THUMB;
                        bool gpad_rthumb     = pad->wButtons & XINPUT_GAMEPAD_RIGHT_THUMB;
                        bool gpad_lshoulder  = pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
                        bool gpad_rshoulder  = pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;
                        bool gpad_a          = pad->wButtons & XINPUT_GAMEPAD_A;
                        bool gpad_b          = pad->wButtons & XINPUT_GAMEPAD_B;
                        bool gpad_x          = pad->wButtons & XINPUT_GAMEPAD_X;
                        bool gpad_y          = pad->wButtons & XINPUT_GAMEPAD_Y;
                        // Gamepad left stick x and y
                        int16_t gpad_lstickx = pad->sThumbLX;
                        int16_t gpad_lsticky = pad->sThumbLY;

                        // TODO(casey): We will do deadzone handling later using
                        // XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE
                        // XINPUT_GAMEPAD_RIGTH_THUMB_DEADZONE

                        xoff += gpad_lstickx / 4096;
                        yoff += gpad_lsticky / 4096;
                    }
                    else {
                        // NOTE(casey): The controller is not available 
                    }
                }
                // Test vibration
                XINPUT_VIBRATION vibration;
                vibration.wLeftMotorSpeed = INT16_MAX;
                vibration.wRightMotorSpeed = INT16_MAX;
                XInputSetState(0, &vibration);

                // NOTE(daniel): my generic controller input
                // NOTE(daniel): This is playstation 2 nomenclatures
                joyinfoex.dwSize = sizeof(joyinfoex);
                joyinfoex.dwFlags = JOY_RETURNBUTTONS|JOY_RETURNX|JOY_RETURNY|JOY_RETURNPOV;
                if (joyGetPosEx(joystick_id, &joyinfoex) != JOYERR_UNPLUGGED) {
                    // TODO(daniel): BUG: limited dpad directions
                    // Joystick dpad
                    bool generic_gpad_down  = joyinfoex.dwPOV == JOY_POVBACKWARD;
                    bool generic_gpad_up    = joyinfoex.dwPOV == JOY_POVFORWARD;
                    bool generic_gpad_left  = joyinfoex.dwPOV == JOY_POVLEFT;
                    bool generic_gpad_right = joyinfoex.dwPOV == JOY_POVRIGHT;
                    // Joystick buttons
                    bool generic_gpad_tri   = joyinfoex.dwButtons & JOY_BUTTON1;
                    bool generic_gpad_cir   = joyinfoex.dwButtons & JOY_BUTTON2;
                    bool generic_gpad_x     = joyinfoex.dwButtons & JOY_BUTTON3;
                    bool generic_gpad_sq    = joyinfoex.dwButtons & JOY_BUTTON4;
                    bool generic_gpad_l2    = joyinfoex.dwButtons & JOY_BUTTON5;
                    bool generic_gpad_r2    = joyinfoex.dwButtons & JOY_BUTTON6;
                    bool generic_gpad_l1    = joyinfoex.dwButtons & JOY_BUTTON7;
                    bool generic_gpad_r1    = joyinfoex.dwButtons & JOY_BUTTON8;
                    // Joystick left analog x and y
                    uint16_t u_generic_gpad_lanalogx = joyinfoex.dwXpos;    // unsigned values
                    uint16_t u_generic_gpad_lanalogy = joyinfoex.dwYpos;
                    int16_t s_generic_gpad_lanalogx = UINT16_MAX / 2 - u_generic_gpad_lanalogx;   // signed values
                    int16_t s_generic_gpad_lanalogy = UINT16_MAX / 2 - u_generic_gpad_lanalogy;
                    
                    xoff += s_generic_gpad_lanalogx / 4096;
                    yoff += s_generic_gpad_lanalogy / 4096;

                    // A2 - A3
                    sound_output.tone_hz = 440 + static_cast<int>(110.0f
                        * static_cast<float>(s_generic_gpad_lanalogy)
                        /30000.0f);
                    sound_output.wave_period = sound_output.sample_per_second / sound_output.tone_hz;
                }

                DWORD byte_to_lock;
                DWORD target_cursor;
                DWORD bytes_to_write;

                DWORD play_cursor_pos;
                DWORD write_cursor_pos;
                bool sound_is_valid = false;
                if (SUCCEEDED(glob_secondary_buffer->GetCurrentPosition(&play_cursor_pos, &write_cursor_pos))) {
                    byte_to_lock = sound_output.running_sample_index
                        * sound_output.bytes_per_sample
                        % sound_output.secondary_buffer_size;
                    target_cursor = (play_cursor_pos
                        + sound_output.latency_sample_count
                        * sound_output.bytes_per_sample)
                        % sound_output.secondary_buffer_size;
                    // TODO(casey): Change this to using a lower latency offset from the playcursor
                    // when we actually start having sound effects.
                    if (byte_to_lock > target_cursor) {
                        bytes_to_write = sound_output.secondary_buffer_size - byte_to_lock;
                        bytes_to_write += target_cursor;
                    }
                    else {
                        bytes_to_write = target_cursor - byte_to_lock;
                    }
                    sound_is_valid = true;
                }


                // TODO(casey): Pool with bitmap VirtualAlloc
                int16_t* samples = reinterpret_cast<int16_t*>(
                    VirtualAlloc(0, sound_output.secondary_buffer_size, MEM_COMMIT, PAGE_READWRITE));
                GameSoundOutputBuffer sound_buffer = {};
                sound_buffer.samples_per_second = sound_output.sample_per_second;
                sound_buffer.sample_count = bytes_to_write / sound_output.bytes_per_sample;
                sound_buffer.samples = samples;

                GameOffscreenBuffer buffer = {};
                buffer.memory = glob_backbuffer.memory;
                buffer.width = glob_backbuffer.width;
                buffer.height = glob_backbuffer.height;
                buffer.pitch = glob_backbuffer.pitch;

                game_update_and_render(buffer, xoff, yoff, sound_buffer, sound_output.tone_hz);


                if (sound_is_valid) {
                    // NOTE(casey): DirectSound output test


                    win32_fill_sound_buffer(sound_output, byte_to_lock, bytes_to_write, sound_buffer);
                }

                Win32WindowDimension dimension = win32_get_window_dimension(window);
                win32_display_buffer_in_window(glob_backbuffer, device_context, dimension.width, dimension.height);

                uint64_t end_cycle_count = __rdtsc();

                LARGE_INTEGER end_count;
                QueryPerformanceCounter(&end_count);


                // TODO(casey): Display the value here
                uint64_t cycles_elapsed = end_cycle_count - last_cycle_count;
                int64_t counter_elapsed = end_count.QuadPart - last_count.QuadPart;
                int32_t msec_per_frame = static_cast<int32_t>(1000 * counter_elapsed / perf_count_frequency);    // milissecond per frame
                int32_t fps = static_cast<int32_t>(perf_count_frequency / counter_elapsed);
                int32_t mcpf = static_cast<int32_t>(cycles_elapsed / (1000 * 1000));        // Mega Cycles Per Frame
                double msec_per_framef = 1000.0*static_cast<double>(counter_elapsed) / static_cast<double>(perf_count_frequency);    // milissecond per frame
                double fpsf = static_cast<double>(perf_count_frequency) / static_cast<double>(counter_elapsed);
                double mcpff = static_cast<double>(cycles_elapsed)/(1000.0*1000.0);        // Mega Cycles Per Frame
#if 0
                char buffer[256];
                char bufferf[256];
                wsprintf(buffer, "%dmsec/d, %dfps, %dmcpf - \n", msec_per_frame, fps, mcpf);
                sprintf_s(bufferf, sizeof(bufferf), "%.02fmsec/f, %.02ffps, %.02fmcpf - \n", msec_per_framef, fpsf, mcpff);
                OutputDebugStringA(bufferf);
#endif
                last_count = end_count;
                last_cycle_count = end_cycle_count;

            }
        }
        else {
            // TODO(casey): Logging
        }
    }
    else {
        // TODO(casey): Logging
    }

}
