#include "lib.h"

extern "C" {

int x;

void add() {
  int y = x;
  y++;
  x = y;
}

int get() { return x; }

LTEST(test_adder) {
  int val = 0;
  add();
  return get();
}

}
