#include <iostream>
#include <sstream>
#include <windows.h>
#include "handmade_hero_config.h"

// TODO(casey): This is a global for now.
static bool running;

static BITMAPINFO bitmap_info;
static void* bitmap_memory;
static int bitmap_width;
static int bitmap_height;
static int bytes_per_pixel = 4;

static void render_test_gradient(int xoff, int yoff)
{
    int w = bitmap_width;
    int h = bitmap_height;
    int pitch = w * bytes_per_pixel;
    uint8_t* row = static_cast<uint8_t*>(bitmap_memory);
    uint32_t* pixel = static_cast<uint32_t*>(bitmap_memory);

    for (int y = 0; y < bitmap_height; ++y) {
        for (int x = 0; x < bitmap_width; ++x) {
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

static void win32_resize_dib_section(int w, int h)
{
    // TODO(casey): Bulletproof this.
    // Maybe don't free first, free after, then free first if that fails.

    /*if (bitmap_memory) {
        VirtualFree(bitmap_memory, 0, MEM_RELEASE);
    }*/

    bitmap_width = w;
    bitmap_height = h;

    bitmap_info.bmiHeader.biSize = sizeof(bitmap_info.bmiHeader);
    bitmap_info.bmiHeader.biWidth = bitmap_width;
    bitmap_info.bmiHeader.biHeight = -bitmap_height;
    bitmap_info.bmiHeader.biPlanes = 1;
    bitmap_info.bmiHeader.biBitCount = 32;
    bitmap_info.bmiHeader.biCompression = BI_RGB;

    int bytes_per_pixel = 4;
    int bitmap_memory_sz = (bitmap_width*bitmap_height)*bytes_per_pixel;
    bitmap_memory = VirtualAlloc(0, bitmap_memory_sz, MEM_COMMIT, PAGE_READWRITE);

    // TODO(casey): Probably clear this to black
}

static void win32_update_window(HDC device_context, RECT* client_rect, int x, int y,
                                int width, int height)
{
    int window_width = client_rect->right - client_rect->left;
    int window_height = client_rect->bottom - client_rect->top;
    StretchDIBits(device_context,
        /*
                  x, y, width, height,
                  x, y, width, height
        */
                  0, 0, bitmap_width, bitmap_height,
                  0, 0, window_width, window_height,
                  bitmap_memory,
                  &bitmap_info,
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
        HDC device_context = GetDC(window);
        RECT client_rect;
        GetClientRect(window, &client_rect);
        int width = client_rect.right - client_rect.left;
        int height = client_rect.bottom - client_rect.top;
        win32_resize_dib_section(width, height);
        win32_update_window(device_context, &client_rect, 0, 0, width, height);
        ReleaseDC(window, device_context);
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

        RECT client_rect;
        GetClientRect(window, &client_rect);


        win32_update_window(device_context, &client_rect, x, y, width, height);
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
    WNDCLASS window_class = {};
    
    // TODO:(casey): Check if HREDRAW/VREDRAW/OWNDC still matter
    window_class.style = CS_OWNDC|CS_HREDRAW|CS_VREDRAW;
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
                render_test_gradient(xoff, yoff);

                HDC device_context = GetDC(window);
                RECT client_rect;
                GetClientRect(window, &client_rect);
                int window_width = client_rect.right - client_rect.left;
                int window_height = client_rect.bottom - client_rect.top;
                win32_update_window(device_context, &client_rect, 0, 0, window_width, window_height);
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
