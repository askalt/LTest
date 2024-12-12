#include <cassert>
#include <functional>
#include <map>
#include <string>

namespace spec {

struct LinearMutex;

using method_t = std::function<int(LinearMutex *l, void *)>;

struct LinearMutex {
  int Lock() {}
  int Unlock() {}

  static auto GetMethods() {
    method_t lock_func = [](LinearMutex *l, void *) -> int {
      return l->Lock();
    };

    method_t unlock_func = [](LinearMutex *l, void *) -> int {
      return l->Unlock();
    };

    return std::map<std::string, method_t>{
        {"Lock", lock_func},
        {"Unlock", unlock_func},
    };
  }
};

struct LinearMutexHash {
  size_t operator()(const LinearMutex &r) const { return 0; }
};

struct LinearMutexEquals {
  bool operator()(const LinearMutex &lhs, const LinearMutex &rhs) const {
    return false;
  }
};

}  // namespace spec
