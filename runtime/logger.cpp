#include "include/logger.h"

#include <iostream>

Logger l{};

void logger_init(bool verbose) { l.verbose = verbose; }

void Logger::flush() {
  if (verbose) {
    std::cout.flush();
  }
}

Logger& log() { return l; }
