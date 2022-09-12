/*
    Example of how to get raw mouse input data using the native Windows API
    through a "message-only" window that outputs to an allocated console window.

    Not prepared for Unicode.
    I don't recommend copy/pasting this, understand it before integrating it.
*/

// Make Windows.h slightly less of a headache.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>

// Only for redirecting stdout to the console.
#include <cstdio>
#include <iostream>

// Structure to store out input data.
// Not necessary, I just find it neat.
struct
{
    struct
    {
        // Keep in mind these are all deltas,
        // they'll change for one frame/cycle
        // before going back to zero.
        int x = 0;
        int y = 0;
        int wheel = 0;
    } mouse;
} input;

// Window message callback.
LRESULT CALLBACK EventHandler(HWND, unsigned, WPARAM, LPARAM);
// Clear the console window.
void ClearConsole();

// Make sure to set the entrypoint to "mainCRTStartup", or change this to WinMain.
int main()
{
    // Why even bother with WinMain?
    HINSTANCE instance = GetModuleHandle(0);
    // Make std::cout faster:
    std::ios_base::sync_with_stdio(false);

    // Get console window:
    FILE *console_output;
    FILE *console_error;

    if (AllocConsole())
    {
        freopen_s(&console_output, "CONOUT$", "w", stdout);
        freopen_s(&console_error, "CONERR$", "w", stderr);
    }
    else
    {
        return -1;
    }

    // Create message-only window:
    const char *class_name = "SimpleEngine Class";

    // "{}" is necessary, otherwise we have to use ZeroMemory() (which is just memset).
    WNDCLASS window_class = {};
    window_class.lpfnWndProc = EventHandler;
    window_class.hInstance = instance;
    window_class.lpszClassName = class_name;

    if (!RegisterClass(&window_class))
        return -1;

    HWND window = CreateWindow(class_name, "SimpleEngine", 0, 0, 0, 0, 0, HWND_MESSAGE, 0, 0, 0);

    if (window == nullptr)
        return -1;
// End of creating window.

// Registering raw input devices
#ifndef HID_USAGE_PAGE_GENERIC
#define HID_USAGE_PAGE_GENERIC ((unsigned short)0x01)
#endif
#ifndef HID_USAGE_GENERIC_MOUSE
#define HID_USAGE_GENERIC_MOUSE ((unsigned short)0x02)
#endif

    // We're configuring just one RAWINPUTDEVICE, the mouse,
    // so it's a single-element array (a pointer).
    RAWINPUTDEVICE rid[1];
    rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
    rid[0].usUsage = HID_USAGE_GENERIC_MOUSE;
    rid[0].dwFlags = RIDEV_INPUTSINK;
    rid[0].hwndTarget = window;
    RegisterRawInputDevices(rid, 1, sizeof(rid[0]));
    // End of resgistering.

    // Main loop:
    MSG event;
    bool quit = false;

    while (!quit)
    {
        while (PeekMessage(&event, 0, 0, 0, PM_REMOVE))
        {
            if (event.message == WM_QUIT)
            {
                quit = true;
                break;
            }

            // Does some Windows magic and sends the message to EventHandler()
            // because it's associated with the window we created.
            TranslateMessage(&event);
            DispatchMessage(&event);
        }

        // Output mouse input to console:
        std::cout << "Mouse input: (" << input.mouse.x;
        std::cout << ", " << input.mouse.y;
        std::cout << ", " << input.mouse.wheel << ")\n";

        // Sleep a bit so that console output can be read...
        Sleep(100);
        // ...before clearing the console:
        ClearConsole();

        // Reset mouse data in case WM_INPUT isn't called:
        input.mouse.x = 0;
        input.mouse.y = 0;
        input.mouse.wheel = 0;
    }

    fclose(console_output);
    fclose(console_error);
    return 0;
}

LRESULT CALLBACK EventHandler(
    HWND hwnd,
    unsigned event,
    WPARAM wparam,
    LPARAM lparam)
{
    switch (event)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_INPUT:
    {
        // The official Microsoft examples are pretty terrible about this.
        // Size needs to be non-constant because GetRawInputData() can return the
        // size necessary for the RAWINPUT data, which is a weird feature.
        unsigned size = sizeof(RAWINPUT);
        static RAWINPUT raw[sizeof(RAWINPUT)];
        GetRawInputData((HRAWINPUT)lparam, RID_INPUT, raw, &size, sizeof(RAWINPUTHEADER));

        if (raw->header.dwType == RIM_TYPEMOUSE)
        {
            input.mouse.x = raw->data.mouse.lLastX;
            input.mouse.y = raw->data.mouse.lLastY;

            // Wheel data needs to be pointer casted to interpret an unsigned short as a short, with no conversion
            // otherwise it'll overflow when going negative.
            // Didn't happen before some minor changes in the code, doesn't seem to go away
            // so it's going to have to be like this.
            if (raw->data.mouse.usButtonFlags & RI_MOUSE_WHEEL)
                input.mouse.wheel = (*(short *)&raw->data.mouse.usButtonData) / WHEEL_DELTA;
        }
    }
        return 0;
    }

    // Run default message processor for any missed events:
    return DefWindowProc(hwnd, event, wparam, lparam);
}

void ClearConsole()
{
    static const COORD top_left = {0, 0};
    CONSOLE_SCREEN_BUFFER_INFO info;
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(console, &info);
    DWORD written, cells = info.dwSize.X * info.dwSize.Y;
    FillConsoleOutputCharacter(console, ' ', cells, top_left, &written);
    SetConsoleCursorPosition(console, top_left);
}