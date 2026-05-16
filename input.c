/* input.c - Full cross-platform mouse + keyboard control */
#define _GNU_SOURCE
#include "input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#if defined(_WIN32) || defined(_WIN64)
    /* ------------------- WINDOWS ------------------- */
    #include <windows.h>

    void mouse_move(int x, int y) {
        SetCursorPos(x, y);
    }

    void mouse_click_left(bool press) {
        INPUT ip = {0};
        ip.type = INPUT_MOUSE;
        ip.mi.dwFlags = press ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
        SendInput(1, &ip, sizeof(INPUT));
    }

    void mouse_click_right(bool press) {
        INPUT ip = {0};
        ip.type = INPUT_MOUSE;
        ip.mi.dwFlags = press ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
        SendInput(1, &ip, sizeof(INPUT));
    }

    void mouse_click_middle(bool press) {
        INPUT ip = {0};
        ip.type = INPUT_MOUSE;
        ip.mi.dwFlags = press ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
        SendInput(1, &ip, sizeof(INPUT));
    }

    void mouse_wheel(int delta) {
        INPUT ip = {0};
        ip.type = INPUT_MOUSE;
        ip.mi.dwFlags = MOUSEEVENTF_WHEEL;
        ip.mi.mouseData = delta;
        SendInput(1, &ip, sizeof(INPUT));
    }

    void key_press(int code, bool press) {
        INPUT ip = {0};
        ip.type = INPUT_KEYBOARD;
        ip.ki.wVk = code;
        ip.ki.dwFlags = press ? 0 : KEYEVENTF_KEYUP;
        SendInput(1, &ip, sizeof(INPUT));
    }

    void key_type(const char *text) {
        for (size_t i = 0; text[i]; i++) {
            SHORT vk = VkKeyScan(text[i]);
            bool shift = (vk & 0x100);
            bool ctrl  = (vk & 0x200);
            bool alt   = (vk & 0x400);
            vk &= 0xFF;

            if (shift) key_press(VK_SHIFT, true);
            if (ctrl)  key_press(VK_CONTROL, true);
            if (alt)   key_press(VK_MENU, true);

            key_press(vk, true);
            key_press(vk, false);

            if (shift) key_press(VK_SHIFT, false);
            if (ctrl)  key_press(VK_CONTROL, false);
            if (alt)   key_press(VK_MENU, false);
        }
    }

    void get_mouse_pos(int *x, int *y) {
        POINT p;
        GetCursorPos(&p);
        *x = p.x; *y = p.y;
    }

#elif defined(__APPLE__)
    /* ------------------- macOS ------------------- */
    #include <ApplicationServices/ApplicationServices.h>

    void mouse_move(int x, int y) {
        CGPoint pt = CGPointMake(x, y);
        CGEventRef move = CGEventCreateMouseEvent(NULL, kCGEventMouseMoved, pt, 0);
        CGEventPost(kCGHIDEventTap, move);
        CFRelease(move);
    }

    void mouse_click_left(bool press) {
        CGEventType type = press ? kCGEventLeftMouseDown : kCGEventLeftMouseUp;
        CGPoint pos = CGEventGetLocation(CGEventCreate(NULL));
        CGEventRef ev = CGEventCreateMouseEvent(NULL, type, pos, kCGMouseButtonLeft);
        CGEventPost(kCGHIDEventTap, ev);
        CFRelease(ev);
    }

    void mouse_click_right(bool press) {
        CGEventType type = press ? kCGEventRightMouseDown : kCGEventRightMouseUp;
        CGPoint pos = CGEventGetLocation(CGEventCreate(NULL));
        CGEventRef ev = CGEventCreateMouseEvent(NULL, type, pos, kCGMouseButtonRight);
        CGEventPost(kCGHIDEventTap, ev);
        CFRelease(ev);
    }

    void mouse_click_middle(bool press) {
        CGEventType type = press ? kCGEventOtherMouseDown : kCGEventOtherMouseUp;
        CGPoint pos = CGEventGetLocation(CGEventCreate(NULL));
        CGEventRef ev = CGEventCreateMouseEvent(NULL, type, pos, kCGMouseButtonCenter);
        CGEventPost(kCGHIDEventTap, ev);
        CFRelease(ev);
    }

    void mouse_wheel(int delta) {
        CGEventRef ev = CGEventCreateScrollWheelEvent(NULL, kCGScrollEventUnitLine, 1, delta > 0 ? 1 : -1);
        CGEventPost(kCGHIDEventTap, ev);
        CFRelease(ev);
    }

    // macOS key codes: https://developer.apple.com/documentation/virtualkeycodes
    static int mac_keycode(char c) {
        static const int map[] = {
            'a',0x00, 's',0x01, 'd',0x02, 'f',0x03, 'h',0x04, 'g',0x05, 'z',0x06, 'x',0x07,
            'c',0x08, 'v',0x09, 'b',0x0B, 'q',0x0C, 'w',0x0D, 'e',0x0E, 'r',0x0F, 'y',0x10,
            't',0x11, '1',0x12, '2',0x13, '3',0x14, '4',0x15, '6',0x16, '5',0x17, '=',0x18,
            '9',0x19, '7',0x1A, '-',0x1B, '8',0x1C, '0',0x1D, ']',0x1E, 'o',0x1F, 'u',0x20,
            '[',0x21, 'i',0x22, 'p',0x23, 'l',0x25, 'j',0x26, '\'',0x27, 'k',0x28, ';',0x29,
            '\\',0x2A, ',',0x2B, '/',0x2C, 'n',0x2D, 'm',0x2E, '.',0x2F, '`',0x32,
            '\t',0x30, ' ',0x31, '\n',0x24, '\b',0x33, '\e',0x35
        };
        c = tolower(c);
        for (int i = 0; i < sizeof(map)/sizeof(map[0]); i += 2)
            if (map[i] == c) return map[i+1];
        return -1;
    }

    void key_press(int code, bool press) {
        CGEventRef ev = CGEventCreateKeyboardEvent(NULL, code, press);
        CGEventPost(kCGHIDEventTap, ev);
        CFRelease(ev);
    }

    void key_type(const char *text) {
        for (size_t i = 0; text[i]; i++) {
            int code = mac_keycode(text[i]);
            if (code == -1) continue;
            key_press(code, true);
            key_press(code, false);
        }
    }

    void get_mouse_pos(int *x, int *y) {
        CGPoint pt = CGEventGetLocation(CGEventCreate(NULL));
        *x = (int)pt.x; *y = (int)pt.y;
    }

