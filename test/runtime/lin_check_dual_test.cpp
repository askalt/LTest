#include <gtest/gtest.h>

#include <queue>

#include "coroutine_queue.h"
#include "lincheck.h"
#include "lincheck_dual.h"
#include "scheduler.h"
#include "stackfulltask_mock.h"

using CoroutineQueue = spec::CoroutineQueue<>;

namespace LinearizabilityDualCheckerTest {
using ::testing::AnyNumber;
using ::testing::Return;

std::function<std::shared_ptr<BlockingMethod>(CoroutineQueue*, void* args)>
    send = [](CoroutineQueue* c,
              [[maybe_unused]] void* args) -> std::shared_ptr<BlockingMethod> {
  // TODO: check type
  auto real_args = reinterpret_cast<std::tuple<int>*>(args);
  return std::shared_ptr<BlockingMethod>(
      new BlockingMethodWrapper<CoroutineQueue::SendPromise>(
          c->Send(std::get<0>(*real_args))));
};
std::function<std::shared_ptr<BlockingMethod>(CoroutineQueue*, void* args)>
    receive =
        [](CoroutineQueue* c,
           [[maybe_unused]] void* args) -> std::shared_ptr<BlockingMethod> {
  return std::shared_ptr<BlockingMethod>(
      new BlockingMethodWrapper<CoroutineQueue::ReceivePromise>(c->Receive()));
};

std::function<int(CoroutineQueue*, void* args)> size =
    [](CoroutineQueue* c, [[maybe_unused]] void* args) -> int {
  return c->ReceiversCount();
};

TEST(LinearizabilityDualCheckerQueueTest, SmallLinearizableHistory) {
  CoroutineQueue q{};

  LinearizabilityDualChecker<CoroutineQueue> checker(
      LinearizabilityDualChecker<CoroutineQueue>::MethodMap{
          {"send", send},
          {"receive", receive},
      },
      q);

  auto first_task_args = std::make_unique<std::tuple<int>>(std::tuple<int>{3});
  DualTask first_task = CreateMockDualTask(
      "send", 0, reinterpret_cast<void*>(first_task_args.get()));

  auto second_task_args = std::make_unique<std::tuple<>>(std::tuple<>{});
  DualTask second_task = CreateMockDualTask(
      "receive", 3, reinterpret_cast<void*>(second_task_args.get()));

  std::vector<HistoryEvent> history{};
  history.emplace_back(RequestInvoke(first_task, 0));
  history.emplace_back(RequestInvoke(second_task, 1));
  history.emplace_back(RequestResponse(first_task, 0));
  history.emplace_back(RequestResponse(second_task, 1));
  history.emplace_back(FollowUpInvoke(second_task, 1));
  history.emplace_back(FollowUpInvoke(first_task, 0));
  history.emplace_back(FollowUpResponse(first_task, 0));
  history.emplace_back(FollowUpResponse(second_task, 1));

  EXPECT_EQ(checker.Check(history), true);
}

TEST(LinearizabilityDualCheckerQueueTest, SmallUnlinearizableHistory) {
  CoroutineQueue q;
  LinearizabilityDualChecker<CoroutineQueue> checker(
      LinearizabilityDualChecker<CoroutineQueue>::MethodMap{
          {"send", send},
          {"receive", receive},
      },
      q);

  auto first_task_args = std::make_unique<std::tuple<int>>(std::tuple<int>{3});
  DualTask first_task = CreateMockDualTask(
      "send", 0, reinterpret_cast<void*>(first_task_args.get()));

  auto second_task_args = std::make_unique<std::tuple<>>(std::tuple<>{});
  DualTask second_task = CreateMockDualTask(
      "receive", 2, reinterpret_cast<void*>(second_task_args.get()));

  std::vector<HistoryEvent> history{};
  history.emplace_back(RequestInvoke(first_task, 0));
  history.emplace_back(RequestInvoke(second_task, 1));
  history.emplace_back(RequestResponse(first_task, 0));
  history.emplace_back(RequestResponse(second_task, 1));
  history.emplace_back(FollowUpInvoke(second_task, 1));
  history.emplace_back(FollowUpInvoke(first_task, 0));
  history.emplace_back(FollowUpResponse(first_task, 0));
  history.emplace_back(FollowUpResponse(second_task, 1));

  EXPECT_EQ(checker.Check(history), false);
}

TEST(LinearizabilityDualCheckerQueueTest, SmallUnlinearizableHistoryBadSend) {
  CoroutineQueue q;
  LinearizabilityDualChecker<CoroutineQueue> checker(
      LinearizabilityDualChecker<CoroutineQueue>::MethodMap{
          {"send", send},
          {"receive", receive},
      },
      q);

  auto first_task_args = std::make_unique<std::tuple<int>>(std::tuple<int>{3});
  DualTask first_task = CreateMockDualTask(
      "send", 0, reinterpret_cast<void*>(first_task_args.get()));

  auto second_task_args = std::make_unique<std::tuple<>>(std::tuple<>{});
  DualTask second_task = CreateMockDualTask(
      "receive", 3, reinterpret_cast<void*>(second_task_args.get()));

  auto third_task_args = std::make_unique<std::tuple<>>(std::tuple<>{});
  DualTask third_task = CreateMockDualTask(
      "receive", 3, reinterpret_cast<void*>(third_task_args.get()));

  std::vector<HistoryEvent> history{};
  history.emplace_back(RequestInvoke(first_task, 0));
  history.emplace_back(RequestInvoke(second_task, 1));
  history.emplace_back(RequestResponse(first_task, 0));
  history.emplace_back(RequestResponse(second_task, 1));
  history.emplace_back(FollowUpInvoke(second_task, 1));
  history.emplace_back(FollowUpInvoke(first_task, 0));
  history.emplace_back(FollowUpResponse(first_task, 0));
  history.emplace_back(FollowUpResponse(second_task, 1));
  history.emplace_back(RequestInvoke(third_task, 0));
  history.emplace_back(RequestResponse(third_task, 0));
  history.emplace_back(FollowUpInvoke(third_task, 1));
  history.emplace_back(FollowUpResponse(third_task, 1));

  EXPECT_EQ(checker.Check(history), false);
}

TEST(LinearizabilityDualCheckerQueueTest,
     SmallLinearizableHistoryWithNonBlocking) {
  CoroutineQueue q;
  LinearizabilityDualChecker<CoroutineQueue> checker(
      LinearizabilityDualChecker<CoroutineQueue>::MethodMap{
          {"send", send}, {"receive", receive}, {"size", size}},
      q);

  auto first_task_args = std::make_unique<std::tuple<>>(std::tuple<>{});
  DualTask first_task = CreateMockDualTask(
      "receive", 3, reinterpret_cast<void*>(first_task_args.get()));

  auto second_task_args = std::make_unique<std::tuple<int>>(std::tuple<int>{3});
  DualTask second_task = CreateMockDualTask(
      "send", 0, reinterpret_cast<void*>(second_task_args.get()));

  auto third_task_args = std::make_unique<std::tuple<>>(std::tuple<>{});
  Task third_task =
      CreateMockTask("size", 1, reinterpret_cast<void*>(third_task_args.get()));

  std::vector<HistoryEvent> history{};
  history.emplace_back(Invoke(third_task, 4));
  history.emplace_back(RequestInvoke(first_task, 0));
  history.emplace_back(RequestInvoke(second_task, 1));
  history.emplace_back(RequestResponse(first_task, 0));
  history.emplace_back(RequestResponse(second_task, 1));
  history.emplace_back(FollowUpInvoke(second_task, 1));
  history.emplace_back(FollowUpInvoke(first_task, 0));
  history.emplace_back(FollowUpResponse(first_task, 0));
  history.emplace_back(FollowUpResponse(second_task, 1));
  history.emplace_back(Response(third_task, 1, 4));

  get_inv_res_full_mapping(history);
  EXPECT_EQ(checker.Check(history), true);
}

TEST(LinearizabilityDualCheckerQueueTest, NonLinearizableHistoryWithBlocking) {
  CoroutineQueue q;
  LinearizabilityDualChecker<CoroutineQueue> checker(
      LinearizabilityDualChecker<CoroutineQueue>::MethodMap{
          {"send", send},
          {"receive", receive},
      },
      q);

  auto first_receive_args = std::make_unique<std::tuple<>>(std::tuple<>{});
  DualTask first_receive = CreateMockDualTask(
      "receive", 778, reinterpret_cast<void*>(first_receive_args.get()));

  auto first_send_args =
      std::make_unique<std::tuple<int>>(std::tuple<int>{778});
  DualTask first_send = CreateMockDualTask(
      "send", 0, reinterpret_cast<void*>(first_send_args.get()));

  auto second_receive_args = std::make_unique<std::tuple<>>(std::tuple<>{});
  DualTask second_receive = CreateMockDualTask(
      "receive", 0, reinterpret_cast<void*>(second_receive_args.get()));

  std::vector<HistoryEvent> history{};
  history.emplace_back(RequestInvoke(first_receive, 1));     // 0
  history.emplace_back(RequestResponse(first_receive, 1));   // 1
  history.emplace_back(FollowUpInvoke(first_receive, 1));    // 2
  history.emplace_back(RequestInvoke(first_send, 0));        // 3
  history.emplace_back(FollowUpResponse(first_receive, 1));  // 4
  history.emplace_back(RequestInvoke(second_receive, 1));    // 5
  history.emplace_back(RequestResponse(second_receive, 1));  // 6
  history.emplace_back(FollowUpInvoke(second_receive, 1));   // 7
  history.emplace_back(RequestResponse(first_send, 0));      // 8
  history.emplace_back(FollowUpInvoke(first_send, 1));       // 9
  history.emplace_back(FollowUpResponse(first_send, 0));     // 10

  EXPECT_EQ(checker.Check(history), true);
}

TEST(LinearizabilityDualCheckerQueueTest,
     NonLinearizableHistoryWithWrongReturnsOrder) {
  CoroutineQueue q;
  LinearizabilityDualChecker<CoroutineQueue> checker(
      LinearizabilityDualChecker<CoroutineQueue>::MethodMap{
          {"send", send},
          {"receive", receive},
      },
      q);

  // receive 2 from the channel
  auto first_receive_args = std::make_unique<std::tuple<>>(std::tuple<>{});
  DualTask first_receive = CreateMockDualTask(
      "receive", 2, reinterpret_cast<void*>(first_receive_args.get()));

  // send 1 to the channel
  auto first_send_args = std::make_unique<std::tuple<int>>(std::tuple<int>{1});
  DualTask first_send = CreateMockDualTask(
      "send", 0, reinterpret_cast<void*>(first_send_args.get()));

  // receive 1 from the channel
  auto second_receive_args = std::make_unique<std::tuple<>>(std::tuple<>{});
  DualTask second_receive = CreateMockDualTask(
      "receive", 1, reinterpret_cast<void*>(second_receive_args.get()));

  // send 2 into the channel
  auto second_send_args = std::make_unique<std::tuple<int>>(std::tuple<int>{2});
  DualTask second_send = CreateMockDualTask(
      "send", 0, reinterpret_cast<void*>(second_send_args.get()));

  std::vector<HistoryEvent> history{};
  history.emplace_back(RequestInvoke(first_receive, 1));
  history.emplace_back(RequestResponse(first_receive, 1));

  history.emplace_back(RequestInvoke(first_send, 1));
  history.emplace_back(RequestResponse(first_send, 1));

  history.emplace_back(RequestInvoke(second_receive, 1));
  history.emplace_back(RequestResponse(second_receive, 1));

  history.emplace_back(FollowUpInvoke(first_receive, 1));
  history.emplace_back(FollowUpResponse(first_receive, 1));

  history.emplace_back(FollowUpInvoke(first_send, 1));
  history.emplace_back(FollowUpResponse(first_send, 1));

  history.emplace_back(RequestInvoke(second_send, 1));
  history.emplace_back(RequestResponse(second_send, 1));

  history.emplace_back(FollowUpInvoke(second_send, 1));
  history.emplace_back(FollowUpResponse(second_send, 1));

  history.emplace_back(FollowUpInvoke(second_receive, 1));
  history.emplace_back(FollowUpResponse(second_receive, 1));

  EXPECT_EQ(checker.Check(history), false);
}

TEST(LinearizabilityDualCheckerQueueTest, SmallLinearizableHistoryWithoutEnd) {
  CoroutineQueue q;
  LinearizabilityDualChecker<CoroutineQueue> checker(
      LinearizabilityDualChecker<CoroutineQueue>::MethodMap{
          {"send", send},
          {"receive", receive},
      },
      q);

  auto first_receive_args = std::make_unique<std::tuple<>>(std::tuple<>{});
  DualTask first_receive = CreateMockDualTask(
      "receive", 1, reinterpret_cast<void*>(first_receive_args.get()));

  auto first_send_args = std::make_unique<std::tuple<int>>(std::tuple<int>{1});
  DualTask first_send = CreateMockDualTask(
      "send", 0, reinterpret_cast<void*>(first_send_args.get()));

  std::vector<HistoryEvent> history{};

  history.emplace_back(RequestInvoke(first_receive, 1));    // 0
  history.emplace_back(RequestResponse(first_receive, 1));  // 1
  history.emplace_back(FollowUpInvoke(first_receive, 1));   // 2

  history.emplace_back(RequestInvoke(first_send, 0));  // 3

  history.emplace_back(FollowUpResponse(first_receive, 1));  // 4

  EXPECT_EQ(checker.Check(history), true);
}

TEST(LinearizabilityDualCheckerQueueTest, LinearizableHistoryWithoutEnd) {
  CoroutineQueue q;
  LinearizabilityDualChecker<CoroutineQueue> checker(
      LinearizabilityDualChecker<CoroutineQueue>::MethodMap{
          {"send", send},
          {"receive", receive},
      },
      q);
  // receive 2 from the channel
  auto first_receive_args = std::make_unique<std::tuple<>>(std::tuple<>{});
  DualTask first_receive = CreateMockDualTask(
      "receive", 1, reinterpret_cast<void*>(first_receive_args.get()));

  // send 1 to the channel
  auto first_send_args = std::make_unique<std::tuple<int>>(std::tuple<int>{1});
  DualTask first_send = CreateMockDualTask(
      "send", 0, reinterpret_cast<void*>(first_send_args.get()));

  // receive 1 from the channel
  auto second_receive_args = std::make_unique<std::tuple<>>(std::tuple<>{});
  DualTask second_receive = CreateMockDualTask(
      "receive", 2, reinterpret_cast<void*>(second_receive_args.get()));

  // send 2 into the channel
  auto second_send_args = std::make_unique<std::tuple<int>>(std::tuple<int>{2});
  DualTask second_send = CreateMockDualTask(
      "send", 0, reinterpret_cast<void*>(second_send_args.get()));

  std::vector<HistoryEvent> history{};
  history.emplace_back(RequestInvoke(first_receive, 1));    // 0
  history.emplace_back(RequestResponse(first_receive, 1));  // 1
  history.emplace_back(FollowUpInvoke(first_receive, 1));   // 2

  history.emplace_back(RequestInvoke(first_send, 0));  // 3

  history.emplace_back(FollowUpResponse(first_receive, 1));  // 4
  history.emplace_back(RequestInvoke(second_receive, 1));    // 5
  history.emplace_back(RequestResponse(second_receive, 1));  // 6
  history.emplace_back(FollowUpInvoke(second_receive, 1));   // 7

  history.emplace_back(RequestResponse(first_send, 0));   // 8
  history.emplace_back(FollowUpInvoke(first_send, 0));    // 9
  history.emplace_back(FollowUpResponse(first_send, 0));  // 10
  history.emplace_back(RequestInvoke(second_send, 0));    // 11

  history.emplace_back(FollowUpResponse(second_receive, 1));  // 12

  EXPECT_EQ(checker.Check(history), true);
}

TEST(LinearizabilityDualCheckerQueueTest, LinearizableHistoryWithoutEnd2) {
  CoroutineQueue q;
  LinearizabilityDualChecker<CoroutineQueue> checker(
      LinearizabilityDualChecker<CoroutineQueue>::MethodMap{
          {"send", send},
          {"receive", receive},
      },
      q);

  auto first_receive_args = std::make_unique<std::tuple<>>(std::tuple<>{});
  DualTask first_receive = CreateMockDualTask(
      "receive", 1, reinterpret_cast<void*>(first_receive_args.get()));

  auto second_receive_args = std::make_unique<std::tuple<>>(std::tuple<>{});
  DualTask second_receive = CreateMockDualTask(
      "receive", 2, reinterpret_cast<void*>(second_receive_args.get()));

  auto first_send_args = std::make_unique<std::tuple<int>>(std::tuple<int>{1});
  DualTask first_send = CreateMockDualTask(
      "send", 0, reinterpret_cast<void*>(first_send_args.get()));

  auto second_send_args = std::make_unique<std::tuple<int>>(std::tuple<int>{2});
  DualTask second_send = CreateMockDualTask(
      "send", 0, reinterpret_cast<void*>(second_send_args.get()));

  std::vector<HistoryEvent> history{};

  history.emplace_back(RequestInvoke(first_receive, 0));
  history.emplace_back(RequestResponse(first_receive, 0));
  history.emplace_back(FollowUpInvoke(first_receive, 0));

  history.emplace_back(RequestInvoke(first_send, 1));

  history.emplace_back(FollowUpResponse(first_receive, 0));
  history.emplace_back(RequestInvoke(second_receive, 0));
  history.emplace_back(RequestResponse(second_receive, 0));
  history.emplace_back(FollowUpInvoke(second_receive, 0));

  history.emplace_back(RequestResponse(first_send, 1));
  history.emplace_back(FollowUpInvoke(first_send, 1));
  history.emplace_back(FollowUpResponse(first_send, 1));
  history.emplace_back(RequestInvoke(second_send, 1));

  history.emplace_back(FollowUpResponse(second_receive, 0));

  EXPECT_EQ(checker.Check(history), true);
}

TEST(LinearizabilityDualCheckerQueueTest, LinearizableHistoryWithoutEnd3) {
  CoroutineQueue q;
  LinearizabilityDualChecker<CoroutineQueue> checker(
      LinearizabilityDualChecker<CoroutineQueue>::MethodMap{
          {"send", send},
          {"receive", receive},
      },
      q);

  auto first_receive_args = std::make_unique<std::tuple<>>(std::tuple<>{});
  DualTask first_receive = CreateMockDualTask(
      "receive", 1, reinterpret_cast<void*>(first_receive_args.get()));

  auto first_send_args = std::make_unique<std::tuple<int>>(std::tuple<int>{1});
  DualTask first_send = CreateMockDualTask(
      "send", 0, reinterpret_cast<void*>(first_send_args.get()));

  std::vector<HistoryEvent> history{};

  history.emplace_back(RequestInvoke(first_receive, 1));
  history.emplace_back(RequestResponse(first_receive, 1));
  history.emplace_back(FollowUpInvoke(first_receive, 1));

  history.emplace_back(RequestInvoke(first_send, 0));

  history.emplace_back(FollowUpResponse(first_receive, 1));

  EXPECT_EQ(checker.Check(history), true);
}

TEST(LinearizabilityDualCheckerQueueTest, EmptyHistory) {
  CoroutineQueue q;
  LinearizabilityDualChecker<CoroutineQueue> checker(
      LinearizabilityDualChecker<CoroutineQueue>::MethodMap{
          {"send", send},
          {"receive", receive},
      },
      q);
  std::vector<HistoryEvent> history{};

  EXPECT_EQ(checker.Check(history), true);
}

};  // namespace LinearizabilityDualCheckerTest