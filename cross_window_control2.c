/********************************************************************
 *  cross_window_control.c
 *
 *  Single-file cross-platform window & process control in C
 *  Supports: Windows, Linux (X11), macOS
 *
 *  Features:
 *    - Find window by exact or partial title
 *    - Minimize / Maximize / Restore
 *    - Move and resize window
 *    - Get process ID from window
 *    - Kill / Suspend / Resume the process
 *
 *  Compile:
 *    Windows: gcc cross_window_control.c -o winctrl.exe -lgdi32 -luser32
 *    Linux:   gcc cross_window_control.c -o winctrl -lX11
 *    macOS:   gcc cross_window_control.c -o winctrl -framework CoreGraphics -framework ApplicationServices
 *
 *  Author: ChatGPT-4o (2025)
 ********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <tlhelp32.h>
    typedef HWND window_t;
    typedef DWORD procid_t;
#elif __APPLE__
    #include <CoreGraphics/CoreGraphics.h>
    #include <ApplicationServices/ApplicationServices.h>
    #include <unistd.h>
    typedef CGWindowID window_t;
    typedef pid_t procid_t;
#elif __linux__
    #include <X11/Xlib.h>
    #include <X11/Xatom.h>
    #include <X11/Xutil.h>
    #include <unistd.h>
    #include <signal.h>
    typedef Window window_t;
    typedef pid_t procid_t;
#else
    #error "Unsupported platform"
#endif

/* ====================== COMMON API ====================== */

window_t find_window_by_title(const char *partial_title);
bool     window_minimize(window_t win);
bool     window_maximize(window_t win);
bool     window_restore(window_t win);
bool     window_move_resize(window_t win, int x, int y, int w, int h);
procid_t window_get_pid(window_t win);
bool     process_kill(procid_t pid);
bool     process_suspend(procid_t pid);
bool     process_resume(procid_t pid);

/* ====================== IMPLEMENTATIONS ====================== */

#ifdef _WIN32
/* -------------------------- WINDOWS -------------------------- */

window_t find_window_by_title(const char *partial_title)
{
    struct Data {
        const char *title;
        HWND hwnd;
    } data = { partial_title, NULL };

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        char buf[512];
        if (IsWindowVisible(hwnd) && GetWindowTextA(hwnd, buf, sizeof(buf)) > 0) {
            struct Data *d = (struct Data*)lParam;
            if (strstr(buf, d->title)) {
                d->hwnd = hwnd;
                return FALSE; // stop enumeration
            }
        }
        return TRUE;
    }, (LPARAM)&data);

    return data.hwnd;
}

bool window_minimize(window_t win) { return ShowWindow(win, SW_MINIMIZE); }
bool window_maximize(window_t win) { return ShowWindow(win, SW_MAXIMIZE); }
bool window_restore(window_t win)   { return ShowWindow(win, SW_RESTORE); }

bool window_move_resize(window_t win, int x, int y, int w, int h)
{
    return SetWindowPos(win, NULL, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
}

procid_t window_get_pid(window_t win)
{
    DWORD pid;
    GetWindowThreadProcessId(win, &pid);
    return (procid_t)pid;
}

static bool change_process_threads(procid_t pid, bool suspend)
{
    HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (h == INVALID_HANDLE_VALUE) return false;

    THREADENTRY32 te = { sizeof(te) };
    bool ok = false;
    if (Thread32First(h, &te);
    do {
        if (te.th32OwnerProcessID == pid) {
            HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
            if (hThread) {
                if (suspend) SuspendThread(hThread);
                else         ResumeThread(hThread);
                CloseHandle(hThread);
                ok = true;
            }
        }
    } while (Thread32Next(h, &te));
    CloseHandle(h);
    return ok;
}

bool process_kill(procid_t pid)
{
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!h) return false;
    bool ok = TerminateProcess(h, 0);
    CloseHandle(h);
    return ok;
}

bool process_suspend(procid_t pid) { return change_process_threads(pid, true); }
bool process_resume(procid_t pid)  { return change_process_threads(pid, false); }

#elif __APPLE__
/* --------------------------- macOS --------------------------- */

static bool get_window_dict(CGWindowID win_id, CFDictionaryRef *out_dict)
{
    CFArrayRef arr = CGWindowListCopyWindowInfo(kCGWindowListOptionIncludingWindow, win_id);
    if (!arr || CFArrayGetCount(arr) == 0) {
        if (arr) CFRelease(arr);
        return false;
    }
    *out_dict = CFDictionaryRef)CFArrayGetValueAtIndex(arr, 0);
    CFRetain(*out_dict);
    CFRelease(arr);
    return true;
}

