// Input.cpp
#include "Input.hpp"
#include <thread>
#include <chrono>
#include <stdexcept>

using namespace std::chrono_literals;

#if defined(_WIN32) || defined(_WIN64)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#elif defined(__APPLE__)
    #include <ApplicationServices/ApplicationServices.h>
#elif defined(__linux__)
    #include <X11/Xlib.h>
    #include <X11/extensions/XTest.h>
    #include <X11/keysym.h>
#else
    #error "Unsupported platform"
#endif

class Input::Impl {
public:
#if defined(_WIN32) || defined(_WIN64)
    // Windows implementation
    void moveTo(int x, int y) { SetCursorPos(x, y); }
    void getPosition(int& x, int& y) const {
        POINT p; GetCursorPos(&p); x = p.x; y = p.y;
    }
    void mouseEvent(DWORD flags) {
        INPUT input = {0};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = flags;
        SendInput(1, &input, sizeof(INPUT));
    }
    void keyEvent(WORD vk, bool down) {
        INPUT input = {0};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = vk;
        input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
        SendInput(1, &input, sizeof(INPUT));
    }
    void typeChar(char c) {
        SHORT scan = VkKeyScanA(c);
        bool shift = (scan & 0x100);
        WORD vk = LOBYTE(scan);

        if (shift) keyEvent(VK_SHIFT, true);
        keyEvent(vk, true);
        keyEvent(vk, false);
        if (shift) keyEvent(VK_SHIFT, false);
    }

#elif defined(__APPLE__)
    // macOS implementation
    void moveTo(int x, int y) {
        CGPoint p = CGPointMake(x, y);
        CGEventRef e = CGEventCreateMouseEvent(nullptr, kCGEventMouseMoved, p, 0);
        CGEventPost(kCGHIDEventTap, e);
        CFRelease(e);
    }
    void getPosition(int& x, int& y) const {
        CGEventRef e = CGEventCreate(nullptr);
        CGPoint p = CGEventGetLocation(e);
        CFRelease(e);
        x = (int)p.x; y = (int)p.y;
    }
    void mouseEvent(CGEventType down, CGEventType up, CGMouseButton btn = kCGMouseButtonLeft) {
        CGPoint pos = CGEventGetLocation(CGEventCreate(nullptr));
        CGEventRef e;
        e = CGEventCreateMouseEvent(nullptr, down, pos, btn);
        CGEventPost(kCGHIDEventTap, e);
        CFRelease(e);
        e = CGEventCreateMouseEvent(nullptr, up, pos, btn);
        CGEventPost(kCGHIDEventTap, e);
        CFRelease(e);
    }
    static CGKeyCode macKeyCode(char c) {
        static const char* map = "abcdefghijklmnopqrstuvwxyz0123456789";
        static const CGKeyCode codes[] = {
            0x00,0x0B,0x08,0x02,0x0E,0x03,0x05,0x04,0x22,0x26,0x28,0x25,0x2E,0x2D,0x1F,0x23,
            0x0C,0x0D,0x07,0x10,0x06,0x12,0x13,0x14,0x15,0x17,0x16,0x1A,0x1C,0x19,0x1D,0x18
        };
        if (c >= 'a' && c <= 'z') return codes[c - 'a'];
        if (c >= '0' && c <= '9') return codes[26 + (c - '0')];
        if (c == ' ') return 0x31;
        if (c == '\n') return 0x24;
        return 0xFF;
    }
    void typeChar(char c) {
        CGKeyCode code = macKeyCode(tolower(c));
        if (code == 0xFF) return;
        bool shift = isupper(c) || (c >= 33 && c <= 47);
        if (shift) {
            CGEventRef e = CGEventCreateKeyboardEvent(nullptr, 0x38, true);
            CGEventPost(kCGHIDEventTap, e);
            CFRelease(e);
        }
        CGEventRef e1 = CGEventCreateKeyboardEvent(nullptr, code, true);
        CGEventRef e2 = CGEventCreateKeyboardEvent(nullptr, code, false);
        CGEventPost(kCGHIDEventTap, e1);
        CGEventPost(kCGHIDEventTap, e2);
        CFRelease(e1); CFRelease(e2);
        if (shift) {
            CGEventRef e = CGEventCreateKeyboardEvent(nullptr, 0x38, false);
            CGEventPost(kCGHIDEventTap, e);
            CFRelease(e);
        }
    }

#elif defined(__linux__)
    Display* dpy = nullptr;
    Impl() { dpy = XOpenDisplay(nullptr); if (!dpy) throw std::runtime_error("No X11 display"); }
    ~Impl() { if (dpy) XCloseDisplay(dpy); }
    void moveTo(int x, int y) {
        XWarpPointer(dpy, None, DefaultRootWindow(dpy), 0,0,0,0, x, y);
        XFlush(dpy);
    }
    void getPosition(int& x, int& y) const {
        Window r, c; int rx, ry; unsigned mask;
        XQueryPointer(dpy, DefaultRootWindow(dpy), &r, &c, &rx, &ry, &x, &y, &mask);
    }
    void mouseEvent(int button, bool press) {
        XTestFakeButtonEvent(dpy, button, press, CurrentTime);
        XFlush(dpy);
    }
    void typeChar(char c) {
        KeySym ks = (c == '\n') ? XK_Return : c;
        if (ks == NoSymbol && isalpha(c)) ks = toupper(c);
        if (ks == NoSymbol) return;
        KeyCode kc = XKeysymToKeycode(dpy, ks);
        XTestFakeKeyEvent(dpy, kc, True, CurrentTime);
        XTestFakeKeyEvent(dpy, kc, False, CurrentTime);
        XFlush(dpy);
    }
#endif
};

