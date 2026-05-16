// // 1. Windows (Win32 API)
// // Use EnumDisplayMonitors for multi-monitor support or simpler functions for the primary display.
// #include <windows.h>
// #include <stdio.h>
// BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
//     MONITORINFO mi = { sizeof(mi) };
//     GetMonitorInfo(hMonitor, &mi);
//     
//     int width = mi.rcMonitor.right - mi.rcMonitor.left;
//     int height = mi.rcMonitor.bottom - mi.rcMonitor.top;
//     
//     printf("Monitor: %dx%d at (%d,%d)\n", 
//            width, height, 
//            mi.rcMonitor.left, mi.rcMonitor.top);
//     
//     return TRUE;
// }
// int main() {
//     // Enumerate all monitors
//     EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, 0);
//     
//     // Primary display only (simple)
//     int primary_w = GetSystemMetrics(SM_CXSCREEN);
//     int primary_h = GetSystemMetrics(SM_CYSCREEN);
//     printf("Primary: %dx%d\n", primary_w, primary_h);
//     
//     return 0;
// }
// // Compile: cl program.c or with MinGW: gcc program.c -lgdi32.

// // 2. Linux (X11)
// // Using Xlib (most common for basic needs):
// #include <X11/Xlib.h>
// #include <stdio.h>
// 
// int main() {
//     Display *dpy = XOpenDisplay(NULL);
//     if (!dpy) {
//         fprintf(stderr, "Cannot open display\n");
//         return 1;
//     }
//     
//     int screen = DefaultScreen(dpy);
//     int width = DisplayWidth(dpy, screen);
//     int height = DisplayHeight(dpy, screen);
//     
//     printf("Screen: %dx%d\n", width, height);
//     
//     XCloseDisplay(dpy);
//     return 0;
// }
// Compile: gcc program.c -lX11

// // For multi-monitor details, use xrandr via popen() or the XRandR extension.
// // 3. Cross-platform (Recommended)
// // Use a library like SDL2 or GLFW — much easier and portable.
// // SDL2 example
// #include <SDL2/SDL.h>
// #include <stdio.h>
// int main() {
//     SDL_Init(SDL_INIT_VIDEO);
//     
//     int num_displays = SDL_GetNumVideoDisplays();
//     printf("Number of displays: %d\n", num_displays);
//     
//     for (int i = 0; i < num_displays; ++i) {
//         SDL_DisplayMode mode;
//         SDL_GetCurrentDisplayMode(i, &mode);
//         printf("Display %d: %dx%d @ %dHz\n", i, mode.w, mode.h, mode.refresh_rate);
//     }
//     
//     SDL_Quit();
//     return 0;
// }
// // Compile: gcc program.c -lSDL2

// GLFW example
#include <GLFW/glfw3.h>
#include <stdio.h>
int Do_Glfw() {
    glfwInit();
    int count;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    printf("Number of monitors: %d\n", count);
    for (int i = 0; i < count; ++i) {
        const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
        printf("Monitor %d: %dx%d\n", i, mode->width, mode->height);
    }
    glfwTerminate();
    return 0;
}