window_t find_window_by_title(const char *partial_title)
{
    CFArrayRef windowList = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly |
                                                      kCGWindowListExcludeDesktopElements,
                                                      kCGNullWindowID);
    if (!windowList) return kCGNullWindowID;

    CFIndex count = CFArrayGetCount(windowList);
    for (CFIndex i = 0; i < count; i++) {
        CFDictionaryRef dict = CFDictionaryRef)CFArrayGetValueAtIndex(windowList, i);

        CFStringRef name = CFDictionaryGetValue(dict, kCGWindowName);
        if (name) {
            char buf[512];
            if (CFStringGetCString(name, buf, sizeof(buf), kCFStringEncodingUTF8) &&
                strstr(buf, partial_title)) {
                CFNumberRef num = CFDictionaryGetValue(dict, kCGWindowNumber);
                CGWindowID wid;
                CFNumberGetValue(num, kCFNumberSInt32Type, &wid);
                CFRelease(windowList);
                return wid;
            }
        }
    }
    CFRelease(windowList);
    return kCGNullWindowID;
}

static bool send_apple_script(const char *script)
{
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "osascript -e '%s'", script);
    return system(cmd) == 0;
}

bool window_minimize(window_t win)
{
    // Use Accessibility API (requires permission) or fallback to AppleScript
    // Here we use AppleScript for simplicity
    char script[512];
    snprintf(script, sizeof(script),
             "tell application \"System Events\" to tell process (first process whose frontmost is true) "
             "to set miniaturized of front window to true");
    return send_apple_script(script);
}

bool window_maximize(window_t win)
{
    char script[512];
    snprintf(script, sizeof(script),
             "tell application \"System Events\" to tell process (first process whose frontmost is true) "
             "to set zoomed of front window to true");
    return send_apple_script(script);
}

bool window_restore(window_t win)
{
    char script[512];
    snprintf(script, sizeof(script),
             "tell application \"System Events\" to tell process (first process whose frontmost is true) "
             "to set zoomed of front window to false");
    return send_apple_script(script);
}

bool window_move_resize(window_t win, int x, int y, int w, int h)
{
    // Requires Accessibility permission
    CFDictionaryRef dict;
    if (!get_window_dict(win, &dict)) return false;

    CFNumberRef pid_ref = CFDictionaryGetValue(dict, kCGWindowOwnerPID);
    pid_t pid;
    CFNumberGetValue(pid_ref, kCFNumberSInt32Type, &pid);
    CFRelease(dict);

    AXUIElementRef app = AXUIElementCreateApplication(pid);
    if (!app) return false;

    CFArrayRef windows;
    if (AXUIElementCopyAttributeValue(app, kAXWindowsAttribute, (CFTypeRef*)&windows) != kAXErrorSuccess || !windows) {
        CFRelease(app);
        return false;
    }

    AXUIElementRef window = (AXUIElementRef)CFArrayGetValueAtIndex(windows, 0);
    CFRetain(window);

    CGPoint pos = { (CGFloat)x, (CGFloat)y };
    CGSize  sz  = { (CGFloat)w, (CGFloat)h };

    AXValueRef pos_val = AXValueCreate(kAXValueCGPointType, &pos);
    AXValueRef sz_val  = AXValueCreate(kAXValueCGSizeType, &sz);

    AXUIElementSetAttributeValue(window, kAXPositionAttribute, pos_val);
    AXUIElementSetAttributeValue(window, kAXSizeAttribute, sz_val);

    CFRelease(pos_val);
    CFRelease(sz_val);
    CFRelease(window);
    CFRelease(windows);
    CFRelease(app);
    return true;
}

procid_t window_get_pid(window_t win)
{
    CFDictionaryRef dict;
    if (!get_window_dict(win, &dict)) return -1;
    CFNumberRef num = CFDictionaryGetValue(dict, kCGWindowOwnerPID);
    pid_t pid;
    CFNumberGetValue(num, kCFNumberSInt32Type, &pid);
    CFRelease(dict);
    return pid;
}

bool process_kill(procid_t pid)     { return kill(pid, SIGKILL) == 0; }
bool process_suspend(procid_t pid)  { return kill(pid, SIGSTOP) == 0; }
bool process_resume(procid_t pid)   { return kill(pid, SIGCONT) == 0; }

#elif __linux__
/* -------------------------- LINUX X11 -------------------------- */

static Display *display = NULL;

static void ensure_display(void)
{
    if (!display) display = XOpenDisplay(NULL);
}