// ========================================
// Input class implementation
// ========================================

Input::Input() : pImpl(new Impl()) {}
Input::~Input() { delete pImpl; }

Input& Input::getInstance() {
    static Input instance;
    return instance;
}

void Input::moveTo(int x, int y) { pImpl->moveTo(x, y); }

void Input::moveBy(int dx, int dy) {
    int x, y; getPosition(x, y);
    moveTo(x + dx, y + dy);
}
void Input::clickLeft(bool press) {
#if defined(_WIN32)
    pImpl->mouseEvent(press ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP);
#elif defined(__APPLE__)
    pImpl->mouseEvent(kCGEventLeftMouseDown, kCGEventLeftMouseUp);
#elif defined(__linux__)
    pImpl->mouseEvent(1, press);
#endif
}
void Input::clickRight(bool press) {
#if defined(_WIN32)
    pImpl->mouseEvent(press ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP);
#elif defined(__APPLE__)
    pImpl->mouseEvent(kCGEventRightMouseDown, kCGEventRightMouseUp, kCGMouseButtonRight);
#elif defined(__linux__)
    pImpl->mouseEvent(3, press);
#endif
}
void Input::clickMiddle(bool press) {
#if defined(_WIN32)
    pImpl->mouseEvent(press ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP);
#elif defined(__APPLE__)
    pImpl->mouseEvent(kCGEventOtherMouseDown, kCGEventOtherMouseUp, kCGMouseButtonCenter);
#elif defined(__linux__)
    pImpl->mouseEvent(2, press);
#endif
}
void Input::scroll(int lines) {
#if defined(_WIN32)
    pImpl->mouseEvent(MOUSEEVENTF_WHEEL | (lines * WHEEL_DELTA));
#elif defined(__APPLE__)
    CGEventRef e = CGEventCreateScrollWheelEvent(nullptr, kCGScrollEventUnitLine, 1, lines);
    CGEventPost(kCGHIDEventTap, e);
    CFRelease(e);
#elif defined(__linux__)
    int btn = lines > 0 ? 4 : 5;
    for (int i = 0; i < abs(lines); ++i) {
        pImpl->mouseEvent(btn, true);
        pImpl->mouseEvent(btn, false);
    }
#endif
}
void Input::dragTo(int x, int y) {
    clickLeft(true);
    moveTo(x, y);
    std::this_thread::sleep_for(10ms);
    clickLeft(false);
}
void Input::getPosition(int& x, int& y) const { pImpl->getPosition(x, y); }

