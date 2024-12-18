
#pragma once
#include <queue>
#include <random>

#include "scheduler.h"

template <typename TargetObj>
struct PickStrategy : Strategy {
  virtual size_t Pick() = 0;

  virtual size_t PickSchedule() = 0;

  explicit PickStrategy(size_t threads_count,
                        std::vector<TaskBuilder> constructors)
      : next_task(0),
        threads_count(threads_count),
        constructors(std::move(constructors)),
        threads() {
    round_schedule.resize(threads_count, -1);

    std::random_device dev;
    rng = std::mt19937(dev());
    distribution = std::uniform_int_distribution<std::mt19937::result_type>(
        0, this->constructors.size() - 1);

    // Create queues.
    for (size_t i = 0; i < threads_count; ++i) {
      threads.emplace_back();
    }
  }

  // If there aren't any non returned tasks and the amount of finished tasks
  // is equal to the max_tasks the finished task will be returned
  std::tuple<Task&, bool, int> Next() override {
    auto current_thread = Pick();

    // it's the first task if the queue is empty
    if (threads[current_thread].empty() ||
        threads[current_thread].back()->IsReturned()) {
      // a task has finished or the queue is empty, so we add a new task
      auto constructor = constructors.at(distribution(rng));
      threads[current_thread].emplace_back(
          constructor.Build(&state, current_thread, new_task_id++));
      return {threads[current_thread].back(), true, current_thread};
    }

    return {threads[current_thread].back(), false, current_thread};
  }

  std::tuple<Task&, bool, int> NextSchedule() override {
    size_t current_thread = PickSchedule();
    int next_task_index = GetNextTaskInThread(current_thread);
    bool is_new = round_schedule[current_thread] != next_task_index;

    round_schedule[current_thread] = next_task_index;
    return { threads[current_thread][next_task_index], is_new, current_thread };
  }

  // TODO: same implementation for pct
  std::optional<std::tuple<Task&, int>> GetTask(int task_id) override {
    // TODO: can this be optimized?
    int thread_id = 0;
    for (auto& thread : threads) {
      size_t tasks = thread.size();

      for (size_t i = 0; i < tasks; ++i) {
        Task& task = thread[i];
        if (task->GetId() == task_id) {
          std::tuple<Task&, int> result = { task, thread_id };
          return result;
          // return std::make_tuple(task, thread_id);
        }
      }

      thread_id++;
    }
    return std::nullopt;
  }

  void StartNextRound() override {
    new_task_id = 0;

    TerminateTasks();
    for (auto& thread : threads) {
      // We don't have to keep references alive
      while (thread.size() > 0) {
        thread.pop_back();
      }
    }

    // Reinitial target as we start from the beginning.
    state.Reset();
  }

  // TODO: this method is identical for pick_strategy and for pct_strategy
  void ResetCurrentRound() override {
    TerminateTasks();
    state.Reset();
    for (auto& thread : threads) {
      size_t tasks_in_thread = thread.size();
      for (size_t i = 0; i < tasks_in_thread; ++i) {
        if (!thread[i]->IsRemoved()) {
          thread[i] = thread[i]->Restart(&state);
        }
      }
    }
  }

  // TODO: same implementation for pct
  int GetValidTasksCount() const override {
    int non_removed_tasks = 0;
    for (auto& thread : threads) {
      for (size_t i = 0; i < thread.size(); ++i) {
        auto& task = thread[i];
        if (!task.get()->IsRemoved()) {
          non_removed_tasks++;
        }
      }
    }
    return non_removed_tasks;
  }

  ~PickStrategy() { TerminateTasks(); }

protected:
  // Terminates all running tasks.
  // We do it in a dangerous way: in random order.
  // Actually, we assume obstruction free here.
  // TODO: for non obstruction-free we need to take into account dependencies.
  void TerminateTasks() {
    assert(round_schedule.size() == threads.size() && "sizes expected to be the same");
    round_schedule.assign(round_schedule.size(), -1);

    for (size_t i = 0; i < threads.size(); ++i) {      
      for (size_t j = 0; j < threads[i].size(); ++j) {
        auto& task = threads[i][j];
        if (!task->IsReturned()) {
          task->Terminate();
        }
      }
    }
  }

  // TODO: same implementation for pct
  int GetNextTaskInThread(int thread_index) const override {
    auto& thread = threads[thread_index];
    int task_index = round_schedule[thread_index];

    while (
      task_index < static_cast<int>(thread.size()) &&
      (
        task_index == -1 ||
        thread[task_index].get()->IsReturned() ||
        thread[task_index].get()->IsRemoved()
      )
    ) {
      task_index++;
    }

    // TODO: we can update `round_schedule[thread_index] = task_index` here
    // in order to optimize multiple calls to this function
    return task_index;
  }

  TargetObj state{};
  size_t next_task = 0;
  size_t threads_count;
  std::vector<TaskBuilder> constructors;
  // RoundRobinStrategy struct is the owner of all tasks, and all
  // references can't be invalidated before the end of the round,
  // so we have to contains all tasks in queues(queue doesn't invalidate the
  // references)
  std::vector<StableVector<Task>> threads;
  std::uniform_int_distribution<std::mt19937::result_type> distribution;
  std::mt19937 rng;
};
