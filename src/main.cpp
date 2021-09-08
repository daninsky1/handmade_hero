#include <iostream>
#include <sstream>
#include <windows.h>
#include "handmade_hero_config.h"

LRESULT CALLBACK win_procedure_cb(HWND   window,
                                  UINT   message,
                                  WPARAM w_parameter,
                                  LPARAM l_parameter)
{
    LRESULT result = 0;
    
    

    switch (message) {
    case WM_SIZE:
        OutputDebugString("WM_SIZE\n");
        break;
    case WM_DESTROY:
        OutputDebugString("WM_DESTROY\n");
        break;
    case WM_CLOSE:
        OutputDebugString("WM_CLOSE\n");
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
        static DWORD color = WHITENESS;
        if (color == WHITENESS)
            color = BLACKNESS;
        else
            color = WHITENESS;
        PatBlt(device_context, x, y, width, height, color);
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
    window_class.lpfnWndProc = win_procedure_cb;    // handle windows messages
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
            MSG message;
            while (true) {
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