window_t find_window_by_title(const char *partial_title)
{
    ensure_display();
    if (!display) return 0;

    Window root = DefaultRootWindow(display);
    Window parent, *children = NULL;
    unsigned int nchildren;

    // Recursive search helper
    Window search(Window w)
    {
        // Check current window title
        XTextProperty tp;
        if (XGetWMName(display, w, &tp)) {
            if (strstr((char*)tp.value, partial_title)) {
                XFree(tp.value);
                return w;
            }
            XFree(tp.value);
        }

        // Try UTF8 title
        Atom net_wm_name = XInternAtom(display, "_NET_WM_NAME", True);
        if (net_wm_name) {
            Atom actual_type;
            int actual_format;
            unsigned long nitems, bytes;
            unsigned char *prop = NULL;
            if (XGetWindowProperty(display, w, net_wm_name, 0, 1024, False,
                                   AnyPropertyType, &actual_type, &actual_format,
                                   &nitems, &bytes, &prop) == Success && prop) {
                if (strstr((char*)prop, partial_title)) {
                    XFree(prop);
                    return w;
                }
                XFree(prop);
            }
        }

        // Recurse into children
        if (XQueryTree(display, w, &root, &parent, &children, &nchildren)) {
            for (unsigned i = 0; i < nchildren; i++) {
                Window found = search(children[i]);
                if (found) {
                    XFree(children);
                    return found;
                }
            }
            if (children) XFree(children);
        }
        return 0;
    }

    return search(root);
}

bool window_minimize(window_t win)
{
    ensure_display();
    return XIconifyWindow(display, win, DefaultScreen(display));
}

bool window_maximize(window_t win)
{
    ensure_display();
    XEvent ev = {0};
    ev.type = ClientMessage;
    ev.xclient.window = win;
    ev.xclient.message_type = XInternAtom(display, "_NET_WM_STATE", False);
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = 1; // _NET_WM_STATE_ADD
    ev.xclient.data.l[1] = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    ev.xclient.data.l[2] = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", False);

    return XSendEvent(display, DefaultRootWindow(display), False,
                      SubstructureRedirectMask | SubstructureNotifyMask, &ev);
}

bool window_restore(window_t win)
{
    ensure_display();
    XEvent ev = {0};
    ev.type = ClientMessage;
    ev.xclient.window = win;
    ev.xclient.message_type = XInternAtom(display, "_NET_WM_STATE", False);
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = 0; // _NET_WM_STATE_REMOVE
    ev.xclient.data.l[1] = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    ev.xclient.data.l[2] = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", False);

    return XSendEvent(display, DefaultRootWindow(display), False,
                      SubstructureRedirectMask | SubstructureNotifyMask, &ev);
}

bool window_move_resize(window_t win, int x, int y, int w, int h)
{
    ensure_display();
    XMoveResizeWindow(display, win, x, y, (unsigned)w, (unsigned)h);
    return true;
}

procid_t window_get_pid(window_t win)
{
    ensure_display();
    Atom pid_atom = XInternAtom(display, "_NET_WM_PID", True);
    if (!pid_atom) return -1;

    Atom type;
    int format;
    unsigned long n, bytes;
    unsigned char *prop = NULL;

    if (XGetWindowProperty(display, win, pid_atom, 0, 1, False, XA_CARDINAL,
                           &type, &format, &n, &bytes, &prop) == Success && prop) {
        procid_t pid = *((unsigned long*)prop);
        XFree(prop);
        return pid;
    }
    return -1;
}

bool process_kill(procid_t pid)     { return kill(pid, SIGKILL) == 0; }
bool process_suspend(procid_t pid)  { return kill(pid, SIGSTOP) == 0; }
bool process_resume(procid_t pid)   { return kill(pid, SIGCONT) == 0; }

#endif

/* ====================== EXAMPLE USAGE ====================== */

int main2(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: %s \"partial window title\" [command]\n", argv[0]);
        printf("Commands: minimize, maximize, restore, move x y w h, kill, suspend, resume\n");
        return 1;
    }

    const char *title = argv[1];
    window_t win = find_window_by_title(title);

    if (!win || win == (window_t)-1) {
        printf("Window containing '%s' not found.\n", title);
        return 1;
    }

    printf("Found window! Handle/ID = %p\n", (void*)win);

    if (argc == 2) {
        // Just show info
        procid_t pid = window_get_pid(win);
        printf("Process ID = %u\n", (unsigned)pid);
        return 0;
    }

    const char *cmd = argv[2];

    if (strcmp(cmd, "minimize") == 0)      window_minimize(win);
    else if (strcmp(cmd, "maximize") == 0) window_maximize(win);
    else if (strcmp(cmd, "restore") == 0)  window_restore(win);
    else if (strcmp(cmd, "kill") == 0) {
        procid_t pid = window_get_pid(win);
        process_kill(pid);
    }
    else if (strcmp(cmd, "suspend") == 0) {
        procid_t pid = window_get_pid(win);
        process_suspend(pid);
    }
    else if (strcmp(cmd, "resume") == 0) {
        procid_t pid = window_get_pid(win);
        process_resume(pid);
    }
    else if (strcmp(cmd, "move") == 0 && argc == 7) {
        int x = atoi(argv[3]), y = atoi(argv[4]);
        int w = atoi(argv[5]), h = atoi(argv[6]);
        window_move_resize(win, x, y, w, h);
    }
    else {
        printf("Unknown command or wrong arguments.\n");
        return 1;
    }

    printf("Command executed.\n");
    return 0;
}
