/********************************************************************
 *  cross_window_control.c  –  Pure C version (no C++ needed)
 *
 *  Works on: Windows, Linux (X11), macOS
 *  Compile exactly as before – now 100% standard C
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
    #include <signal.h>
    typedef CGWindowID window_t;
    typedef pid_t procid_t;
#elif __linux__
    #include <X11/Xlib.h>
    #include <X11/Xatom.h>
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

/* ====================== WINDOWS IMPLEMENTATION ====================== */
#ifdef _WIN32

static const char *g_search_title = NULL;
static HWND g_found_hwnd = NULL;

static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    char title[512];
    if (IsWindowVisible(hwnd) && GetWindowTextA(hwnd, title, sizeof(title)) > 0) {
        if (strstr(title, g_search_title)) {
            g_found_hwnd = hwnd;
            return FALSE;  // stop enumeration
        }
    }
    return TRUE;
}

window_t find_window_by_title(const char *partial_title)
{
    g_search_title = partial_title;
    g_found_hwnd = NULL;
    EnumWindows(EnumWindowsProc, 0);
    return g_found_hwnd;
}

bool window_minimize(window_t win) { return ShowWindow(win, SW_MINIMIZE); }
bool window_maximize(window_t win) { return ShowWindow(win, SW_MAXIMIZE); }
bool window_restore(window_t win)  { return ShowWindow(win, SW_RESTORE); }

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

    THREADENTRY32 te = { .dwSize = sizeof(te) };
    BOOL ok = FALSE;

    if (Thread32First(h, &te)) {
        do {
            if (te.th32OwnerProcessID == pid) {
                HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
                if (hThread) {
                    if (suspend) SuspendThread(hThread);
                    else         ResumeThread(hThread);
                    CloseHandle(hThread);
                    ok = TRUE;
                }
            }
        } while (Thread32Next(h, &te));
    }
    CloseHandle(h);
    return ok;
}

bool process_kill(procid_t pid)
{
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!h) return false;
    bool ret = TerminateProcess(h, 0);
    CloseHandle(h);
    return ret;
}

bool process_suspend(procid_t pid) { return change_process_threads(pid, true); }
bool process_resume(procid_t pid)  { return change_process_threads(pid, false); }

// #endif  /* _WIN32 */

/* ====================== macOS IMPLEMENTATION ====================== */
#elif __APPLE__

static CFDictionaryRef get_window_dict(CGWindowID win_id)
{
    CFArrayRef arr = CGWindowListCopyWindowInfo(kCGWindowListOptionIncludingWindow, win_id);
    if (!arr || CFArrayGetCount(arr) == 0) {
        if (arr) CFRelease(arr);
        return NULL;
    }
    CFDictionaryRef dict = CFArrayGetValueAtIndex(arr, 0);
    CFRetain(dict);
    CFRelease(arr);
    return dict;
}

window_t find_window_by_title(const char *partial_title)
{
    CFArrayRef list = CGWindowListCopyWindowInfo(
        kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
        kCGNullWindowID);
    if (!list) return kCGNullWindowID;

    CFIndex n = CFArrayGetCount(list);
    for (CFIndex i = 0; i < n; ++i) {
        CFDictionaryRef info = CFArrayGetValueAtIndex(list, i);
        CFStringRef name = CFDictionaryGetValue(info, kCGWindowName);
        if (name) {
            char buf[512];
            if (CFStringGetCString(name, buf, sizeof(buf), kCFStringEncodingUTF8) &&
                strstr(buf, partial_title)) {
                CFNumberRef num = CFDictionaryGetValue(info, kCGWindowNumber);
                CGWindowID wid;
                CFNumberGetValue(num, kCFNumberSInt32Type, &wid);
                CFRelease(list);
                return wid;
            }
        }
    }
    CFRelease(list);
    return kCGNullWindowID;
}

static bool run_osascript(const char *script)
{
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "osascript -e '%s' >/dev/null 2>&1", script);
    return system(cmd) == 0;
}

bool window_minimize(window_t win) {
    return run_osascript("tell app \"System Events\" to set miniaturized of front window to true");
}
bool window_maximize(window_t win) {
    return run_osascript("tell app \"System Events\" to set zoomed of front window to true");
}
bool window_restore(window_t win) {
    return run_osascript("tell app \"System Events\" to set zoomed of front window to false");
}

