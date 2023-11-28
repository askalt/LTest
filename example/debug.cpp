#include <iostream>

//#define _DEBUG_PRINT 0

extern "C" {

void debug_print(int a) {
#ifdef _DEBUG_PRINT
  std::cout << a << std::endl;
#endif
}
}
