// Input.hpp - Cross-platform mouse & keyboard control (C++17)
#pragma once
#include <string>

class Input {
public:
    static Input& getInstance();

    // Mouse
    void moveTo(int x, int y);
    void moveBy(int dx, int dy);
    void clickLeft(bool press = true);
    void clickRight(bool press = true);
    void clickMiddle(bool press = true);
    void scroll(int lines);                 // positive = up
    void dragTo(int x, int y);              // left button drag
    void getPosition(int& x, int& y) const;

    // Keyboard
    void keyDown(int virtualKey);
    void keyUp(int virtualKey);
    void keyPress(int virtualKey);          // down + up
    void type(const std::string& text);     // types real characters

    // Convenience
    void clickAt(int x, int y);             // move + click
    void doubleClickAt(int x, int y);

private:
    Input();
    ~Input();
    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;

    class Impl;
    Impl* pImpl;
};
