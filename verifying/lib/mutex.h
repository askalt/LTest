#include <cassert>
#include <deque>
#include <memory>

#include "../../runtime/include/verifying.h"

// Mutex operates with ltest token.
// After unsuccessful Lock() it parks the method.
// Ltest runtime doesn't launch parked tasks.
// It's important to call `CoroYield()` implicitly after possible parking.
struct Mutex {
  // First - holds the lock.
  // Others - want the lock.
  std::deque<std::shared_ptr<Token>> waiters{};

  Mutex() {}

  // `noexcept` here is needeed to tell clang to generate just `call` instead of
  // `invoke`. The full support for the `invoke` will be added later.
  void Lock(std::shared_ptr<Token> token) noexcept {
    waiters.push_back(token);
    if (waiters.size() == 1) {
      return;
    }
    token->Park();
  }

  // Assume this method is atomic.
  void Unlock() noexcept {
    assert(!waiters.empty());
    waiters.pop_front();
    if (!waiters.empty()) {
      waiters.front()->Unpark();
    }
  }
};