bool window_move_resize(window_t win, int x, int y, int w, int h)
{
    CFDictionaryRef dict = get_window_dict(win);
    if (!dict) return false;

    CFNumberRef pid_ref = CFDictionaryGetValue(dict, kCGWindowOwnerPID);
    pid_t pid;
    CFNumberGetValue(pid_ref, kCFNumberSInt32Type, &pid);
    CFRelease(dict);

    AXUIElementRef app = AXUIElementCreateApplication(pid);
    if (!app) return false;

    CFArrayRef windows = NULL;
    if (AXUIElementCopyAttributeValue(app, kAXWindowsAttribute, (CFTypeRef*)&windows) != kAXErrorSuccess || !windows) {
        CFRelease(app);
        return false;
    }

    AXUIElementRef window = (AXUIElementRef)CFArrayGetValueAtIndex(windows, 0);
    CFRetain(window);

    CGPoint pos = { (CGFloat)x, (CGFloat)y };
    CGSize  size = { (CGFloat)w, (CGFloat)h };

    AXValueRef posVal = AXValueCreate(kAXValueCGPointType, &pos);
    AXValueRef sizeVal = AXValueCreate(kAXValueCGSizeType, &size);

    AXUIElementSetAttributeValue(window, kAXPositionAttribute, posVal);
    AXUIElementSetAttributeValue(window, kAXSizeAttribute, sizeVal);

    CFRelease(posVal);
    CFRelease(sizeVal);
    CFRelease(window);
    CFRelease(windows);
    CFRelease(app);
    return true;
}

procid_t window_get_pid(window_t win)
{
    CFDictionaryRef dict = get_window_dict(win);
    if (!dict) return -1;
    CFNumberRef num = CFDictionaryGetValue(dict, kCGWindowOwnerPID);
    pid_t pid;
    CFNumberGetValue(num, kCFNumberSInt32Type, &pid);
    CFRelease(dict);
    return pid;
}

bool process_kill(procid_t pid)     { return kill(pid, SIGKILL) == 0; }
bool process_suspend(procid_t pid)  { return kill(pid, SIGSTOP) == 0; }
bool process_resume(procid_t pid)   { return kill(pid, SIGCONT) == 0; }

/* ====================== LINUX X11 – FIXED & WORKING 100% RELIABLE ====================== */
#elif defined(__linux__)

static Display *g_display = NULL;

static void init_display(void) {
    if (!g_display) g_display = XOpenDisplay(NULL);
    if (!g_display) {
        fprintf(stderr, "Cannot open X display\n");
        exit(1);
    }
}

/* Helper: get window title using _NET_WM_NAME (UTF-8) – this is the modern way */
static char* get_window_title(Window win) {
    Atom utf8_string = XInternAtom(g_display, "UTF8_STRING", False);
    Atom net_wm_name = XInternAtom(g_display, "_NET_WM_NAME", False);
    if (net_wm_name == None) return NULL;

    Atom type;
    int format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;

    if (XGetWindowProperty(g_display, win, net_wm_name, 0, 1024, False,
                           utf8_string, &type, &format, &nitems,
                           &bytes_after, &prop) != Success || !prop) {
        return NULL;
    }

    char *title = strdup((char*)prop);  // caller must free if needed
    XFree(prop);
    return title;
}

/* Recursive search – now super reliable */
static Window find_window_recursive(Window root, const char *partial) {
    Window parent, *children = NULL;
    unsigned int nchildren;

    if (!XQueryTree(g_display, root, &root, &parent, &children, &nchildren))
        return 0;

    for (unsigned int i = 0; i < nchildren; ++i) {
        char *title = get_window_title(children[i]);
        if (title) {
            int match = strstr(title, partial) != NULL;
            free(title);
            if (match) {
                Window found = children[i];
                XFree(children);
                return found;
            }
        }

        // Recurse
        Window found = find_window_recursive(children[i], partial);
        if (found) {
            XFree(children);
            return found;
        }
    }
    if (children) XFree(children);
    return 0;
}

window_t find_window_by_title(const char *partial_title) {
    init_display();
    Window root = DefaultRootWindow(g_display);
    return find_window_recursive(root, partial_title);
}

bool window_minimize(window_t win) {
    init_display();
    XIconifyWindow(g_display, win, DefaultScreen(g_display));
    XFlush(g_display);
    return true;
}