#elif defined(__linux__)
    /* ------------------- LINUX (X11) ------------------- */
    #include <X11/Xlib.h>
    #include <X11/keysym.h>
    #include <X11/extensions/XTest.h>

    #include <X11/Xutil.h>

    static Display *dpy = NULL;

    static void ensure_display(void) {
        if (!dpy) dpy = XOpenDisplay(NULL);
        if (!dpy) { fprintf(stderr, "Cannot open X display\n"); exit(1); }
    }

    void mouse_move(int x, int y) {
        ensure_display();
        XWarpPointer(dpy, None, DefaultRootWindow(dpy), 0, 0, 0, 0, x, y);
        XFlush(dpy);
    }

    void mouse_click_left(bool press) {
        ensure_display();
        XTestFakeButtonEvent(dpy, 1, press, CurrentTime);
        XFlush(dpy);
    }

    void mouse_click_right(bool press) {
        ensure_display();
        XTestFakeButtonEvent(dpy, 3, press, CurrentTime);
        XFlush(dpy);
    }

    void mouse_click_middle(bool press) {
        ensure_display();
        XTestFakeButtonEvent(dpy, 2, press, CurrentTime);
        XFlush(dpy);
    }

    void mouse_wheel(int delta) {
        ensure_display();
        int button = delta > 0 ? 4 : 5;
        XTestFakeButtonEvent(dpy, button, True, CurrentTime);
        XTestFakeButtonEvent(dpy, button, False, CurrentTime);
        XFlush(dpy);
    }

    static KeySym keycode_to_keysym(int code) {
        return code; // assume direct KeySym for simplicity
    }

    void key_press(int code, bool press) {
        ensure_display();
        KeySym ks = keycode_to_keysym(code);
        KeyCode kc = XKeysymToKeycode(dpy, ks);
        XTestFakeKeyEvent(dpy, kc, press, CurrentTime);
        XFlush(dpy);
    }

    void key_type(const char *text) {
        ensure_display();
        for (size_t i = 0; text[i]; i++) {
            KeySym ks = XStringToKeysym(text[i] == '\n' ? "Return" : (char[]){text[i],0});
            if (ks == NoSymbol && isalpha(text[i]))
                ks = XStringToKeysym((char[]){toupper(text[i]),0});
                //{(char)toupper((unsigned char)text[i]),0}
                // ks = XStringToKeysym(std::string(1,(char)std::toupper(static_cast<unsigned char>(text[i])).c_str());
            if (ks == NoSymbol) continue;
            KeyCode kc = XKeysymToKeycode(dpy, ks);
            XTestFakeKeyEvent(dpy, kc, True, CurrentTime);
            XTestFakeKeyEvent(dpy, kc, False, CurrentTime);
        }
        XFlush(dpy);
    }

    void get_mouse_pos(int *x, int *y) {
        ensure_display();
        Window root = DefaultRootWindow(dpy);
        Window win; int rx, ry; unsigned mask;
        XQueryPointer(dpy, root, &win, &win, &rx, &ry, x, y, &mask);
    }

#else
    #error "Unsupported platform"
#endif
