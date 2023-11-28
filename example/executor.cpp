#include "scheduler.h"

extern "C" {

typedef int8_t *(task_generator)();

// sample демонстрирует работу планировщика по шагам.
// на тесте test_adder.
void sample(task_generator gen) {
  auto root_task = std::coroutine_handle<Promise>::from_address(gen());
  root_task.resume();

  // test_add зовёт add()
  auto child = root_task.promise().child_hdl;
  child.resume();
  child.resume();
  child.resume();
  child.resume();
  child.resume();
  child.resume();
  child.resume();
  std::cout << "returned from add(): " << child.promise().result << std::endl;

  root_task.resume();

  // test_add зовёт get()
  child = root_task.promise().child_hdl;
  child.resume();
  child.resume();
  child.resume();
  // Планировщик сообщает вызывавшей корутине о результате ребенка.
  root_task.promise().child_returned = child.promise().result;
  // Заметим, что здесь мы не можем разрушать корутину в момент завершения,
  // потому что в её промисе хранится результат, который прочтет отец.
  // Надо придумать, как собирать мусор.

  root_task.resume();
  std::cout << "Root task result is " << root_task.promise().result
            << std::endl;
}

const int THREADS = 2;

void init_func();

void test_task(task_generator gen) {
  for (int iter = 0; iter < 15; ++iter) {
    init_func();
    std::vector<Scheduler> tasks;
    for (int i = 0; i < THREADS; ++i)
      tasks.emplace_back(Scheduler{gen()});
    // Random execution.
    while (true) {
      std::vector<int> vars;
      for (int i = 0; i < THREADS; ++i) {
        if (!tasks[i].Done()) {
          vars.emplace_back(i);
        }
      }
      if (vars.empty())
        break;
      tasks[vars[rand() % vars.size()]].DoStep();
    }
    std::cout << "Execution result is (" << tasks[0].GetResult() << ", "
              << tasks[1].GetResult() << ")" << std::endl;
  }
}
}