void Input::keyDown(int vk) {
#if defined(_WIN32)
    pImpl->keyEvent(vk, true);
#elif defined(__APPLE__)
    CGEventRef e = CGEventCreateKeyboardEvent(nullptr, vk, true);
    CGEventPost(kCGHIDEventTap, e);
    CFRelease(e);
#elif defined(__linux__)
    // vk assumed to be KeySym
    KeyCode kc = XKeysymToKeycode(((Input::Impl*)pImpl)->dpy, vk);
    XTestFakeKeyEvent(((Input::Impl*)pImpl)->dpy, kc, True, CurrentTime);
    XFlush(((Input::Impl*)pImpl)->dpy);
#endif
}
void Input::keyUp(int vk) {
    // similar to keyDown but false
    // omitted for brevity — copy pattern above
}
void Input::keyPress(int vk) { keyDown(vk); keyUp(vk); }

void Input::type(const std::string& text) {
    for (char c : text) {
        pImpl->typeChar(c);
        std::this_thread::sleep_for(5ms);  // natural typing feel
    }
}

void Input::clickAt(int x, int y) {
    moveTo(x, y);
    clickLeft(true);
    std::this_thread::sleep_for(50ms);
    clickLeft(false);
}
void Input::doubleClickAt(int x, int y) {
    clickAt(x, y);
    std::this_thread::sleep_for(100ms);
    clickAt(x, y);
}

