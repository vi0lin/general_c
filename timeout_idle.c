#include <unistd.h> // for sleep
#include <stdio.h>
#include "timeout.h"
#include "input.h"
void timeout_idle(int duration_s, int move_every_s) {
  // check taeglicher bonus
  // check login status / login
  // int duration_ms=(100*1000*duration_s);
  // int move_every_ms=(100*1000*move_every_s);
  int distance=2;
  int steps=duration_s/move_every_s;
  int click_dur_ms=100;
  printf("\n\ntimeout_idle\n");
  printf("duration_s: %d\n"
         "move_every_s: %d\n"
         "distance: %d\n"
         "steps: %d\n"
         "click_dur_ms: %d\n", duration_s, move_every_s, distance, steps, click_dur_ms);
  for (int i = 0; i < (int)steps; i++)
  {
    timeout(move_every_s);
    int x, y;
    get_mouse_pos(&x, &y);
    mouse_move(x+distance, y);
    timeout_ms(click_dur_ms);
    mouse_click_left(true);
    timeout_ms(click_dur_ms);
    mouse_click_left(false);
    timeout_ms(click_dur_ms);
    mouse_move(x, y);
    i++;
  }
  // int divider=30;
  // int step=s/divider;
  // int limit=s;
  // if(step<=0){
  //   sleep(s);
  // } else {
    // for(int i =0; i<limit; i++0){
    //   sleep(30);
    //   mouse_move();
    //   mouse_click();
    //   mouse_move_back();
    // }
  // }
}
