// gcc time_ms.c timeout.c -o bin/time_ms && ./bin/time_ms
#include <stdio.h>
#include <stdbool.h>
#include <sys/time.h>

#include "timeout.h"

int main() {
  struct timeval currentTime;
  while (true) {
    gettimeofday(&currentTime, NULL);
    long long milliseconds = (long long)currentTime.tv_sec * 1000 + currentTime.tv_usec / 1000;
    printf("Current time in milliseconds: %lld\n", milliseconds);
    timeout_ms(30);
    // timeout_ms(1); // works
  }
  return 0;
}
