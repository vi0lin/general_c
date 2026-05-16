#include <unistd.h> // for sleep
#include "timeout.hpp"
#include "Input.hpp"

void timeout(int ms) {
  int every=(1000*5);
  int distance=100;
  int x=ms/every;
  int click_dur=100*1000;
  int i = 0;
  for (i = 0; i < (int)x; i++)
  {
    int x, y;
    // get_mouse_pos(&x, &y);
    // mouse_move(x+distance, y);
    usleep(click_dur);
    // mouse_move(x, y);
    usleep(every*1000-click_dur);
  }
}
void timeout_ms(int ms){
  timeout(ms); 
}
void timeout_s(int s){
  timeout(s*1000); 
}
void timeout_m(int m){
  timeout(m*1000*60); 
}
void timeout_h(int m){
  timeout(m*1000*60*60); 
}