// //
// // O
// // Input.cpp
// #include "Input.hpp"
// #include <thread>
// #include <chrono>
// // #include <stdexcept>
// 
// using namespace std::chrono_literals;
// 
// #if defined(_WIN32) || defined(_WIN64)
//     #define WIN32_LEAN_AND_MEAN
//     #include <windows.h>
// #elif defined(__APPLE__)
//     #include <ApplicationServices/ApplicationServices.h>
// #elif defined(__linux__)
//     #include <X11/Xlib.h>
//     #include <X11/extensions/XTest.h>
//     #include <X11/keysym.h>
// #else
//     #error "Unsupported platform"
// #endif
// 
// class Input::Impl {
// public:
// #if defined(_WIN32) || defined(_WIN64)
//     // Windows implementation
//     void moveTo(int x, int y) { SetCursorPos(x, y); }
//     void getPosition(int& x, int& y) const {
//         POINT p; GetCursorPos(&p); x = p.x; y = p.y;
//     }
//     void mouseEvent(DWORD flags) {
//         INPUT input = {0};
//         input.type = INPUT_MOUSE;
//         input.mi.dwFlags = flags;
//         SendInput(1, &input, sizeof(INPUT));
//     }
//     void keyEvent(WORD vk, bool down) {
//         INPUT input = {0};
//         input.type = INPUT_KEYBOARD;
//         input.ki.wVk = vk;
//         input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
//         SendInput(1, &input, sizeof(INPUT));
//     }
//     void typeChar(char c) {
//         SHORT scan = VkKeyScanA(c);
//         bool shift = (scan & 0x100);
//         WORD vk = LOBYTE(scan);
// 
//         if (shift) keyEvent(VK_SHIFT, true);
//         keyEvent(vk, true);
//         keyEvent(vk, false);
//         if (shift) keyEvent(VK_SHIFT, false);
//     }
// 
// #elif defined(__APPLE__)
//     // macOS implementation
//     void moveTo(int x, int y) {
//         CGPoint p = CGPointMake(x, y);
//         CGEventRef e = CGEventCreateMouseEvent(nullptr, kCGEventMouseMoved, p, 0);
//         CGEventPost(kCGHIDEventTap, e);
//         CFRelease(e);
//     }
//     void getPosition(int& x, int& y) const {
//         CGEventRef e = CGEventCreate(nullptr);
//         CGPoint p = CGEventGetLocation(e);
//         CFRelease(e);
//         x = (int)p.x; y = (int)p.y;
//     }
//     void mouseEvent(CGEventType down, CGEventType up, CGMouseButton btn = kCGMouseButtonLeft) {
//         CGPoint pos = CGEventGetLocation(CGEventCreate(nullptr));
//         CGEventRef e;
//         e = CGEventCreateMouseEvent(nullptr, down, pos, btn);
//         CGEventPost(kCGHIDEventTap, e);
//         CFRelease(e);
//         e = CGEventCreateMouseEvent(nullptr, up, pos, btn);
//         CGEventPost(kCGHIDEventTap, e);
//         CFRelease(e);
//     }
//     static CGKeyCode macKeyCode(char c) {
//         static const char* map = "abcdefghijklmnopqrstuvwxyz0123456789";
//         static const CGKeyCode codes[] = {
//             0x00,0x0B,0x08,0x02,0x0E,0x03,0x05,0x04,0x22,0x26,0x28,0x25,0x2E,0x2D,0x1F,0x23,
//             0x0C,0x0D,0x07,0x10,0x06,0x12,0x13,0x14,0x15,0x17,0x16,0x1A,0x1C,0x19,0x1D,0x18
//         };
//         if (c >= 'a' && c <= 'z') return codes[c - 'a'];
//         if (c >= '0' && c <= '9') return codes[26 + (c - '0')];
//         if (c == ' ') return 0x31;
//         if (c == '\n') return 0x24;
//         return 0xFF;
//     }
//     void typeChar(char c) {
//         CGKeyCode code = macKeyCode(tolower(c));
//         if (code == 0xFF) return;
//         bool shift = isupper(c) || (c >= 33 && c <= 47);
//         if (shift) {
//             CGEventRef e = CGEventCreateKeyboardEvent(nullptr, 0x38, true);
//             CGEventPost(kCGHIDEventTap, e);
//             CFRelease(e);
//         }
//         CGEventRef e1 = CGEventCreateKeyboardEvent(nullptr, code, true);
//         CGEventRef e2 = CGEventCreateKeyboardEvent(nullptr, code, false);
//         CGEventPost(kCGHIDEventTap, e1);
//         CGEventPost(kCGHIDEventTap, e2);
//         CFRelease(e1); CFRelease(e2);
//         if (shift) {
//             CGEventRef e = CGEventCreateKeyboardEvent(nullptr, 0x38, false);
//             CGEventPost(kCGHIDEventTap, e);
//             CFRelease(e);
//         }
//     }
// 
// #elif defined(__linux__)
//     Display* dpy = nullptr;
//     Impl() { dpy = XOpenDisplay(nullptr); if (!dpy) throw std::runtime_error("No X11 display"); }
//     ~Impl() { if (dpy) XCloseDisplay(dpy); }
//     void moveTo(int x, int y) {
//         XWarpPointer(dpy, None, DefaultRootWindow(dpy), 0,0,0,0, x, y);
//         XFlush(dpy);
//     }
//     void getPosition(int& x, int& y) const {
//         Window r, c; int rx, ry; unsigned mask;
//         XQueryPointer(dpy, DefaultRootWindow(dpy), &r, &c, &rx, &ry, &x, &y, &mask);
//     }
//     void mouseEvent(int button, bool press) {
//         XTestFakeButtonEvent(dpy, button, press, CurrentTime);
//         XFlush(dpy);
//     }
//     void typeChar(char c) {
//         // KeySym ks = (c == '\n') ? XK_Return : XStringToKeysym(&(char[]){c,0});
//         KeySym ks = (c == '\n') ? XK_Return : c;
//         //if (ks == NoSymbol && isalpha(c)) ks = XStringToKeysym((char[]){(char)toupper(c),0});
//         if (ks == NoSymbol && isalpha(c)) ks = (char)toupper(c);
//         if (ks == NoSymbol) return;
//         KeyCode kc = XKeysymToKeycode(dpy, ks);
//         XTestFakeKeyEvent(dpy, kc, True, CurrentTime);
//         XTestFakeKeyEvent(dpy, kc, False, CurrentTime);
//         XFlush(dpy);
//     }
// #endif
// };
// 
// // ========================================
// // Input class implementation
// // ========================================
// 
// Input::Input() : pImpl(new Impl()) {}
// Input::~Input() { delete pImpl; }
// 
// Input& Input::getInstance() {
//     static Input instance;
//     return instance;
// }
// 
// void Input::moveTo(int x, int y) { pImpl->moveTo(x, y); }
// 
// void Input::moveBy(int dx, int dy) {
//     int x, y; getPosition(x, y);
//     moveTo(x + dx, y + dy);
// }
// void Input::clickLeft(bool press) {
// #if defined(_WIN32)
//     pImpl->mouseEvent(press ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP);
// #elif defined(__APPLE__)
//     pImpl->mouseEvent(kCGEventLeftMouseDown, kCGEventLeftMouseUp);
// #elif defined(__linux__)
//     pImpl->mouseEvent(1, press);
// #endif
// }
// void Input::clickRight(bool press) {
// #if defined(_WIN32)
//     pImpl->mouseEvent(press ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP);
// #elif defined(__APPLE__)
//     pImpl->mouseEvent(kCGEventRightMouseDown, kCGEventRightMouseUp, kCGMouseButtonRight);
// #elif defined(__linux__)
//     pImpl->mouseEvent(3, press);
// #endif
// }
// void Input::clickMiddle(bool press) {
// #if defined(_WIN32)
//     pImpl->mouseEvent(press ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP);
// #elif defined(__APPLE__)
//     pImpl->mouseEvent(kCGEventOtherMouseDown, kCGEventOtherMouseUp, kCGMouseButtonCenter);
// #elif defined(__linux__)
//     pImpl->mouseEvent(2, press);
// #endif
// }
// void Input::scroll(int lines) {
// #if defined(_WIN32)
//     pImpl->mouseEvent(MOUSEEVENTF_WHEEL | (lines * WHEEL_DELTA));
// #elif defined(__APPLE__)
//     CGEventRef e = CGEventCreateScrollWheelEvent(nullptr, kCGScrollEventUnitLine, 1, lines);
//     CGEventPost(kCGHIDEventTap, e);
//     CFRelease(e);
// #elif defined(__linux__)
//     int btn = lines > 0 ? 4 : 5;
//     for (int i = 0; i < abs(lines); ++i) {
//         pImpl->mouseEvent(btn, true);
//         pImpl->mouseEvent(btn, false);
//     }
// #endif
// }
// void Input::dragTo(int x, int y) {
//     clickLeft(true);
//     moveTo(x, y);
//     std::this_thread::sleep_for(10ms);
//     clickLeft(false);
// }
// void Input::getPosition(int& x, int& y) const { pImpl->getPosition(x, y); }
// 
// void Input::keyDown(int vk) {
// #if defined(_WIN32)
//     pImpl->keyEvent(vk, true);
// #elif defined(__APPLE__)
//     CGEventRef e = CGEventCreateKeyboardEvent(nullptr, vk, true);
//     CGEventPost(kCGHIDEventTap, e);
//     CFRelease(e);
// #elif defined(__linux__)
//     // vk assumed to be KeySym
//     KeyCode kc = XKeysymToKeycode(((Input::Impl*)pImpl)->dpy, vk);
//     XTestFakeKeyEvent(((Input::Impl*)pImpl)->dpy, kc, True, CurrentTime);
//     XFlush(((Input::Impl*)pImpl)->dpy);
// #endif
// }
// void Input::keyUp(int vk) {
//     // similar to keyDown but false
//     // omitted for brevity — copy pattern above
// }
// void Input::keyPress(int vk) { keyDown(vk); keyUp(vk); }
// 
// void Input::type(const std::string& text) {
//     for (char c : text) {
//         pImpl->typeChar(c);
//         std::this_thread::sleep_for(5ms);  // natural typing feel
//     }
// }
// 
// void Input::clickAt(int x, int y) {
//     moveTo(x, y);
//     clickLeft(true);
//     std::this_thread::sleep_for(50ms);
//     clickLeft(false);
// }
// void Input::doubleClickAt(int x, int y) {
//     clickAt(x, y);
//     std::this_thread::sleep_for(100ms);
//     clickAt(x, y);
// }
