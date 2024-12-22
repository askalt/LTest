#pragma once
#include <gflags/gflags.h>

#include <memory>

#include "lib.h"
#include "lincheck_recursive.h"
#include "logger.h"
#include "pct_strategy.h"
#include "pretty_print.h"
#include "random_strategy.h"
#include "round_robin_strategy.h"
#include "scheduler.h"
#include "strategy_verifier.h"
#include "syscall_trap.h"
#include "verifying_macro.h"

namespace ltest {

enum StrategyType { RR, RND, TLA, PCT };

constexpr const char *GetLiteral(StrategyType t);

template <class TargetObj, class LinearSpec,
          class LinearSpecHash = std::hash<LinearSpec>,
          class LinearSpecEquals = std::equal_to<LinearSpec>>
struct Spec {
  using target_obj_t = TargetObj;
  using linear_spec_t = LinearSpec;
  using linear_spec_hash_t = LinearSpecHash;
  using linear_spec_equals_t = LinearSpecEquals;
};

struct Opts {
  size_t threads;
  size_t tasks;
  size_t switches;
  size_t rounds;
  bool verbose;
  bool syscall_trap;
  StrategyType typ;
  std::vector<int> thread_weights;
};

Opts parse_opts();

std::vector<std::string> split(const std::string &s, char delim);

template <typename TargetObj, typename StrategyVerifier>
std::unique_ptr<Strategy<StrategyVerifier>> MakeStrategy(
    Opts &opts, std::vector<TaskBuilder> l) {
  switch (opts.typ) {
    case RR: {
      std::cout << "round-robin\n";
      return std::make_unique<RoundRobinStrategy<TargetObj, StrategyVerifier>>(
          opts.threads, std::move(l));
    }
    case RND: {
      std::cout << "random\n";
      std::vector<int> weights = opts.thread_weights;
      if (weights.empty()) {
        weights.assign(opts.threads, 1);
      }
      if (weights.size() != opts.threads) {
        throw std::invalid_argument{
            "number of threads not equal to number of weights"};
      }
      return std::make_unique<RandomStrategy<TargetObj, StrategyVerifier>>(
          opts.threads, std::move(l), std::move(weights));
    }
    case PCT: {
      std::cout << "pct\n";
      // return std::make_unique<PctStrategy<TargetObj>>(opts.threads,
      //                                                 std::move(l), true);
    }
    default:
      assert(false && "unexpected typ");
  }
}

// Keeps pointer to strategy to pass reference to base scheduler.
// TODO: refactor.
template <typename StrategyVerifier>
struct StrategySchedulerWrapper : StrategyScheduler<StrategyVerifier> {
  StrategySchedulerWrapper(std::unique_ptr<Strategy<StrategyVerifier>> strategy,
                           ModelChecker &checker, PrettyPrinter &pretty_printer,
                           size_t max_tasks, size_t max_rounds)
      : strategy(std::move(strategy)),
        StrategyScheduler<StrategyVerifier>(
            *strategy.get(), checker, pretty_printer, max_tasks, max_rounds) {};

 private:
  std::unique_ptr<Strategy<StrategyVerifier>> strategy;
};

template <typename TargetObj, typename StrategyVerifier>
std::unique_ptr<Scheduler> MakeScheduler(ModelChecker &checker, Opts &opts,
                                         std::vector<TaskBuilder> l,
                                         PrettyPrinter &pretty_printer) {
  std::cout << "strategy = ";
  switch (opts.typ) {
    case RR:
    case PCT:
    case RND: {
      auto strategy =
          MakeStrategy<TargetObj, StrategyVerifier>(opts, std::move(l));
      auto scheduler =
          std::make_unique<StrategySchedulerWrapper<StrategyVerifier>>(
              std::move(strategy), checker, pretty_printer, opts.tasks,
              opts.rounds);
      return scheduler;
    }
    case TLA: {
      auto scheduler = std::make_unique<TLAScheduler<TargetObj>>(
          opts.tasks, opts.rounds, opts.threads, opts.switches, std::move(l),
          checker, pretty_printer);
      return scheduler;
    }
  }
}

template <typename StrategyVerifier>
int NoTrapRun(std::unique_ptr<Scheduler> &&scheduler,
              PrettyPrinter &pretty_printer) {
  auto result = scheduler->Run();
  if (result.has_value()) {
    std::cout << "non linearized:\n";
    pretty_printer.PrettyPrint(result.value().second, std::cout);
    return 1;
  } else {
    std::cout << "success!\n";
    return 0;
  }
}

template <class Spec, class StrategyVerifier = DefaultStrategyVerifier>
int Run(int argc, char *argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  Opts opts = parse_opts();

  logger_init(opts.verbose);
  std::cout << "threads  = " << opts.threads << "\n";
  std::cout << "tasks    = " << opts.tasks << "\n";
  std::cout << "switches = " << opts.switches << "\n";
  std::cout << "rounds   = " << opts.rounds << "\n";
  std::cout << "targets  = " << task_builders.size() << "\n";

  PrettyPrinter pretty_printer{opts.threads};

  using lchecker_t =
      LinearizabilityCheckerRecursive<typename Spec::linear_spec_t,
                                      typename Spec::linear_spec_hash_t,
                                      typename Spec::linear_spec_equals_t>;
  lchecker_t checker{Spec::linear_spec_t::GetMethods(),
                     typename Spec::linear_spec_t{}};

  auto scheduler = MakeScheduler<typename Spec::target_obj_t, StrategyVerifier>(
      checker, opts, std::move(task_builders), pretty_printer);
  std::cout << "\n\n";
  std::cout.flush();
  if (!opts.syscall_trap) {
    return NoTrapRun<StrategyVerifier>(std::move(scheduler), pretty_printer);
  } else {
    auto guard = SyscallTrapGuard{};
    return NoTrapRun<StrategyVerifier>(std::move(scheduler), pretty_printer);
  }
}

}  // namespace ltest

#define LTEST_ENTRYPOINT_CONSTRAINT(spec_obj_t, sched_constraint) \
  namespace ltest {                                               \
  std::vector<TaskBuilder> task_builders{};                       \
  }                                                               \
  int main(int argc, char *argv[]) {                              \
    return ltest::Run<spec_obj_t, sched_constraint>(argc, argv);  \
  }

#define LTEST_ENTRYPOINT(spec_obj_t)           \
  namespace ltest {                            \
  std::vector<TaskBuilder> task_builders{};    \
  }                                            \
  int main(int argc, char *argv[]) {           \
    return ltest::Run<spec_obj_t>(argc, argv); \
  }\
