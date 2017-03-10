#include <stdio.h>

char *store;
int main(int argc, char *argv[]) {
  int data = argc + **argv;
  int i = 0;
  for (i = 0; i < data; i++) {
    data *= data;
  }
  store[0] = data;
}