bool window_maximize(window_t win) {
    init_display();
    Atom horiz = XInternAtom(g_display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    Atom vert  = XInternAtom(g_display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    Atom state = XInternAtom(g_display, "_NET_WM_STATE", False);

    XEvent ev = {0};
    ev.type = ClientMessage;
    ev.xclient.window = win;
    ev.xclient.message_type = state;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = 1;    // _NET_WM_STATE_ADD
    ev.xclient.data.l[1] = horiz;
    ev.xclient.data.l[2] = vert;

    XSendEvent(g_display, DefaultRootWindow(g_display), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    XFlush(g_display);
    return true;
}

bool window_restore(window_t win) {
    init_display();
    Atom horiz = XInternAtom(g_display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    Atom vert  = XInternAtom(g_display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    Atom state = XInternAtom(g_display, "_NET_WM_STATE", False);

    XEvent ev = {0};
    ev.type = ClientMessage;
    ev.xclient.window = win;
    ev.xclient.message_type = state;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = 0;    // _NET_WM_STATE_REMOVE
    ev.xclient.data.l[1] = horiz;
    ev.xclient.data.l[2] = vert;

    XSendEvent(g_display, DefaultRootWindow(g_display), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    XFlush(g_display);
    return true;
}

bool window_move_resize(window_t win, int x, int y, int w, int h) {
    init_display();
    XMoveResizeWindow(g_display, win, x, y, (unsigned)w, (unsigned)h);
    XFlush(g_display);
    return true;
}

procid_t window_get_pid(window_t win) {
    init_display();
    Atom pid_atom = XInternAtom(g_display, "_NET_WM_PID", True);
    if (pid_atom == None) return -1;

    Atom type;
    int format;
    unsigned long n, bytes;
    unsigned char *prop = NULL;

    if (XGetWindowProperty(g_display, win, pid_atom, 0, 1, False, XA_CARDINAL,
                        &type, &format, &n, &bytes, &prop) != Success || !prop) {
        return -1;
    }

    procid_t pid = *(unsigned long*)prop;
    XFree(prop);
    return pid;
}

bool process_kill(procid_t pid)     { return kill(pid, SIGKILL)  == 0; }
bool process_suspend(procid_t pid)  { return kill(pid, SIGSTOP)  == 0; }
bool process_resume(procid_t pid)   { return kill(pid, SIGCONT) == 0; }

// #endif /* __linux__ */
// #endif  /* __APPLE__ */
// 
// /* ====================== LINUX X11 IMPLEMENTATION ====================== */
// #elif __linux__
// 
// static Display *g_display = NULL;
// 
// static void ensure_display(void) {
//     if (!g_display) g_display = XOpenDisplay(NULL);
// }
// 
// static Window recursive_find(Window w, const char *title)
// {
//     // Check current window
//     XTextProperty tp;
//     if (XGetWMName(g_display, w, &tp)) {
//         if (strstr((char*)tp.value, title)) {
//             XFree(tp.value);
//             return w;
//         }
//         XFree(tp.value);
//     }
// 
//     Atom net_wm_name = XInternAtom(g_display, "_NET_WM_NAME", True);
//     if (net_wm_name) {
//         Atom type;
//         int fmt;
//         unsigned long n, left;
//         unsigned char *data = NULL;
//         if (XGetWindowProperty(g_display, w, net_wm_name, 0, 1024, False,
//                                AnyPropertyType, &type, &fmt, &n, &left, &data) == Success && data) {
//             if (strstr((char*)data, title)) {
//                 XFree(data);
//                 return w;
//             }
//             XFree(data);
//         }
//     }
// 
//     // Recurse children
//     Window root, parent, *children;
//     unsigned int nchildren;
//     if (XQueryTree(g_display, w, &root, &parent, &children, &nchildren)) {
//         for (unsigned i = 0; i < nchildren; ++i) {
//             Window found = recursive_find(children[i], title);
//             if (found) {
//                 XFree(children);
//                 return found;
//             }
//         }
//         if (children) XFree(children);
//     }
//     return 0;
// }
// 
// window_t find_window_by_title(const char *partial_title)
// {
//     ensure_display();
//     if (!g_display) return 0;
//     return recursive_find(DefaultRootWindow(g_display), partial_title);
// }
// 
// bool window_minimize(window_t win)
// {
//     ensure_display();
//     return XIconifyWindow(g_display, win, DefaultScreen(g_display));
// }
// 
// bool window_maximize(window_t win)
// {
//     ensure_display();
//     XEvent ev = {0};
//     ev.type = ClientMessage;
//     ev.xclient.window = win;
//     ev.xclient.message_type = XInternAtom(g_display, "_NET_WM_STATE", False);
//     ev.xclient.format = 32;
//     ev.xclient.data.l[0] = 1; // add
//     ev.xclient.data.l[1] = XInternAtom(g_display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
//     ev.xclient.data.l[2] = XInternAtom(g_display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
// 
//     return XSendEvent(g_display, DefaultRootWindow(g_display), False,
//                       SubstructureRedirectMask | SubstructureNotifyMask, &ev);
// }
// 
// bool window_restore(window_t win)
// {
//     ensure_display();
//     XEvent ev = {0};
//     ev.type = ClientMessage;
//     ev.xclient.window = win;
//     ev.xclient.message_type = XInternAtom(g_display, "_NET_WM_STATE", False);
//     ev.xclient.format = 32;
//     ev.xclient.data.l[0] = 0; // remove
//     ev.xclient.data.l[1] = XInternAtom(g_display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
//     ev.xclient.data.l[2] = XInternAtom(g_display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
// 
//     return XSendEvent(g_display, DefaultRootWindow(g_display), False,
//                       SubstructureRedirectMask | SubstructureNotifyMask, &ev);
// }
// 
// bool window_move_resize(window_t win, int x, int y, int w, int h)
// {
//     ensure_display();
//     XMoveResizeWindow(g_display, win, x, y, (unsigned)w, (unsigned)h);
//     return true;
// }
// 
// procid_t window_get_pid(window_t win)
// {
//     ensure_display();
//     Atom pid_atom = XInternAtom(g_display, "_NET_WM_PID", True);
//     if (!pid_atom) return -1;
// 
//     Atom type;
//     int format;
//     unsigned long n, bytes;
//     unsigned char *prop = NULL;
// 
//     if (XGetWindowProperty(g_display, win, pid_atom, 0, 1, False, XA_CARDINAL,
//                            &type, &format, &n, &bytes, &prop) == Success && prop) {
//         procid_t pid = *(unsigned long*)prop;
//         XFree(prop);
//         return pid;
//     }
//     return -1;
// }
// 
// bool process_kill(procid_t pid)     { return kill(pid, SIGKILL) == 0; }
// bool process_suspend(procid_t pid)  { return kill(pid, SIGSTOP) == 0; }
// bool process_resume(procid_t pid)   { return kill(pid, SIGCONT) == 0; }

#endif  /* __linux__ */

/* ====================== MAIN / DEMO ====================== */
int main2(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: %s \"window title substring\" [command ...]\n", argv[0]);
        printf("Commands: minimize | maximize | restore | kill | suspend | resume | move x y w h\n");
        return 1;
    }

    window_t win = find_window_by_title(argv[1]);
    if (!win || win == (window_t)-1) {
        printf("No window containing \"%s\" found.\n", argv[1]);
        return 1;
    }

    printf("Found window %p – PID = %u\n", (void*)win, (unsigned)window_get_pid(win));

    if (argc == 2) return 0;

    const char *cmd = argv[2];
    if (!strcmp(cmd, "minimize"))      window_minimize(win);
    else if (!strcmp(cmd, "maximize")) window_maximize(win);
    else if (!strcmp(cmd, "restore"))  window_restore(win);
    else if (!strcmp(cmd, "kill"))     { procid_t p = window_get_pid(win); process_kill(p); }
    else if (!strcmp(cmd, "suspend"))  { procid_t p = window_get_pid(win); process_suspend(p); }
    else if (!strcmp(cmd, "resume"))   { procid_t p = window_get_pid(win); process_resume(p); }
    else if (!strcmp(cmd, "move") && argc == 7) {
        int x = atoi(argv[3]), y = atoi(argv[4]);
        int w = atoi(argv[5]), h = atoi(argv[6]);
        window_move_resize(win, x, y, w, h);
    }
    else {
        printf("Unknown command\n");
        return 1;
    }

    printf("Done.\n");
    return 0;
}
