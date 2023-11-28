#include <cassert>
#include <coroutine>
#include <cstdint>
#include <iostream>
#include <optional>
#include <vector>

extern "C" {

struct Promise {
  int result;
  std::coroutine_handle<Promise> child_hdl;
  int child_returned;
  // ...
};

void set_child_hdl(Promise *p, int8_t *hdl) {
  p->child_hdl = std::coroutine_handle<Promise>::from_address(hdl);
}

void set_result(Promise *p, int result) { p->result = result; }

int get_child_return(Promise *p) { return p->child_returned; }

struct Scheduler {
  Scheduler(int8_t *hdl)
      : hdl(std::coroutine_handle<Promise>::from_address(hdl)) {}

  void DoStep() {
    // TODO: хранить стэк между вызовами.
    // TODO: сборка мусора.
    auto current_handle = hdl;
    std::vector<std::coroutine_handle<Promise>> stack;
    stack.push_back(current_handle);

    while (current_handle.promise().child_hdl != nullptr) {
      current_handle = current_handle.promise().child_hdl;
      stack.push_back(current_handle);
    }

    assert(!stack.empty());
    if (stack.back().promise().result != 0) {
      // Текущая корутина на стэке завершилась на прошлом шагу.
      int returned = stack.back().promise().result;
      if (stack.size() == 1) {
        // Корневая задача вернулась.
        result = returned;
        return;
      }
      stack[(int)stack.size() - 2].promise().child_returned = returned;
      // Говорим предыдущей корутине на стэке, что у нее больше нет ребенка.
      stack[(int)stack.size() - 2].promise().child_hdl = nullptr;
      DoStep();
      return;
    }

    stack.back().resume();
  }

  bool Done() { return result.has_value(); }

  int GetResult() { return result.value(); }

  std::coroutine_handle<Promise> hdl;
  std::optional<int> result;
  // ...
};
}
