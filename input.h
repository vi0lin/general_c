#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>

void mouse_move(int x, int y);
void mouse_click_left(bool press);
void mouse_click_right(bool press);
void mouse_click_middle(bool press);
void mouse_wheel(int delta);
void key_press(int code, bool press);
void key_type(const char *text);
void get_mouse_pos(int *x, int *y);

#endif
