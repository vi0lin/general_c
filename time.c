// gcc time.c timeout.c -o bin/time && ./bin/time
#include <stdio.h>
#include <time.h>

#include <stdbool.h>
#include "timeout.h"

int main() {
  time_t currentTime;
  while (true) {
    time(&currentTime);
    printf("Current time: %s", ctime(&currentTime));
    timeout_ms(1000);
    // timeout_ms(1000); // works
  }
  return 0;
}
