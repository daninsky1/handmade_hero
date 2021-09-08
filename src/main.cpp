#include <iostream>
#include <sstream>
#include <windows.h>
#include "handmade_hero_config.h"

// TODO(casey): This is a global for now.
static bool running;

static BITMAPINFO bitmap_info;
static void* bitmap_memory;
static HBITMAP bitmap_handle;
static HDC bitmap_device_context;

static void win32_resize_dib_section(int w, int h)
{
    // TODO(casey): Bulletproof this.
    // Maybe don't free first, free after, then free first if that fails.

    // TODO(casey): Free our DIBSection;
    
    if (bitmap_handle) {
        DeleteObject(bitmap_handle);
    }

    if (!bitmap_device_context) {
        // TODO(casey): Should we recreate these under certain special circumstances?
        bitmap_device_context = CreateCompatibleDC(0);
    }

    bitmap_info.bmiHeader.biSize = sizeof(bitmap_info.bmiHeader);
    bitmap_info.bmiHeader.biWidth = w;
    bitmap_info.bmiHeader.biHeight = h;
    bitmap_info.bmiHeader.biPlanes = 1;
    bitmap_info.bmiHeader.biBitCount = 32;
    bitmap_info.bmiHeader.biCompression = BI_RGB;
    
    bitmap_handle = CreateDIBSection(
        bitmap_device_context, &bitmap_info,
        DIB_RGB_COLORS, &bitmap_memory, 0, 0);   
}

static void win32_update_window(HDC device_context, int x, int y,
                                int width, int height)
{
    StretchDIBits(
        device_context,
        x, y, width, height,
        x, y, width, height,
        bitmap_memory,
        &bitmap_info,
        DIB_RGB_COLORS,
        SRCCOPY
    );
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
        RECT client_rect;
        GetClientRect(window, &client_rect);
        int width = client_rect.right - client_rect.left;
        int height = client_rect.bottom - client_rect.top;
        win32_resize_dib_section(width, height);
        OutputDebugString("WM_SIZE\n");
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
        win32_update_window(device_context, x, y, width, height);
        static DWORD color = WHITENESS;

        EndPaint(window, &paint);
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
        HWND window_handle = CreateWindowExA(
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
        if (window_handle) {
            running = true;
            MSG message;
            
            while (running) {
                BOOL message_result = GetMessage(&message, 0, 0, 0);
                if (message_result > 0) {
                    TranslateMessage(&message);
                    DispatchMessage(&message);
                }
                else {
                    break;
                }
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
