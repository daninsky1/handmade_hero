#include <iostream>
#include <sstream>
#include <windows.h>
#include "handmade_hero_config.h"

// TODO(casey): This is a global for now.

struct Win32OffscreenBuffer {
    BITMAPINFO info;
    void* memory;
    int width;
    int height;
    int bytes_per_pixel;
};

// TODO:(casey): This is a global for now
static bool running;
static Win32OffscreenBuffer global_backbuffer;

struct Win32WindowDimension {
    int width;
    int height;
};

Win32WindowDimension win32_get_window_dimension(HWND window)
{
    Win32WindowDimension result;
    RECT client_rect;
    GetClientRect(window, &client_rect);
    result.width = client_rect.right - client_rect.left;
    result.height = client_rect.bottom - client_rect.top;

    return result;
}

static void render_test_gradient(Win32OffscreenBuffer buffer, int xoff, int yoff)
{
    int w = buffer.width;
    int h = buffer.height;
    int pitch = w * buffer.bytes_per_pixel;
    uint8_t* row = static_cast<uint8_t*>(buffer.memory);
    uint32_t* pixel = static_cast<uint32_t*>(buffer.memory);

    for (int y = 0; y < buffer.height; ++y) {
        for (int x = 0; x < buffer.width; ++x) {
            // pixel int memory: BB GG RR XX
            // LITTLE ENDIAN ARCHITECTURE
            // Windows invert pixel on registers
            uint8_t blue= x + xoff;
            uint8_t green = y + yoff;
            
            *pixel++ = (green << 8) | blue;
        }
        row += pitch;
    }
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
    buffer.bytes_per_pixel = 4;

    buffer.info.bmiHeader.biSize = sizeof(buffer.info.bmiHeader);
    buffer.info.bmiHeader.biWidth = buffer.width;
    buffer.info.bmiHeader.biHeight = -buffer.height;
    buffer.info.bmiHeader.biPlanes = 1;
    buffer.info.bmiHeader.biBitCount = 32;
    buffer.info.bmiHeader.biCompression = BI_RGB;

    int bitmap_memory_sz = buffer.width * buffer.height * buffer.bytes_per_pixel;
    buffer.memory = VirtualAlloc(0, bitmap_memory_sz, MEM_COMMIT, PAGE_READWRITE);

    // TODO(casey): Probably clear this to black
}

static void win32_display_buffer_in_window(HDC device_context,
                                           int window_width, int window_height,
                                           Win32OffscreenBuffer buffer,
                                           int x, int y,
                                           int width, int height)
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
        running = false;
        OutputDebugString("WM_CLOSE\n");
        break;
    case WM_DESTROY:
        // TODO(casey): Handle this as an error - recreate window?
        running = false;
        break;
    case WM_ACTIVATEAPP:
        OutputDebugString("WM_ACTIVATEAPP\n");
        break;
    case WM_PAINT:
    {
        PAINTSTRUCT paint;
        HDC device_context = BeginPaint(window, &paint);
        LONG x = paint.rcPaint.left;
        LONG y = paint.rcPaint.top;
        LONG width = paint.rcPaint.right - paint.rcPaint.left;
        LONG height = paint.rcPaint.bottom - paint.rcPaint.top;

        Win32WindowDimension dimension = win32_get_window_dimension(window);
        win32_display_buffer_in_window(device_context, dimension.width, dimension.height, global_backbuffer, x, y, width, height);
        static DWORD color = WHITENESS;

        EndPaint(window, &paint);
        break;
    }
    default:
        OutputDebugString("default\n");
        result = DefWindowProc(window, message, w_parameter, l_parameter);
        break;
    }

    return result;
}

int CALLBACK WinMain(HINSTANCE instance,
                     HINSTANCE, LPSTR lpCmdLine,
                     int nCmdShow)
{
    // NOTE(daniel): Resolution
    int w = 1280;
    int h = 720;
    win32_resize_dib_section(global_backbuffer, w, h);
    WNDCLASS window_class = {};
    
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
            running = true;
            MSG message;
            
            int xoff = 0;
            int yoff = 0;
            while (running) {
                // NOTE(daniel): Flush queue
                while (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
                    if (message.message == WM_QUIT) running = false;
                    TranslateMessage(&message);
                    DispatchMessage(&message);
                }
                render_test_gradient(global_backbuffer, xoff, yoff);

                HDC device_context = GetDC(window);
                Win32WindowDimension dimension = win32_get_window_dimension(window);
                win32_display_buffer_in_window(device_context, dimension.width, dimension.height, global_backbuffer, 0, 0, dimension.width, dimension.height);
                ReleaseDC(window, device_context);

                ++xoff;
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
