#include "pretty_print.h"
#include "scheduler.h"

namespace ltest {
extern "C" int TrapRun(std::unique_ptr<Scheduler>&& scheduler,
                       PrettyPrinter& pretty_printer);
}